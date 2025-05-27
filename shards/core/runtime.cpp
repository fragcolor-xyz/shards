/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "runtime.hpp"
#include "type_matcher.hpp"
#include <shards/common_types.hpp>
#include <shards/core/foundation.hpp>
#include <shards/core/platform.hpp>
#include <shards/core/compose.hpp>
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
#include <shards/log/log.hpp>
#include <shared_mutex>
#include <boost/atomic/atomic_ref.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/algorithm/string.hpp>
#include <shards/fast_string/fast_string.hpp>
#include "hash.inl"
#include "utils.hpp"
#include "trait.hpp"
#include "platform.hpp"
#include "serialization.hpp"
#include "lang_api.hpp"
#include "log_api.hpp"

#if SH_APPLE || SH_LINUX
#include <dlfcn.h>
#endif

#ifdef SH_COMPRESSED_STRINGS
#include <shards/wire_dsl.hpp>
#endif

namespace fs = boost::filesystem;

using namespace shards;

#if SH_EMSCRIPTEN
#include <emscripten.h>
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

inline shards::logging::Logger getPerfLogger() { return shards::logging::getOrCreate("perf"); }
#ifndef NDEBUG
#define SHLOG_PERF_WARN(...) SPDLOG_LOGGER_DEBUG(shards::getPerfLogger(), __VA_ARGS__)
#else
#define SHLOG_PERF_WARN(...) (void)0
#endif

auto &getCompiledCompressedStrings() {
  static std::remove_pointer_t<decltype(Globals::CompressedStrings)> CompiledCompressedStrings;
  if (GetGlobals().CompressedStrings == nullptr)
    GetGlobals().CompressedStrings = &CompiledCompressedStrings;
  return CompiledCompressedStrings;
}

#ifdef SH_COMPRESSED_STRINGS
SHOptionalString getCompiledCompressedString(uint32_t id) {
  auto &_comp = getCompiledCompressedStrings(); // make sure it's initialized

  auto it = _comp.find(id);
  if (it != _comp.end()) {
    auto val = it->second;
    val.crc = id; // make sure we return with crc to allow later lookups!
    return val;
  } else {
    return SHOptionalString{nullptr, id}; // make sure we return with crc to allow later lookups!
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
  if (!wire->finishedOutput.has_value() || wire->finishedOutput->valueType != SHType::Seq) {
    throw shards::SHException("Failed to decompress strings!");
  }

  for (uint32_t i = 0; i < wire->finishedOutput->payload.seqValue.len; i++) {
    auto pair = wire->finishedOutput->payload.seqValue.elements[i];
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
  auto &_comp = getCompiledCompressedStrings(); // make sure it's initialized

  SHOptionalString ls{str, id};
  _comp.emplace(id, ls);
  return ls;
}
#endif

#ifdef SH_USE_UBSAN
extern "C" void __sanitizer_set_report_path(const char *path);
#endif

void loadExternalShards(std::string from) {
  static std::unordered_set<std::string> loaded;
  static std::mutex loadedMutex;

  std::unique_lock<std::mutex> lock(loadedMutex);

  namespace fs = boost::filesystem;
  auto root = fs::path(from);
  auto pluginPath = root / "externals";
  if (!fs::exists(pluginPath))
    return;

  for (auto &p : fs::recursive_directory_iterator(pluginPath)) {
    if (p.status().type() == fs::file_type::regular_file) {
      auto ext = p.path().extension();
      if (ext == ".dll" || ext == ".so" || ext == ".dylib") {
        auto filename = p.path().filename().string();
        if (loaded.find(filename) != loaded.end()) {
          continue;
        }

        // Skip files starting with "lib"
        if (filename.rfind("lib", 0) == 0) {
          continue;
        }

        auto dllstr = p.path().string();

        SHLOG_INFO("Loading external dll: {} path: {}", filename, dllstr);
#if SH_WINDOWS
        auto handle = LoadLibraryExA(dllstr.c_str(), NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!handle) {
          SHLOG_ERROR("LoadLibrary failed, error: {}", GetLastError());
        }
#elif SH_LINUX || SH_APPLE
        auto handle = dlopen(dllstr.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
          SHLOG_ERROR("LoadLibrary failed, error: {}", dlerror());
        }
#endif
        loaded.insert(filename);
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
  ZoneScoped;
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
  SHLOG_DEBUG("Loading external shards from exe path: {}", GetGlobals().ExePath);
  loadExternalShards(std::string(GetGlobals().ExePath.c_str()));
  if (GetGlobals().RootPath != GetGlobals().ExePath) {
    SHLOG_DEBUG("Loading external shards from root path: {}", GetGlobals().RootPath);
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

  shard->nameLength = uint32_t(name.length());

  // Pre-allocated ID range implementation (512 IDs per batch)
  static std::atomic_uint64_t shardIdRangeCounter = 1;
  static thread_local uint64_t nextLocalId = 0;
  static thread_local uint64_t localIdEnd = 0;
  static constexpr uint64_t ID_RANGE_SIZE = 512;

  // Check if we need a new range of IDs
  if (nextLocalId >= localIdEnd) {
    // Claim a new range of IDs with a single atomic operation
    uint64_t rangeStart = shardIdRangeCounter.fetch_add(ID_RANGE_SIZE, std::memory_order_relaxed);
    nextLocalId = rangeStart;
    localIdEnd = rangeStart + ID_RANGE_SIZE;
  }

  shard->id = nextLocalId++;

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

void registerShard(std::string_view name, SHShardConstructor constructor, std::string_view fullTypeName) {
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

void imageIncRef(SHImage *ptr) {
  shassert(ptr);
  auto atomicRefCount = boost::atomics::make_atomic_ref(ptr->refCount);
  shassert(atomicRefCount > 0);
  atomicRefCount.add(1);
}
void imageDecRef(SHImage *ptr) {
  shassert(ptr);
  auto atomicRefCount = boost::atomics::make_atomic_ref(ptr->refCount);
  shassert(atomicRefCount > 0);
  if (atomicRefCount.fetch_sub(1) == 1) {
    if (ptr->free)
      ptr->free(ptr);
    ::operator delete[](ptr, std::align_val_t(16));
  }
}
SHImage *imageNew(uint32_t dataLen) {
  size_t headerSize = sizeof(SHImage);
  SHImage *image = reinterpret_cast<SHImage *>(new (std::align_val_t(16)) uint8_t[headerSize + dataLen]);
  memset(image, 0, sizeof(SHImage));
  image->refCount = 1;
  if (dataLen > 0) {
    image->data = (uint8_t *)&image[1];
    shassert((size_t(image->data) & 0xf) == 0);
  }
  return image;
}
SHImage *imageClone(SHImage *src) {
  shassert(src);
  uint32_t dataLen = imageDeriveDataLength(src);
  SHImage *newImage = imageNew(dataLen);
  memcpy(newImage, src->data, dataLen);
  return src;
}

uint32_t imageGetPixelSize(SHImage *img) {
  shassert(img);
  auto pixsize = 1;
  if ((img->flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
    pixsize = 2;
  else if ((img->flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
    pixsize = 4;
  return pixsize;
}

uint32_t imageGetRowStride(SHImage *img) {
  shassert(img);
  if (img->rowStride)
    return img->rowStride;
  return uint32_t(img->width * img->channels * imageGetPixelSize(img));
}

uint32_t imageDeriveDataLength(SHImage *img) {
  shassert(img);
  auto spixsize = imageGetPixelSize(img);

  return uint32_t(img->height * img->width * img->channels * spixsize);
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

SHWireState suspend(SHContext *context, double seconds, bool sleepOnWorker) {
  if (unlikely(!context->shouldContinue())) {
    throw ActivationError(fmt::format("Trying to suspend a context that is not running! - state: {}", context->getState()));
  } else if (unlikely(!context->continuation)) {
    throw ActivationError("Trying to suspend a context without coroutine!");
  }

  if (unlikely(context->onWorkerThread) && sleepOnWorker) {
    // ok in this case use thread sleep and exit
    if (seconds <= 0.0) {
      // yield to other threads
      std::this_thread::yield();
    } else {
      std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    }
  } else {
    if (seconds <= 0) {
      context->next = SHDuration(0);
    } else {
      context->next = SHClock::now().time_since_epoch() + SHDuration(seconds);
    }

    auto currentWire = context->currentWire();
    coroSuspended(context);
    coroutineSuspend(*context->continuation);
    shassert(context->currentWire() == currentWire);
    coroResumed(context);
  }

  // still advancing the step counter, to flag we are in another time step
  ++context->stepCounter;

  return context->getState();
}

ALWAYS_INLINE bool is_stack_within_limit(volatile void *stack_start_address, size_t hard_max, size_t recursion_buffer) {
  if (stack_start_address == nullptr) {
    return true;
  }

  // Create a local variable
  volatile uint8_t local_var;
  // Get the address of the local variable
  uintptr_t local_var_address = reinterpret_cast<uintptr_t>(&local_var);
  uintptr_t start_address = reinterpret_cast<uintptr_t>(stack_start_address);

  // Calculate the approximate stack size
  size_t stack_size;
#ifdef EMSCRIPTEN
  // Emscripten stack grows upward
  stack_size = local_var_address - start_address;
#else
  // Normal stack grows downward
  stack_size = start_address - local_var_address;
#endif

  // Adjust hard max to accommodate recursion buffer
  size_t adjusted_max = hard_max - recursion_buffer;

  return stack_size <= adjusted_max;
}

NO_INLINE void handleActivationError(SHContext *context, Shard *blk) {
  auto &err = context->getErrorMessage();
  auto msg = fmt::format("{} -> Error: {}, Line: {}, Column: {}", blk->name(blk), err, blk->line, blk->column);
  SHLOG_ERROR(msg);
  context->pushError(std::move(msg));
  auto wire = context->currentWire();
  if (wire) {
    auto mesh = wire->mesh.lock();
    if (mesh) {
      shards::OwnedVar errVar((Var(context->getErrorMessage())));
      mesh->dispatcher.trigger(SHWire::OnErrorEvent{wire, blk, std::move(errVar)});
    }
  }
}

template <typename T, bool HANDLES_RETURN>
ALWAYS_INLINE SHWireState shardsActivation(T &shards, SHContext *context, const SHVar &initialInput, SHVar &finalOutput,
                                           SHVar *outHash = nullptr) noexcept {
  // check for stack overflow
#if SH_CORO_NEED_STACK_MEM
#if SH_USE_UBSAN
  // Slightly bigger for assertions, etc.
  const uint32_t padding = 16 * 1024;
#else
  const uint32_t padding = 8 * 1024;
#endif
  if (!context->onWorkerThread && !is_stack_within_limit(context->stackStart, context->main->stackSize, padding)) {
    // we let the top level handle this
    SHLOG_ERROR("Stack overflow detected, wire: {}", context->currentWire()->name);
    context->cancelFlow("Stack overflow detected");
    return SHWireState::Error;
  }
#endif

  // store initial input, as pointer, otherwise we risk corruption if the input changes while we are processing
  auto *input = &initialInput;
  const auto *output = &finalOutput;

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

    context->internal.currentShard = blk;

    {
#ifdef TRACY_ENABLE
#define ZoneNoCallstack(varname, name, active)                                                                               \
  static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location, TracyLine){name, TracyFunction, TracyFile, \
                                                                                             (uint32_t)TracyLine, 0};        \
  tracy::ScopedZone varname(&TracyConcat(__tracy_source_location, TracyLine), active)

      ZoneNoCallstack(___tracy_scoped_zone, "activateShard", true);
      ZoneName(blk->name(blk), blk->nameLength);

#undef ZoneNoCallstack
#endif

      if (blk->inlineShardId != InlineShard::NotInline) {
        output = activateShardInline(blk, context, *input);
        shassert(output && "activateShardInline returned nullptr");
      } else {
        output = blk->activate(blk, context, input);
      }
    }

    // Deal with aftermath of activation
    if (unlikely(!context->shouldContinue())) {
      finalOutput = *output; // shallow copy it anyways
      auto state = context->getState();
      switch (state) {
      case SHWireState::Return:
        if constexpr (HANDLES_RETURN)
          context->continueFlow();
        return SHWireState::Return;
      case SHWireState::Error: {
        handleActivationError(context, blk);
      }
      case SHWireState::Stop:
      case SHWireState::Restart:
        return state;
      case SHWireState::Rebase:
        // reset input to wire one and reset state
        input = &initialInput;
        context->continueFlow();
        continue;
      case SHWireState::Continue:
        break;
      }
    }

    // Pass output to next block input
    input = output;
  }

  finalOutput = *output;
  return SHWireState::Continue;
}

SHWireState activateShards(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return shardsActivation<Shards, false>(shards, context, wireInput, output);
}

SHWireState activateShards2(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return shardsActivation<Shards, true>(shards, context, wireInput, output);
}

SHWireState activateShards(SHSeq shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return shardsActivation<SHSeq, false>(shards, context, wireInput, output);
}

SHWireState activateShards2(SHSeq shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return shardsActivation<SHSeq, true>(shards, context, wireInput, output);
}

ALWAYS_INLINE void coroResumed(SHContext *context) {
  SHWire *wire = context->currentWire();
  if (!wire)
    return;

#if SH_DEBUG_THREAD_NAMES
  shards::pushThreadName(wire->threadNameStrings.init(wire).resumeStr);
#endif

#ifdef SH_VERBOSE_COROUTINES_LOGGING
  SHLOG_TRACE("> Resumed wire {}", wire->name);
#endif

#if SH_DEBUG
  shassert(!context->isResumed);
  context->isResumed = true;
#endif

  auto &logTs = shards::logging::ThreadState::get();
  if (context->linkedLogContext) {
    // Push thread logging state
    auto prevContext = logTs.current;
    std::swap(context->prevLogContext, logTs.current);
    // Reattach the parent log context, in case we are stepping from somewhere else
    context->linkedLogContext->linkRootTo(prevContext);
  } else {
    // Push thread logging state
    std::swap(context->prevLogContext, logTs.current);
  }
}

ALWAYS_INLINE void coroSuspended(SHContext *context) {
  SHWire *wire = context->currentWire();
  if (!wire)
    return;

#if SH_DEBUG
  shassert(context->isResumed);
  context->isResumed = false;
#endif

#if SH_DEBUG_THREAD_NAMES
  shards::popThreadName();
#endif

#ifdef SH_VERBOSE_COROUTINES_LOGGING
  SHLOG_TRACE("< Suspended wire {}", wire->name);
#endif

  auto &logTs = shards::logging::ThreadState::get();
  if (context->linkedLogContext) {
    shassert(context->prevLogContext != &*context->linkedLogContext && "Prev log context should not be linked log context");
    context->linkedLogContext->unlink();
  }
  std::swap(context->prevLogContext, logTs.current);
}

ALWAYS_INLINE void coroExtResume(SHWire *wire) {
  if (!wire)
    return;

#if SH_DEBUG_THREAD_NAMES
  shards::pushThreadName(wire->threadNameStrings.init(wire).extResumeStr);
#endif

  TracyCoroEnter(wire);

#ifdef SH_VERBOSE_COROUTINES_LOGGING
  SHLOG_TRACE("Resuming wire {}", wire->name);
#endif
}

ALWAYS_INLINE void coroExtSuspend(SHWire *wire) {
  if (!wire)
    return;

#if SH_DEBUG_THREAD_NAMES
  shards::popThreadName();
#endif

  TracyCoroExit(wire);

#ifdef SH_VERBOSE_COROUTINES_LOGGING
  SHLOG_TRACE("Suspending wire {}", wire->name);
#endif
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
    SHLOG_ERROR("{}", boost::stacktrace::to_string(boost::stacktrace::stacktrace()));
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

// pub extern "C" fn setup_panic_hook()
extern "C" void setup_panic_hook();
extern "C" void shards_flush_logs() { spdlog::default_logger()->flush(); }

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
  } else {
    setup_panic_hook();
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

  try {
    auto state = shardsActivation<std::vector<ShardPtr>, false>(wire->shards, context, wireInput, wire->previousOutput);
    switch (state) {
    case SHWireState::Return:
      return {context->getFlowStorage(), SHRunWireOutputState::Returned};
    case SHWireState::Restart:
      return {context->getFlowStorage(), SHRunWireOutputState::Restarted};
    case SHWireState::Error:
      // shardsActivation handles error logging and such
      shassert(context->failed());
      return {Var::Empty, SHRunWireOutputState::Failed};
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
    return {Var::Empty, SHRunWireOutputState::Failed};
  }

  return {wire->previousOutput, SHRunWireOutputState::Running};
}

void run(SHWire *wire, shards::Coroutine *coro) {
  // store stack start address here
  volatile void *stackStart = nullptr;
  auto running = true;

  // we need this cos by the end of this call we might get suspended/resumed and state changes! this wont
  bool failed = false;

  // Reset state
  wire->state = SHWire::State::Prepared;
  wire->finishedOutput.reset();
  wire->finishedError.clear();

  // Create a new context and copy the sink in
  SHContext context(coro, wire);
  context.stackStart = &stackStart;

  // if the wire had a context (Stepped wires in wires.cpp)
  // copy some stuff from it
  if (wire->context) {
    context.wireStack = wire->context->wireStack;
    // need to add back ourself
    context.wireStack.push_back(wire);
    // also set parent
    context.parent = wire->context;
    // Used for warmup context (owning shard, e.g. Step, Branch)
    context.internal.currentShard = wire->context->internal.currentShard;
  }

  // also populate context in wire
  wire->context = &context;

  auto &logTs = shards::logging::ThreadState::get();
  auto prevLogContext = logTs.current;

  // Populate context before triggering coroResumed/logs
  // This switches to the coroutine log context
  coroResumed(&context);

  auto shouldInheritMeshLogContext = [](SHWire *wire) {
    if (auto m = wire->mesh.lock()) {
      return m->inheritLogContext;
    }
    return false;
  };

  // Link to parent logging context
  // NOTE: This is after coroResumed, so we are on the coroutine log context
  if (context.parent || shouldInheritMeshLogContext(wire)) {
    context.linkedLogContext.emplace();
    context.linkedLogContext->linkRootTo(prevLogContext);
  }

  SHLOG_TRACE("Wire {} rolling", wire->name);

  // We pre-rolled our coro, suspend here before actually starting.
  // This allows us to allocate the stack ahead of time.
  // And call warmup on all the shards!
  try {
    wire->warmup(&context);
  } catch (std::exception &e) {
    SHLOG_ERROR("Wire {} warmup failed with error: {}", wire->name, e.what());
    wire->state = SHWire::State::Failed;
    goto endOfWire;
  } catch (...) {
    // inside warmup we re-throw, we handle logging and such there
    wire->state = SHWire::State::Failed;
    SHLOG_ERROR("Wire {} warmup failed", wire->name);
    goto endOfWire;
  }

  // yield after warming up
  coroSuspended(&context);
  coroutineSuspend(*context.continuation);
  coroResumed(&context);

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
      wire->state = SHWire::State::Failed;
      failed = true;
      context.stopFlow(Var::Empty);
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

      coroSuspended(&context);
      coroutineSuspend(*context.continuation);
      coroResumed(&context);

      ++context.stepCounter;

      // This is delayed upon continuation!!
      if (context.shouldStop()) {
        SHLOG_DEBUG("Wire {} aborted on resume", wire->name);
        break;
      }
    }
  }

endOfWire:
  if (failed || context.failed()) {
    wire->finishedError = context.getErrorMessage();
    if (wire->finishedError.empty()) {
      wire->finishedError = "Generic error";
    }

    // print our stack log nicely now
    auto msg = fmt::format("Wire {} failed with error:\n{}", wire->name, context.formatErrorStack());
    SHLOG_ERROR(msg);
    shards::OwnedVar errVar((Var(context.formatErrorStack())));
    {
      // NOTE: Keep the mesh ptr scoped so we don't keep the mesh referenced
      std::shared_ptr<SHMesh> mesh = wire->mesh.lock();
      mesh->dispatcher.trigger(SHWire::OnErrorEvent{wire, nullptr, std::move(errVar)});
    }

    if (wire->resumer) {
      // also stop the resumer parent in this case
      wire->resumer->context->cancelFlow(wire->finishedError);
    }
  } else {
    wire->finishedOutput = wire->previousOutput; // cloning over! (OwnedVar)
  }

  // run cleanup on all the shards
  // ensure stop state is set
  context.stopFlow(wire->previousOutput);

  // if we have a resumer we return to it
  if (wire->resumer) {
    SHLOG_TRACE("Wire {} ending and resuming {}", wire->name, wire->resumer->name);
    wire->resumer->childWire = nullptr; // reset childWire, this will resume the wire
    wire->resumer = nullptr;
  }

  // NOTE: This is here because cleanup resets the mesh
  std::shared_ptr<SHMesh> mesh = wire->mesh.lock();

  // Set onLastResume so tick keeps processing mesh tasks on cleanup
  context.onLastResume = true;
  wire->cleanup(true);
  context.onLastResume = false;

  // Need to take care that we might have stopped the wire very early due to
  // errors and the next eventual stop() should avoid resuming
  if (wire->state != SHWire::State::Failed)
    wire->state = SHWire::State::Ended;

  // NOTE: Keep the mesh ptr scoped so we don't keep the mesh referenced
  if (mesh) {
    mesh->dispatcher.trigger(SHWire::OnStopEvent{wire});
    mesh.reset();
  }

  // Final call to coroSuspend & log before clearing context
  SHLOG_TRACE("Wire {} ended", wire->name);

  // Need to clear log context here
  context.linkedLogContext.reset();

  coroSuspended(&context);

  // Make sure to clear context at the end so it doesn't point to invalid stack memory
  wire->context = nullptr;

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

void EventDispatcher::assignType(SHTypeInfo type) {
  if (this->type->basicType != SHType::None) {
    bool matching = matchTypes(type, this->type, false, true, true);
    if (!matching)
      throw std::runtime_error(fmt::format("Event type mismatch, expected {} got {}", *this->type, type));
  } else {
    this->type = type;
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
  } break;
  case SHType::Image:
    shassert(var.payload.imageValue);
    imageDecRef(var.payload.imageValue);
    break;
  case SHType::ShardRef:
    shassert(var.payload.shardValue);
    decRef(var.payload.shardValue);
    break;
  case SHType::Type:
    shassert(var.payload.typeValue);
    freeDerivedInfo(*var.payload.typeValue);
    delete var.payload.typeValue;
    break;
  case SHType::Trait:
    shassert(var.payload.traitValue);
    freeTrait(*var.payload.traitValue);
    delete var.payload.traitValue;
    break;
  case SHType::Audio:
    delete[] var.payload.audioValue.samples;
    break;
  case SHType::Object:
    if ((var.flags & SHVAR_FLAGS_USES_OBJINFO) == SHVAR_FLAGS_USES_OBJINFO) {
      shassert(var.objectInfo && "ObjectInfo is null");
      // check if weak ref
      if ((var.flags & SHVAR_FLAGS_WEAK_OBJECT) == SHVAR_FLAGS_WEAK_OBJECT) {
        shassert(var.objectInfo->weakRelease && "Weak release function is null");
        var.objectInfo->weakRelease(var.payload.objectValue);
      } else if (var.objectInfo->release) {
        // in this case the custom object needs actual destruction
        var.objectInfo->release(var.payload.objectValue);
      }
    }
    break;
  case SHType::Wire:
    SHWire::deleteRef(var.payload.wireValue);
    break;
  default:
    break;
  };
}

NO_INLINE void _cloneVarSlow(SHVar &dst, const SHVar &src) {
  if (&dst == &src)
    return;

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
    destroyVar(dst);
    dst.valueType = SHType::Image;
    dst.payload.imageValue = src.payload.imageValue;
    imageIncRef(dst.payload.imageValue);
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
      // also we assume mutable tables are of our internal type!!
      shassert(dst.payload.tableValue.api == &GetGlobals().TableInterface);

      map = (SHMap *)dst.payload.tableValue.opaque;

      // Attempt to update the existing table to match the source table
      // This is important to keep references to the table stable, even when adding elements
      shassert(dst.payload.tableValue.api == src.payload.tableValue.api);

      auto sMap = (SHMap *)src.payload.tableValue.opaque;

      // Try a fast update first, assuming matching table layouts
      bool fastUpdateSuccessful = sMap->size() == map->size();
      auto dstIt = map->begin();
      if (fastUpdateSuccessful) {
        auto srcIt = sMap->begin();
        // copy values fast, hoping keys are the same
        // we might end up with some extra copies if keys are not the same but
        // given shards nature, it's unlikely it will be the majority of cases
        while (srcIt != sMap->end()) {
          if (srcIt->first != dstIt->first) {
            fastUpdateSuccessful = false;
            break;
          }

          cloneVar(dstIt->second, srcIt->second);

          ++srcIt;
          ++dstIt;
        }
      }

      // Slower stable update
      if (!fastUpdateSuccessful) {
        SHLOG_PERF_WARN("Performing slow table clone on {} => {}", src, dst);

        // Delete/update set
        for (; dstIt != map->end();) {
          auto srcIt = sMap->find(dstIt->first);
          if (srcIt == sMap->end()) {
            dstIt = map->erase(dstIt);
          } else {
            cloneVar(dstIt->second, srcIt->second);
            ++dstIt;
          }
        }

        // If the source table is larger than the destination table, add the missing elements
        if (map->size() != sMap->size()) {
          for (auto srcIt = sMap->begin(); srcIt != sMap->end(); ++srcIt) {
            if (map->find(srcIt->first) == map->end()) {
              (*map)[srcIt->first] = srcIt->second;
            }
          }
        }
      }
    } else {
      destroyVar(dst);
      dst.valueType = SHType::Table;
      dst.payload.tableValue.api = &GetGlobals().TableInterface;
      map = new SHMap();
      dst.payload.tableValue.opaque = map;

      auto &t = src.payload.tableValue;
      SHTableIterator tit;
      t.api->tableGetIterator(t, &tit);
      SHVar k;
      SHVar v;
      while (t.api->tableNext(t, &tit, &k, &v)) {
        (*map)[k] = v;
      }
    }
    dst.version++;
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
    destroyVar(dst);

    dst.valueType = SHType::Object;
    dst.payload.objectValue = src.payload.objectValue;
    dst.payload.objectVendorId = src.payload.objectVendorId;
    dst.payload.objectTypeId = src.payload.objectTypeId;

    if ((src.flags & SHVAR_FLAGS_USES_OBJINFO) == SHVAR_FLAGS_USES_OBJINFO && src.objectInfo) {
      // in this case the custom object needs actual destruction
      dst.flags |= SHVAR_FLAGS_USES_OBJINFO;
      dst.objectInfo = src.objectInfo;
      // remove weak flag if dst had it before
      dst.flags &= ~SHVAR_FLAGS_WEAK_OBJECT;

      // ok if we are a weak object, take a weak ref and upgrade it to a strong ref
      if ((src.flags & SHVAR_FLAGS_WEAK_OBJECT) == SHVAR_FLAGS_WEAK_OBJECT) {
        shassert(src.objectInfo->weakReference && "weak object must have weakReference");
        dst.objectInfo->weakReference(dst.payload.objectValue);
        auto strong = dst.objectInfo->upgradeWeak(dst.payload.objectValue);
        if (!strong) {
          throw std::runtime_error("Failed to upgrade weak object to strong object");
        }
      } else if (src.objectInfo->reference) {
        dst.objectInfo->reference(dst.payload.objectValue);
      }
    }
    break;
  case SHType::Type:
    destroyVar(dst);
    dst.payload.typeValue = new SHTypeInfo(cloneTypeInfo(*src.payload.typeValue));
    dst.valueType = SHType::Type;
    break;
  case SHType::Trait:
    destroyVar(dst);
    dst.payload.traitValue = new SHTrait(cloneTrait(*src.payload.traitValue));
    dst.valueType = SHType::Trait;
    break;

  default:
    SHLOG_FATAL("Unhandled type {}", src.valueType);
    break;
  };
}

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
  static thread_local HashState<XXH128_hash_t> hasher;
  hasher.reset();
  auto digest = hasher.hash(var);
  return Var(int64_t(digest.low64), int64_t(digest.high64));
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

void triggerVarValueChange(SHContext *context, const SHVar *name, const SHVar *key, bool isGlobal, const SHVar *var) {
  if ((var->flags & SHVAR_FLAGS_TRACKED) == 0)
    return;

  auto &w = context->main;
  auto nameStr = SHSTRVIEW((*name));
  OnTrackedVarSet ev{w->id, nameStr, *key, *var, isGlobal, context->currentWire()};
  w->mesh.lock()->dispatcher.trigger(ev);
}

void triggerVarValueChange(SHWire *w, const SHVar *name, const SHVar *key, bool isGlobal, const SHVar *var) {
  if ((var->flags & SHVAR_FLAGS_TRACKED) == 0)
    return;

  auto nameStr = SHSTRVIEW((*name));
  OnTrackedVarSet ev{w->id, nameStr, *key, *var, isGlobal, w};
  w->mesh.lock()->dispatcher.trigger(ev);
}

void stringGrow(SHStringPayload *str, uint32_t newCap) {
  size_t oldLen = str->len;
  if (size_t(newCap) > str->cap) {
    arrayResize(*str, newCap);
    str->len = oldLen;
  }
}
void stringFree(SHStringPayload *str) { arrayFree(*str); }

}; // namespace shards

extern "C" void shards_install_signal_handlers() { installSignalHandlers(); }

// NO NAMESPACE here!

void shInit() {
  static bool globalInitDone = false;
  if (globalInitDone)
    return;
  globalInitDone = true;

  ZoneScopedN("shInit");

  // read env var for log file
  auto logFile = std::getenv("SHARDS_LOG_FILE");
  if (logFile) {
    logging::setupDefaultLoggerConditional(logFile);
  } else {
    logging::setupDefaultLoggerConditional("shards.log");
  }

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
  static_assert(sizeof(OwnedVar) == sizeof(SHVar));
  static_assert(sizeof(TableVar) == sizeof(SHVar));
  static_assert(sizeof(SeqVar) == sizeof(SHVar));

  shards::registerShards();

#if SH_EMSCRIPTEN
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
int64_t shards_find_enum_id(SHStringWithLen name) { return shards::findEnumId(std::string_view{name.string, size_t(name.len)}); }

int64_t shards_find_object_type_id(SHStringWithLen name) {
  return shards::findObjectTypeId(std::string_view{name.string, size_t(name.len)});
}

const SHEnumInfo *shards_get_enum_info(int64_t id) {
  // we need two uint32_t vendor and type from the single int64_t id
  int32_t vendorId = (int32_t)((id & 0xFFFFFFFF00000000) >> 32);
  int32_t enumId = (int32_t)(id & 0x00000000FFFFFFFF);
  return shards::findEnumInfo(vendorId, enumId);
}

const SHObjectInfo *shards_get_object_info(int64_t id) {
  // we need two uint32_t vendor and type from the single int64_t id
  int32_t vendorId = (int32_t)((id & 0xFFFFFFFF00000000) >> 32);
  int32_t typeId = (int32_t)(id & 0x00000000FFFFFFFF);
  return shards::findObjectInfo(vendorId, typeId);
}

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

#ifdef SH_COMPRESSED_STRINGS
const char *shards_get_compressed_string(uint32_t crc_id) {
  auto str = getCompiledCompressedString(crc_id);
  return str.string;
}
#else
const char *shards_get_compressed_string(uint32_t crc_id) { return nullptr; }
#endif

void triggerVarValueChange(SHContext *ctx, const SHVar *name, const SHVar *key, bool isGlobal, const SHVar *var) {
  shards::triggerVarValueChange(ctx, name, key, isGlobal, var);
}

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

  result->stringGrow = &shards::stringGrow;
  result->stringFree = &shards::stringFree;

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

  result->referenceVariable = [](SHContext *context, SHStringWithLen name) noexcept {
    std::string_view nameView{name.string, size_t(name.len)};
    return shards::referenceVariable(context, nameView);
  };

  result->referenceGlobalVariable = [](SHContext *context, SHStringWithLen name) noexcept {
    std::string_view nameView{name.string, size_t(name.len)};
    return shards::referenceGlobalVariable(context, nameView);
  };

  result->referenceWireVariable = [](SHWireRef wire, SHStringWithLen name) noexcept {
    std::string_view nameView{name.string, size_t(name.len)};
    return shards::referenceWireVariable(wire, nameView);
  };

  result->findVariable = [](SHContext *context, SHStringWithLen name) noexcept {
    std::string_view nameView{name.string, size_t(name.len)};
    return shards::findVariable(context, nameView);
  };

  result->releaseVariable = [](SHVar *variable) noexcept { return shards::releaseVariable(variable); };

  result->referenceVariableSlot = [](SHContext *context, SHStringWithLen name) noexcept {
    std::string_view nameView{name.string, size_t(name.len)};
    return shards::referenceVariableSlot(context, nameView);
  };

  result->releaseVariableSlot = [](SHVar **slot) noexcept { return shards::releaseVariableSlot(slot); };  

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

  result->getMeshVariable = [](SHMeshRef mesh, SHStringWithLen name) noexcept {
    auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
    auto vName = shards::OwnedVar::Foreign(name);
    return &(*smesh)->getVariables()[vName];
  };

  result->setMeshVariableType = [](SHMeshRef mesh, SHStringWithLen name, const SHExposedTypeInfo *type) noexcept {
    auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
    auto vName = shards::OwnedVar::Foreign(name);
    (*smesh)->setMetadata(&(*smesh)->getVariables()[vName], *type);
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
  SH_ARRAY_IMPL(SHTraitVariables, SHTraitVariable, traitVariables);

  result->tableNew = []() noexcept {
    SHTable res;
    res.api = &shards::GetGlobals().TableInterface;
    res.opaque = new shards::SHMap();
    return res;
  };

  result->tableInit = [](SHTable *table) noexcept {
    table->api = &shards::GetGlobals().TableInterface;
    table->opaque = new shards::SHMap();
  };

  result->composeWire = [](SHWireRef wire, SHInstanceData data) noexcept {
    auto &sc = SHWire::sharedFromRef(wire);
    try {
      return composeWire(sc.get(), data);
    } catch (const std::exception &e) {
      SHComposeResult res{};
      res.failed = true;
      auto msgTmp = shards::Var(e.what(), 0); // explict strlen call with 0
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

  result->composeShards = [](Shards shards, SHInstanceData data) noexcept {
    try {
      return shards::composeWire(shards, data);
    } catch (const std::exception &e) {
      SHLOG_TRACE("composeShards failed: {}", e.what());
      SHComposeResult res{};
      res.failed = true;
      auto msgTmp = shards::Var(e.what(), 0); // explict strlen call with 0
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

  result->validateSetParam = [](Shard *shard, int index, const SHVar *param) noexcept {
    try {
      return shards::validateSetParam(shard, index, *param);
    } catch (...) {
      // validateSetParam prints logs on failure so we don't need to do anything here
      return false;
    }
  };

  result->runShards = [](Shards shards, SHContext *context, const SHVar *input, SHVar *output) noexcept {
    return shards::activateShards(shards, context, *input, *output);
  };

  result->runShards2 = [](Shards shards, SHContext *context, const SHVar *input, SHVar *output) noexcept {
    return shards::activateShards2(shards, context, *input, *output);
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
                    wire->finishedOutput.has_value() ? &wire->finishedOutput.value() : nullptr};
    return info;
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

  result->setWirePriority = [](SHWireRef wireref, int priority) noexcept {
    auto &sc = SHWire::sharedFromRef(wireref);
    sc->priority = priority;
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

  result->setWireTraits = [](SHWireRef wireref, SHSeq traits) noexcept {
    auto &wire = SHWire::sharedFromRef(wireref);
    for (auto &trait : traits) {
      shassert(trait.valueType == SHType::Trait);
      wire->addTrait(*trait.payload.traitValue);
    }
  };

  result->addShard = [](SHWireRef wireref, ShardPtr blk) noexcept {
    auto &sc = SHWire::sharedFromRef(wireref);
    sc->addShard(blk);
  };

  result->removeShard = [](SHWireRef wireref, ShardPtr blk) noexcept {
    auto &sc = SHWire::sharedFromRef(wireref);
    sc->removeShard(blk);
  };

  result->referenceWire = [](SHWireRef wire) noexcept { return SHWire::addRef(wire); };

  result->destroyWire = [](SHWireRef wire) noexcept { SHWire::deleteRef(wire); };

  result->isWireRunning = [](SHWireRef wire) noexcept {
    auto &sc = SHWire::sharedFromRef(wire);
    return shards::isRunning(sc.get());
  };

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

  result->setMeshLabel = [](SHMeshRef mesh, SHStringWithLen label) noexcept {
    auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
    (*smesh)->setLabel(std::string_view(label.string, label.len));
  };

  result->destroyMesh = [](SHMeshRef mesh) noexcept {
    auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
    SHLOG_TRACE("destroyMesh {}", (void *)(*smesh).get());
    delete smesh;
  };

  result->compose = [](SHMeshRef mesh, SHWireRef wire, struct SHVar *errorCloned) noexcept {
    try {
      auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
      (*smesh)->compose(SHWire::sharedFromRef(wire));
      return true;
    } catch (const std::exception &e) {
      shards::cloneVar(*errorCloned, shards::Var(e.what(), 0)); // 0 to force strlen
      return false;
    } catch (...) {
      shards::cloneVar(*errorCloned, shards::Var("foreign exception failure during compose"));
      return false;
    }
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

  result->sleep = [](double seconds) noexcept { shards::sleep(seconds); };

  result->getStepCount = [](SHContext *context) noexcept { return context->stepCounter; };

  result->getRootPath = []() noexcept { return shards::GetGlobals().RootPath.c_str(); };

  result->setRootPath = [](const char *p) noexcept {
    auto &p1 = shards::GetGlobals().RootPath = p;
#ifdef _WIN32
    if (boost::starts_with(p1, "\\\\?\\")) {
      p1 = p1.substr(4);
    }
#endif
    shards::loadExternalShards(p1);
    fs::current_path(p1);
    SHLOG_DEBUG("Root path set to: {}", p1);
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

  // we need this for rust PartialOrd partial_cmp
  result->compareVar = [](const SHVar *v1, const SHVar *v2) -> int {
    if (*v1 < *v2)
      return -1;
    if (*v1 == *v2)
      return 0;
    return 1;
  };

  result->isEqualType = [](const SHTypeInfo *t1, const SHTypeInfo *t2) -> SHBool { return *t1 == *t2; };

  result->deriveTypeInfo = [](const SHVar *v, const struct SHInstanceData *dat, bool mutable_) -> SHTypeInfo {
    return deriveTypeInfo(*v, *dat, nullptr, true, mutable_);
  };

  result->freeDerivedTypeInfo = [](SHTypeInfo *t) { freeDerivedInfo(*t); };

  result->findEnumInfo = &shards::findEnumInfo;

  result->findObjectInfo = &shards::findObjectInfo;

  result->type2Name = [](SHType type) { return type2Name_raw(type); };

  result->imageIncRef = [](SHImage *img) { return imageIncRef(img); };
  result->imageDecRef = [](SHImage *img) { return imageDecRef(img); };
  result->imageNew = [](uint32_t len) { return imageNew(len); };
  result->imageClone = [](SHImage *img) { return imageClone(img); };
  result->imageDeriveDataLength = [](SHImage *img) { return imageDeriveDataLength(img); };

  setupCoreLang(result);

  result->registerErrorEvent = [](SHMeshRef mesh, void *userData,
                                  void (*callback)(void *userData, SHStringWithLen message, uint32_t line, uint32_t column)) {
    auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
    (*smesh)->registerErrorEvent(userData, callback);
  };

  result->unregisterErrorEvent = [](SHMeshRef mesh, void *userData) {
    auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
    (*smesh)->unregisterErrorEvent(userData);
  };

  setupCoreLogging(result);

  return result;
}

SHVar hash_bytes_xx64(const void *data, size_t len, uint64_t seed) {
  XXH3_state_t state;
  XXH3_64bits_reset_withSeed(&state, seed);
  XXH3_64bits_update(&state, data, len);
  XXH64_hash_t hash = XXH3_64bits_digest(&state);
  SHVar res{};
  res.valueType = SHType::Int;
  res.payload.intValue = hash;
  return res;
}

SHVar hash_bytes_xx128(const void *data, size_t len, uint64_t seed) {
  XXH3_state_t state;
  XXH3_128bits_reset_withSeed(&state, seed);
  XXH3_128bits_update(&state, data, len);
  XXH128_hash_t hash = XXH3_128bits_digest(&state);
  SHVar res{};
  res.valueType = SHType::Int2;
  res.payload.int2Value[0] = hash.low64;
  res.payload.int2Value[1] = hash.high64;
  return res;
}

SHVar hash_bytes_xx64_legacy(const void *data, size_t len, uint64_t seed) {
  XXH64_hash_t hash = XXH64(data, len, seed);
  SHVar res{};
  res.valueType = SHType::Int;
  res.payload.intValue = hash;
  return res;
}

void shards_set_wire_debug_id(SHWireRef wire, uint64_t id) {
  auto &sc = SHWire::sharedFromRef(wire);
  sc->debugId = id;
}

void shards_decompress_strings() {
#ifdef SH_COMPRESSED_STRINGS
  shards::decompressStrings();
#endif
}

void shards_log(int level, SHStringWithLen msg, const char *file, const char *function, int line) {
  std::string_view sv(msg.string, size_t(msg.len));
  spdlog::default_logger_raw()->log(spdlog::source_loc{file, line, function}, (spdlog::level::level_enum)level, sv);
};

SHVar shards_serialize_var(const SHVar *var) {
  static thread_local shards::Serialization serialization;
  static thread_local std::vector<uint8_t> buffer;

  serialization.reset();
  shards::BufferRefWriter writer(buffer); // will clear the buffer

  serialization.serialize(*var, writer);

  SHVar leaking_tmp{}; // to be destroyed by the caller
  shards::Var container(buffer);
  shards::cloneVar(leaking_tmp, container);
  return leaking_tmp;
}

SHVar shards_deserialize_var(const SHVar *bytes_buffer_var) {
  static thread_local shards::Serialization serialization;

  serialization.reset();
  shards::VarReader reader(*bytes_buffer_var);
  SHVar leaking_tmp{}; // to be destroyed by the caller
  serialization.deserialize(reader, leaking_tmp);
  return leaking_tmp;
}

SHBool shards_cancel_abort(SHContext *context) {
  if (context->shouldStop() || context->onLastResume) {
    // ok this flow should stop already... so we can just return false
    return false;
  }
  context->resetErrorStack();
  context->continueFlow();
  return true;
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

#if TRACY_ENABLE
void *operator new(std::size_t count) {
  void *ptr = std::malloc(count);
#ifdef TRACY_ENABLE
  if (GetTracy().isInitialized())
    TracyAlloc(ptr, count);
#endif
  return ptr;
}

void *operator new[](std::size_t count) {
  void *ptr = std::malloc(count);
#ifdef TRACY_ENABLE
  if (GetTracy().isInitialized())
    TracyAlloc(ptr, count);
#endif
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
#ifdef TRACY_ENABLE
  if (GetTracy().isInitialized())
    TracyAlloc(ptr, count);
#endif
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
#ifdef TRACY_ENABLE
  if (GetTracy().isInitialized())
    TracyAlloc(ptr, count);
#endif
  return ptr;
}

void operator delete(void *ptr) noexcept {
#ifdef TRACY_ENABLE
  if (GetTracy().isInitialized())
    TracyFree(ptr);
#endif
  std::free(ptr);
}

void operator delete[](void *ptr) noexcept {
#ifdef TRACY_ENABLE
  if (GetTracy().isInitialized())
    TracyFree(ptr);
#endif
  std::free(ptr);
}

void operator delete(void *ptr, std::align_val_t alignment) noexcept {
#ifdef TRACY_ENABLE
  if (GetTracy().isInitialized())
    TracyFree(ptr);
#endif
#ifdef WIN32
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

void operator delete[](void *ptr, std::align_val_t alignment) noexcept {
#ifdef TRACY_ENABLE
  if (GetTracy().isInitialized())
    TracyFree(ptr);
#endif
#ifdef WIN32
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

void operator delete(void *ptr, std::size_t count) noexcept {
#ifdef TRACY_ENABLE
  if (GetTracy().isInitialized())
    TracyFree(ptr);
#endif
  std::free(ptr);
}

void operator delete[](void *ptr, std::size_t count) noexcept {
#ifdef TRACY_ENABLE
  if (GetTracy().isInitialized())
    TracyFree(ptr);
#endif
  std::free(ptr);
}

#endif
