/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "runtime.hpp"
#include "type_matcher.hpp"
#include <shards/common_types.hpp>
#include "core/foundation.hpp"
#include "foundation.hpp"
#include <shards/shards.h>
#include <shards/shards.hpp>
#include "shards/ops.hpp"
#include "shared.hpp"
#include <shards/utility.hpp>
#include <shards/inlined.hpp>
#include "inline.hpp"
#include "async.hpp"
#include <boost/asio/thread_pool.hpp>
#include <boost/filesystem.hpp>
#include <boost/stacktrace.hpp>
#include <csignal>
#include <cstdarg>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <string.h>
#include <unordered_set>
#include <log/log.hpp>
#include <shared_mutex>
#include <boost/atomic/atomic_ref.hpp>
#include <boost/container/small_vector.hpp>
#include <shards/fast_string/fast_string.hpp>
#include <shards/wire_dsl.hpp>
#include "utils.hpp"

namespace fs = boost::filesystem;

using namespace shards;

#ifdef __EMSCRIPTEN__
// clang-format off
EM_JS(void, sh_emscripten_init, (), {
  // inject some of our types
  if(typeof globalThis.shards === 'undefined') {
    globalThis.shards = {};
  }
  if(typeof globalThis.shards.bonds === 'undefined') {
    globalThis.shards.bonds = {};
  }
  if(typeof globalThis.ShardsBonder === 'undefined') {
    globalThis.ShardsBonder = class ShardsBonder {
      constructor(promise) {
        this.finished = false;
        this.hadErrors = false;
        this.promise = promise;
        this.result = null;
      }

      async run() {
        try {
          this.result = await this.promise;
        } catch (err) {
          console.error(err);
          this.hadErrors = true;
        }
        this.finished = true;
      }
    };
  }
});
// clang-format on
#endif

namespace shards {
#ifdef SH_COMPRESSED_STRINGS
SHOptionalString getCompiledCompressedString(uint32_t id) {
  static std::remove_pointer_t<decltype(Globals::CompressedStrings)> CompiledCompressedStrings;
  if (GetGlobals().CompressedStrings == nullptr)
    GetGlobals().CompressedStrings = &CompiledCompressedStrings;

  auto it = CompiledCompressedStrings.find(id);
  if (it != CompiledCompressedStrings.end()) {
    auto val = it->second;
    val.crc = id; // make sure we return with crc to allow later lookups!
    return val;
  } else {
    return SHOptionalString{nullptr, 0};
  }
}

#include <shards/core/shccstrings.hpp>

static oneapi::tbb::concurrent_unordered_map<uint32_t, std::string> strings_storage;

void decompressStrings() {
  if (!shards::GetGlobals().CompressedStrings) {
    throw shards::SHException("String storage was null");
  }

  // run the script to populate compressed strings
  auto bytes = Var(__shards_compressed_strings);
  auto wire = ::shards::Wire("decompress strings").let(bytes).shard("Brotli.Decompress").shard("FromBytes");
  auto mesh = SHMesh::make();
  mesh->schedule(wire);
  mesh->tick();
  if (wire->finishedOutput.valueType != SHType::Seq) {
    throw shards::SHException("Failed to decompress strings!");
  }

  for (uint32_t i = 0; i < wire->finishedOutput.payload.seqValue.len; i++) {
    auto pair = wire->finishedOutput.payload.seqValue.elements[i];
    if (pair.valueType != SHType::Seq || pair.payload.seqValue.len != 2) {
      throw shards::SHException("Failed to decompress strings!");
    }
    auto crc = pair.payload.seqValue.elements[0];
    auto str = pair.payload.seqValue.elements[1];
    if (crc.valueType != SHType::Int || str.valueType != SHType::String) {
      throw shards::SHException("Failed to decompress strings!");
    }
    auto emplaced = strings_storage.emplace(uint32_t(crc.payload.intValue), str.payload.stringValue);
    auto &s = emplaced.first->second;
    SHOptionalString ls{s.c_str(), uint32_t(crc.payload.intValue)};
    (*shards::GetGlobals().CompressedStrings).emplace(uint32_t(crc.payload.intValue), ls);
  }
}
#else
SHOptionalString setCompiledCompressedString(uint32_t id, const char *str) {
  static std::remove_pointer_t<decltype(Globals::CompressedStrings)> CompiledCompressedStrings;
  if (GetGlobals().CompressedStrings == nullptr)
    GetGlobals().CompressedStrings = &CompiledCompressedStrings;

  SHOptionalString ls{str, id};
  CompiledCompressedStrings.emplace(id, ls);
  return ls;
}
#endif

#ifdef SH_USE_UBSAN
extern "C" void __sanitizer_set_report_path(const char *path);
#endif

void loadExternalShards(std::string from) {
  namespace fs = boost::filesystem;
  auto root = fs::path(from);
  auto pluginPath = root / "externals";
  if (!fs::exists(pluginPath))
    return;

  for (auto &p : fs::recursive_directory_iterator(pluginPath)) {
    if (p.status().type() == fs::file_type::regular_file) {
      auto ext = p.path().extension();
      if (ext == ".dll" || ext == ".so" || ext == ".dylib") {
        auto filename = p.path().filename();
        auto dllstr = p.path().string();
        SHLOG_INFO("Loading external dll: {} path: {}", filename, dllstr);
#if _WIN32
        auto handle = LoadLibraryExA(dllstr.c_str(), NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!handle) {
          SHLOG_ERROR("LoadLibrary failed, error: {}", GetLastError());
        }
#elif defined(__linux__) || defined(__APPLE__)
        auto handle = dlopen(dllstr.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
          SHLOG_ERROR("LoadLibrary failed, error: {}", dlerror());
        }
#endif
      }
    }
  }
}

#ifdef TRACY_ENABLE
GlobalTracy &GetTracy() {
  static GlobalTracy tracy;
  return tracy;
}

#ifdef TRACY_FIBERS
std::vector<SHWire *> &getCoroWireStack() {
  // Here is the thing, this currently works.. but only because we don't move coroutines between threads
  // When we will do if we do this will break...
  static thread_local std::vector<SHWire *> wireStack;
  return wireStack;
}
#endif
#endif

#ifdef SH_USE_TSAN
std::vector<SHWire *> &getCoroWireStack2() {
  // Here is the thing, this currently works.. but only because we don't move coroutines between threads
  // When we will do if we do this will break...
  static thread_local std::vector<SHWire *> wireStack;
  return wireStack;
}
#endif

extern void registerModuleShards(SHCore *core);
void registerShards() {
  SHLOG_DEBUG("Registering shards");

  // at this point we might have some auto magical static linked shard already
  // keep them stored here and re-register them
  // as we assume the observers were setup in this call caller so too late for
  // them
  std::vector<std::pair<std::string_view, SHShardConstructor>> earlyshards;
  for (auto &pair : GetGlobals().ShardsRegister) {
    earlyshards.push_back(pair);
  }
  GetGlobals().ShardsRegister.clear();

  SHCore *core = shardsInterface(SHARDS_CURRENT_ABI);
  registerModuleShards(core);

  // Enums are auto registered we need to propagate them to observers
  for (auto &einfo : GetGlobals().EnumTypesRegister) {
    int32_t vendorId = (int32_t)((einfo.first & 0xFFFFFFFF00000000) >> 32);
    int32_t enumId = (int32_t)(einfo.first & 0x00000000FFFFFFFF);
    for (auto &pobs : GetGlobals().Observers) {
      if (pobs.expired())
        continue;
      auto obs = pobs.lock();
      obs->registerEnumType(vendorId, enumId, einfo.second);
    }
  }

  // re run early shards registration!
  for (auto &pair : earlyshards) {
    registerShard(pair.first, pair.second);
  }

  // finally iterate shard directory and load external dlls
  loadExternalShards(std::string(GetGlobals().ExePath.c_str()));
  if (GetGlobals().RootPath != GetGlobals().ExePath) {
    loadExternalShards(std::string(GetGlobals().RootPath.c_str()));
  }
}

Shard *createShard(std::string_view name) {
  ZoneScoped;
  ZoneName(name.data(), name.size());

  auto it = GetGlobals().ShardsRegister.find(name);
  if (it == GetGlobals().ShardsRegister.end()) {
    return nullptr;
  }

  auto shard = it->second();

  shards::setInlineShardId(shard, name);
  shard->nameLength = uint32_t(name.length());

#ifndef NDEBUG
  auto props = shard->properties(shard);
  if (props) {
    shassert(props->opaque && props->api && "Shard properties are not initialized!");
    auto experimental = props->api->tableGet(*props, Var("experimental"));
    if (experimental) {
      shassert(experimental->valueType == SHType::Bool);
      SHLOG_WARNING("Experimental shard used: {}", name);
    }
  }
#endif

  return shard;
}

inline void setupRegisterLogging() {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
  logging::setupDefaultLoggerConditional();
#endif
}

void registerShard(std::string_view name, SHShardConstructor constructor, std::string_view fullTypeName) {
  // setupRegisterLogging();
  // SHLOG_TRACE("registerBlock({})", name);

  auto findIt = GetGlobals().ShardsRegister.find(name);
  if (findIt == GetGlobals().ShardsRegister.end()) {
    GetGlobals().ShardsRegister.emplace(name, constructor);
  } else {
    GetGlobals().ShardsRegister[name] = constructor;
    SHLOG_WARNING("Overriding shard: {}", name);
  }

  GetGlobals().ShardNamesToFullTypeNames[name] = fullTypeName;

  for (auto &pobs : GetGlobals().Observers) {
    if (pobs.expired())
      continue;
    auto obs = pobs.lock();
    obs->registerShard(name.data(), constructor);
  }
}

void registerObjectType(int32_t vendorId, int32_t typeId, SHObjectInfo info) {
  // setupRegisterLogging();
  // SHLOG_TRACE("registerObjectType({})", info.name);

  int64_t id = (int64_t)vendorId << 32 | typeId;
  auto typeName = std::string_view(info.name);

  auto findIt = GetGlobals().ObjectTypesRegister.find(id);
  if (findIt == GetGlobals().ObjectTypesRegister.end()) {
    GetGlobals().ObjectTypesRegister.insert(std::make_pair(id, info));
  } else {
    GetGlobals().ObjectTypesRegister[id] = info;
    SHLOG_WARNING("Overriding object type: {}", typeName);
  }

  auto findIt2 = GetGlobals().ObjectTypesRegisterByName.find(typeName);
  if (findIt2 == GetGlobals().ObjectTypesRegisterByName.end()) {
    GetGlobals().ObjectTypesRegisterByName.emplace(info.name, id);
  } else {
    GetGlobals().ObjectTypesRegisterByName[info.name] = id;
    SHLOG_WARNING("Overriding enum type by name: {}", typeName);
  }

  for (auto &pobs : GetGlobals().Observers) {
    if (pobs.expired())
      continue;
    auto obs = pobs.lock();
    obs->registerObjectType(vendorId, typeId, info);
  }
}

void registerEnumType(int32_t vendorId, int32_t typeId, SHEnumInfo info) {
  // setupRegisterLogging();
  // SHLOG_TRACE("registerEnumType({})", info.name);

  int64_t id = (int64_t)vendorId << 32 | typeId;
  auto enumName = std::string_view(info.name);

  auto findIt = GetGlobals().EnumTypesRegister.find(id);
  if (findIt == GetGlobals().EnumTypesRegister.end()) {
    GetGlobals().EnumTypesRegister.insert(std::make_pair(id, info));
  } else {
    GetGlobals().EnumTypesRegister[id] = info;
    SHLOG_WARNING("Overriding enum type: {}", enumName);
  }

  auto findIt2 = GetGlobals().EnumTypesRegisterByName.find(enumName);
  if (findIt2 == GetGlobals().EnumTypesRegisterByName.end()) {
    GetGlobals().EnumTypesRegisterByName.emplace(info.name, id);
  } else {
    GetGlobals().EnumTypesRegisterByName[info.name] = id;
    SHLOG_WARNING("Overriding enum type by name: {}", enumName);
  }

  for (auto &pobs : GetGlobals().Observers) {
    if (pobs.expired())
      continue;
    auto obs = pobs.lock();
    obs->registerEnumType(vendorId, typeId, info);
  }
}

const SHObjectInfo *findObjectInfo(int32_t vendorId, int32_t typeId) {
  int64_t id = (int64_t)vendorId << 32 | typeId;
  auto it = shards::GetGlobals().ObjectTypesRegister.find(id);
  if (it != shards::GetGlobals().ObjectTypesRegister.end()) {
    return &it->second;
  }
  return nullptr;
}

int64_t findObjectTypeId(std::string_view name) {
  auto it = shards::GetGlobals().ObjectTypesRegisterByName.find(name);
  if (it != shards::GetGlobals().ObjectTypesRegisterByName.end()) {
    return it->second;
  }
  return 0;
}

const SHEnumInfo *findEnumInfo(int32_t vendorId, int32_t typeId) {
  int64_t id = (int64_t)vendorId << 32 | typeId;
  auto it = shards::GetGlobals().EnumTypesRegister.find(id);
  if (it != shards::GetGlobals().EnumTypesRegister.end()) {
    return &it->second;
  }
  return nullptr;
}

int64_t findEnumId(std::string_view name) {
  auto it = shards::GetGlobals().EnumTypesRegisterByName.find(name);
  if (it != shards::GetGlobals().EnumTypesRegisterByName.end()) {
    return it->second;
  }
  return 0;
}

void registerRunLoopCallback(std::string_view eventName, SHCallback callback) {
  shards::GetGlobals().RunLoopHooks[eventName] = callback;
}

void unregisterRunLoopCallback(std::string_view eventName) {
  auto findIt = shards::GetGlobals().RunLoopHooks.find(eventName);
  if (findIt != shards::GetGlobals().RunLoopHooks.end()) {
    shards::GetGlobals().RunLoopHooks.erase(findIt);
  }
}

void registerExitCallback(std::string_view eventName, SHCallback callback) {
  shards::GetGlobals().ExitHooks[eventName] = callback;
}

void unregisterExitCallback(std::string_view eventName) {
  auto findIt = shards::GetGlobals().ExitHooks.find(eventName);
  if (findIt != shards::GetGlobals().ExitHooks.end()) {
    shards::GetGlobals().ExitHooks.erase(findIt);
  }
}

void registerWire(SHWire *wire) {
  std::shared_ptr<SHWire> sc(wire);
  shards::GetGlobals().GlobalWires[wire->name] = sc;
}

void unregisterWire(SHWire *wire) {
  auto findIt = shards::GetGlobals().GlobalWires.find(wire->name);
  if (findIt != shards::GetGlobals().GlobalWires.end()) {
    shards::GetGlobals().GlobalWires.erase(findIt);
  }
}

void callExitCallbacks() {
  // Iterate backwards
  for (auto it = shards::GetGlobals().ExitHooks.begin(); it != shards::GetGlobals().ExitHooks.end(); ++it) {
    it->second();
  }
}

entt::id_type findId(SHContext *ctx) noexcept {
  entt::id_type id = entt::null;

  // try find a wire id
  // from top to bottom of wire stack
  {
    auto rit = ctx->wireStack.rbegin();
    for (; rit != ctx->wireStack.rend(); ++rit) {
      // prioritize local variables
      auto wire = *rit;
      if (wire->id != entt::null) {
        id = wire->id;
        break;
      }
    }
  }

  return id;
}

SHVar *referenceWireVariable(SHWire *wire, std::string_view name) {
  SHVar &v = wire->getVariable(ToSWL(name));
  v.refcount++;
  v.flags |= SHVAR_FLAGS_REF_COUNTED;
  return &v;
}

SHVar *referenceWireVariable(SHWireRef wire, std::string_view name) {
  auto swire = SHWire::sharedFromRef(wire);
  return referenceWireVariable(swire.get(), name);
}

SHVar *referenceGlobalVariable(SHContext *ctx, std::string_view name) {
  auto mesh = ctx->main->mesh.lock();
  shassert(mesh);

  SHVar &v = mesh->getVariable(ToSWL(name));
  v.refcount++;
  if (v.refcount == 1) {
    SHLOG_TRACE("Creating a global variable, wire: {} name: {}", ctx->wireStack.back()->name, name);
  }
  v.flags |= SHVAR_FLAGS_REF_COUNTED;
  return &v;
}

SHVar *referenceVariable(SHContext *ctx, std::string_view name) {
  // try find a wire variable
  // from top to bottom of wire stack
  {
    auto rit = ctx->wireStack.rbegin();
    for (; rit != ctx->wireStack.rend(); ++rit) {
      // prioritize local variables
      auto wire = *rit;
      auto ov = wire->getVariableIfExists(ToSWL(name));
      if (ov) {
        // found, lets get out here
        SHVar &cv = (*ov).get();
        cv.refcount++;
        cv.flags |= SHVAR_FLAGS_REF_COUNTED;
        return &cv;
      }
      // try external variables
      auto ev = wire->getExternalVariableIfExists(ToSWL(name));
      if (ev) {
        // found, lets get out here
        SHVar &cv = *ev;
        shassert((cv.flags & SHVAR_FLAGS_EXTERNAL) != 0);
        return &cv;
      }
      // if this wire is pure we break here and do not look further
      if (wire->pure) {
        goto create;
      }
    }
  }

  // try using mesh
  {
    auto mesh = ctx->main->mesh.lock();
    shassert(mesh);

    // Was not in wires.. find in mesh
    {
      auto ov = mesh->getVariableIfExists(ToSWL(name));
      if (ov) {
        // found, lets get out here
        SHVar &cv = (*ov).get();
        cv.refcount++;
        cv.flags |= SHVAR_FLAGS_REF_COUNTED;
        return &cv;
      }
    }

    // Was not in mesh directly.. try find in meshs refs
    {
      auto rv = mesh->getRefIfExists(ToSWL(name));
      if (rv) {
        SHLOG_TRACE("Referencing a parent node variable, wire: {} name: {}", ctx->wireStack.back()->name, name);
        // found, lets get out here
        rv->refcount++;
        rv->flags |= SHVAR_FLAGS_REF_COUNTED;
        return rv;
      }
    }
  }

create:
  // worst case create in current top wire!
  SHLOG_TRACE("Creating a variable, wire: {} name: {}", ctx->wireStack.back()->name, name);
  SHVar &cv = ctx->wireStack.back()->getVariable(ToSWL(name));
  shassert(cv.refcount == 0);
  cv.refcount++;
  // can safely set this here, as we are creating a new variable
  cv.flags = SHVAR_FLAGS_REF_COUNTED;
  return &cv;
}

void releaseVariable(SHVar *variable) {
  if (!variable)
    return;

  if ((variable->flags & SHVAR_FLAGS_EXTERNAL) != 0) {
    return;
  }

  shassert((variable->flags & SHVAR_FLAGS_REF_COUNTED) == SHVAR_FLAGS_REF_COUNTED);
  shassert(variable->refcount > 0);

  variable->refcount--;
  if (variable->refcount == 0) {
    SHLOG_TRACE("Destroying a variable (0 ref count), type: {}", type2Name(variable->valueType));
    destroyVar(*variable);
  }
}

SHWireState suspend(SHContext *context, double seconds) {
  if (unlikely(!context->shouldContinue())) {
    throw ActivationError(fmt::format("Trying to suspend a context that is not running! - state: {}", context->getState()));
  } else if (unlikely(!context->continuation)) {
    throw ActivationError("Trying to suspend a context without coroutine!");
  }

  if (seconds <= 0) {
    context->next = SHDuration(0);
  } else {
    context->next = SHClock::now().time_since_epoch() + SHDuration(seconds);
  }

  auto currentWire = context->currentWire();
  SH_CORO_SUSPENDED(currentWire);
  coroutineSuspend(*context->continuation);
  shassert(context->currentWire() == currentWire);
  SH_CORO_RESUMED(currentWire);

  return context->getState();
}

void hash_update(const SHVar &var, void *state);

#define SH_WIRE_SET_STACK(prefix)                                                              \
  std::deque<std::unordered_set<const SHWire *>> &prefix##WiresStack() {                       \
    thread_local std::deque<std::unordered_set<const SHWire *>> s;                             \
    return s;                                                                                  \
  }                                                                                            \
  std::optional<std::unordered_set<const SHWire *> *> &prefix##WiresStorage() {                \
    thread_local std::optional<std::unordered_set<const SHWire *> *> wiresOpt;                 \
    return wiresOpt;                                                                           \
  }                                                                                            \
  std::unordered_set<const SHWire *> &prefix##Wires() {                                        \
    auto wiresPtr = *prefix##WiresStorage();                                                   \
    shassert(wiresPtr);                                                                        \
    return *wiresPtr;                                                                          \
  }                                                                                            \
  void prefix##WiresPush() { prefix##WiresStorage() = &prefix##WiresStack().emplace_front(); } \
  void prefix##WiresPop() {                                                                    \
    prefix##WiresStack().pop_front();                                                          \
    if (prefix##WiresStack().empty()) {                                                        \
      prefix##WiresStorage() = std::nullopt;                                                   \
    } else {                                                                                   \
      prefix##WiresStorage() = &prefix##WiresStack().front();                                  \
    }                                                                                          \
  }

SH_WIRE_SET_STACK(gathering);
SH_WIRE_SET_STACK(hashing);

template <typename T, bool HANDLES_RETURN, bool HASHED>
ALWAYS_INLINE SHWireState shardsActivation(T &shards, SHContext *context, const SHVar &wireInput, SHVar &output,
                                           SHVar *outHash = nullptr) noexcept {
  XXH3_state_s hashState; // optimized out in release if not HASHED
  if constexpr (HASHED) {
    shassert(outHash);
    XXH3_INITSTATE(&hashState);
    XXH3_128bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);
  }
  DEFER(if constexpr (HASHED) {
    auto digest = XXH3_128bits_digest(&hashState);
    outHash->valueType = SHType::Int2;
    outHash->payload.int2Value[0] = int64_t(digest.low64);
    outHash->payload.int2Value[1] = int64_t(digest.high64);
    SHLOG_TRACE("Hash digested {}", *outHash);
  });

  // store initial input
  auto input = wireInput;

  // find len based on shards type
  size_t len;
  if constexpr (std::is_same<T, Shards>::value || std::is_same<T, SHSeq>::value) {
    len = shards.len;
  } else if constexpr (std::is_same<T, std::vector<ShardPtr>>::value) {
    len = shards.size();
  } else {
    len = 0;
    SHLOG_FATAL("Unreachable shardsActivation case");
  }

  for (size_t i = 0; i < len; i++) {
    ShardPtr blk;
    if constexpr (std::is_same<T, Shards>::value) {
      blk = shards.elements[i];
    } else if constexpr (std::is_same<T, SHSeq>::value) {
      blk = shards.elements[i].payload.shardValue;
    } else if constexpr (std::is_same<T, std::vector<ShardPtr>>::value) {
      blk = shards[i];
    } else {
      blk = nullptr;
      SHLOG_FATAL("Unreachable shardsActivation case");
    }

    if constexpr (HASHED) {
      const auto shardHash = blk->hash(blk);
      SHLOG_TRACE("Hashing shard {}", shardHash);
      XXH3_128bits_update(&hashState, &shardHash, sizeof(shardHash));

      SHLOG_TRACE("Hashing input {}", input);
      hash_update(input, &hashState);

      const auto params = blk->parameters(blk);
      for (uint32_t nParam = 0; nParam < params.len; nParam++) {
        const auto param = blk->getParam(blk, nParam);
        SHLOG_TRACE("Hashing param {}", param);
        hash_update(param, &hashState);
      }

      output = activateShard(blk, context, input);
      SHLOG_TRACE("Hashing output {}", output);
      hash_update(output, &hashState);
    } else {
      output = activateShard(blk, context, input);
    }

    // Deal with aftermath of activation
    if (unlikely(!context->shouldContinue())) {
      auto state = context->getState();
      switch (state) {
      case SHWireState::Return:
        if constexpr (HANDLES_RETURN)
          context->continueFlow();
        return SHWireState::Return;
      case SHWireState::Error:
        SHLOG_ERROR("Shard activation error, failed shard: {}, error: {}, line: {}, column: {}", blk->name(blk),
                    context->getErrorMessage(), blk->line, blk->column);
      case SHWireState::Stop:
      case SHWireState::Restart:
        return state;
      case SHWireState::Rebase:
        // reset input to wire one
        // and reset state
        input = wireInput;
        context->continueFlow();
        continue;
      case SHWireState::Continue:
        break;
      }
    }

    // Pass output to next block input
    input = output;
  }
  return SHWireState::Continue;
}

SHWireState activateShards(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return shardsActivation<Shards, false, false>(shards, context, wireInput, output);
}

SHWireState activateShards2(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return shardsActivation<Shards, true, false>(shards, context, wireInput, output);
}

SHWireState activateShards(SHSeq shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return shardsActivation<SHSeq, false, false>(shards, context, wireInput, output);
}

SHWireState activateShards2(SHSeq shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return shardsActivation<SHSeq, true, false>(shards, context, wireInput, output);
}

SHWireState activateShards(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output, SHVar &outHash) noexcept {
  return shardsActivation<Shards, false, true>(shards, context, wireInput, output, &outHash);
}

SHWireState activateShards2(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output, SHVar &outHash) noexcept {
  return shardsActivation<Shards, true, true>(shards, context, wireInput, output, &outHash);
}

bool matchTypes(const SHTypeInfo &inputType, const SHTypeInfo &receiverType, bool isParameter, bool strict,
                bool relaxEmptySeqCheck) {
  return TypeMatcher{.isParameter = isParameter, .strict = strict, .relaxEmptySeqCheck = relaxEmptySeqCheck}.match(inputType,
                                                                                                                   receiverType);
}

struct ValidationContext {
  std::unordered_map<std::string_view, SHExposedTypeInfo> inherited;
  std::unordered_map<std::string_view, SHExposedTypeInfo> exposed;
  std::unordered_set<std::string_view> variables;
  std::unordered_set<std::string_view> references;
  std::unordered_set<SHExposedTypeInfo> required;

  SHTypeInfo previousOutputType{};
  SHTypeInfo originalInputType{};

  Shard *bottom{};
  Shard *next{};
  SHWire *wire{};

  SHValidationCallback cb{};
  void *userData{};

  bool onWorkerThread{false};

  std::unordered_map<std::string_view, SHExposedTypeInfo> *fullRequired{nullptr};
};

void validateConnection(ValidationContext &ctx) {
  auto previousOutput = ctx.previousOutputType;

  auto inputInfos = ctx.bottom->inputTypes(ctx.bottom);
  auto inputMatches = false;
  // validate our generic input
  if (inputInfos.len == 1 && inputInfos.elements[0].basicType == SHType::None) {
    // in this case a None always matches
    inputMatches = true;
  } else {
    for (uint32_t i = 0; inputInfos.len > i; i++) {
      auto &inputInfo = inputInfos.elements[i];
      if (matchTypes(previousOutput, inputInfo, false, true, false)) {
        inputMatches = true;
        break;
      }
    }
  }

  if (!inputMatches) {
    const auto msg =
        fmt::format("Could not find a matching input type, shard: {} (line: {}, column: {}) expected: {}. Found instead: {}",
                    ctx.bottom->name(ctx.bottom), ctx.bottom->line, ctx.bottom->column, inputInfos, ctx.previousOutputType);
    ctx.cb(ctx.bottom, SHStringWithLen{msg.data(), msg.size()}, false, ctx.userData);
  }

  // infer and specialize types if we need to
  // If we don't we assume our output will be of the same type of the previous!
  if (ctx.bottom->compose) {
    SHInstanceData data{};
    data.shard = ctx.bottom;
    data.wire = ctx.wire;
    data.inputType = previousOutput;
    data.requiredVariables = ctx.fullRequired;
    if (ctx.next) {
      data.outputTypes = ctx.next->inputTypes(ctx.next);
    }
    data.onWorkerThread = ctx.onWorkerThread;

    // Pass all we got in the context!
    // notice that shards might add new records to this array
    for (auto &pair : ctx.exposed) {
      shards::arrayPush(data.shared, pair.second);
    }
    // and inherited
    for (auto &pair : ctx.inherited) {
      shards::arrayPush(data.shared, pair.second);
    }
    DEFER(shards::arrayFree(data.shared));

    // this ensures e.g. SetVariable exposedVars have right type from the actual
    // input type (previousOutput)!
    auto composeResult = ctx.bottom->compose(ctx.bottom, &data);
    if (composeResult.error.code != SH_ERROR_NONE) {
      std::string_view msg(composeResult.error.message.string, size_t(composeResult.error.message.len));
      SHLOG_ERROR("Error composing shard: {}, wire: {}", msg, ctx.wire ? ctx.wire->name : "(unwired)");
      throw ComposeError(msg);
    }
    ctx.previousOutputType = composeResult.result;
  } else {
    // Short-cut if it's just one type and not any type
    auto outputTypes = ctx.bottom->outputTypes(ctx.bottom);
    if (outputTypes.len == 1) {
      if (outputTypes.elements[0].basicType != SHType::Any) {
        ctx.previousOutputType = outputTypes.elements[0];
      } else {
        // Any type tho means keep previous output type!
        // Unless we require a specific input type, in that case
        // We assume we are not a passthru shard
        auto inputTypes = ctx.bottom->inputTypes(ctx.bottom);
        if (inputTypes.len == 1 && inputTypes.elements[0].basicType != SHType::Any) {
          ctx.previousOutputType = outputTypes.elements[0];
        }
      }
    } else {
      SHLOG_ERROR("Shard {} needs to implement the compose method", ctx.bottom->name(ctx.bottom));
      throw ComposeError("Shard has multiple possible output types and is missing the compose method");
    }
  }

#ifndef NDEBUG
  // do some sanity checks that also provide coverage on outputTypes
  if (!ctx.bottom->compose) {
    auto outputTypes = ctx.bottom->outputTypes(ctx.bottom);
    shards::IterableTypesInfo otypes(outputTypes);
    auto flowStopper = [&]() {
      if (strcmp(ctx.bottom->name(ctx.bottom), "Restart") == 0 || strcmp(ctx.bottom->name(ctx.bottom), "Stop") == 0 ||
          strcmp(ctx.bottom->name(ctx.bottom), "Return") == 0 || strcmp(ctx.bottom->name(ctx.bottom), "Fail") == 0) {
        return true;
      } else {
        return false;
      }
    }();

    auto blockHasValidOutputTypes =
        flowStopper || std::any_of(otypes.begin(), otypes.end(), [&](const auto &t) {
          return t.basicType == SHType::Any ||
                 (t.basicType == SHType::Seq && t.seqTypes.len == 1 && t.seqTypes.elements[0].basicType == SHType::Any &&
                  ctx.previousOutputType.basicType == SHType::Seq) || // any seq
                 (t.basicType == SHType::Table &&
                  // TODO find Any in table types
                  ctx.previousOutputType.basicType == SHType::Table) || // any table
                 t == ctx.previousOutputType;
        });
    if (!blockHasValidOutputTypes) {
      std::string blockName = ctx.bottom->name(ctx.bottom);
      auto msg = fmt::format("Shard {} doesn't have a valid output type", blockName);
      throw ComposeError(msg);
    }
  }
#endif

  // Grab those after type inference!
  auto exposedVars = ctx.bottom->exposedVariables(ctx.bottom);
  // Add the vars we expose
  for (uint32_t i = 0; exposedVars.len > i; i++) {
    auto &exposed_param = exposedVars.elements[i];
    std::string_view name(exposed_param.name);
    ctx.exposed[name] = exposed_param;

    // Reference mutability checks
    if (strcmp(ctx.bottom->name(ctx.bottom), "Ref") == 0) {
      // make sure we are not Ref-ing a Set
      // meaning target would be overwritten, yet Set will try to deallocate
      // it/manage it
      if (ctx.variables.count(name)) {
        // Error
        auto err = fmt::format(
            "Ref variable name already used as Set. Overwriting a previously Set variable with Ref is not allowed, name: {}",
            name);
        ctx.cb(ctx.bottom, SHStringWithLen{err.data(), err.size()}, false, ctx.userData);
      }
      ctx.references.insert(name);
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Set") == 0) {
      // make sure we are not Set-ing a Ref
      // meaning target memory could be any shard temporary buffer, yet Set will
      // try to deallocate it/manage it
      if (ctx.references.count(name)) {
        // Error
        auto err = fmt::format(
            "Set variable name already used as Ref. Overwriting a previously Ref variable with Set is not allowed, name: {}",
            name);
        ctx.cb(ctx.bottom, SHStringWithLen{err.data(), err.size()}, false, ctx.userData);
      }
      ctx.variables.insert(name);
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Update") == 0) {
      // make sure we are not Set-ing a Ref
      // meaning target memory could be any shard temporary buffer, yet Set will
      // try to deallocate it/manage it
      if (ctx.references.count(name)) {
        // Error
        auto err = fmt::format("Update variable name already used as Ref. Overwriting a previously Ref variable with Update is "
                               "not allowed, name: {}",
                               name);
        ctx.cb(ctx.bottom, SHStringWithLen{err.data(), err.size()}, false, ctx.userData);
      }
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Push") == 0) {
      // make sure we are not Push-ing a Ref
      // meaning target memory could be any shard temporary buffer, yet Push
      // will try to deallocate it/manage it
      if (ctx.references.count(name)) {
        // Error
        auto err = fmt::format(
            "Push variable name already used as Ref. Overwriting a previously Ref variable with Push is not allowed, name: {}",
            name);
        ctx.cb(ctx.bottom, SHStringWithLen{err.data(), err.size()}, false, ctx.userData);
      }
      ctx.variables.insert(name);
    }
  }

  // Finally do checks on what we consume
  auto requiredVar = ctx.bottom->requiredVariables(ctx.bottom);

  std::unordered_map<std::string, SHExposedTypeInfo> requiredVars;
  for (uint32_t i = 0; requiredVar.len > i; i++) {
    auto &required_param = requiredVar.elements[i];
    std::string name(required_param.name);
    requiredVars[name] = required_param;
  }

  // make sure we have the vars we need, collect first
  for (const auto &required : requiredVars) {
    auto matching = false;
    SHExposedTypeInfo match{};

    const auto &required_param = required.second;
    std::string_view name(required_param.name);
    if (name.find(' ') != std::string::npos) { // take only the first part of variable name
      // the remaining should be a table key which we don't care here
      name = name.substr(0, name.find(' '));
    }

    auto end = ctx.exposed.end();
    auto findIt = ctx.exposed.find(name);
    if (findIt == end) {
      end = ctx.inherited.end();
      findIt = ctx.inherited.find(name);
    }
    if (findIt == end) {
      auto err = fmt::format("Required variable not found: {}", name);
      // Warning only, delegate compose to decide
      ctx.cb(ctx.bottom, SHStringWithLen{err.data(), err.size()}, true, ctx.userData);
    } else {
      auto exposedType = findIt->second.exposedType;
      auto requiredType = required_param.exposedType;
      // Finally deep compare types
      if (matchTypes(exposedType, requiredType, false, true, false)) {
        matching = true;
      }
    }

    if (matching) {
      match = required_param;
    }

    if (!matching) {
      std::stringstream ss;
      ss << "Required types do not match currently exposed ones for variable '" << required.first
         << "' required possible types: ";
      auto &type = required.second;
      ss << "{\"" << type.name << "\" (" << type.exposedType << ")} ";

      ss << "exposed types: ";
      for (const auto &info : ctx.exposed) {
        auto &type = info.second;
        ss << "{\"" << type.name << "\" (" << type.exposedType << ")} ";
      }
      for (const auto &info : ctx.inherited) {
        auto &type = info.second;
        ss << "{\"" << type.name << "\" (" << type.exposedType << ")} ";
      }
      auto sss = ss.str();
      ctx.cb(ctx.bottom, SHStringWithLen{sss.data(), sss.size()}, false, ctx.userData);
    } else {
      // Add required stuff that we do not expose ourself
      if (ctx.exposed.find(match.name) == ctx.exposed.end())
        ctx.required.emplace(match);
    }
  }
}

SHComposeResult composeWire(const std::vector<Shard *> &wire, SHValidationCallback callback, void *userData,
                            SHInstanceData data) {
  ValidationContext ctx{};
  ctx.originalInputType = data.inputType;
  ctx.previousOutputType = data.inputType;
  ctx.cb = callback;
  ctx.wire = data.wire;
  ctx.userData = userData;
  ctx.onWorkerThread = data.onWorkerThread;
  ctx.fullRequired = reinterpret_cast<decltype(ValidationContext::fullRequired)>(data.requiredVariables);

  // add externally added variables
  if (ctx.wire) {
    for (const auto &[key, pVar] : ctx.wire->getExternalVariables()) {
      const SHExternalVariable &extVar = pVar;
      const SHVar &var = *extVar.var;
      shassert((var.flags & SHVAR_FLAGS_EXTERNAL) != 0);

      const SHTypeInfo *type{};
      if (extVar.type) {
        type = extVar.type;
      } else {
        auto hash = deriveTypeHash(var);
        TypeInfo *info = nullptr;
        if (ctx.wire->typesCache.find(hash) == ctx.wire->typesCache.end()) {
          info = &ctx.wire->typesCache.emplace(hash, TypeInfo(var, data)).first->second;
        } else {
          info = &ctx.wire->typesCache.at(hash);
        }
        type = &(const SHTypeInfo &)*info;
      }

      SHExposedTypeInfo expInfo{key.payload.stringValue, {}, *type, true /* mutable */};
      expInfo.exposed = var.flags & SHVAR_FLAGS_EXPOSED;
      std::string_view sName(key.payload.stringValue, key.payload.stringLen);
      ctx.inherited[sName] = expInfo;
    }

    // add present mesh variables as well if we have a mesh
    auto mesh = ctx.wire->mesh.lock();
    if (mesh) {
      for (auto &v : mesh->getVariables()) {
        // only add variables with metadata basically
        auto metadata = mesh->getMetadata(&v.second);
        if (metadata) {
          std::string_view sName(v.first.payload.stringValue, v.first.payload.stringLen);
          ctx.inherited[sName] = *metadata;
        }
      }
    }
  }

  if (data.shared.elements) {
    for (uint32_t i = 0; i < data.shared.len; i++) {
      auto &info = data.shared.elements[i];
      ctx.inherited[info.name] = info;
    }
  }

  size_t chsize = wire.size();
  for (size_t i = 0; i < chsize; i++) {
    Shard *blk = wire[i];
    ctx.next = nullptr;
    if (i < chsize - 1)
      ctx.next = wire[i + 1];

    if (strcmp(blk->name(blk), "Input") == 0) {
      // Hard code behavior for Input shard and And and Or, in order to validate
      // with actual wire input the followup
      ctx.previousOutputType = ctx.wire->inputType;
    } else if (strcmp(blk->name(blk), "And") == 0 || strcmp(blk->name(blk), "Or") == 0) {
      // Hard code behavior for Input shard and And and Or, in order to validate
      // with actual wire input the followup
      ctx.previousOutputType = ctx.originalInputType;
    } else {
      ctx.bottom = blk;
      try {
        validateConnection(ctx);
      } catch (std::exception &ex) {
        auto verboseMsg = fmt::format("Error validating shard: {}, line: {}, column: {}, wire: {}, error: {}", blk->name(blk),
                                      blk->line, blk->column, ctx.wire ? ctx.wire->name : "(unwired)", ex.what());
        SHLOG_ERROR("{}", verboseMsg);
        throw ComposeError(verboseMsg);
      }
    }
  }

  SHComposeResult result = {ctx.previousOutputType};

  for (auto &exposed : ctx.exposed) {
    shards::arrayPush(result.exposedInfo, exposed.second);
  }

  if (ctx.fullRequired) {
    for (auto &req : ctx.required) {
      shards::arrayPush(result.requiredInfo, req);
      (*ctx.fullRequired)[req.name] = req;
    }
  } else {
    for (auto &req : ctx.required) {
      shards::arrayPush(result.requiredInfo, req);
    }
  }

  if (wire.size() > 0) {
    auto &last = wire.back();
    if (strcmp(last->name(last), "Restart") == 0 || strcmp(last->name(last), "Return") == 0 ||
        strcmp(last->name(last), "Fail") == 0) {
      result.flowStopper = true;
    } else if (strcmp(last->name(last), "Stop") == 0) {
      // need to check if first param is none
      auto fp = last->getParam(last, 0);
      if (fp.valueType == SHType::None)
        result.flowStopper = true;
    }
  }

  return result;
}

SHComposeResult composeWire(const SHWire *wire, SHValidationCallback callback, void *userData, SHInstanceData data) {
  // compare exchange and then shassert we were not composing
  bool expected = false;
  if (!const_cast<SHWire *>(wire)->composing.compare_exchange_strong(expected, true)) {
    SHLOG_ERROR("Wire {} is already being composed", wire->name);
    throw ComposeError("Wire is already being composed");
  }
  // defer reset compose state
  DEFER(const_cast<SHWire *>(wire)->composing.store(false));

  // settle input type of wire before compose
  if (wire->shards.size() > 0 && strncmp(wire->shards[0]->name(wire->shards[0]), "Expect", 6) == 0) {
    // If first shard is an Expect, this wire can accept ANY input type as the type is checked at runtime
    wire->inputType = SHTypeInfo{SHType::Any};
    wire->ignoreInputTypeCheck = true;
  } else if (wire->shards.size() > 0 && !std::any_of(wire->shards.begin(), wire->shards.end(), [&](const auto &shard) {
               return strcmp(shard->name(shard), "Input") == 0;
             })) {
    // If first shard is a plain None, mark this wire has None input
    // But make sure we have no (Input) shards
    auto inTypes = wire->shards[0]->inputTypes(wire->shards[0]);
    if (inTypes.len == 1 && inTypes.elements[0].basicType == SHType::None) {
      wire->inputType = SHTypeInfo{};
      wire->ignoreInputTypeCheck = true;
    } else {
      wire->inputType = data.inputType;
      wire->ignoreInputTypeCheck = false;
    }
  } else {
    wire->inputType = data.inputType;
    wire->ignoreInputTypeCheck = false;
  }

  shassert(wire == data.wire); // caller must pass the same wire as data.wire

  auto res = composeWire(wire->shards, callback, userData, data);

  // set output type
  wire->outputType = res.outputType;

  const_cast<SHWire *>(wire)->dispatcher.trigger(SHWire::OnComposedEvent{wire});

  return res;
}

SHComposeResult composeWire(const Shards wire, SHValidationCallback callback, void *userData, SHInstanceData data) {
  std::vector<Shard *> shards;
  for (uint32_t i = 0; wire.len > i; i++) {
    shards.push_back(wire.elements[i]);
  }
  return composeWire(shards, callback, userData, data);
}

SHComposeResult composeWire(const SHSeq wire, SHValidationCallback callback, void *userData, SHInstanceData data) {
  std::vector<Shard *> shards;
  for (uint32_t i = 0; wire.len > i; i++) {
    shards.push_back(wire.elements[i].payload.shardValue);
  }
  return composeWire(shards, callback, userData, data);
}

void freeDerivedInfo(SHTypeInfo info) {
  switch (info.basicType) {
  case SHType::Seq: {
    for (uint32_t i = 0; info.seqTypes.len > i; i++) {
      freeDerivedInfo(info.seqTypes.elements[i]);
    }
    shards::arrayFree(info.seqTypes);
  } break;
  case SHType::Set: {
    for (uint32_t i = 0; info.setTypes.len > i; i++) {
      freeDerivedInfo(info.setTypes.elements[i]);
    }
    shards::arrayFree(info.setTypes);
  } break;
  case SHType::Table: {
    for (uint32_t i = 0; info.table.types.len > i; i++) {
      freeDerivedInfo(info.table.types.elements[i]);
    }
    for (uint32_t i = 0; info.table.keys.len > i; i++) {
      destroyVar(info.table.keys.elements[i]);
    }
    shards::arrayFree(info.table.types);
    shards::arrayFree(info.table.keys);
  } break;
  default:
    break;
  };
}

SHTypeInfo deriveTypeInfo(const SHVar &value, const SHInstanceData &data, std::vector<SHExposedTypeInfo> *expInfo,
                          bool resolveContextVariables) {
  // We need to guess a valid SHTypeInfo for this var in order to validate
  // Build a SHTypeInfo for the var
  // this is not complete at all, missing Array and SHType::ContextVar for example
  SHTypeInfo varType{};
  varType.basicType = value.valueType;
  varType.innerType = value.innerType;
  switch (value.valueType) {
  case SHType::Object: {
    varType.object.vendorId = value.payload.objectVendorId;
    varType.object.typeId = value.payload.objectTypeId;
    break;
  }
  case SHType::Enum: {
    varType.enumeration.vendorId = value.payload.enumVendorId;
    varType.enumeration.typeId = value.payload.enumTypeId;
    break;
  }
  case SHType::Seq: {
    std::unordered_set<SHTypeInfo> types;
    for (uint32_t i = 0; i < value.payload.seqValue.len; i++) {
      auto derived = deriveTypeInfo(value.payload.seqValue.elements[i], data, expInfo, resolveContextVariables);
      if (!types.count(derived)) {
        shards::arrayPush(varType.seqTypes, derived);
        types.insert(derived);
      }
    }
    varType.fixedSize = value.payload.seqValue.len;
    break;
  }
  case SHType::Table: {
    auto &t = value.payload.tableValue;
    SHTableIterator tit;
    t.api->tableGetIterator(t, &tit);
    SHVar k;
    SHVar v;
    while (t.api->tableNext(t, &tit, &k, &v)) {
      auto derived = deriveTypeInfo(v, data, expInfo, resolveContextVariables);
      shards::arrayPush(varType.table.types, derived);
      auto idx = varType.table.keys.len;
      shards::arrayResize(varType.table.keys, idx + 1);
      cloneVar(varType.table.keys.elements[idx], k);
    }
  } break;
  case SHType::Set: {
    auto &s = value.payload.setValue;
    SHSetIterator sit;
    s.api->setGetIterator(s, &sit);
    SHVar v;
    while (s.api->setNext(s, &sit, &v)) {
      auto derived = deriveTypeInfo(v, data, expInfo, resolveContextVariables);
      shards::arrayPush(varType.setTypes, derived);
    }
  } break;
  case SHType::ContextVar: {
    if (expInfo) {
      auto sv = SHSTRVIEW(value);
      const auto varName = sv;
      for (auto info : data.shared) {
        std::string_view name(info.name);
        if (name == sv) {
          expInfo->push_back(SHExposedTypeInfo{.name = info.name, .exposedType = info.exposedType});
          if (resolveContextVariables) {
            return cloneTypeInfo(info.exposedType);
          } else {
            shards::arrayPush(varType.contextVarTypes, cloneTypeInfo(info.exposedType));
            return varType;
          }
        }
      }
      SHLOG_ERROR("Could not find variable {} when deriving type info", varName);
      throw std::runtime_error("Could not find variable when deriving type info");
    }
    // if we reach this point, no variable was found...
    // not our job to trigger errors, just continue
    // specifically we are used to verify parameters as well
    // and in that case data is empty
  } break;
  default:
    break;
  };
  return varType;
}

SHTypeInfo cloneTypeInfo(const SHTypeInfo &other) {
  // We need to guess a valid SHTypeInfo for this var in order to validate
  // Build a SHTypeInfo for the var
  // this is not complete at all, missing Array and SHType::ContextVar for example
  SHTypeInfo varType;
  memcpy(&varType, &other, sizeof(SHTypeInfo));
  switch (varType.basicType) {
  case SHType::ContextVar:
  case SHType::Set:
  case SHType::Seq: {
    varType.seqTypes = {};
    for (uint32_t i = 0; i < other.seqTypes.len; i++) {
      auto cloned = cloneTypeInfo(other.seqTypes.elements[i]);
      shards::arrayPush(varType.seqTypes, cloned);
    }
    break;
  }
  case SHType::Table: {
    varType.table = {};
    for (uint32_t i = 0; i < other.table.types.len; i++) {
      auto cloned = cloneTypeInfo(other.table.types.elements[i]);
      shards::arrayPush(varType.table.types, cloned);
    }
    for (uint32_t i = 0; i < other.table.keys.len; i++) {
      auto idx = varType.table.keys.len;
      shards::arrayResize(varType.table.keys, idx + 1);
      cloneVar(varType.table.keys.elements[idx], other.table.keys.elements[i]);
    }
    break;
  }
  default:
    break;
  };
  return varType;
}

// this is potentially called from unsafe code (e.g. networking)
// let's do some crude stack protection here
thread_local int deriveTypeHashRecursionCounter;
constexpr int MAX_DERIVED_TYPE_HASH_RECURSION = 100;

uint64_t _deriveTypeHash(const SHVar &value);

void updateTypeHash(const SHVar &var, XXH3_state_s *state) {
  // this is not complete at all, missing Array and SHType::ContextVar for example
  XXH3_64bits_update(state, &var.valueType, sizeof(var.valueType));
  XXH3_64bits_update(state, &var.innerType, sizeof(var.innerType));

  switch (var.valueType) {
  case SHType::Object: {
    XXH3_64bits_update(state, &var.payload.objectVendorId, sizeof(var.payload.objectVendorId));
    XXH3_64bits_update(state, &var.payload.objectTypeId, sizeof(var.payload.objectTypeId));
    break;
  }
  case SHType::Enum: {
    XXH3_64bits_update(state, &var.payload.enumVendorId, sizeof(var.payload.enumVendorId));
    XXH3_64bits_update(state, &var.payload.enumTypeId, sizeof(var.payload.enumTypeId));
    break;
  }
  case SHType::Seq: {
    std::set<uint64_t, std::less<uint64_t>, stack_allocator<uint64_t>> hashes;
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      auto typeHash = _deriveTypeHash(var.payload.seqValue.elements[i]);
      hashes.insert(typeHash);
    }
    constexpr auto recursive = false;
    XXH3_64bits_update(state, &recursive, sizeof(recursive));
    for (const uint64_t hash : hashes) {
      XXH3_64bits_update(state, &hash, sizeof(uint64_t));
    }
  } break;
  case SHType::Table: {
    auto &t = var.payload.tableValue;
    SHTableIterator tit;
    t.api->tableGetIterator(t, &tit);
    SHVar k;
    SHVar v;
    while (t.api->tableNext(t, &tit, &k, &v)) {
      auto hk = shards::hash(k);
      XXH3_64bits_update(state, &hk.payload.int2Value, sizeof(SHInt2));
      auto hv = _deriveTypeHash(v);
      XXH3_64bits_update(state, &hv, sizeof(uint64_t));
    }
  } break;
  case SHType::Set: {
    // this is unsafe because allocates on the stack
    // but we need to sort hashes
    std::set<uint64_t, std::less<uint64_t>, stack_allocator<uint64_t>> hashes;
    // set is unordered so just collect
    auto &s = var.payload.setValue;
    SHSetIterator sit;
    s.api->setGetIterator(s, &sit);
    SHVar v;
    while (s.api->setNext(s, &sit, &v)) {
      hashes.insert(_deriveTypeHash(v));
    }
    constexpr auto recursive = false;
    XXH3_64bits_update(state, &recursive, sizeof(recursive));
    for (const uint64_t hash : hashes) {
      XXH3_64bits_update(state, &hash, sizeof(uint64_t));
    }
  } break;
  default:
    break;
  };
}

uint64_t _deriveTypeHash(const SHVar &value) {
  if (deriveTypeHashRecursionCounter >= MAX_DERIVED_TYPE_HASH_RECURSION)
    throw SHException("deriveTypeHash maximum recursion exceeded");
  deriveTypeHashRecursionCounter++;
  DEFER(deriveTypeHashRecursionCounter--);

  XXH3_state_s hashState;
  XXH3_INITSTATE(&hashState);

  XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);

  updateTypeHash(value, &hashState);

  return XXH3_64bits_digest(&hashState);
}

uint64_t deriveTypeHash(const SHVar &value) {
  deriveTypeHashRecursionCounter = 0;
  return _deriveTypeHash(value);
}

uint64_t deriveTypeHash(const SHTypeInfo &value);

void updateTypeHash(const SHTypeInfo &t, XXH3_state_s *state) {
  XXH3_64bits_update(state, &t.basicType, sizeof(t.basicType));
  XXH3_64bits_update(state, &t.innerType, sizeof(t.innerType));

  switch (t.basicType) {
  case SHType::Object: {
    XXH3_64bits_update(state, &t.object.vendorId, sizeof(t.object.vendorId));
    XXH3_64bits_update(state, &t.object.typeId, sizeof(t.object.typeId));
  } break;
  case SHType::Enum: {
    XXH3_64bits_update(state, &t.enumeration.vendorId, sizeof(t.enumeration.vendorId));
    XXH3_64bits_update(state, &t.enumeration.typeId, sizeof(t.enumeration.typeId));
  } break;
  case SHType::Seq: {
    // this is unsafe because allocates on the stack, but faster...
    std::set<uint64_t, std::less<uint64_t>, stack_allocator<uint64_t>> hashes;
    bool recursive = false;

    for (uint32_t i = 0; i < t.seqTypes.len; i++) {
      if (t.seqTypes.elements[i].recursiveSelf) {
        recursive = true;
      } else {
        auto typeHash = deriveTypeHash(t.seqTypes.elements[i]);
        hashes.insert(typeHash);
      }
    }
    XXH3_64bits_update(state, &recursive, sizeof(recursive));
    for (const auto hash : hashes) {
      XXH3_64bits_update(state, &hash, sizeof(hash));
    }
  } break;
  case SHType::Table: {
    if (t.table.keys.len == t.table.types.len) {
      for (uint32_t i = 0; i < t.table.types.len; i++) {
        auto keyHash = shards::hash(t.table.keys.elements[i]);
        XXH3_64bits_update(state, &keyHash.payload.int2Value, sizeof(SHInt2));
        auto typeHash = deriveTypeHash(t.table.types.elements[i]);
        XXH3_64bits_update(state, &typeHash, sizeof(uint64_t));
      }
    } else {
      std::set<uint64_t, std::less<uint64_t>, stack_allocator<uint64_t>> hashes;
      for (uint32_t i = 0; i < t.table.types.len; i++) {
        auto typeHash = deriveTypeHash(t.table.types.elements[i]);
        hashes.insert(typeHash);
      }
      for (const auto hash : hashes) {
        XXH3_64bits_update(state, &hash, sizeof(hash));
      }
    }
  } break;
  case SHType::Set: {
    // this is unsafe because allocates on the stack, but faster...
    std::set<uint64_t, std::less<uint64_t>, stack_allocator<uint64_t>> hashes;
    bool recursive = false;
    for (uint32_t i = 0; i < t.setTypes.len; i++) {
      if (t.setTypes.elements[i].recursiveSelf) {
        recursive = true;
      } else {
        auto typeHash = deriveTypeHash(t.setTypes.elements[i]);
        hashes.insert(typeHash);
      }
    }
    XXH3_64bits_update(state, &recursive, sizeof(recursive));
    for (const auto hash : hashes) {
      XXH3_64bits_update(state, &hash, sizeof(hash));
    }
  } break;
  default:
    break;
  };
}

uint64_t deriveTypeHash(const SHTypeInfo &value) {
  XXH3_state_s hashState;
  XXH3_INITSTATE(&hashState);

  XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);

  updateTypeHash(value, &hashState);

  return XXH3_64bits_digest(&hashState);
}

bool validateSetParam(Shard *shard, int index, const SHVar &value, SHValidationCallback callback, void *userData) {
  auto params = shard->parameters(shard);
  if (params.len <= (uint32_t)index) {
    std::string_view err("Parameter index out of range");
    callback(shard, SHStringWithLen{err.data(), err.size()}, false, userData);
    return false;
  }

  auto param = params.elements[index];

  // Build a SHTypeInfo for the var
  SHInstanceData data{};
  auto varType = deriveTypeInfo(value, data);
  DEFER({ freeDerivedInfo(varType); });

  for (uint32_t i = 0; param.valueTypes.len > i; i++) {
    // This only does a quick check to see if the type is roughly correct
    // ContextVariable types will be checked in validateConnection based on requiredVariables
    if (matchTypes(varType, param.valueTypes.elements[i], true, true, true)) {
      return true; // we are good just exit
    }
  }

  std::string err(fmt::format("Parameter {} not accepting this kind of variable: {} (valid types: {}), line: {}, column: {}",
                              param.name, varType, param.valueTypes, shard->line, shard->column));
  callback(shard, SHStringWithLen{err.data(), err.size()}, false, userData);

  return false;
}

void error_handler(int err_sig) {
  std::signal(err_sig, SIG_DFL);

  // using an atomic static bool here we prevent multiple signals to be handled
  // at the same time
  static std::atomic<bool> handling{false};
  if (handling.exchange(true))
    return;

  auto crashed = false;

  switch (err_sig) {
  case SIGINT:
  case SIGTERM:
    SHLOG_INFO("Exiting due to INT/TERM signal");
    shards::GetGlobals().SigIntTerm++;
    if (shards::GetGlobals().SigIntTerm > 5)
      std::exit(-1);
    break;
  case SIGFPE:
    SHLOG_ERROR("Fatal SIGFPE");
    crashed = true;
    break;
  case SIGILL:
    SHLOG_ERROR("Fatal SIGILL");
    crashed = true;
    break;
  case SIGABRT:
    SHLOG_ERROR("Fatal SIGABRT");
    crashed = true;
    break;
  case SIGSEGV:
    SHLOG_ERROR("Fatal SIGSEGV");
    crashed = true;
    break;
#ifndef _WIN32
  case SIGBUS:
    SHLOG_ERROR("Fatal SIGBUS");
    crashed = true;
    break;
  case SIGSYS:
    SHLOG_ERROR("Fatal SIGSYS");
    crashed = true;
    break;
  case SIGPIPE:
    SHLOG_ERROR("Fatal SIGPIPE");
    crashed = true;
    break;
  case SIGTRAP:
    SHLOG_ERROR("Fatal SIGTRAP");
    crashed = true;
    break;
#endif
  }

  if (crashed) {
#ifndef __EMSCRIPTEN__
    SHLOG_ERROR(boost::stacktrace::stacktrace());
#endif

    auto handler = GetGlobals().CrashHandler;
    if (handler)
      handler->crash();
  }

  spdlog::default_logger()->flush();
  spdlog::shutdown();

  // reset handling flag
  handling.store(false);

  std::raise(err_sig);
}

#ifdef _WIN32
#include "debugapi.h"
bool isDebuggerPresent() { return (bool)IsDebuggerPresent(); }
#elif SH_APPLE
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdbool.h>

bool isDebuggerPresent() {
  int mib[4];
  struct kinfo_proc info;
  size_t size = sizeof(info);

  info.kp_proc.p_flag = 0;
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PID;
  mib[3] = getpid();

  if (sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0) == -1) {
    return false;
  }

  return (info.kp_proc.p_flag & P_TRACED) != 0;
}
#else
bool isDebuggerPresent() { return false; }
#endif

void installSignalHandlers() {
  if (!isDebuggerPresent()) {
    SHLOG_TRACE("Installing signal handlers");
    std::signal(SIGINT, &error_handler);
    std::signal(SIGTERM, &error_handler);
    std::signal(SIGFPE, &error_handler);
    std::signal(SIGILL, &error_handler);
    std::signal(SIGABRT, &error_handler);
    std::signal(SIGSEGV, &error_handler);
#ifndef _WIN32
    std::signal(SIGBUS, &error_handler);
    std::signal(SIGSYS, &error_handler);
    std::signal(SIGPIPE, &error_handler);
    std::signal(SIGTRAP, &error_handler);
#endif
  }
}

SHRunWireOutput runWire(SHWire *wire, SHContext *context, const SHVar &wireInput) {
  ZoneScoped;
  ZoneName(wire->name.c_str(), wire->name.size());

  memset(&wire->previousOutput, 0x0, sizeof(SHVar));
  wire->currentInput = wireInput;
  wire->state = SHWire::State::Iterating;
  wire->context = context;
  DEFER({ wire->state = SHWire::State::IterationEnded; });

  wire->dispatcher.trigger(SHWire::OnUpdateEvent{wire});

  try {
    auto state =
        shardsActivation<std::vector<ShardPtr>, false, false>(wire->shards, context, wire->currentInput, wire->previousOutput);
    switch (state) {
    case SHWireState::Return:
      return {context->getFlowStorage(), SHRunWireOutputState::Returned};
    case SHWireState::Restart:
      return {context->getFlowStorage(), SHRunWireOutputState::Restarted};
    case SHWireState::Error:
      // shardsActivation handles error logging and such
      shassert(context->failed());
      return {wire->previousOutput, SHRunWireOutputState::Failed};
    case SHWireState::Stop:
      shassert(!context->failed());
      return {context->getFlowStorage(), SHRunWireOutputState::Stopped};
    case SHWireState::Rebase:
      // Handled inside shardsActivation
      SHLOG_FATAL("Invalid wire state");
    case SHWireState::Continue:
      break;
    }
  }
#if SH_BOOST_COROUTINE
  catch (boost::context::detail::forced_unwind const &e) {
    SHLOG_WARNING("Wire {} forced unwind", wire->name);
    throw; // required for Boost Coroutine!
  }
#endif
  catch (...) {
    // shardsActivation handles error logging and such
    return {wire->previousOutput, SHRunWireOutputState::Failed};
  }

  return {wire->previousOutput, SHRunWireOutputState::Running};
}

void run(SHWire *wire, SHFlow *flow, shards::Coroutine *coro) {
  SH_CORO_RESUMED(wire);

  SHLOG_TRACE("Wire {} rolling", wire->name);
  auto running = true;

  // we need this cos by the end of this call we might get suspended/resumed and state changes! this wont
  bool failed = false;

  // Reset state
  wire->state = SHWire::State::Prepared;
  wire->finishedOutput = Var::Empty;
  wire->finishedError.clear();

  // Create a new context and copy the sink in
  SHFlow anonFlow{wire};
  SHContext context(coro, wire, flow ? flow : &anonFlow);

  // if the wire had a context (Stepped wires in wires.cpp)
  // copy some stuff from it
  if (wire->context) {
    context.wireStack = wire->context->wireStack;
    // need to add back ourself
    context.wireStack.push_back(wire);
    // also set parent
    context.parent = wire->context;
  }

  // also populate context in wire
  wire->context = &context;

  // We pre-rolled our coro, suspend here before actually starting.
  // This allows us to allocate the stack ahead of time.
  // And call warmup on all the shards!
  try {
    wire->warmup(&context);
  } catch (...) {
    // inside warmup we re-throw, we handle logging and such there
    wire->state = SHWire::State::Failed;
    SHLOG_ERROR("Wire {} warmup failed", wire->name);
    goto endOfWire;
  }

  // yield after warming up
  SH_CORO_SUSPENDED(wire);
  coroutineSuspend(*context.continuation);
  SH_CORO_RESUMED(wire);

  SHLOG_TRACE("Wire {} starting", wire->name);

  if (context.shouldStop()) {
    // We might have stopped before even starting!
    SHLOG_ERROR("Wire {} stopped before starting", wire->name);
    goto endOfWire;
  }

  wire->dispatcher.trigger(SHWire::OnStartEvent{wire});

  while (running) {
    running = wire->looped;

    // reset context state
    context.continueFlow();

    auto runRes = runWire(wire, &context, wire->currentInput);
    if (unlikely(runRes.state == SHRunWireOutputState::Failed)) {
      SHLOG_DEBUG("Wire {} failed", wire->name);
      wire->state = SHWire::State::Failed;
      failed = true;
      context.stopFlow(runRes.output);
      break;
    } else if (unlikely(runRes.state == SHRunWireOutputState::Stopped || runRes.state == SHRunWireOutputState::Returned)) {
      SHLOG_DEBUG("Wire {} stopped", wire->name);
      context.stopFlow(runRes.output);
      // also replace the previous output with actual output
      // as it's likely coming from flowStorage of context!
      wire->previousOutput = runRes.output;
      break;
    } else if (unlikely(runRes.state == SHRunWireOutputState::Restarted)) {
      // must clone over currentInput!
      // restart overwrites currentInput on purpose
      wire->currentInput = context.getFlowStorage();
      running = true; // keep in this case!
    }

    if (!wire->unsafe && running) {
      // Ensure no while(true), yield anyway every run
      context.next = SHDuration(0);

      SH_CORO_SUSPENDED(wire);
      coroutineSuspend(*context.continuation);
      SH_CORO_RESUMED(wire);

      // This is delayed upon continuation!!
      if (context.shouldStop()) {
        SHLOG_DEBUG("Wire {} aborted on resume", wire->name);
        break;
      }
    }
  }

endOfWire:
  wire->finishedOutput = wire->previousOutput; // cloning over! (OwnedVar)

  if (failed || context.failed()) {
    wire->finishedError = context.getErrorMessage();
    if (wire->finishedError.empty()) {
      wire->finishedError = "Generic error";
    }
    SHLOG_DEBUG("Wire {} failed with error {}", wire->name, wire->finishedError);

    if (wire->resumer) {
      // also stop the resumer parent in this case
      wire->resumer->context->cancelFlow(wire->finishedError);
    }
  }

  // if we have a resumer we return to it
  if (wire->resumer) {
    SHLOG_TRACE("Wire {} ending and resuming {}", wire->name, wire->resumer->name);
    context.flow->wire = wire->resumer;
    wire->resumer = nullptr;
  }

  // run cleanup on all the shards
  // ensure stop state is set
  context.stopFlow(wire->previousOutput);

  // Set onLastResume so tick keeps processing mesh tasks on cleanup
  context.onLastResume = true;
  wire->cleanup(true);
  context.onLastResume = false;

  // Need to take care that we might have stopped the wire very early due to
  // errors and the next eventual stop() should avoid resuming
  if (wire->state != SHWire::State::Failed)
    wire->state = SHWire::State::Ended;

  wire->dispatcher.trigger(SHWire::OnStopEvent{wire});

  // Make sure to clear context at the end so it doesn't point to invalid stack memory
  wire->context = nullptr;

  SHLOG_TRACE("Wire {} ended", wire->name);

  SH_CORO_SUSPENDED(wire)

#if SH_USE_THREAD_FIBER || !defined(__EMSCRIPTEN__)
  return;
#else
  // Emscripten needs to suspend and never continue
  coroutineSuspend(*context.continuation);

  // we should never resume here!
  SHLOG_FATAL("Wire {} resumed after ending", wire->name);
#endif
}

void parseArguments(int argc, const char **argv) {
  shards::fast_string::init();

  namespace fs = boost::filesystem;

  auto &globals = GetGlobals();
  auto absExePath = fs::weakly_canonical(argv[0]);
  globals.ExePath = absExePath.string();
}

Globals &GetGlobals() {
  static Globals globals;
  return globals;
}

static std::unordered_map<std::string, EventDispatcher> dispatchers;
static std::shared_mutex mutex;
EventDispatcher &getEventDispatcher(const std::string &name) {

  std::shared_lock<decltype(mutex)> _l(mutex);
  auto it = dispatchers.find(name);
  if (it == dispatchers.end()) {
    _l.unlock();
    std::scoped_lock<decltype(mutex)> _l1(mutex);
    auto &result = dispatchers[name];
    result.name = name;
    return result;
  } else {
    return it->second;
  }
}

NO_INLINE void _destroyVarSlow(SHVar &var) {
  switch (var.valueType) {
  case SHType::String:
  case SHType::Path:
  case SHType::ContextVar:
#if 0
    shassert(var.payload.stringCapacity >= 7 && "string capacity is too small, it should be at least 7");
    if (var.payload.stringCapacity > 7) {
      delete[] var.payload.stringValue;
    } else {
      memset(var.shortString, 0, 7);
      shassert(var.shortString[7] == 0 && "0 terminator should be 0 always");
    }
#else
    delete[] var.payload.stringValue;
#endif
    break;
  case SHType::Bytes:
#if 0
    shassert(var.payload.bytesCapacity >= 8 && "bytes capacity is too small, it should be at least 8");
    if (var.payload.bytesCapacity > 8) {
      delete[] var.payload.bytesValue;
    } else {
      memset(var.shortBytes, 0, 8);
    }
#else
    delete[] var.payload.bytesValue;
#endif
    break;
  case SHType::Seq: {
    // notice we use .cap! because we make sure to 0 new empty elements
    for (size_t i = var.payload.seqValue.cap; i > 0; i--) {
      destroyVar(var.payload.seqValue.elements[i - 1]);
    }
    shards::arrayFree(var.payload.seqValue);
  } break;
  case SHType::Table: {
    shassert(var.payload.tableValue.api == &GetGlobals().TableInterface);
    shassert(var.payload.tableValue.opaque);
    auto map = (SHMap *)var.payload.tableValue.opaque;
    delete map;
    var.version = 0;
  } break;
  case SHType::Set: {
    shassert(var.payload.setValue.api == &GetGlobals().SetInterface);
    shassert(var.payload.setValue.opaque);
    auto set = (SHHashSet *)var.payload.setValue.opaque;
    delete set;
  } break;
  case SHType::ShardRef:
    decRef(var.payload.shardValue);
    break;
  case SHType::Type:
    freeDerivedInfo(*var.payload.typeValue);
    delete var.payload.typeValue;
    break;
  default:
    break;
  };
}

NO_INLINE void _cloneVarSlow(SHVar &dst, const SHVar &src) {
  shassert((dst.flags & SHVAR_FLAGS_FOREIGN) != SHVAR_FLAGS_FOREIGN && "cannot clone into a foreign var");
  switch (src.valueType) {
  case SHType::Seq: {
    uint32_t srcLen = src.payload.seqValue.len;

    // try our best to re-use memory
    if (dst.valueType != SHType::Seq) {
      destroyVar(dst);
      dst.valueType = SHType::Seq;
    }

    shards::arrayResize(dst.payload.seqValue, srcLen);

    if (src.payload.seqValue.elements == dst.payload.seqValue.elements)
      return;

    for (uint32_t i = 0; i < srcLen; i++) {
      const auto &subsrc = src.payload.seqValue.elements[i];
      cloneVar(dst.payload.seqValue.elements[i], subsrc);
    }
  } break;
  case SHType::Path:
  case SHType::ContextVar:
  case SHType::String: {
    auto srcSize = src.payload.stringLen > 0 || src.payload.stringValue == nullptr ? src.payload.stringLen
                                                                                   : uint32_t(strlen(src.payload.stringValue));
    if (dst.valueType != src.valueType || dst.payload.stringCapacity < srcSize) {
      destroyVar(dst);
      dst.valueType = src.valueType;
#if 0
      if (srcSize <= 7) {
        // short string, no need to allocate
        // capacity is 8 but last is 0 terminator
        dst.payload.stringValue = dst.shortString;
        dst.payload.stringCapacity = 7; // this also marks it as short string, lucky 7
      } else
#endif
      {
        // allocate a 0 terminator too
        dst.payload.stringValue = new char[srcSize + 1];
        dst.payload.stringCapacity = srcSize;
      }
    } else {
      if (src.payload.stringValue == dst.payload.stringValue && src.payload.stringLen == dst.payload.stringLen)
        return;
    }

    if (srcSize > 0) {
      shassert(src.payload.stringValue != nullptr && "string value is null but length is not 0");
      memcpy((void *)dst.payload.stringValue, (void *)src.payload.stringValue, srcSize);
    }

    shassert(dst.payload.stringValue && "destination stringValue cannot be null");

    // make sure to 0 terminate
    ((char *)dst.payload.stringValue)[srcSize] = 0;

    // fill the len field
    dst.payload.stringLen = srcSize;
  } break;
  case SHType::Image: {
    auto spixsize = 1;
    auto dpixsize = 1;
    if ((src.payload.imageValue.flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
      spixsize = 2;
    else if ((src.payload.imageValue.flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
      spixsize = 4;
    if ((dst.payload.imageValue.flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
      dpixsize = 2;
    else if ((dst.payload.imageValue.flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
      dpixsize = 4;

    size_t srcImgSize = src.payload.imageValue.height * src.payload.imageValue.width * src.payload.imageValue.channels * spixsize;
    size_t dstCapacity =
        dst.payload.imageValue.height * dst.payload.imageValue.width * dst.payload.imageValue.channels * dpixsize;
    if (dst.valueType != SHType::Image || srcImgSize > dstCapacity) {
      destroyVar(dst);
      dst.valueType = SHType::Image;
      dst.payload.imageValue.data = new uint8_t[srcImgSize];
    }

    dst.version++;

    dst.payload.imageValue.flags = src.payload.imageValue.flags;
    dst.payload.imageValue.height = src.payload.imageValue.height;
    dst.payload.imageValue.width = src.payload.imageValue.width;
    dst.payload.imageValue.channels = src.payload.imageValue.channels;

    if (src.payload.imageValue.data == dst.payload.imageValue.data)
      return;

    memcpy(dst.payload.imageValue.data, src.payload.imageValue.data, srcImgSize);
  } break;
  case SHType::Audio: {
    size_t srcSize = src.payload.audioValue.nsamples * src.payload.audioValue.channels * sizeof(float);
    size_t dstCapacity = dst.payload.audioValue.nsamples * dst.payload.audioValue.channels * sizeof(float);
    if (dst.valueType != SHType::Audio || srcSize > dstCapacity) {
      destroyVar(dst);
      dst.valueType = SHType::Audio;
      dst.payload.audioValue.samples = new float[src.payload.audioValue.nsamples * src.payload.audioValue.channels];
    }

    dst.payload.audioValue.sampleRate = src.payload.audioValue.sampleRate;
    dst.payload.audioValue.nsamples = src.payload.audioValue.nsamples;
    dst.payload.audioValue.channels = src.payload.audioValue.channels;

    if (src.payload.audioValue.samples == dst.payload.audioValue.samples)
      return;

    memcpy(dst.payload.audioValue.samples, src.payload.audioValue.samples, srcSize);
  } break;
  case SHType::Table: {
    SHMap *map;
    if (dst.valueType == SHType::Table) {
      // This code checks if the source and destination of a copy are the same.
      if (src.payload.tableValue.opaque == dst.payload.tableValue.opaque)
        return;

      // also we assume mutable tables are of our internal type!!
      shassert(dst.payload.tableValue.api == &GetGlobals().TableInterface);

      map = (SHMap *)dst.payload.tableValue.opaque;

      // let's do some magic here, while KISS at same time
      if (dst.payload.tableValue.api == src.payload.tableValue.api) {
        auto sMap = (SHMap *)src.payload.tableValue.opaque;
        if (sMap->size() == map->size()) {
          auto it = sMap->begin();
          auto dit = map->begin();
          // copy values fast, hoping keys are the same
          // we might end up with some extra copies if keys are not the same but
          // given shards nature, it's unlikely it will be the majority of cases
          while (it != sMap->end()) {
            if (it->first != dit->first)
              goto early_exit;

            cloneVar(dit->second, it->second);

            ++it;
            ++dit;
          }
          return;
        }
      }

    early_exit:
      // ok clear destination and copy values the old way
      map->clear();
    } else {
      destroyVar(dst);
      dst.valueType = SHType::Table;
      dst.payload.tableValue.api = &GetGlobals().TableInterface;
      map = new SHMap();
      dst.payload.tableValue.opaque = map;
    }

    dst.version++;

    auto &t = src.payload.tableValue;
    SHTableIterator tit;
    t.api->tableGetIterator(t, &tit);
    SHVar k;
    SHVar v;
    while (t.api->tableNext(t, &tit, &k, &v)) {
      (*map)[k] = v;
    }
  } break;
  case SHType::Set: {
    SHHashSet *set;
    if (dst.valueType == SHType::Set) {
      if (src.payload.setValue.opaque == dst.payload.setValue.opaque)
        return;
      shassert(dst.payload.setValue.api == &GetGlobals().SetInterface);
      set = (SHHashSet *)dst.payload.setValue.opaque;
      set->clear();
    } else {
      destroyVar(dst);
      dst.valueType = SHType::Set;
      dst.payload.setValue.api = &GetGlobals().SetInterface;
      set = new SHHashSet();
      dst.payload.setValue.opaque = set;
    }

    auto &s = src.payload.setValue;
    SHSetIterator sit;
    s.api->setGetIterator(s, &sit);
    SHVar v;
    while (s.api->setNext(s, &sit, &v)) {
      (*set).emplace(v);
    }
  } break;
  case SHType::Bytes: {
    if (dst.valueType != SHType::Bytes || dst.payload.bytesCapacity < src.payload.bytesSize) {
      destroyVar(dst);
      dst.valueType = SHType::Bytes;
#if 0
      if (src.payload.bytesSize <= 8) {
        // small bytes are stored directly in the payload
        dst.payload.bytesValue = dst.shortBytes;
        dst.payload.bytesCapacity = 8;
      } else
#endif
      {
        dst.payload.bytesValue = new uint8_t[src.payload.bytesSize];
        dst.payload.bytesCapacity = src.payload.bytesSize;
      }
    } else {
      if (src.payload.bytesValue == dst.payload.bytesValue && src.payload.bytesSize == dst.payload.bytesSize)
        return;
    }

    dst.payload.bytesSize = src.payload.bytesSize;
    memcpy((void *)dst.payload.bytesValue, (void *)src.payload.bytesValue, src.payload.bytesSize);
  } break;
  case SHType::Array: {
    auto srcLen = src.payload.arrayValue.len;

    // try our best to re-use memory
    if (dst.valueType != SHType::Array) {
      destroyVar(dst);
      dst.valueType = SHType::Array;
    }

    if (src.payload.arrayValue.elements == dst.payload.arrayValue.elements)
      return;

    dst.innerType = src.innerType;
    shards::arrayResize(dst.payload.arrayValue, srcLen);
    // array holds only blittables and is packed so a single memcpy is enough
    memcpy(&dst.payload.arrayValue.elements[0], &src.payload.arrayValue.elements[0], sizeof(SHVarPayload) * srcLen);
  } break;
  case SHType::Wire:
    if (dst.valueType == SHType::Wire) {
      auto &aWire = SHWire::sharedFromRef(src.payload.wireValue);
      auto &bWire = SHWire::sharedFromRef(dst.payload.wireValue);
      if (aWire == bWire)
        return;
    }

    destroyVar(dst);

    dst.valueType = SHType::Wire;
    dst.payload.wireValue = SHWire::addRef(src.payload.wireValue);
    break;
  case SHType::ShardRef:
    destroyVar(dst);
    dst.valueType = SHType::ShardRef;
    dst.payload.shardValue = src.payload.shardValue;
    incRef(dst.payload.shardValue);
    break;
  case SHType::Object:
    if (dst != src) {
      destroyVar(dst);
    }

    dst.valueType = SHType::Object;
    dst.payload.objectValue = src.payload.objectValue;
    dst.payload.objectVendorId = src.payload.objectVendorId;
    dst.payload.objectTypeId = src.payload.objectTypeId;

    if ((src.flags & SHVAR_FLAGS_USES_OBJINFO) == SHVAR_FLAGS_USES_OBJINFO && src.objectInfo) {
      // in this case the custom object needs actual destruction
      dst.flags |= SHVAR_FLAGS_USES_OBJINFO;
      dst.objectInfo = src.objectInfo;
      if (src.objectInfo->reference)
        dst.objectInfo->reference(dst.payload.objectValue);
    }
    break;
  case SHType::Type:
    destroyVar(dst);
    dst.payload.typeValue = new SHTypeInfo(cloneTypeInfo(*src.payload.typeValue));
    dst.valueType = SHType::Type;
    break;
  default:
    SHLOG_FATAL("Unhandled type {}", src.valueType);
    break;
  };
}

void _gatherShards(const ShardsCollection &coll, std::vector<ShardInfo> &out, const SHWire *wire) {
  // TODO out should be a set?
  switch (coll.index()) {
  case 0: {
    // wire
    auto wire = std::get<const SHWire *>(coll);
    if (!gatheringWires().count(wire)) {
      gatheringWires().insert(wire);
      for (auto blk : wire->shards) {
        _gatherShards(blk, out, wire);
      }
    }
  } break;
  case 1: {
    // Single shard
    auto blk = std::get<ShardPtr>(coll);
    std::string_view name(blk->name(blk));
    out.emplace_back(name, blk, wire);
    // Also find nested shards
    const auto params = blk->parameters(blk);
    for (uint32_t i = 0; i < params.len; i++) {
      const auto &param = params.elements[i];
      const auto &types = param.valueTypes;
      bool potential = false;
      for (uint32_t j = 0; j < types.len; j++) {
        const auto &type = types.elements[j];
        if (type.basicType == SHType::ShardRef || type.basicType == SHType::Wire) {
          potential = true;
        } else if (type.basicType == SHType::Seq) {
          const auto &stypes = type.seqTypes;
          for (uint32_t k = 0; k < stypes.len; k++) {
            if (stypes.elements[k].basicType == SHType::ShardRef) {
              potential = true;
            }
          }
        }
      }
      if (potential)
        _gatherShards(blk->getParam(blk, i), out, wire);
    }
  } break;
  case 2: {
    // Shards seq
    auto bs = std::get<Shards>(coll);
    for (uint32_t i = 0; i < bs.len; i++) {
      _gatherShards(bs.elements[i], out, wire);
    }
  } break;
  case 3: {
    // Var
    auto var = std::get<SHVar>(coll);
    if (var.valueType == SHType::ShardRef) {
      _gatherShards(var.payload.shardValue, out, wire);
    } else if (var.valueType == SHType::Wire) {
      auto &wire = SHWire::sharedFromRef(var.payload.wireValue);
      _gatherShards(wire.get(), out, wire.get());
    } else if (var.valueType == SHType::Seq) {
      auto bs = var.payload.seqValue;
      for (uint32_t i = 0; i < bs.len; i++) {
        _gatherShards(bs.elements[i], out, wire);
      }
    }
  } break;
  default:
    SHLOG_FATAL("invalid state");
  }
}

void gatherShards(const ShardsCollection &coll, std::vector<ShardInfo> &out) {
  gatheringWiresPush();
  DEFER(gatheringWiresPop());
  _gatherShards(coll, out, coll.index() == 0 ? std::get<const SHWire *>(coll) : nullptr);
}

void _gatherWires(const ShardsCollection &coll, std::vector<WireNode> &out, const SHWire *wire) {
  switch (coll.index()) {
  case 0: {
    // wire
    auto currentWire = std::get<const SHWire *>(coll);
    if (!gatheringWires().count(currentWire)) {
      out.emplace_back(currentWire, wire); // current, previous
      gatheringWires().insert(currentWire);
      for (auto blk : currentWire->shards) {
        _gatherWires(blk, out, currentWire);
      }
    }
  } break;
  case 1: {
    // Single shard
    auto blk = std::get<ShardPtr>(coll);
    std::string_view name(blk->name(blk));

    if (name == "Events.Send") {
      out.back().eventsSent.push_back(blk->getParam(blk, 0));
    } else if (name == "Events.Receive") {
      out.back().eventsReceived.push_back(blk->getParam(blk, 0));
    } else if (name == "Produce") {
      out.back().channelsProduced.push_back(blk->getParam(blk, 0));
    } else if (name == "Consume") {
      out.back().channelsConsumed.push_back(blk->getParam(blk, 0));
    } else if (name == "Broadcast") {
      out.back().channelsBroadcasted.push_back(blk->getParam(blk, 0));
    } else if (name == "Listen") {
      out.back().channelsListened.push_back(blk->getParam(blk, 0));
    }

    // Also find nested shards
    const auto params = blk->parameters(blk);
    for (uint32_t i = 0; i < params.len; i++) {
      const auto &param = params.elements[i];
      const auto &types = param.valueTypes;
      bool potential = false;
      for (uint32_t j = 0; j < types.len; j++) {
        const auto &type = types.elements[j];
        if (type.basicType == SHType::ShardRef || type.basicType == SHType::Wire) {
          potential = true;
        } else if (type.basicType == SHType::Seq) {
          const auto &stypes = type.seqTypes;
          for (uint32_t k = 0; k < stypes.len; k++) {
            if (stypes.elements[k].basicType == SHType::ShardRef) {
              potential = true;
            }
          }
        }
      }
      if (potential)
        _gatherWires(blk->getParam(blk, i), out, wire);
    }
  } break;
  case 2: {
    // Shards seq
    auto bs = std::get<Shards>(coll);
    for (uint32_t i = 0; i < bs.len; i++) {
      _gatherWires(bs.elements[i], out, wire);
    }
  } break;
  case 3: {
    // Var
    auto var = std::get<SHVar>(coll);
    if (var.valueType == SHType::ShardRef) {
      _gatherWires(var.payload.shardValue, out, wire);
    } else if (var.valueType == SHType::Wire) {
      auto &nextWire = SHWire::sharedFromRef(var.payload.wireValue);
      _gatherWires(nextWire.get(), out, wire);
    } else if (var.valueType == SHType::Seq) {
      auto bs = var.payload.seqValue;
      for (uint32_t i = 0; i < bs.len; i++) {
        _gatherWires(bs.elements[i], out, wire);
      }
    }
  } break;
  default:
    SHLOG_FATAL("invalid state");
  }
}

void gatherWires(const ShardsCollection &coll, std::vector<WireNode> &out) {
  gatheringWiresPush();
  DEFER(gatheringWiresPop());
  _gatherWires(coll, out, nullptr);
}

SHVar hash(const SHVar &var) {
  hashingWiresPush();
  DEFER(hashingWiresPop());

  XXH3_state_s hashState;
  XXH3_INITSTATE(&hashState);
  XXH3_128bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);

  hash_update(var, &hashState);

  auto digest = XXH3_128bits_digest(&hashState);
  return Var(int64_t(digest.low64), int64_t(digest.high64));
}

void hash_update(const SHVar &var, void *state) {
  auto hashState = reinterpret_cast<XXH3_state_s *>(state);

  auto error = XXH3_128bits_update(hashState, &var.valueType, sizeof(var.valueType));
  shassert(error == XXH_OK);

  switch (var.valueType) {
  case SHType::Type: {
    updateTypeHash(*var.payload.typeValue, hashState);
  } break;
  case SHType::Bytes: {
    error = XXH3_128bits_update(hashState, var.payload.bytesValue, size_t(var.payload.bytesSize));
    shassert(error == XXH_OK);
  } break;
  case SHType::Path:
  case SHType::ContextVar:
  case SHType::String: {
    error = XXH3_128bits_update(hashState, var.payload.stringValue,
                                size_t(var.payload.stringLen > 0 || var.payload.stringValue == nullptr
                                           ? var.payload.stringLen
                                           : strlen(var.payload.stringValue)));
    shassert(error == XXH_OK);
  } break;
  case SHType::Image: {
    SHImage i = var.payload.imageValue;
    i.data = nullptr;
    error = XXH3_128bits_update(hashState, &i, sizeof(SHImage));
    shassert(error == XXH_OK);
    auto pixsize = getPixelSize(var);
    error = XXH3_128bits_update(
        hashState, var.payload.imageValue.data,
        size_t(var.payload.imageValue.channels * var.payload.imageValue.width * var.payload.imageValue.height * pixsize));
    shassert(error == XXH_OK);
  } break;
  case SHType::Audio: {
    SHAudio a = var.payload.audioValue;
    a.samples = nullptr;
    error = XXH3_128bits_update(hashState, &a, sizeof(SHAudio));
    shassert(error == XXH_OK);
    error = XXH3_128bits_update(hashState, var.payload.audioValue.samples,
                                size_t(var.payload.audioValue.channels * var.payload.audioValue.nsamples * sizeof(float)));
    shassert(error == XXH_OK);
  } break;
  case SHType::Seq: {
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      hash_update(var.payload.seqValue.elements[i], state);
    }
  } break;
  case SHType::Array: {
    for (uint32_t i = 0; i < var.payload.arrayValue.len; i++) {
      SHVar tmp{}; // only of blittable types and hash uses just type, so no init
                   // needed
      tmp.valueType = var.innerType;
      tmp.payload = var.payload.arrayValue.elements[i];
      hash_update(tmp, state);
    }
  } break;
  case SHType::Table: {
    // table is sorted, do all in 1 iteration
    auto &t = var.payload.tableValue;
    SHTableIterator it;
    t.api->tableGetIterator(t, &it);
    SHVar key;
    SHVar value;
    while (t.api->tableNext(t, &it, &key, &value)) {
      const auto kh = hash(key);
      XXH3_128bits_update(hashState, &kh, sizeof(SHInt2));
      const auto h = hash(value);
      XXH3_128bits_update(hashState, &h, sizeof(SHInt2));
    }
  } break;
  case SHType::Set: {
    boost::container::small_vector<std::pair<uint64_t, uint64_t>, 8> hashes;

    // just store hashes, sort and actually combine later
    auto &s = var.payload.setValue;
    SHSetIterator it;
    s.api->setGetIterator(s, &it);
    SHVar value;
    while (s.api->setNext(s, &it, &value)) {
      const auto h = hash(value);
      hashes.emplace_back(uint64_t(h.payload.int2Value[0]), uint64_t(h.payload.int2Value[1]));
    }

    std::sort(hashes.begin(), hashes.end());
    for (const auto &hash : hashes) {
      XXH3_128bits_update(hashState, &hash, sizeof(uint64_t));
    }
  } break;
  case SHType::ShardRef: {
    auto blk = var.payload.shardValue;
    auto name = blk->name(blk);
    auto error = XXH3_128bits_update(hashState, name, strlen(name));
    shassert(error == XXH_OK);

    auto params = blk->parameters(blk);
    for (uint32_t i = 0; i < params.len; i++) {
      auto pval = blk->getParam(blk, int(i));
      hash_update(pval, state);
    }

    if (blk->getState) {
      auto bstate = blk->getState(blk);
      hash_update(bstate, state);
    }
  } break;
  case SHType::Wire: {
    auto wire = SHWire::sharedFromRef(var.payload.wireValue);
    if (hashingWires().count(wire.get()) == 0) {
      hashingWires().insert(wire.get());

      error = XXH3_128bits_update(hashState, wire->name.c_str(), wire->name.length());
      shassert(error == XXH_OK);

      error = XXH3_128bits_update(hashState, &wire->looped, sizeof(wire->looped));
      shassert(error == XXH_OK);

      error = XXH3_128bits_update(hashState, &wire->unsafe, sizeof(wire->unsafe));
      shassert(error == XXH_OK);

      error = XXH3_128bits_update(hashState, &wire->pure, sizeof(wire->pure));
      shassert(error == XXH_OK);

      for (auto &blk : wire->shards) {
        SHVar tmp{};
        tmp.valueType = SHType::ShardRef;
        tmp.payload.shardValue = blk;
        hash_update(tmp, state);
      }

      for (auto &wireVar : wire->getVariables()) {
        error = XXH3_128bits_update(hashState, wireVar.first.payload.stringValue, wireVar.first.payload.stringLen);
        shassert(error == XXH_OK);
        hash_update(wireVar.second, state);
      }
    }
  } break;
  case SHType::Object: {
    error = XXH3_128bits_update(hashState, &var.payload.objectVendorId, sizeof(var.payload.objectVendorId));
    shassert(error == XXH_OK);
    error = XXH3_128bits_update(hashState, &var.payload.objectTypeId, sizeof(var.payload.objectTypeId));
    shassert(error == XXH_OK);
    if ((var.flags & SHVAR_FLAGS_USES_OBJINFO) == SHVAR_FLAGS_USES_OBJINFO && var.objectInfo && var.objectInfo->hash) {
      // hash of the hash...
      auto objHash = var.objectInfo->hash(var.payload.objectValue);
      error = XXH3_128bits_update(hashState, &objHash, sizeof(uint64_t));
      shassert(error == XXH_OK);
    } else {
      // this will be valid only within this process memory space
      // better then nothing
      error = XXH3_128bits_update(hashState, &var.payload.objectValue, sizeof(var.payload.objectValue));
      shassert(error == XXH_OK);
    }
  } break;
  case SHType::None:
  case SHType::Any:
  case SHType::EndOfBlittableTypes:
    break;
  case SHType::Enum:
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHEnum));
    shassert(error == XXH_OK);
    break;
  case SHType::Bool:
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHBool));
    shassert(error == XXH_OK);
    break;
  case SHType::Int:
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHInt));
    shassert(error == XXH_OK);
    break;
  case SHType::Int2:
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHInt2));
    shassert(error == XXH_OK);
    break;
  case SHType::Int3:
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHInt3));
    shassert(error == XXH_OK);
    break;
  case SHType::Int4:
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHInt4));
    shassert(error == XXH_OK);
    break;
  case SHType::Int8:
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHInt8));
    shassert(error == XXH_OK);
    break;
  case SHType::Int16:
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHInt16));
    shassert(error == XXH_OK);
    break;
  case SHType::Color:
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHColor));
    shassert(error == XXH_OK);
    break;
  case SHType::Float:
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHFloat));
    shassert(error == XXH_OK);
    break;
  case SHType::Float2:
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHFloat2));
    shassert(error == XXH_OK);
    break;
  case SHType::Float3:
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHFloat3));
    shassert(error == XXH_OK);
    break;
  case SHType::Float4:
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHFloat4));
    shassert(error == XXH_OK);
    break;
  }
}

SHString getString(uint32_t crc) {
  shassert(shards::GetGlobals().CompressedStrings);
  auto it = (*shards::GetGlobals().CompressedStrings).find(crc);
  return it != (*shards::GetGlobals().CompressedStrings).end() ? it->second.string : "";
}

void setString(uint32_t crc, SHString str) {
  shassert(shards::GetGlobals().CompressedStrings);
  SHOptionalString ls{str, crc};
  (*shards::GetGlobals().CompressedStrings).emplace(crc, ls);
}

void abortWire(SHContext *ctx, std::string_view errorText) { ctx->cancelFlow(errorText); }

void triggerVarValueChange(SHContext *context, const SHVar *name, const SHVar *var) {
  if ((var->flags & SHVAR_FLAGS_EXPOSED) == 0)
    return;

  auto &w = context->main;
  auto nameStr = SHSTRVIEW((*name));
  OnExposedVarSet ev{w->id, nameStr, *var, context->currentWire()};
  w->dispatcher.trigger(ev);
}

void triggerVarValueChange(SHWire *w, const SHVar *name, const SHVar *var) {
  if ((var->flags & SHVAR_FLAGS_EXPOSED) == 0)
    return;

  auto nameStr = SHSTRVIEW((*name));
  OnExposedVarSet ev{w->id, nameStr, *var, w};
  w->dispatcher.trigger(ev);
}
}; // namespace shards

// NO NAMESPACE here!

void SHWire::destroy() {
  for (auto it = shards.rbegin(); it != shards.rend(); ++it) {
    (*it)->cleanup(*it, nullptr);
  }
  for (auto it = shards.rbegin(); it != shards.rend(); ++it) {
    decRef(*it);
  }

  // find dangling variables, notice but do not destroy
  for (auto var : variables) {
    if (var.second.refcount > 0) {
      SHLOG_ERROR("Found a dangling variable: {}, wire: {}", var.first, name);
    }
  }

  if (composeResult) {
    shards::arrayFree(composeResult->requiredInfo);
    shards::arrayFree(composeResult->exposedInfo);
  }

  // finally reset the mesh
  mesh.reset();

#if SH_CORO_NEED_STACK_MEM
  if (stackMem) {
    ::operator delete[](stackMem, std::align_val_t{16});
  }
#endif
}

void SHWire::warmup(SHContext *context) {
  if (!warmedUp) {
    SHLOG_TRACE("Running warmup on wire: {}", name);

    // we likely need this early!
    mesh = context->main->mesh;
    warmedUp = true;

    context->wireStack.push_back(this);
    DEFER({ context->wireStack.pop_back(); });
    for (auto blk : shards) {
      try {
        if (blk->warmup) {
          auto status = blk->warmup(blk, context);
          if (status.code != SH_ERROR_NONE) {
            std::string_view msg(status.message.string, size_t(status.message.len));
            SHLOG_ERROR("Warmup failed on wire: {}, shard: {} (line: {}, column: {})", name, blk->name(blk), blk->line,
                        blk->column);
            throw shards::WarmupError(msg);
          }
        }
        if (context->failed()) {
          throw shards::WarmupError(context->getErrorMessage());
        }
      } catch (const std::exception &e) {
        SHLOG_ERROR("Shard warmup error, failed shard: {}", blk->name(blk));
        SHLOG_ERROR(e.what());
        // if the failure is from an exception context might not be uptodate
        if (!context->failed()) {
          context->cancelFlow(e.what());
        }
        throw;
      } catch (...) {
        SHLOG_ERROR("Shard warmup error, failed shard: {}", blk->name(blk));
        if (!context->failed()) {
          context->cancelFlow("foreign exception failure, check logs");
        }
        throw;
      }
    }

    SHLOG_TRACE("Ran warmup on wire: {}", name);
  } else {
    SHLOG_TRACE("Warmup already run on wire: {}", name);
  }
}

void SHWire::cleanup(bool force) {
  if (warmedUp && (force || wireUsers.size() == 0)) {
    SHLOG_TRACE("Running cleanup on wire: {} users count: {}", name, wireUsers.size());

    dispatcher.trigger(SHWire::OnCleanupEvent{this});

    warmedUp = false;

    // Run cleanup on all shards, prepare them for a new start if necessary
    // Do this in reverse to allow a safer cleanup
    for (auto it = shards.rbegin(); it != shards.rend(); ++it) {
      auto blk = *it;
      try {
        blk->cleanup(blk, context);
      }
#if SH_BOOST_COROUTINE
      catch (boost::context::detail::forced_unwind const &e) {
        SHLOG_WARNING("Shard cleanup boost forced unwind, failed shard: {}", blk->name(blk));
        throw; // required for Boost Coroutine!
      }
#endif
      catch (const std::exception &e) {
        SHLOG_ERROR("Shard cleanup error, failed shard: {}, error: {}", blk->name(blk), e.what());
      } catch (...) {
        SHLOG_ERROR("Shard cleanup error, failed shard: {}", blk->name(blk));
      }
    }

    // Also clear all variables reporting dangling ones
    for (auto var : variables) {
      if (var.second.refcount > 0) {
        SHLOG_ERROR("Found a dangling variable: {} in wire: {}", var.first, name);
      }
    }
    variables.clear();

    // finally reset the mesh
    auto mesh_ = mesh.lock();
    if (mesh_) {
      mesh_->visitedWires.erase(this);
      mesh_->scheduled.erase(shared_from_this());
    }
    mesh.reset();

    resumer = nullptr;

    SHLOG_TRACE("Ran cleanup on wire: {}", name);
  }
}

void shInit() {
  static bool globalInitDone = false;
  if (globalInitDone)
    return;
  globalInitDone = true;

  logging::setupDefaultLoggerConditional();

  if (GetGlobals().RootPath.size() > 0) {
    // set root path as current directory
    fs::current_path(GetGlobals().RootPath.c_str());
  } else {
    // set current path as root path
    auto cp = fs::current_path();
    GetGlobals().RootPath = cp.string();
  }

#ifdef SH_USE_UBSAN_REPORT
  auto absPath = fs::absolute(GetGlobals().RootPath);
  auto absPathStr = absPath.string();
  SHLOG_TRACE("Setting UBSAN report path to: {}", absPathStr);
  __sanitizer_set_report_path(absPathStr.c_str());
#endif

// UTF8 on windows
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  namespace fs = boost::filesystem;
  if (GetGlobals().ExePath.size() > 0) {
    auto pluginPath = fs::absolute(GetGlobals().ExePath.c_str()) / "shards";
    auto pluginPathStr = pluginPath.wstring();
    SHLOG_DEBUG("Adding dll path: {}", pluginPath.string());
    AddDllDirectory(pluginPathStr.c_str());
  }
  if (GetGlobals().RootPath.size() > 0) {
    auto pluginPath = fs::absolute(GetGlobals().RootPath.c_str()) / "shards";
    auto pluginPathStr = pluginPath.wstring();
    SHLOG_DEBUG("Adding dll path: {}", pluginPath.string());
    AddDllDirectory(pluginPathStr.c_str());
  }
#endif

  SHLOG_DEBUG("Hardware concurrency: {}", std::thread::hardware_concurrency());

  static_assert(sizeof(SHVarPayload) == 16);
  static_assert(sizeof(SHVar) == 32);
  static_assert(sizeof(SHMapIt) <= sizeof(SHTableIterator));
  static_assert(sizeof(SHHashSetIt) <= sizeof(SHSetIterator));
  static_assert(sizeof(OwnedVar) == sizeof(SHVar));
  static_assert(sizeof(TableVar) == sizeof(SHVar));
  static_assert(sizeof(SeqVar) == sizeof(SHVar));

  shards::registerShards();

#ifdef __EMSCRIPTEN__
  sh_emscripten_init();
  // fill up some interface so we don't need to know mem offsets JS side
  EM_ASM({ Module["SHCore"] = {}; });
  emscripten::val shInterface = emscripten::val::module_property("SHCore");
  SHCore *iface = shardsInterface(SHARDS_CURRENT_ABI);
  shInterface.set("log", emscripten::val(reinterpret_cast<uintptr_t>(iface->log)));
  shInterface.set("createMesh", emscripten::val(reinterpret_cast<uintptr_t>(iface->createMesh)));
  shInterface.set("destroyMesh", emscripten::val(reinterpret_cast<uintptr_t>(iface->destroyMesh)));
  shInterface.set("schedule", emscripten::val(reinterpret_cast<uintptr_t>(iface->schedule)));
  shInterface.set("unschedule", emscripten::val(reinterpret_cast<uintptr_t>(iface->unschedule)));
  shInterface.set("tick", emscripten::val(reinterpret_cast<uintptr_t>(iface->tick)));
  shInterface.set("sleep", emscripten::val(reinterpret_cast<uintptr_t>(iface->sleep)));
  shInterface.set("getGlobalWire", emscripten::val(reinterpret_cast<uintptr_t>(iface->getGlobalWire)));
  emscripten_get_now(); // force emscripten to link this call
#endif
}

#define API_TRY_CALL(_name_, _shard_)                       \
  {                                                         \
    try {                                                   \
      _shard_                                               \
    } catch (const std::exception &ex) {                    \
      SHLOG_ERROR(#_name_ " failed, error: {}", ex.what()); \
    }                                                       \
  }

bool sh_current_interface_loaded{false};
SHCore sh_current_interface{};

extern "C" {
SHVar *getWireVariable(SHWireRef wireRef, const char *name, uint32_t nameLen) {
  auto &wire = SHWire::sharedFromRef(wireRef);
  std::string_view nameView{name, nameLen};
  auto vName = shards::OwnedVar::Foreign(nameView);
  auto it = wire->getExternalVariables().find(vName);
  if (it != wire->getExternalVariables().end()) {
    return it->second.var;
  } else {
    auto it2 = wire->getVariables().find(vName);
    if (it2 != wire->getVariables().end()) {
      return &it2->second;
    }
  }
  return nullptr;
}

void triggerVarValueChange(SHContext *ctx, const SHVar *name, const SHVar *var) { shards::triggerVarValueChange(ctx, name, var); }

SHContext *getWireContext(SHWireRef wireRef) {
  auto &wire = SHWire::sharedFromRef(wireRef);
  return wire->context;
}

SHCore *__cdecl shardsInterface(uint32_t abi_version) {
  // for now we ignore abi_version
  if (sh_current_interface_loaded)
    return &sh_current_interface;

  // Load everything we know if we did not yet!
  try {
    shInit();
  } catch (const std::exception &ex) {
    SHLOG_ERROR("Failed to register core shards, error: {}", ex.what());
    return nullptr;
  }

  if (SHARDS_CURRENT_ABI != abi_version) {
    SHLOG_ERROR("A plugin requested an invalid ABI version.");
    return nullptr;
  }

  auto result = &sh_current_interface;
  sh_current_interface_loaded = true;

  result->alloc = [](uint32_t size) -> void * {
    auto mem = ::operator new(size, std::align_val_t{16});
    memset(mem, 0, size);
    return mem;
  };

  result->free = [](void *ptr) { ::operator delete(ptr, std::align_val_t{16}); };

  result->registerShard = [](const char *fullName, SHShardConstructor constructor) noexcept {
    API_TRY_CALL(registerShard, shards::registerShard(fullName, constructor);)
  };

  result->registerObjectType = [](int32_t vendorId, int32_t typeId, SHObjectInfo info) noexcept {
    API_TRY_CALL(registerObjectType, shards::registerObjectType(vendorId, typeId, info);)
  };

  result->findObjectTypeId = [](SHStringWithLen name) noexcept {
    return shards::findObjectTypeId(std::string_view{name.string, size_t(name.len)});
  };

  result->registerEnumType = [](int32_t vendorId, int32_t typeId, SHEnumInfo info) noexcept {
    API_TRY_CALL(registerEnumType, shards::registerEnumType(vendorId, typeId, info);)
  };

  result->findEnumId = [](SHStringWithLen name) noexcept {
    return shards::findEnumId(std::string_view{name.string, size_t(name.len)});
  };

  result->registerRunLoopCallback = [](const char *eventName, SHCallback callback) noexcept {
    API_TRY_CALL(registerRunLoopCallback, shards::registerRunLoopCallback(eventName, callback);)
  };

  result->registerExitCallback = [](const char *eventName, SHCallback callback) noexcept {
    API_TRY_CALL(registerExitCallback, shards::registerExitCallback(eventName, callback);)
  };

  result->unregisterRunLoopCallback = [](const char *eventName) noexcept {
    API_TRY_CALL(unregisterRunLoopCallback, shards::unregisterRunLoopCallback(eventName);)
  };

  result->unregisterExitCallback = [](const char *eventName) noexcept {
    API_TRY_CALL(unregisterExitCallback, shards::unregisterExitCallback(eventName);)
  };

  result->referenceVariable = [](SHContext *context, SHStringWithLen name) noexcept {
    std::string_view nameView{name.string, size_t(name.len)};
    return shards::referenceVariable(context, nameView);
  };

  result->referenceWireVariable = [](SHWireRef wire, SHStringWithLen name) noexcept {
    std::string_view nameView{name.string, size_t(name.len)};
    return shards::referenceWireVariable(wire, nameView);
  };

  result->releaseVariable = [](SHVar *variable) noexcept { return shards::releaseVariable(variable); };

  result->setExternalVariable = [](SHWireRef wire, SHStringWithLen name, SHExternalVariable *extVar) noexcept {
    auto &sc = SHWire::sharedFromRef(wire);
    auto vName = shards::OwnedVar::Foreign(name);
    sc->getExternalVariables()[vName] = *extVar;
  };

  result->removeExternalVariable = [](SHWireRef wire, SHStringWithLen name) noexcept {
    auto &sc = SHWire::sharedFromRef(wire);
    auto vName = shards::OwnedVar::Foreign(name);
    sc->getExternalVariables().erase(vName);
  };

  result->allocExternalVariable = [](SHWireRef wire, SHStringWithLen name, const struct SHTypeInfo *type) noexcept {
    auto &sc = SHWire::sharedFromRef(wire);
    auto vName = shards::OwnedVar::Foreign(name);
    auto res = new (std::align_val_t{16}) SHVar();
    sc->getExternalVariables()[vName] = SHExternalVariable{.var = res, .type = type};
    return res;
  };

  result->freeExternalVariable = [](SHWireRef wire, SHStringWithLen name) noexcept {
    auto &sc = SHWire::sharedFromRef(wire);
    auto vName = shards::OwnedVar::Foreign(name);
    auto extVar = sc->getExternalVariables()[vName];
    if (extVar.var) {
      ::operator delete(extVar.var, std::align_val_t{16});
    }
    sc->getExternalVariables().erase(vName);
  };

  result->suspend = [](SHContext *context, double seconds) noexcept {
    try {
      return shards::suspend(context, seconds);
    } catch (const shards::ActivationError &ex) {
      SHLOG_ERROR(ex.what());
      return SHWireState::Stop;
    }
  };

  result->getState = [](SHContext *context) noexcept { return context->getState(); };

  result->abortWire = [](SHContext *context, SHStringWithLen message) noexcept {
    std::string_view messageView{message.string, size_t(message.len)};
    context->cancelFlow(messageView);
  };

  result->cloneVar = [](SHVar *dst, const SHVar *src) noexcept { shards::cloneVar(*dst, *src); };

  result->destroyVar = [](SHVar *var) noexcept { shards::destroyVar(*var); };

  result->hashVar = [](const SHVar *var) noexcept { return shards::hash(*var); };

#define SH_ARRAY_IMPL(_arr_, _val_, _name_)                                                                    \
  result->_name_##Free = [](_arr_ *seq) noexcept { shards::arrayFree(*seq); };                                 \
                                                                                                               \
  result->_name_##Resize = [](_arr_ *seq, uint32_t size) noexcept { shards::arrayResize(*seq, size); };        \
                                                                                                               \
  result->_name_##Push = [](_arr_ *seq, const _val_ *value) noexcept { shards::arrayPush(*seq, *value); };     \
                                                                                                               \
  result->_name_##Insert = [](_arr_ *seq, uint32_t index, const _val_ *value) noexcept {                       \
    shards::arrayInsert(*seq, index, *value);                                                                  \
  };                                                                                                           \
                                                                                                               \
  result->_name_##Pop = [](_arr_ *seq) noexcept { return shards::arrayPop<_arr_, _val_>(*seq); };              \
                                                                                                               \
  result->_name_##FastDelete = [](_arr_ *seq, uint32_t index) noexcept { shards::arrayDelFast(*seq, index); }; \
                                                                                                               \
  result->_name_##SlowDelete = [](_arr_ *seq, uint32_t index) noexcept { shards::arrayDel(*seq, index); }

  SH_ARRAY_IMPL(SHSeq, SHVar, seq);
  SH_ARRAY_IMPL(SHTypesInfo, SHTypeInfo, types);
  SH_ARRAY_IMPL(SHParametersInfo, SHParameterInfo, params);
  SH_ARRAY_IMPL(Shards, ShardPtr, shards);
  SH_ARRAY_IMPL(SHExposedTypesInfo, SHExposedTypeInfo, expTypes);
  SH_ARRAY_IMPL(SHEnums, SHEnum, enums);
  SH_ARRAY_IMPL(SHStrings, SHString, strings);

  result->tableNew = []() noexcept {
    SHTable res;
    res.api = &shards::GetGlobals().TableInterface;
    res.opaque = new shards::SHMap();
    return res;
  };

  result->setNew = []() noexcept {
    SHSet res;
    res.api = &shards::GetGlobals().SetInterface;
    res.opaque = new shards::SHHashSet();
    return res;
  };

  result->composeWire = [](SHWireRef wire, SHValidationCallback callback, void *userData, SHInstanceData data) noexcept {
    auto &sc = SHWire::sharedFromRef(wire);
    try {
      return composeWire(sc.get(), callback, userData, data);
    } catch (const std::exception &e) {
      SHComposeResult res{};
      res.failed = true;
      auto msgTmp = shards::Var(e.what());
      shards::cloneVar(res.failureMessage, msgTmp);
      return res;
    } catch (...) {
      SHComposeResult res{};
      res.failed = true;
      auto msgTmp = shards::Var("foreign exception failure during composeWire");
      shards::cloneVar(res.failureMessage, msgTmp);
      return res;
    }
  };

  result->runWire = [](SHWireRef wire, SHContext *context, const SHVar *input) noexcept {
    auto &sc = SHWire::sharedFromRef(wire);
    return shards::runSubWire(sc.get(), context, *input);
  };

  result->composeShards = [](Shards shards, SHValidationCallback callback, void *userData, SHInstanceData data) noexcept {
    try {
      return shards::composeWire(shards, callback, userData, data);
    } catch (const std::exception &e) {
      SHLOG_TRACE("composeShards failed: {}", e.what());
      SHComposeResult res{};
      res.failed = true;
      auto msgTmp = shards::Var(e.what());
      shards::cloneVar(res.failureMessage, msgTmp);
      return res;
    } catch (...) {
      SHLOG_TRACE("composeShards failed: ...");
      SHComposeResult res{};
      res.failed = true;
      auto msgTmp = shards::Var("foreign exception failure during composeWire");
      shards::cloneVar(res.failureMessage, msgTmp);
      return res;
    }
  };

  result->validateSetParam = [](Shard *shard, int index, const SHVar *param, SHValidationCallback callback,
                                void *userData) noexcept {
    return shards::validateSetParam(shard, index, *param, callback, userData);
  };

  result->runShards = [](Shards shards, SHContext *context, const SHVar *input, SHVar *output) noexcept {
    return shards::activateShards(shards, context, *input, *output);
  };

  result->runShards2 = [](Shards shards, SHContext *context, const SHVar *input, SHVar *output) noexcept {
    return shards::activateShards2(shards, context, *input, *output);
  };

  result->runShardsHashed = [](Shards shards, SHContext *context, const SHVar *input, SHVar *output, SHVar *outHash) noexcept {
    return shards::activateShards(shards, context, *input, *output, *outHash);
  };

  result->runShardsHashed2 = [](Shards shards, SHContext *context, const SHVar *input, SHVar *output, SHVar *outHash) noexcept {
    return shards::activateShards2(shards, context, *input, *output, *outHash);
  };

  result->getWireInfo = [](SHWireRef wireref) noexcept {
    auto &sc = SHWire::sharedFromRef(wireref);
    auto wire = sc.get();
    SHWireInfo info{SHStringWithLen{wire->name.c_str(), wire->name.size()},
                    wire->looped,
                    wire->unsafe,
                    wire,
                    {!wire->shards.empty() ? &wire->shards[0] : nullptr, uint32_t(wire->shards.size()), 0},
                    shards::isRunning(wire),
                    wire->state == SHWire::State::Failed || !wire->finishedError.empty(),
                    SHStringWithLen{wire->finishedError.c_str(), wire->finishedError.size()},
                    &wire->finishedOutput};
    return info;
  };

  result->log = [](SHStringWithLen msg) noexcept {
    std::string_view sv(msg.string, size_t(msg.len));
    SHLOG_INFO(sv);
  };

  result->logLevel = [](int level, SHStringWithLen msg) noexcept {
    std::string_view sv(msg.string, size_t(msg.len));
    spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, (spdlog::level::level_enum)level,
                                      sv);
  };

  result->createShard = [](SHStringWithLen name) noexcept {
    std::string_view sv(name.string, size_t(name.len));
    auto shard = shards::createShard(sv);
    if (shard) {
      shassert(shard->refCount == 0 && "shard should have zero refcount");
      incRef(shard);
    }
    return shard;
  };

  result->releaseShard = [](struct Shard *shard) noexcept { decRef(shard); };

  result->createWire = [](SHStringWithLen name) noexcept {
    std::string_view sv(name.string, size_t(name.len));
    auto wire = SHWire::make(sv);
    return wire->newRef();
  };

  result->setWireLooped = [](SHWireRef wireref, SHBool looped) noexcept {
    auto &sc = SHWire::sharedFromRef(wireref);
    sc->looped = looped;
  };

  result->setWireUnsafe = [](SHWireRef wireref, SHBool unsafe) noexcept {
    auto &sc = SHWire::sharedFromRef(wireref);
    sc->unsafe = unsafe;
  };

  result->setWirePure = [](SHWireRef wireref, SHBool pure) noexcept {
    auto &sc = SHWire::sharedFromRef(wireref);
    sc->pure = pure;
  };

  result->setWireStackSize = [](SHWireRef wireref, uint64_t size) noexcept {
#if SH_CORO_NEED_STACK_MEM
    auto &sc = SHWire::sharedFromRef(wireref);
    sc->stackSize = size;
#endif
  };

  result->addShard = [](SHWireRef wireref, ShardPtr blk) noexcept {
    auto &sc = SHWire::sharedFromRef(wireref);
    sc->addShard(blk);
  };

  result->removeShard = [](SHWireRef wireref, ShardPtr blk) noexcept {
    auto &sc = SHWire::sharedFromRef(wireref);
    sc->removeShard(blk);
  };

  result->destroyWire = [](SHWireRef wire) noexcept { SHWire::deleteRef(wire); };

  result->stopWire = [](SHWireRef wire) {
    auto &sc = SHWire::sharedFromRef(wire);
    SHVar output{};
    shards::stop(sc.get(), &output);
    return output;
  };

  result->destroyWire = [](SHWireRef wire) noexcept { SHWire::deleteRef(wire); };

  result->destroyWire = [](SHWireRef wire) noexcept { SHWire::deleteRef(wire); };

  result->getGlobalWire = [](SHStringWithLen name) noexcept {
    std::string sv(name.string, size_t(name.len));
    auto it = shards::GetGlobals().GlobalWires.find(std::move(sv));
    if (it != shards::GetGlobals().GlobalWires.end()) {
      return SHWire::weakRef(it->second);
    } else {
      return (SHWireRef) nullptr;
    }
  };

  result->setGlobalWire = [](SHStringWithLen name, SHWireRef wire) noexcept {
    std::string sv(name.string, size_t(name.len));
    shards::GetGlobals().GlobalWires[std::move(sv)] = SHWire::sharedFromRef(wire);
  };

  result->unsetGlobalWire = [](SHStringWithLen name) noexcept {
    std::string sv(name.string, size_t(name.len));
    auto it = shards::GetGlobals().GlobalWires.find(std::move(sv));
    if (it != shards::GetGlobals().GlobalWires.end()) {
      shards::GetGlobals().GlobalWires.erase(it);
    }
  };

  result->createMesh = []() noexcept {
    auto mesh = SHMesh::makePtr();
    SHLOG_TRACE("createMesh {}", (void *)(*mesh).get());
    return reinterpret_cast<SHMeshRef>(mesh);
  };

  result->createMeshVar = []() noexcept {
    auto mesh = SHMesh::make();
    auto meshVar = SHMesh::MeshVar.Emplace(std::move(mesh));
    return SHMesh::MeshVar.Get(meshVar);
  };

  result->destroyMesh = [](SHMeshRef mesh) noexcept {
    auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
    SHLOG_TRACE("destroyMesh {}", (void *)(*smesh).get());
    delete smesh;
  };

  result->compose = [](SHMeshRef mesh, SHWireRef wire) noexcept {
    try {
      auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
      (*smesh)->compose(SHWire::sharedFromRef(wire));
    } catch (const std::exception &e) {
      SHLOG_ERROR("Errors while composing: {}", e.what());
      return false;
    } catch (...) {
      SHLOG_ERROR("Errors while composing");
      return false;
    }
    return true;
  };

  result->schedule = [](SHMeshRef mesh, SHWireRef wire, SHBool compose) noexcept {
    try {
      auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
      (*smesh)->schedule(SHWire::sharedFromRef(wire), shards::Var::Empty, compose);
    } catch (const std::exception &e) {
      SHLOG_ERROR("Errors while scheduling: {}", e.what());
    } catch (...) {
      SHLOG_ERROR("Errors while scheduling");
    }
  };

  result->unschedule = [](SHMeshRef mesh, SHWireRef wire) noexcept {
    auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
    (*smesh)->remove(SHWire::sharedFromRef(wire));
  };

  result->tick = [](SHMeshRef mesh) noexcept {
    auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
    if ((*smesh)->tick())
      return true; // continue
    else
      return false; // had an error
  };

  result->isEmpty = [](SHMeshRef mesh) noexcept {
    auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
    if ((*smesh)->empty())
      return true;
    else
      return false;
  };

  result->terminate = [](SHMeshRef mesh) noexcept {
    auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
    (*smesh)->terminate();
  };

  result->sleep = [](double seconds, bool runCallbacks) noexcept { shards::sleep(seconds, runCallbacks); };

  result->getRootPath = []() noexcept { return shards::GetGlobals().RootPath.c_str(); };

  result->setRootPath = [](const char *p) noexcept {
    shards::GetGlobals().RootPath = p;
    shards::loadExternalShards(p);
    fs::current_path(p);
  };

  result->asyncActivate = [](SHContext *context, void *userData, SHAsyncActivateProc call, SHAsyncCancelProc cancel_call) {
    return shards::awaitne(
        context, [=] { return call(context, userData); },
        [=] {
          if (cancel_call)
            cancel_call(context, userData);
        });
  };

  result->getShards = []() {
    SHStrings s{};
    for (auto [name, _] : shards::GetGlobals().ShardsRegister) {
      shards::arrayPush(s, name.data());
    }
    return s;
  };

  result->readCachedString = [](uint32_t crc) {
    auto s = shards::getString(crc);
    return SHOptionalString{s, crc};
  };

  result->writeCachedString = [](uint32_t crc, SHString str) {
    shards::setString(crc, str);
    return SHOptionalString{str, crc};
  };

  result->decompressStrings = []() {
#ifdef SH_COMPRESSED_STRINGS
    shards::decompressStrings();
#endif
  };

  result->isEqualVar = [](const SHVar *v1, const SHVar *v2) -> SHBool { return *v1 == *v2; };

  result->isEqualType = [](const SHTypeInfo *t1, const SHTypeInfo *t2) -> SHBool { return *t1 == *t2; };

  result->deriveTypeInfo = [](const SHVar *v, const struct SHInstanceData *data) -> SHTypeInfo {
    return deriveTypeInfo(*v, *data);
  };

  result->freeDerivedTypeInfo = [](SHTypeInfo *t) { freeDerivedInfo(*t); };

  result->findEnumInfo = &shards::findEnumInfo;

  result->findObjectInfo = &shards::findObjectInfo;

  result->type2Name = [](SHType type) { return type2Name_raw(type); };

  return result;
}
}

namespace shards {
void decRef(ShardPtr shard) {
  auto atomicRefCount = boost::atomics::make_atomic_ref(shard->refCount);
  shassert(atomicRefCount > 0);
  if (atomicRefCount.fetch_sub(1) == 1) {
    // SHLOG_TRACE("DecRef 0 shard {:x} {}", (size_t)shard, shard->name(shard));
    shard->destroy(shard);
  }
}

void incRef(ShardPtr shard) {
  auto atomicRefCount = boost::atomics::make_atomic_ref(shard->refCount);
  if (atomicRefCount.fetch_add(1) == 0) {
    // SHLOG_TRACE("IncRef 0 shard {:x} {}", (size_t)shard, shard->name(shard));
  }
}
} // namespace shards

#ifdef TRACY_ENABLE
void *operator new(std::size_t count) {
  void *ptr = std::malloc(count);
  if (GetTracy().isInitialized())
    TracyAlloc(ptr, count);
  return ptr;
}

void *operator new[](std::size_t count) {
  void *ptr = std::malloc(count);
  if (GetTracy().isInitialized())
    TracyAlloc(ptr, count);
  return ptr;
}

void *operator new(std::size_t count, std::align_val_t alignment) {
  std::size_t align_value = static_cast<std::size_t>(alignment);
  std::size_t aligned_count = (count + align_value - 1) / align_value * align_value;
#ifdef WIN32
  void *ptr = _aligned_malloc(aligned_count, align_value);
#else
  void *ptr = std::aligned_alloc(align_value, aligned_count);
#endif
  if (GetTracy().isInitialized())
    TracyAlloc(ptr, count);
  return ptr;
}

void *operator new[](std::size_t count, std::align_val_t alignment) {
  std::size_t align_value = static_cast<std::size_t>(alignment);
  std::size_t aligned_count = (count + align_value - 1) / align_value * align_value;
#ifdef WIN32
  void *ptr = _aligned_malloc(aligned_count, align_value);
#else
  void *ptr = std::aligned_alloc(align_value, aligned_count);
#endif
  if (GetTracy().isInitialized())
    TracyAlloc(ptr, count);
  return ptr;
}

void operator delete(void *ptr) noexcept {
  if (GetTracy().isInitialized())
    TracyFree(ptr);
  std::free(ptr);
}

void operator delete[](void *ptr) noexcept {
  if (GetTracy().isInitialized())
    TracyFree(ptr);
  std::free(ptr);
}

void operator delete(void *ptr, std::align_val_t alignment) noexcept {
  if (GetTracy().isInitialized())
    TracyFree(ptr);
#ifdef WIN32
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

void operator delete[](void *ptr, std::align_val_t alignment) noexcept {
  if (GetTracy().isInitialized())
    TracyFree(ptr);
#ifdef WIN32
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

void operator delete(void *ptr, std::size_t count) noexcept {
  if (GetTracy().isInitialized())
    TracyFree(ptr);
  std::free(ptr);
}

void operator delete[](void *ptr, std::size_t count) noexcept {
  if (GetTracy().isInitialized())
    TracyFree(ptr);
  std::free(ptr);
}

#endif
