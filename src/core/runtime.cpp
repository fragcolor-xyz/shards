/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "runtime.hpp"
#include "shards/shared.hpp"
#include "spdlog/sinks/dist_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif
#include "utility.hpp"
#include <boost/asio/thread_pool.hpp>
#include <boost/filesystem.hpp>
#include <boost/stacktrace.hpp>
#include <csignal>
#include <cstdarg>
#include <pdqsort.h>
#include <set>
#include <string.h>
#include <unordered_set>

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
  static std::unordered_map<uint32_t, SHOptionalString> CompiledCompressedStrings;
  if (GetGlobals().CompressedStrings == nullptr)
    GetGlobals().CompressedStrings = &CompiledCompressedStrings;
  auto &val = CompiledCompressedStrings[id];
  val.crc = id; // make sure we return with crc to allow later lookups!
  return val;
}
#else
SHOptionalString setCompiledCompressedString(uint32_t id, const char *str) {
  static std::unordered_map<uint32_t, SHOptionalString> CompiledCompressedStrings;
  if (GetGlobals().CompressedStrings == nullptr)
    GetGlobals().CompressedStrings = &CompiledCompressedStrings;
  SHOptionalString ls{str, id};
  CompiledCompressedStrings[id] = ls;
  return ls;
}
#endif

extern void registerShardsCoreShards();
extern void registerWiresShards();
extern void registerLoggingShards();
extern void registerFlowShards();
extern void registerSeqsShards();
extern void registerCastingShards();
extern void registerSerializationShards();
extern void registerFSShards();
extern void registerJsonShards();
extern void registerNetworkShards();
extern void registerStructShards();
extern void registerTimeShards();
// extern void registerOSShards();
extern void registerProcessShards();

namespace Math {
extern void registerShards();
namespace LinAlg {
extern void registerShards();
}
} // namespace Math

namespace Regex {
extern void registerShards();
}

namespace channels {
extern void registerShards();
}

namespace Assert {
extern void registerShards();
}

namespace Genetic {
extern void registerShards();
}

namespace Random {
extern void registerShards();
}

namespace Imaging {
extern void registerShards();
}

namespace Http {
extern void registerShards();
}

namespace WS {
extern void registerShards();
}

namespace BigInt {
extern void registerShards();
}

namespace Wasm {
extern void registerShards();
}

namespace edn {
extern void registerShards();
}

namespace reflection {
extern void registerShards();
}

#ifdef SHARDS_WITH_EXTRA_SHARDS
extern void shInitExtras();
#endif

static bool globalRegisterDone = false;

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

void setupSpdLog() {
  auto dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();

  auto sink1 = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  dist_sink->add_sink(sink1);

#ifdef SHARDS_DESKTOP
  auto sink2 = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("shards.log", 1048576, 3, false);
  dist_sink->add_sink(sink2);
#endif

  auto logger = std::make_shared<spdlog::logger>("shards_logger", dist_sink);
  logger->flush_on(spdlog::level::err);
  spdlog::set_default_logger(logger);

#ifdef __ANDROID__
  auto android_sink = std::make_shared<spdlog::sinks::android_sink_mt>("android");
  logger->sinks().push_back(android_sink);
#endif

#ifdef __ANDROID
  // Logcat already countains timestamps & log level
  spdlog::set_pattern("[T-%t] [%s::%#] %v");
#else
  spdlog::set_pattern("%^[%l]%$ [%Y-%m-%d %T.%e] [T-%t] [%s::%#] %v");
#endif

#ifdef NDEBUG
#if (SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG)
  spdlog::set_level(spdlog::level::debug);
#else
  spdlog::set_level(spdlog::level::info);
#endif
#else
  spdlog::set_level(spdlog::level::trace);
#endif
}

void registerCoreShards() {
  if (globalRegisterDone)
    return;

  globalRegisterDone = true;

  setupSpdLog();

  if (GetGlobals().RootPath.size() > 0) {
    // set root path as current directory
    fs::current_path(GetGlobals().RootPath);
  } else {
    // set current path as root path
    auto cp = fs::current_path();
    GetGlobals().RootPath = cp.string();
  }

#ifdef SH_USE_UBSAN
  auto absPath = fs::absolute(GetGlobals().RootPath);
  auto absPathStr = absPath.string();
  SHLOG_TRACE("Setting ASAN report path to: {}", absPathStr);
  __sanitizer_set_report_path(absPathStr.c_str());
#endif

// UTF8 on windows
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  namespace fs = boost::filesystem;
  if (GetGlobals().ExePath.size() > 0) {
    auto pluginPath = fs::absolute(GetGlobals().ExePath) / "shards";
    auto pluginPathStr = pluginPath.wstring();
    SHLOG_DEBUG("Adding dll path: {}", pluginPath.string());
    AddDllDirectory(pluginPathStr.c_str());
  }
  if (GetGlobals().RootPath.size() > 0) {
    auto pluginPath = fs::absolute(GetGlobals().RootPath) / "shards";
    auto pluginPathStr = pluginPath.wstring();
    SHLOG_DEBUG("Adding dll path: {}", pluginPath.string());
    AddDllDirectory(pluginPathStr.c_str());
  }
#endif

  SHLOG_DEBUG("Registering shards");

  // precap some exceptions to avoid allocations
  try {
    throw StopWireException();
  } catch (...) {
    GetGlobals().StopWireEx = std::current_exception();
  }

  try {
    throw RestartWireException();
  } catch (...) {
    GetGlobals().RestartWireEx = std::current_exception();
  }

  // at this point we might have some auto magical static linked shard already
  // keep them stored here and re-register them
  // as we assume the observers were setup in this call caller so too late for
  // them
  std::vector<std::pair<std::string_view, SHShardConstructor>> earlyshards;
  for (auto &pair : GetGlobals().ShardsRegister) {
    earlyshards.push_back(pair);
  }
  GetGlobals().ShardsRegister.clear();

  static_assert(sizeof(SHVarPayload) == 16);
  static_assert(sizeof(SHVar) == 32);
  static_assert(sizeof(SHMapIt) <= sizeof(SHTableIterator));

  Assert::registerShards();
  registerWiresShards();
  registerLoggingShards();
  registerFlowShards();
  registerSeqsShards();
  registerCastingShards();
  registerShardsCoreShards();
  registerSerializationShards();
  Math::registerShards();
  Math::LinAlg::registerShards();
  registerJsonShards();
  registerStructShards();
  registerTimeShards();
  Regex::registerShards();
  channels::registerShards();
  Random::registerShards();
  Imaging::registerShards();

#ifndef SHARDS_NO_BIGINT_SHARDS
  BigInt::registerShards();
#endif

  registerFSShards();
  Wasm::registerShards();
  Http::registerShards();
  edn::registerShards();
  reflection::registerShards();

#ifdef SHARDS_DESKTOP
  // registerOSShards();
  registerProcessShards();
  Genetic::registerShards();
  registerNetworkShards();
  WS::registerShards();
#endif

#ifdef SHARDS_WITH_EXTRA_SHARDS
  shInitExtras();
#endif

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
  loadExternalShards(GetGlobals().ExePath);
  if (GetGlobals().RootPath != GetGlobals().ExePath) {
    loadExternalShards(GetGlobals().RootPath);
  }

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

Shard *createShard(std::string_view name) {
  auto it = GetGlobals().ShardsRegister.find(name);
  if (it == GetGlobals().ShardsRegister.end()) {
    return nullptr;
  }

  auto blkp = it->second();

  // Hook inline shards to override activation in runWire
  if (name == "Const") {
    blkp->inlineShardId = SHInlineShards::CoreConst;
  } else if (name == "Pass") {
    blkp->inlineShardId = SHInlineShards::NoopShard;
  } else if (name == "OnCleanup") {
    blkp->inlineShardId = SHInlineShards::NoopShard;
  } else if (name == "Comment") {
    blkp->inlineShardId = SHInlineShards::NoopShard;
  } else if (name == "Input") {
    blkp->inlineShardId = SHInlineShards::CoreInput;
  } else if (name == "Pause") {
    blkp->inlineShardId = SHInlineShards::CoreSleep;
  } else if (name == "ForRange") {
    blkp->inlineShardId = SHInlineShards::CoreForRange;
  } else if (name == "Repeat") {
    blkp->inlineShardId = SHInlineShards::CoreRepeat;
  } else if (name == "Once") {
    blkp->inlineShardId = SHInlineShards::CoreOnce;
  } else if (name == "Set") {
    blkp->inlineShardId = SHInlineShards::CoreSet;
  } else if (name == "Update") {
    blkp->inlineShardId = SHInlineShards::CoreUpdate;
  } else if (name == "Swap") {
    blkp->inlineShardId = SHInlineShards::CoreSwap;
  } else if (name == "Push") {
    blkp->inlineShardId = SHInlineShards::CorePush;
  } else if (name == "Is") {
    blkp->inlineShardId = SHInlineShards::CoreIs;
  } else if (name == "IsNot") {
    blkp->inlineShardId = SHInlineShards::CoreIsNot;
  } else if (name == "IsMore") {
    blkp->inlineShardId = SHInlineShards::CoreIsMore;
  } else if (name == "IsLess") {
    blkp->inlineShardId = SHInlineShards::CoreIsLess;
  } else if (name == "IsMoreEqual") {
    blkp->inlineShardId = SHInlineShards::CoreIsMoreEqual;
  } else if (name == "IsLessEqual") {
    blkp->inlineShardId = SHInlineShards::CoreIsLessEqual;
  } else if (name == "And") {
    blkp->inlineShardId = SHInlineShards::CoreAnd;
  } else if (name == "Or") {
    blkp->inlineShardId = SHInlineShards::CoreOr;
  } else if (name == "Not") {
    blkp->inlineShardId = SHInlineShards::CoreNot;
  } else if (name == "Math.Add") {
    blkp->inlineShardId = SHInlineShards::MathAdd;
  } else if (name == "Math.Subtract") {
    blkp->inlineShardId = SHInlineShards::MathSubtract;
  } else if (name == "Math.Multiply") {
    blkp->inlineShardId = SHInlineShards::MathMultiply;
  } else if (name == "Math.Divide") {
    blkp->inlineShardId = SHInlineShards::MathDivide;
  } else if (name == "Math.Xor") {
    blkp->inlineShardId = SHInlineShards::MathXor;
  } else if (name == "Math.And") {
    blkp->inlineShardId = SHInlineShards::MathAnd;
  } else if (name == "Math.Or") {
    blkp->inlineShardId = SHInlineShards::MathOr;
  } else if (name == "Math.Mod") {
    blkp->inlineShardId = SHInlineShards::MathMod;
  } else if (name == "Math.LShift") {
    blkp->inlineShardId = SHInlineShards::MathLShift;
  } else if (name == "Math.RShift") {
    blkp->inlineShardId = SHInlineShards::MathRShift;
  }

  // Unary math is dealt inside math.hpp compose

  return blkp;
}

void registerShard(std::string_view name, SHShardConstructor constructor, std::string_view fullTypeName) {
  auto findIt = GetGlobals().ShardsRegister.find(name);
  if (findIt == GetGlobals().ShardsRegister.end()) {
    GetGlobals().ShardsRegister.insert(std::make_pair(name, constructor));
  } else {
    GetGlobals().ShardsRegister[name] = constructor;
    SHLOG_INFO("Overriding shard: {}", name);
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
  int64_t id = (int64_t)vendorId << 32 | typeId;
  auto typeName = std::string(info.name);
  auto findIt = GetGlobals().ObjectTypesRegister.find(id);
  if (findIt == GetGlobals().ObjectTypesRegister.end()) {
    GetGlobals().ObjectTypesRegister.insert(std::make_pair(id, info));
  } else {
    GetGlobals().ObjectTypesRegister[id] = info;
    SHLOG_INFO("Overriding object type: {}", typeName);
  }

  for (auto &pobs : GetGlobals().Observers) {
    if (pobs.expired())
      continue;
    auto obs = pobs.lock();
    obs->registerObjectType(vendorId, typeId, info);
  }
}

void registerEnumType(int32_t vendorId, int32_t typeId, SHEnumInfo info) {
  int64_t id = (int64_t)vendorId << 32 | typeId;
  auto typeName = std::string(info.name);
  auto findIt = GetGlobals().EnumTypesRegister.find(id);
  if (findIt == GetGlobals().EnumTypesRegister.end()) {
    GetGlobals().EnumTypesRegister.insert(std::make_pair(id, info));
  } else {
    GetGlobals().EnumTypesRegister[id] = info;
    SHLOG_DEBUG("Overriding enum type: {}", typeName);
  }

  for (auto &pobs : GetGlobals().Observers) {
    if (pobs.expired())
      continue;
    auto obs = pobs.lock();
    obs->registerEnumType(vendorId, typeId, info);
  }
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

SHVar *referenceWireVariable(SHWireRef wire, const char *name) {
  auto swire = SHWire::sharedFromRef(wire);
  SHVar &v = swire->variables[name];
  v.refcount++;
  v.flags |= SHVAR_FLAGS_REF_COUNTED;
  return &v;
}

SHVar *referenceGlobalVariable(SHContext *ctx, const char *name) {
  auto mesh = ctx->main->mesh.lock();
  assert(mesh);

  SHVar &v = mesh->variables[name];
  v.refcount++;
  if (v.refcount == 1) {
    SHLOG_TRACE("Creating a global variable, wire: {} name: {}", ctx->wireStack.back()->name, name);
  }
  v.flags |= SHVAR_FLAGS_REF_COUNTED;
  return &v;
}

SHVar *referenceVariable(SHContext *ctx, const char *name) {
  // try find a wire variable
  // from top to bottom of wire stack
  {
    auto rit = ctx->wireStack.rbegin();
    for (; rit != ctx->wireStack.rend(); ++rit) {
      // prioritize local variables
      auto it = (*rit)->variables.find(name);
      if (it != (*rit)->variables.end()) {
        // found, lets get out here
        SHVar &cv = it->second;
        cv.refcount++;
        cv.flags |= SHVAR_FLAGS_REF_COUNTED;
        return &cv;
      }
      // try external variables
      auto pit = (*rit)->externalVariables.find(name);
      if (pit != (*rit)->externalVariables.end()) {
        // found, lets get out here
        SHVar &cv = *pit->second;
        assert((cv.flags & SHVAR_FLAGS_EXTERNAL) != 0);
        return &cv;
      }
    }
  }

  // try using mesh
  auto mesh = ctx->main->mesh.lock();
  assert(mesh);

  // Was not in wires.. find in meshs
  {
    auto it = mesh->variables.find(name);
    if (it != mesh->variables.end()) {
      // found, lets get out here
      SHVar &cv = it->second;
      cv.refcount++;
      cv.flags |= SHVAR_FLAGS_REF_COUNTED;
      return &cv;
    }
  }

  // Was not in mesh directly.. try find in meshs refs
  {
    auto it = mesh->refs.find(name);
    if (it != mesh->refs.end()) {
      // found, lets get out here
      SHVar *cv = it->second;
      cv->refcount++;
      cv->flags |= SHVAR_FLAGS_REF_COUNTED;
      return cv;
    }
  }

  // worst case create in current top wire!
  SHLOG_TRACE("Creating a variable, wire: {} name: {}", ctx->wireStack.back()->name, name);
  SHVar &cv = ctx->wireStack.back()->variables[name];
  cv.refcount++;
  cv.flags |= SHVAR_FLAGS_REF_COUNTED;
  return &cv;
}

void releaseVariable(SHVar *variable) {
  if (!variable)
    return;

  if ((variable->flags & SHVAR_FLAGS_EXTERNAL) != 0) {
    return;
  }

  assert((variable->flags & SHVAR_FLAGS_REF_COUNTED) == SHVAR_FLAGS_REF_COUNTED);
  assert(variable->refcount > 0);

  variable->refcount--;
  if (variable->refcount == 0) {
    SHLOG_TRACE("Destroying a variable (0 ref count), type: {}", type2Name(variable->valueType));
    destroyVar(*variable);
  }
}

SHWireState suspend(SHContext *context, double seconds) {
  if (unlikely(!context->shouldContinue() || context->onCleanup)) {
    throw ActivationError("Trying to suspend a terminated context!");
  } else if (unlikely(!context->continuation)) {
    throw ActivationError("Trying to suspend a context without coroutine!");
  }

  if (seconds <= 0) {
    context->next = SHDuration(0);
  } else {
    context->next = SHClock::now().time_since_epoch() + SHDuration(seconds);
  }

#ifdef SH_USE_TSAN
  auto curr = __tsan_get_current_fiber();
  __tsan_switch_to_fiber(context->tsan_handle, 0);
#endif

#ifndef __EMSCRIPTEN__
  context->continuation = context->continuation.resume();
#else
  context->continuation->yield();
#endif

#ifdef SH_USE_TSAN
  __tsan_switch_to_fiber(curr, 0);
#endif

  return context->getState();
}

void hash_update(const SHVar &var, void *state);

std::unordered_set<const SHWire *> &gatheringWires() {
#ifdef WIN32
  // we have to leak.. or windows tls emulation will crash at process end
  thread_local std::unordered_set<const SHWire *> *wires = new std::unordered_set<const SHWire *>();
  return *wires;
#else
  thread_local std::unordered_set<const SHWire *> wires;
  return wires;
#endif
}

template <typename T, bool HANDLES_RETURN, bool HASHED>
ALWAYS_INLINE SHWireState shardsActivation(T &shards, SHContext *context, const SHVar &wireInput, SHVar &output,
                                            SHVar *outHash = nullptr) {
  XXH3_state_s hashState; // optimized out in release if not HASHED
  if constexpr (HASHED) {
    assert(outHash);
    XXH3_INITSTATE(&hashState);
    XXH3_128bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);
    gatheringWires().clear();
  }
  DEFER(if constexpr (HASHED) {
    auto digest = XXH3_128bits_digest(&hashState);
    outHash->valueType = SHType::Int2;
    outHash->payload.int2Value[0] = int64_t(digest.low64);
    outHash->payload.int2Value[1] = int64_t(digest.high64);
    SHLOG_TRACE("Hashing digested {}", *outHash);
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
    try {
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
      if (unlikely(!context->shouldContinue())) {
        // TODO #64 try and benchmark: remove this switch and state
        // Replace with preallocated exceptions like StopWireException
        // removing this should make every shard activate faster, 1 check less
        switch (context->getState()) {
        case SHWireState::Return:
          if constexpr (HANDLES_RETURN)
            context->continueFlow();
          return SHWireState::Return;
        case SHWireState::Stop:
          if (context->failed()) {
            throw ActivationError(context->getErrorMessage());
          }
        case SHWireState::Restart:
          return context->getState();
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
    } catch (const StopWireException &ex) {
      return SHWireState::Stop;
    } catch (const RestartWireException &ex) {
      return SHWireState::Restart;
    } catch (const std::exception &e) {
      SHLOG_ERROR("Shard activation error, failed shard: {}, error: {}", blk->name(blk), e.what());
      // failure from exceptions need update on context
      if (!context->failed()) {
        context->cancelFlow(e.what());
      }
      throw; // bubble up
    } catch (...) {
      SHLOG_ERROR("Shard activation error, failed shard: {}, error: generic", blk->name(blk));
      if (!context->failed()) {
        context->cancelFlow("foreign exception failure, check logs");
      }
      throw; // bubble up
    }
    input = output;
  }
  return SHWireState::Continue;
}

SHWireState activateShards(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output) {
  return shardsActivation<Shards, false, false>(shards, context, wireInput, output);
}

SHWireState activateShards2(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output) {
  return shardsActivation<Shards, true, false>(shards, context, wireInput, output);
}

SHWireState activateShards(SHSeq shards, SHContext *context, const SHVar &wireInput, SHVar &output) {
  return shardsActivation<SHSeq, false, false>(shards, context, wireInput, output);
}

SHWireState activateShards2(SHSeq shards, SHContext *context, const SHVar &wireInput, SHVar &output) {
  return shardsActivation<SHSeq, true, false>(shards, context, wireInput, output);
}

SHWireState activateShards(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output, SHVar &outHash) {
  return shardsActivation<Shards, false, true>(shards, context, wireInput, output, &outHash);
}

SHWireState activateShards2(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output, SHVar &outHash) {
  return shardsActivation<Shards, true, true>(shards, context, wireInput, output, &outHash);
}

// Lazy and also avoid windows Loader (Dead)Lock
// https://docs.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices?redirectedfrom=MSDN
#ifdef __EMSCRIPTEN__
// limit to 4 under emscripten
Shared<boost::asio::thread_pool, int, 4> SharedThreadPool{};
#else
Shared<boost::asio::thread_pool> SharedThreadPool{};
#endif

bool matchTypes(const SHTypeInfo &inputType, const SHTypeInfo &receiverType, bool isParameter, bool strict) {
  if (receiverType.basicType == SHType::Any)
    return true;

  if (inputType.basicType != receiverType.basicType) {
    // Fail if basic type differs
    return false;
  }

  switch (inputType.basicType) {
  case Object: {
    if (inputType.object.vendorId != receiverType.object.vendorId || inputType.object.typeId != receiverType.object.typeId) {
      return false;
    }
    break;
  }
  case Enum: {
    // special case: any enum
    if (receiverType.enumeration.vendorId == 0 && receiverType.enumeration.typeId == 0)
      return true;
    // otherwise, exact match
    if (inputType.enumeration.vendorId != receiverType.enumeration.vendorId ||
        inputType.enumeration.typeId != receiverType.enumeration.typeId) {
      return false;
    }
    break;
  }
  case Seq: {
    if (strict) {
      if (inputType.seqTypes.len > 0 && receiverType.seqTypes.len > 0) {
        // all input types must be in receiver, receiver can have more ofc
        for (uint32_t i = 0; i < inputType.seqTypes.len; i++) {
          for (uint32_t j = 0; j < receiverType.seqTypes.len; j++) {
            if (receiverType.seqTypes.elements[j].basicType == SHType::Any ||
                inputType.seqTypes.elements[i] == receiverType.seqTypes.elements[j])
              goto matched;
          }
          return false;
        matched:
          continue;
        }
      } else if (inputType.seqTypes.len == 0 && receiverType.seqTypes.len > 0) {
        // find Any
        for (uint32_t j = 0; j < receiverType.seqTypes.len; j++) {
          if (receiverType.seqTypes.elements[j].basicType == SHType::Any)
            return true;
        }
        return false;
      } else if (inputType.seqTypes.len == 0 || receiverType.seqTypes.len == 0) {
        return false;
      }
      // if a fixed size is requested make sure it fits at least enough elements
      if (receiverType.fixedSize != 0 && inputType.fixedSize < receiverType.fixedSize) {
        return false;
      }
    }
    break;
  }
  case Table: {
    if (strict) {
      // Table is a complicated one
      // We use it as many things.. one of it as structured data
      // So we have many possible cases:
      // 1. a receiver table with just type info is flexible, accepts only those
      // types but the keys are open to anything, if no types are available, it
      // accepts any type
      // 2. a receiver table with type info and key info is strict, means that
      // input has to match 1:1, an exception is done if the last key is empty
      // as in
      // "" on the receiver side, in such case any input is allowed (types are
      // still checked)
      const auto atypes = inputType.table.types.len;
      const auto btypes = receiverType.table.types.len;
      const auto akeys = inputType.table.keys.len;
      const auto bkeys = receiverType.table.keys.len;
      if (bkeys == 0) {
        // case 1, consumer is not strict, match types if avail
        // ignore input keys information
        if (atypes == 0) {
          // assume this as an Any
          if (btypes == 0)
            return true; // both Any
          auto matched = false;
          SHTypeInfo anyType{SHType::Any};
          for (uint32_t y = 0; y < btypes; y++) {
            auto btype = receiverType.table.types.elements[y];
            if (matchTypes(anyType, btype, isParameter, strict)) {
              matched = true;
              break;
            }
          }
          if (!matched) {
            return false;
          }
        } else {
          if (isParameter || btypes != 0) {
            // receiver doesn't accept anything, match further
            for (uint32_t i = 0; i < atypes; i++) {
              // Go thru all exposed types and make sure we get a positive match
              // with the consumer
              auto atype = inputType.table.types.elements[i];
              auto matched = false;
              for (uint32_t y = 0; y < btypes; y++) {
                auto btype = receiverType.table.types.elements[y];
                if (matchTypes(atype, btype, isParameter, strict)) {
                  matched = true;
                  break;
                }
              }
              if (!matched) {
                return false;
              }
            }
          }
        }
      } else {
        if (!isParameter && akeys == 0 && atypes == 0)
          return true; // update case {} >= .edit-me {"x" 10} > .edit-me

        const auto anyLast = // last element can be a jolly
            strlen(receiverType.table.keys.elements[bkeys - 1]) == 0;
        if (!anyLast && (akeys != bkeys || akeys != atypes)) {
          // we need a 1:1 match in this case, fail early
          return false;
        }
        auto missingMatches = akeys;
        for (uint32_t i = 0; i < akeys; i++) {
          auto atype = inputType.table.types.elements[i];
          auto akey = inputType.table.keys.elements[i];
          for (uint32_t y = 0; y < bkeys; y++) {
            auto btype = receiverType.table.types.elements[y];
            auto bkey = receiverType.table.keys.elements[y];
            // if anyLast and the last perform a match anyway
            if (strcmp(akey, bkey) == 0 || (anyLast && y == (bkeys - 1))) {
              if (matchTypes(atype, btype, isParameter, strict))
                missingMatches--;
              else
                return false; // fail quick in this case
            }
          }
        }

        if (missingMatches)
          return false;
      }
    }
    break;
  }
  default:
    return true;
  }
  return true;
}

struct ValidationContext {
  std::unordered_map<std::string, std::unordered_set<SHExposedTypeInfo>> inherited;
  std::unordered_map<std::string, std::unordered_set<SHExposedTypeInfo>> exposed;
  std::unordered_set<std::string> variables;
  std::unordered_set<std::string> references;
  std::unordered_set<SHExposedTypeInfo> required;

  SHTypeInfo previousOutputType{};
  SHTypeInfo originalInputType{};

  Shard *bottom{};
  Shard *next{};
  SHWire *wire{};

  SHValidationCallback cb{};
  void *userData{};

  bool onWorkerThread{false};
};

void validateConnection(ValidationContext &ctx) {
  auto previousOutput = ctx.previousOutputType;

  auto inputInfos = ctx.bottom->inputTypes(ctx.bottom);
  auto inputMatches = false;
  // validate our generic input
  if (inputInfos.len == 1 && inputInfos.elements[0].basicType == None) {
    // in this case a None always matches
    inputMatches = true;
  } else {
    for (uint32_t i = 0; inputInfos.len > i; i++) {
      auto &inputInfo = inputInfos.elements[i];
      if (matchTypes(previousOutput, inputInfo, false, true)) {
        inputMatches = true;
        break;
      }
    }
  }

  if (!inputMatches) {
    std::stringstream ss;
    ss << "Could not find a matching input type, shard: " << ctx.bottom->name(ctx.bottom) << " expected: " << inputInfos
       << " found instead: " << ctx.previousOutputType;
    const auto sss = ss.str();
    ctx.cb(ctx.bottom, sss.c_str(), false, ctx.userData);
  }

  // infer and specialize types if we need to
  // If we don't we assume our output will be of the same type of the previous!
  if (ctx.bottom->compose) {
    SHInstanceData data{};
    data.shard = ctx.bottom;
    data.wire = ctx.wire;
    data.inputType = previousOutput;
    if (ctx.next) {
      data.outputTypes = ctx.next->inputTypes(ctx.next);
    }
    data.onWorkerThread = ctx.onWorkerThread;

    struct ComposeContext {
      std::string externalError;
      bool externalFailure = false;
      bool warning = false;
    } externalCtx;
    data.privateContext = &externalCtx;
    data.reportError = [](auto ctx, auto message, auto warningOnly) {
      auto cctx = reinterpret_cast<ComposeContext *>(ctx);
      cctx->externalError = message;
      cctx->externalFailure = true;
      cctx->warning = warningOnly;
    };

    // Pass all we got in the context!
    // notice that shards might add new records to this array
    for (auto &info : ctx.exposed) {
      for (auto &type : info.second) {
        shards::arrayPush(data.shared, type);
      }
    }
    // and inherited
    for (auto &info : ctx.inherited) {
      for (auto &type : info.second) {
        shards::arrayPush(data.shared, type);
      }
    }
    DEFER(shards::arrayFree(data.shared));

    // this ensures e.g. SetVariable exposedVars have right type from the actual
    // input type (previousOutput)!
    ctx.previousOutputType = ctx.bottom->compose(ctx.bottom, data);

    if (externalCtx.externalFailure) {
      if (externalCtx.warning) {
        externalCtx.externalError.insert(0, "Wire compose warning: ");
        ctx.cb(ctx.bottom, externalCtx.externalError.c_str(), true, ctx.userData);
      } else {
        externalCtx.externalError.insert(0, "Wire compose failed with error: ");
        ctx.cb(ctx.bottom, externalCtx.externalError.c_str(), false, ctx.userData);
      }
    }
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
      throw ComposeError("Shard has multiple possible output types and is "
                         "missing the compose method");
    }
  }

#ifndef NDEBUG
  // do some sanity checks that also provide coverage on outputTypes
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
  auto valid_shard_outputTypes =
      flowStopper || std::any_of(otypes.begin(), otypes.end(), [&](const auto &t) {
        return t.basicType == SHType::Any ||
               (t.basicType == Seq && t.seqTypes.len == 1 && t.seqTypes.elements[0].basicType == SHType::Any &&
                ctx.previousOutputType.basicType == Seq) || // any seq
               (t.basicType == Table &&
                // TODO find Any in table types
                ctx.previousOutputType.basicType == Table) || // any table
               t == ctx.previousOutputType;
      });
  assert(ctx.bottom->compose || valid_shard_outputTypes);
#endif

  // Grab those after type inference!
  auto exposedVars = ctx.bottom->exposedVariables(ctx.bottom);
  // Add the vars we expose
  for (uint32_t i = 0; exposedVars.len > i; i++) {
    auto exposed_param = exposedVars.elements[i];
    std::string name(exposed_param.name);
    exposed_param.scope = exposed_param.global ? nullptr : ctx.wire;
    ctx.exposed[name].emplace(exposed_param);

    // Reference mutability checks
    if (strcmp(ctx.bottom->name(ctx.bottom), "Ref") == 0) {
      // make sure we are not Ref-ing a Set
      // meaning target would be overwritten, yet Set will try to deallocate
      // it/manage it
      if (ctx.variables.count(name)) {
        // Error
        std::string err("Ref variable name already used as Set. Overwriting a previously "
                        "Set variable with Ref is not allowed, name: " +
                        name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
      ctx.references.insert(name);
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Set") == 0) {
      // make sure we are not Set-ing a Ref
      // meaning target memory could be any shard temporary buffer, yet Set will
      // try to deallocate it/manage it
      if (ctx.references.count(name)) {
        // Error
        std::string err("Set variable name already used as Ref. Overwriting a previously "
                        "Ref variable with Set is not allowed, name: " +
                        name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
      ctx.variables.insert(name);
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Update") == 0) {
      // make sure we are not Set-ing a Ref
      // meaning target memory could be any shard temporary buffer, yet Set will
      // try to deallocate it/manage it
      if (ctx.references.count(name)) {
        // Error
        std::string err("Update variable name already used as Ref. Overwriting "
                        "a previously "
                        "Ref variable with Update is not allowed, name: " +
                        name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Push") == 0) {
      // make sure we are not Push-ing a Ref
      // meaning target memory could be any shard temporary buffer, yet Push
      // will try to deallocate it/manage it
      if (ctx.references.count(name)) {
        // Error
        std::string err("Push variable name already used as Ref. Overwriting a previously "
                        "Ref variable with Push is not allowed, name: " +
                        name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
      ctx.variables.insert(name);
    }
  }

  // Finally do checks on what we consume
  auto requiredVar = ctx.bottom->requiredVariables(ctx.bottom);

  std::unordered_map<std::string, std::vector<SHExposedTypeInfo>> requiredVars;
  for (uint32_t i = 0; requiredVar.len > i; i++) {
    auto &required_param = requiredVar.elements[i];
    std::string name(required_param.name);
    requiredVars[name].push_back(required_param);
  }

  // make sure we have the vars we need, collect first
  for (const auto &required : requiredVars) {
    auto matching = false;
    SHExposedTypeInfo match{};

    for (const auto &required_param : required.second) {
      std::string name(required_param.name);
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
        std::string err("Required variable not found: " + name);
        // Warning only, delegate compose to decide
        ctx.cb(ctx.bottom, err.c_str(), true, ctx.userData);
      } else {
        for (auto type : findIt->second) {
          auto exposedType = type.exposedType;
          auto requiredType = required_param.exposedType;
          // Finally deep compare types
          if (matchTypes(exposedType, requiredType, false, true)) {
            matching = true;
            break;
          }
        }
      }
      if (matching) {
        match = required_param;
        break;
      }
    }

    if (!matching) {
      std::stringstream ss;
      ss << "Required types do not match currently exposed ones for variable '" << required.first
         << "' required possible types: ";
      for (auto type : required.second) {
        ss << "{\"" << type.name << "\" (" << type.exposedType << ")} ";
      }
      ss << "exposed types: ";
      for (const auto &info : ctx.exposed) {
        for (auto type : info.second) {
          ss << "{\"" << type.name << "\" (" << type.exposedType << ")} ";
        }
      }
      for (const auto &info : ctx.inherited) {
        for (auto type : info.second) {
          ss << "{\"" << type.name << "\" (" << type.exposedType << ")} ";
        }
      }
      auto sss = ss.str();
      ctx.cb(ctx.bottom, sss.c_str(), false, ctx.userData);
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

  // add externally added variables
  if (ctx.wire) {
    for (const auto &[key, pVar] : ctx.wire->externalVariables) {
      SHVar &var = *pVar;
      assert((var.flags & SHVAR_FLAGS_EXTERNAL) != 0);

      auto hash = deriveTypeHash(var);
      TypeInfo *info = nullptr;
      if (ctx.wire->typesCache.find(hash) == ctx.wire->typesCache.end()) {
        info = &ctx.wire->typesCache.emplace(hash, TypeInfo(var, data)).first->second;
      } else {
        info = &ctx.wire->typesCache.at(hash);
      }

      ctx.inherited[key].insert(SHExposedTypeInfo{key.c_str(), {}, *info, true /* mutable */});
    }
  }

  if (data.shared.elements) {
    for (uint32_t i = 0; i < data.shared.len; i++) {
      auto &info = data.shared.elements[i];
      ctx.inherited[info.name].insert(info);
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
      validateConnection(ctx);
    }
  }

  SHComposeResult result = {ctx.previousOutputType};

  for (auto &exposed : ctx.exposed) {
    for (auto &type : exposed.second) {
      shards::arrayPush(result.exposedInfo, type);
    }
  }

  for (auto &req : ctx.required) {
    shards::arrayPush(result.requiredInfo, req);
  }

  if (wire.size() > 0) {
    auto &last = wire.back();
    if (strcmp(last->name(last), "Restart") == 0 || strcmp(last->name(last), "Stop") == 0 ||
        strcmp(last->name(last), "Return") == 0 || strcmp(last->name(last), "Fail") == 0) {
      result.flowStopper = true;
    }
  }

  return result;
}

SHComposeResult composeWire(const SHWire *wire, SHValidationCallback callback, void *userData, SHInstanceData data) {
  // settle input type of wire before compose
  if (wire->shards.size() > 0 && !std::any_of(wire->shards.begin(), wire->shards.end(),
                                               [&](const auto &shard) { return strcmp(shard->name(shard), "Input") == 0; })) {
    // If first shard is a plain None, mark this wire has None input
    // But make sure we have no (Input) shards
    auto inTypes = wire->shards[0]->inputTypes(wire->shards[0]);
    if (inTypes.len == 1 && inTypes.elements[0].basicType == None)
      wire->inputType = SHTypeInfo{};
    else
      wire->inputType = data.inputType;
  } else {
    wire->inputType = data.inputType;
  }

  auto res = composeWire(wire->shards, callback, userData, data);

  // set outputtype
  wire->outputType = res.outputType;

  std::vector<shards::ShardInfo> allShards;
  shards::gatherShards(wire, allShards);
  // call composed on all shards if they have it
  for (auto &blk : allShards) {
    if (blk.shard->composed)
      blk.shard->composed(const_cast<Shard *>(blk.shard), wire, &res);
  }

  // add variables
  for (auto req : res.requiredInfo) {
    wire->requiredVariables.emplace_back(req.name);
  }

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
  case Seq: {
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
  case Table: {
    for (uint32_t i = 0; info.table.types.len > i; i++) {
      freeDerivedInfo(info.table.types.elements[i]);
    }
    for (uint32_t i = 0; info.table.keys.len > i; i++) {
      free((void *)info.table.keys.elements[i]);
    }
    shards::arrayFree(info.table.types);
    shards::arrayFree(info.table.keys);
  } break;
  default:
    break;
  };
}

SHTypeInfo deriveTypeInfo(const SHVar &value, const SHInstanceData &data, bool *containsVariables) {
  // We need to guess a valid SHTypeInfo for this var in order to validate
  // Build a SHTypeInfo for the var
  // this is not complete at all, missing Array and ContextVar for example
  SHTypeInfo varType{};
  varType.basicType = value.valueType;
  varType.innerType = value.innerType;
  switch (value.valueType) {
  case Object: {
    varType.object.vendorId = value.payload.objectVendorId;
    varType.object.typeId = value.payload.objectTypeId;
    break;
  }
  case Enum: {
    varType.enumeration.vendorId = value.payload.enumVendorId;
    varType.enumeration.typeId = value.payload.enumTypeId;
    break;
  }
  case Seq: {
    std::unordered_set<SHTypeInfo> types;
    for (uint32_t i = 0; i < value.payload.seqValue.len; i++) {
      auto derived = deriveTypeInfo(value.payload.seqValue.elements[i], data, containsVariables);
      if (!types.count(derived)) {
        shards::arrayPush(varType.seqTypes, derived);
        types.insert(derived);
      }
    }
    varType.fixedSize = value.payload.seqValue.len;
    break;
  }
  case Table: {
    auto &t = value.payload.tableValue;
    SHTableIterator tit;
    t.api->tableGetIterator(t, &tit);
    SHString k;
    SHVar v;
    while (t.api->tableNext(t, &tit, &k, &v)) {
      auto derived = deriveTypeInfo(v, data, containsVariables);
      shards::arrayPush(varType.table.types, derived);
      shards::arrayPush(varType.table.keys, strdup(k));
    }
  } break;
  case SHType::Set: {
    auto &s = value.payload.setValue;
    SHSetIterator sit;
    s.api->setGetIterator(s, &sit);
    SHVar v;
    while (s.api->setNext(s, &sit, &v)) {
      auto derived = deriveTypeInfo(v, data, containsVariables);
      shards::arrayPush(varType.setTypes, derived);
    }
  } break;
  case SHType::ContextVar: {
    if (containsVariables) {
      // containsVariables is used by Const shard mostly
      *containsVariables = true;
      const auto varName = value.payload.stringValue;
      for (auto info : data.shared) {
        if (strcmp(info.name, varName) == 0) {
          return info.exposedType;
        }
      }
      // not found! reset containsVariables.
      *containsVariables = false;
      SHLOG_WARNING("Could not find variable {} when deriving type info", varName);
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
  // this is not complete at all, missing Array and ContextVar for example
  SHTypeInfo varType;
  memcpy(&varType, &other, sizeof(SHTypeInfo));
  switch (varType.basicType) {
  case Seq: {
    varType.seqTypes = {};
    for (uint32_t i = 0; i < other.seqTypes.len; i++) {
      auto cloned = cloneTypeInfo(other.seqTypes.elements[i]);
      shards::arrayPush(varType.seqTypes, cloned);
    }
    break;
  }
  case SHType::Set: {
    varType.setTypes = {};
    for (uint32_t i = 0; i < other.setTypes.len; i++) {
      auto cloned = cloneTypeInfo(other.setTypes.elements[i]);
      shards::arrayPush(varType.setTypes, cloned);
    }
    break;
  }
  case Table: {
    varType.table = {};
    for (uint32_t i = 0; i < other.table.types.len; i++) {
      auto cloned = cloneTypeInfo(other.table.types.elements[i]);
      shards::arrayPush(varType.table.types, cloned);
    }
    for (uint32_t i = 0; i < other.table.keys.len; i++) {
      shards::arrayPush(varType.table.keys, strdup(other.table.keys.elements[i]));
    }
    break;
  }
  default:
    break;
  };
  return varType;
} // namespace shards

// this is potentially called from unsafe code (e.g. networking)
// let's do some crude stack protection here
thread_local int deriveTypeHashRecursionCounter;
constexpr int MAX_DERIVED_TYPE_HASH_RECURSION = 100;

uint64_t _deriveTypeHash(const SHVar &value);

void updateTypeHash(const SHVar &var, XXH3_state_s *state) {
  // this is not complete at all, missing Array and ContextVar for example
  XXH3_64bits_update(state, &var.valueType, sizeof(var.valueType));
  XXH3_64bits_update(state, &var.innerType, sizeof(var.innerType));

  switch (var.valueType) {
  case Object: {
    XXH3_64bits_update(state, &var.payload.objectVendorId, sizeof(var.payload.objectVendorId));
    XXH3_64bits_update(state, &var.payload.objectTypeId, sizeof(var.payload.objectTypeId));
    break;
  }
  case Enum: {
    XXH3_64bits_update(state, &var.payload.enumVendorId, sizeof(var.payload.enumVendorId));
    XXH3_64bits_update(state, &var.payload.enumTypeId, sizeof(var.payload.enumTypeId));
    break;
  }
  case Seq: {
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
  case Table: {
    // this is unsafe because allocates on the stack
    // but we need to sort hashes
    std::vector<std::pair<uint64_t, SHString>, stack_allocator<std::pair<uint64_t, SHString>>> hashes;
    // table is unordered so just collect
    auto &t = var.payload.tableValue;
    SHTableIterator tit;
    t.api->tableGetIterator(t, &tit);
    SHString k;
    SHVar v;
    while (t.api->tableNext(t, &tit, &k, &v)) {
      hashes.emplace_back(_deriveTypeHash(v), k);
    }
    // sort and actually do the hashing
    pdqsort(hashes.begin(), hashes.end());
    for (const auto &pair : hashes) {
      XXH3_64bits_update(state, pair.second, strlen(k));
      XXH3_64bits_update(state, &pair.first, sizeof(uint64_t));
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
  case Object: {
    XXH3_64bits_update(state, &t.object.vendorId, sizeof(t.object.vendorId));
    XXH3_64bits_update(state, &t.object.typeId, sizeof(t.object.typeId));
  } break;
  case Enum: {
    XXH3_64bits_update(state, &t.enumeration.vendorId, sizeof(t.enumeration.vendorId));
    XXH3_64bits_update(state, &t.enumeration.typeId, sizeof(t.enumeration.typeId));
  } break;
  case Seq: {
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
  case Table: {
    if (t.table.keys.len == t.table.types.len) {
      std::vector<std::pair<uint64_t, SHString>, stack_allocator<std::pair<uint64_t, SHString>>> hashes;
      for (uint32_t i = 0; i < t.table.types.len; i++) {
        auto typeHash = deriveTypeHash(t.table.types.elements[i]);
        const char *key = t.table.keys.elements[i];
        hashes.emplace_back(typeHash, key);
      }
      pdqsort(hashes.begin(), hashes.end());
      for (const auto &hash : hashes) {
        XXH3_64bits_update(state, hash.second, strlen(hash.second));
        XXH3_64bits_update(state, &hash.first, sizeof(uint64_t));
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
    std::string err("Parameter index out of range");
    callback(shard, err.c_str(), false, userData);
    return false;
  }

  auto param = params.elements[index];

  // Build a SHTypeInfo for the var
  SHInstanceData data{};
  auto varType = deriveTypeInfo(value, data);
  DEFER({ freeDerivedInfo(varType); });

  for (uint32_t i = 0; param.valueTypes.len > i; i++) {
    if (matchTypes(varType, param.valueTypes.elements[i], true, true)) {
      return true; // we are good just exit
    }
  }

  std::string err("Parameter not accepting this kind of variable (" + type2Name(value.valueType) + ")");
  callback(shard, err.c_str(), false, userData);

  return false;
}

void error_handler(int err_sig) {
  std::signal(err_sig, SIG_DFL);

  auto printTrace = false;

  switch (err_sig) {
  case SIGINT:
  case SIGTERM:
    SHLOG_INFO("Exiting due to INT/TERM signal");
    shards::GetGlobals().SigIntTerm++;
    if (shards::GetGlobals().SigIntTerm > 5)
      std::exit(-1);
    spdlog::shutdown();
    break;
  case SIGFPE:
    SHLOG_ERROR("Fatal SIGFPE");
    printTrace = true;
    break;
  case SIGILL:
    SHLOG_ERROR("Fatal SIGILL");
    printTrace = true;
    break;
  case SIGABRT:
    SHLOG_ERROR("Fatal SIGABRT");
    printTrace = true;
    break;
  case SIGSEGV:
    SHLOG_ERROR("Fatal SIGSEGV");
    printTrace = true;
    break;
  }

  if (printTrace) {
#ifndef __EMSCRIPTEN__
    SHLOG_ERROR(boost::stacktrace::stacktrace());
#endif
  }

  std::raise(err_sig);
}

#ifdef _WIN32
#include "debugapi.h"
static bool isDebuggerPresent() { return (bool)IsDebuggerPresent(); }
#else
constexpr bool isDebuggerPresent() { return false; }
#endif

void installSignalHandlers() {
  if (!isDebuggerPresent()) {
    std::signal(SIGINT, &error_handler);
    std::signal(SIGTERM, &error_handler);
    std::signal(SIGFPE, &error_handler);
    std::signal(SIGILL, &error_handler);
    std::signal(SIGABRT, &error_handler);
    std::signal(SIGSEGV, &error_handler);
  }
}

Weave &Weave::shard(std::string_view name, std::vector<Var> params) {
  auto blk = createShard(name.data());
  if (!blk) {
    SHLOG_ERROR("Shard {} was not found", name);
    throw SHException("Shard not found");
  }

  blk->setup(blk);

  const auto psize = params.size();
  for (size_t i = 0; i < psize; i++) {
    // skip Any, as they mean default value
    if (params[i] != Var::Any)
      blk->setParam(blk, int(i), &params[i]);
  }

  _shards.emplace_back(blk);
  return *this;
}

Weave &Weave::let(Var value) {
  auto blk = createShard("Const");
  blk->setup(blk);
  blk->setParam(blk, 0, &value);
  _shards.emplace_back(blk);
  return *this;
}

Wire::Wire(std::string_view name) : _wire(SHWire::make(name)) {}

Wire &Wire::looped(bool looped) {
  _wire->looped = looped;
  return *this;
}

Wire &Wire::unsafe(bool unsafe) {
  _wire->unsafe = unsafe;
  return *this;
}

Wire &Wire::stackSize(size_t stackSize) {
  _wire->stackSize = stackSize;
  return *this;
}

Wire &Wire::name(std::string_view name) {
  _wire->name = name;
  return *this;
}

Wire &Wire::shard(std::string_view name, std::vector<Var> params) {
  auto blk = createShard(name.data());
  if (!blk) {
    SHLOG_ERROR("Shard {} was not found", name);
    throw SHException("Shard not found");
  }

  blk->setup(blk);

  const auto psize = params.size();
  for (size_t i = 0; i < psize; i++) {
    // skip Any, as they mean default value
    if (params[i] != Var::Any)
      blk->setParam(blk, int(i), &params[i]);
  }

  _wire->addShard(blk);
  return *this;
}

Wire &Wire::let(Var value) {
  auto blk = createShard("Const");
  blk->setup(blk);
  blk->setParam(blk, 0, &value);
  _wire->addShard(blk);
  return *this;
}

SHWireRef Wire::weakRef() const { return SHWire::weakRef(_wire); }

Var::Var(const Wire &wire) : Var(wire.weakRef()) {}

SHRunWireOutput runWire(SHWire *wire, SHContext *context, const SHVar &wireInput) {
  memset(&wire->previousOutput, 0x0, sizeof(SHVar));
  wire->currentInput = wireInput;
  wire->state = SHWire::State::Iterating;
  wire->context = context;
  DEFER({ wire->state = SHWire::State::IterationEnded; });

  try {
    auto state =
        shardsActivation<std::vector<ShardPtr>, false, false>(wire->shards, context, wireInput, wire->previousOutput);
    switch (state) {
    case SHWireState::Return:
      return {context->getFlowStorage(), Stopped};
    case SHWireState::Restart:
      return {context->getFlowStorage(), Restarted};
    case SHWireState::Stop:
      // On failure shardsActivation throws!
      assert(!context->failed());
      return {context->getFlowStorage(), Stopped};
    case SHWireState::Rebase:
      // Handled inside shardsActivation
      SHLOG_FATAL("invalid state");
    case SHWireState::Continue:
      break;
    }
  }
#ifndef __EMSCRIPTEN__
  catch (boost::context::detail::forced_unwind const &e) {
    throw; // required for Boost Coroutine!
  }
#endif
  catch (...) {
    // shardsActivation handles error logging and such
    return {wire->previousOutput, Failed};
  }

  return {wire->previousOutput, Running};
}

#ifndef __EMSCRIPTEN__
boost::context::continuation run(SHWire *wire, SHFlow *flow, boost::context::continuation &&sink)
#else
void run(SHWire *wire, SHFlow *flow, SHCoro *coro)
#endif
{
  SHLOG_TRACE("wire {} rolling", wire->name);

  auto running = true;

  // Reset state
  wire->state = SHWire::State::Prepared;
  wire->finishedOutput = Var::Empty;
  wire->finishedError.clear();

  // Create a new context and copy the sink in
  SHFlow anonFlow{wire};
#ifndef __EMSCRIPTEN__
  SHContext context(std::move(sink), wire, flow ? flow : &anonFlow);
#else
  SHContext context(coro, wire, flow ? flow : &anonFlow);
#endif

  // if the wire had a context (Stepped wires in wires.cpp)
  // copy some stuff from it
  if (wire->context) {
    context.wireStack = wire->context->wireStack;
    // need to add back ourself
    context.wireStack.push_back(wire);
  }

#ifdef SH_USE_TSAN
  context.tsan_handle = wire->tsan_coro;
#endif
  // also pupulate context in wire
  wire->context = &context;

  // We prerolled our coro, suspend here before actually starting.
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
#ifndef __EMSCRIPTEN__
  context.continuation = context.continuation.resume();
#else
  context.continuation->yield();
#endif

  SHLOG_TRACE("wire {} starting", wire->name);

  if (context.shouldStop()) {
    // We might have stopped before even starting!
    SHLOG_ERROR("Wire {} stopped before starting", wire->name);
    goto endOfWire;
  }

  while (running) {
    running = wire->looped;
    // reset context state
    context.continueFlow();

    // call optional nextFrame calls here
    for (auto blk : context.nextFrameCallbacks()) {
      auto shard = const_cast<Shard *>(blk);
      if (shard->nextFrame) {
        shard->nextFrame(shard, &context);
      }
    }

    auto runRes = runWire(wire, &context, wire->rootTickInput);
    if (unlikely(runRes.state == Failed)) {
      SHLOG_DEBUG("Wire {} failed", wire->name);
      wire->state = SHWire::State::Failed;
      context.stopFlow(runRes.output);
      break;
    } else if (unlikely(runRes.state == Stopped)) {
      SHLOG_DEBUG("Wire {} stopped", wire->name);
      context.stopFlow(runRes.output);
      // also replace the previous output with actual output
      // as it's likely coming from flowStorage of context!
      wire->previousOutput = runRes.output;
      break;
    } else if (unlikely(runRes.state == Restarted)) {
      // must clone over rootTickInput!
      // restart overwrites rootTickInput on purpose
      cloneVar(wire->rootTickInput, context.getFlowStorage());
    }

    if (!wire->unsafe && wire->looped) {
      // Ensure no while(true), yield anyway every run
      context.next = SHDuration(0);
#ifndef __EMSCRIPTEN__
      context.continuation = context.continuation.resume();
#else
      context.continuation->yield();
#endif
      // This is delayed upon continuation!!
      if (context.shouldStop()) {
        SHLOG_DEBUG("Wire {} aborted on resume", wire->name);
        break;
      }
    }
  }

endOfWire:
  wire->finishedOutput = wire->previousOutput;
  if (context.failed())
    wire->finishedError = context.getErrorMessage();

  // run cleanup on all the shards
  // ensure stop state is set
  context.stopFlow(wire->previousOutput);
  wire->cleanup(true);

  // Need to take care that we might have stopped the wire very early due to
  // errors and the next eventual stop() should avoid resuming
  if (wire->state != SHWire::State::Failed)
    wire->state = SHWire::State::Ended;

  SHLOG_TRACE("wire {} ended", wire->name);

#ifndef __EMSCRIPTEN__
  return std::move(context.continuation);
#else
  context.continuation->yield();
#endif
}

Globals &GetGlobals() {
  static Globals globals;
  return globals;
}

NO_INLINE void _destroyVarSlow(SHVar &var) {
  switch (var.valueType) {
  case Seq: {
    // notice we use .cap! because we make sure to 0 new empty elements
    for (size_t i = var.payload.seqValue.cap; i > 0; i--) {
      destroyVar(var.payload.seqValue.elements[i - 1]);
    }
    shards::arrayFree(var.payload.seqValue);
  } break;
  case Table: {
    assert(var.payload.tableValue.api == &GetGlobals().TableInterface);
    assert(var.payload.tableValue.opaque);
    auto map = (SHMap *)var.payload.tableValue.opaque;
    delete map;
  } break;
  case SHType::Set: {
    assert(var.payload.setValue.api == &GetGlobals().SetInterface);
    assert(var.payload.setValue.opaque);
    auto set = (SHHashSet *)var.payload.setValue.opaque;
    delete set;
  } break;
  default:
    break;
  };
}

NO_INLINE void _cloneVarSlow(SHVar &dst, const SHVar &src) {
  switch (src.valueType) {
  case Seq: {
    uint32_t srcLen = src.payload.seqValue.len;

    // try our best to re-use memory
    if (dst.valueType != Seq) {
      destroyVar(dst);
      dst.valueType = Seq;
    }

    if (src.payload.seqValue.elements == dst.payload.seqValue.elements)
      return;

    shards::arrayResize(dst.payload.seqValue, srcLen);
    for (uint32_t i = 0; i < srcLen; i++) {
      const auto &subsrc = src.payload.seqValue.elements[i];
      cloneVar(dst.payload.seqValue.elements[i], subsrc);
    }
  } break;
  case Path:
  case ContextVar:
  case String: {
    auto srcSize = src.payload.stringLen > 0 || src.payload.stringValue == nullptr ? src.payload.stringLen
                                                                                   : uint32_t(strlen(src.payload.stringValue));
    if ((dst.valueType != String && dst.valueType != ContextVar) || dst.payload.stringCapacity < srcSize) {
      destroyVar(dst);
      // allocate a 0 terminator too
      dst.payload.stringValue = new char[srcSize + 1];
      dst.payload.stringCapacity = srcSize;
    } else {
      if (src.payload.stringValue == dst.payload.stringValue)
        return;
    }

    dst.valueType = src.valueType;
    memcpy((void *)dst.payload.stringValue, (void *)src.payload.stringValue, srcSize);
    ((char *)dst.payload.stringValue)[srcSize] = 0;
    // fill the optional len field
    dst.payload.stringLen = srcSize;
  } break;
  case Image: {
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
    if (dst.valueType != Image || srcImgSize > dstCapacity) {
      destroyVar(dst);
      dst.valueType = Image;
      dst.payload.imageValue.data = new uint8_t[srcImgSize];
    }

    dst.payload.imageValue.flags = src.payload.imageValue.flags;
    dst.payload.imageValue.height = src.payload.imageValue.height;
    dst.payload.imageValue.width = src.payload.imageValue.width;
    dst.payload.imageValue.channels = src.payload.imageValue.channels;

    if (src.payload.imageValue.data == dst.payload.imageValue.data)
      return;

    memcpy(dst.payload.imageValue.data, src.payload.imageValue.data, srcImgSize);
  } break;
  case Audio: {
    size_t srcSize = src.payload.audioValue.nsamples * src.payload.audioValue.channels * sizeof(float);
    size_t dstCapacity = dst.payload.audioValue.nsamples * dst.payload.audioValue.channels * sizeof(float);
    if (dst.valueType != Audio || srcSize > dstCapacity) {
      destroyVar(dst);
      dst.valueType = Audio;
      dst.payload.audioValue.samples = new float[src.payload.audioValue.nsamples * src.payload.audioValue.channels];
    }

    dst.payload.audioValue.sampleRate = src.payload.audioValue.sampleRate;
    dst.payload.audioValue.nsamples = src.payload.audioValue.nsamples;
    dst.payload.audioValue.channels = src.payload.audioValue.channels;

    if (src.payload.audioValue.samples == dst.payload.audioValue.samples)
      return;

    memcpy(dst.payload.audioValue.samples, src.payload.audioValue.samples, srcSize);
  } break;
  case Table: {
    SHMap *map;
    if (dst.valueType == Table) {
      if (src.payload.tableValue.opaque == dst.payload.tableValue.opaque)
        return;
      assert(dst.payload.tableValue.api == &GetGlobals().TableInterface);
      map = (SHMap *)dst.payload.tableValue.opaque;
      map->clear();
    } else {
      destroyVar(dst);
      dst.valueType = Table;
      dst.payload.tableValue.api = &GetGlobals().TableInterface;
      map = new SHMap();
      dst.payload.tableValue.opaque = map;
    }

    auto &t = src.payload.tableValue;
    SHTableIterator tit{};
    t.api->tableGetIterator(t, &tit);
    SHString k;
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
      assert(dst.payload.setValue.api == &GetGlobals().SetInterface);
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
    SHSetIterator sit{};
    s.api->setGetIterator(s, &sit);
    SHVar v;
    while (s.api->setNext(s, &sit, &v)) {
      (*set).emplace(v);
    }
  } break;
  case Bytes: {
    if (dst.valueType != Bytes || dst.payload.bytesCapacity < src.payload.bytesSize) {
      destroyVar(dst);
      dst.valueType = Bytes;
      dst.payload.bytesValue = new uint8_t[src.payload.bytesSize];
      dst.payload.bytesCapacity = src.payload.bytesSize;
    }

    dst.payload.bytesSize = src.payload.bytesSize;

    if (src.payload.bytesValue == dst.payload.bytesValue)
      return;

    memcpy((void *)dst.payload.bytesValue, (void *)src.payload.bytesValue, src.payload.bytesSize);
  } break;
  case SHType::Array: {
    auto srcLen = src.payload.arrayValue.len;

    // try our best to re-use memory
    if (dst.valueType != Array) {
      destroyVar(dst);
      dst.valueType = Array;
    }

    if (src.payload.arrayValue.elements == dst.payload.arrayValue.elements)
      return;

    dst.innerType = src.innerType;
    shards::arrayResize(dst.payload.arrayValue, srcLen);
    // array holds only blittables and is packed so a single memcpy is enough
    memcpy(&dst.payload.arrayValue.elements[0], &src.payload.arrayValue.elements[0], sizeof(SHVarPayload) * srcLen);
  } break;
  case SHType::Wire:
    if (dst != src) {
      destroyVar(dst);
    }
    dst.valueType = SHType::Wire;
    dst.payload.wireValue = SHWire::addRef(src.payload.wireValue);
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
  default:
    break;
  };
}

void _gatherShards(const ShardsCollection &coll, std::vector<ShardInfo> &out) {
  // TODO out should be a set?
  switch (coll.index()) {
  case 0: {
    // wire
    auto wire = std::get<const SHWire *>(coll);
    if (!gatheringWires().count(wire)) {
      gatheringWires().insert(wire);
      for (auto blk : wire->shards) {
        _gatherShards(blk, out);
      }
    }
  } break;
  case 1: {
    // Single shard
    auto blk = std::get<ShardPtr>(coll);
    std::string_view name(blk->name(blk));
    out.emplace_back(name, blk);
    // Also find nested shards
    const auto params = blk->parameters(blk);
    for (uint32_t i = 0; i < params.len; i++) {
      const auto &param = params.elements[i];
      const auto &types = param.valueTypes;
      bool potential = false;
      for (uint32_t j = 0; j < types.len; j++) {
        const auto &type = types.elements[j];
        if (type.basicType == ShardRef || type.basicType == SHType::Wire) {
          potential = true;
        } else if (type.basicType == Seq) {
          const auto &stypes = type.seqTypes;
          for (uint32_t k = 0; k < stypes.len; k++) {
            if (stypes.elements[k].basicType == ShardRef) {
              potential = true;
            }
          }
        }
      }
      if (potential)
        _gatherShards(blk->getParam(blk, i), out);
    }
  } break;
  case 2: {
    // Shards seq
    auto bs = std::get<Shards>(coll);
    for (uint32_t i = 0; i < bs.len; i++) {
      _gatherShards(bs.elements[i], out);
    }
  } break;
  case 3: {
    // Var
    auto var = std::get<SHVar>(coll);
    if (var.valueType == ShardRef) {
      _gatherShards(var.payload.shardValue, out);
    } else if (var.valueType == SHType::Wire) {
      auto wire = SHWire::sharedFromRef(var.payload.wireValue);
      _gatherShards(wire.get(), out);
    } else if (var.valueType == Seq) {
      auto bs = var.payload.seqValue;
      for (uint32_t i = 0; i < bs.len; i++) {
        _gatherShards(bs.elements[i], out);
      }
    }
  } break;
  default:
    SHLOG_FATAL("invalid state");
  }
}

void gatherShards(const ShardsCollection &coll, std::vector<ShardInfo> &out) {
  gatheringWires().clear();
  _gatherShards(coll, out);
}

SHVar hash(const SHVar &var) {
  gatheringWires().clear();

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
  assert(error == XXH_OK);

  switch (var.valueType) {
  case SHType::Bytes: {
    error = XXH3_128bits_update(hashState, var.payload.bytesValue, size_t(var.payload.bytesSize));
    assert(error == XXH_OK);
  } break;
  case SHType::Path:
  case SHType::ContextVar:
  case SHType::String: {
    error = XXH3_128bits_update(hashState, var.payload.stringValue,
                                size_t(var.payload.stringLen > 0 || var.payload.stringValue == nullptr
                                           ? var.payload.stringLen
                                           : strlen(var.payload.stringValue)));
    assert(error == XXH_OK);
  } break;
  case SHType::Image: {
    SHImage i = var.payload.imageValue;
    i.data = nullptr;
    error = XXH3_128bits_update(hashState, &i, sizeof(SHImage));
    assert(error == XXH_OK);
    auto pixsize = 1;
    if ((var.payload.imageValue.flags & SHIMAGE_FLAGS_16BITS_INT) == SHIMAGE_FLAGS_16BITS_INT)
      pixsize = 2;
    else if ((var.payload.imageValue.flags & SHIMAGE_FLAGS_32BITS_FLOAT) == SHIMAGE_FLAGS_32BITS_FLOAT)
      pixsize = 4;
    error = XXH3_128bits_update(
        hashState, var.payload.imageValue.data,
        size_t(var.payload.imageValue.channels * var.payload.imageValue.width * var.payload.imageValue.height * pixsize));
    assert(error == XXH_OK);
  } break;
  case SHType::Audio: {
    SHAudio a = var.payload.audioValue;
    a.samples = nullptr;
    error = XXH3_128bits_update(hashState, &a, sizeof(SHAudio));
    assert(error == XXH_OK);
    error = XXH3_128bits_update(hashState, var.payload.audioValue.samples,
                                size_t(var.payload.audioValue.channels * var.payload.audioValue.nsamples * sizeof(float)));
    assert(error == XXH_OK);
  } break;
  case SHType::Seq: {
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      hash_update(var.payload.seqValue.elements[i], state);
    }
  } break;
  case SHType::Array: {
    for (uint32_t i = 0; i < var.payload.arrayValue.len; i++) {
      SHVar tmp; // only of blittable types and hash uses just type, so no init
                 // needed
      tmp.valueType = var.innerType;
      tmp.payload = var.payload.arrayValue.elements[i];
      hash_update(tmp, state);
    }
  } break;
  case SHType::Table: {
    // this is unsafe because allocates on the stack
    // but we need to sort hashes
    std::vector<std::pair<std::pair<uint64_t, uint64_t>, SHString>,
                stack_allocator<std::pair<std::pair<uint64_t, uint64_t>, SHString>>>
        hashes;

    auto &t = var.payload.tableValue;
    SHTableIterator it;
    t.api->tableGetIterator(t, &it);
    SHString key;
    SHVar value;
    while (t.api->tableNext(t, &it, &key, &value)) {
      const auto h = hash(value);
      hashes.emplace_back(std::make_pair(uint64_t(h.payload.int2Value[0]), uint64_t(h.payload.int2Value[1])), key);
    }

    pdqsort(hashes.begin(), hashes.end());
    for (const auto &pair : hashes) {
      error = XXH3_128bits_update(hashState, pair.second, strlen(pair.second));
      assert(error == XXH_OK);
      XXH3_128bits_update(hashState, &pair.first, sizeof(std::pair<uint64_t, uint64_t>));
    }
  } break;
  case SHType::Set: {
    // this is unsafe because allocates on the stack
    // but we need to sort hashes
    std::vector<std::pair<uint64_t, uint64_t>, stack_allocator<std::pair<uint64_t, uint64_t>>> hashes;

    // just store hashes, sort and actually combine later
    auto &s = var.payload.setValue;
    SHSetIterator it;
    s.api->setGetIterator(s, &it);
    SHVar value;
    while (s.api->setNext(s, &it, &value)) {
      const auto h = hash(value);
      hashes.emplace_back(uint64_t(h.payload.int2Value[0]), uint64_t(h.payload.int2Value[1]));
    }

    pdqsort(hashes.begin(), hashes.end());
    for (const auto &hash : hashes) {
      XXH3_128bits_update(hashState, &hash, sizeof(uint64_t));
    }
  } break;
  case SHType::ShardRef: {
    auto blk = var.payload.shardValue;
    auto name = blk->name(blk);
    auto error = XXH3_128bits_update(hashState, name, strlen(name));
    assert(error == XXH_OK);

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
    if (gatheringWires().count(wire.get()) == 0) {
      gatheringWires().insert(wire.get());

      error = XXH3_128bits_update(hashState, wire->name.c_str(), wire->name.length());
      assert(error == XXH_OK);

      error = XXH3_128bits_update(hashState, &wire->looped, sizeof(wire->looped));
      assert(error == XXH_OK);

      error = XXH3_128bits_update(hashState, &wire->unsafe, sizeof(wire->unsafe));
      assert(error == XXH_OK);

      for (auto &blk : wire->shards) {
        SHVar tmp;
        tmp.valueType = SHType::ShardRef;
        tmp.payload.shardValue = blk;
        hash_update(tmp, state);
      }

      for (auto &wireVar : wire->variables) {
        error = XXH3_128bits_update(hashState, wireVar.first.c_str(), wireVar.first.length());
        assert(error == XXH_OK);
        hash_update(wireVar.second, state);
      }
    }
  } break;
  case SHType::Object: {
    error = XXH3_128bits_update(hashState, &var.payload.objectVendorId, sizeof(var.payload.objectVendorId));
    assert(error == XXH_OK);
    error = XXH3_128bits_update(hashState, &var.payload.objectTypeId, sizeof(var.payload.objectTypeId));
    assert(error == XXH_OK);
    if ((var.flags & SHVAR_FLAGS_USES_OBJINFO) == SHVAR_FLAGS_USES_OBJINFO && var.objectInfo && var.objectInfo->hash) {
      // hash of the hash...
      auto objHash = var.objectInfo->hash(var.payload.objectValue);
      error = XXH3_128bits_update(hashState, &objHash, sizeof(uint64_t));
      assert(error == XXH_OK);
    } else {
      // this will be valid only within this process memory space
      // better then nothing
      error = XXH3_128bits_update(hashState, &var.payload.objectValue, sizeof(var.payload.objectValue));
      assert(error == XXH_OK);
    }
  } break;
  case SHType::None:
  case SHType::Any:
    break;
  default: {
    error = XXH3_128bits_update(hashState, &var.payload, sizeof(SHVarPayload));
    assert(error == XXH_OK);
  }
  }
}

void Serialization::varFree(SHVar &output) {
  switch (output.valueType) {
  case SHType::None:
  case SHType::EndOfBlittableTypes:
  case SHType::Any:
  case SHType::Enum:
  case SHType::Bool:
  case SHType::Int:
  case SHType::Int2:
  case SHType::Int3:
  case SHType::Int4:
  case SHType::Int8:
  case SHType::Int16:
  case SHType::Float:
  case SHType::Float2:
  case SHType::Float3:
  case SHType::Float4:
  case SHType::Color:
    break;
  case SHType::Bytes:
    delete[] output.payload.bytesValue;
    break;
  case SHType::Array:
    shards::arrayFree(output.payload.arrayValue);
    break;
  case SHType::Path:
  case SHType::String:
  case SHType::ContextVar: {
    delete[] output.payload.stringValue;
    break;
  }
  case SHType::Seq: {
    for (uint32_t i = 0; i < output.payload.seqValue.cap; i++) {
      varFree(output.payload.seqValue.elements[i]);
    }
    shards::arrayFree(output.payload.seqValue);
    break;
  }
  case SHType::Table: {
    output.payload.tableValue.api->tableFree(output.payload.tableValue);
    break;
  }
  case SHType::Set: {
    output.payload.setValue.api->setFree(output.payload.setValue);
    break;
  }
  case SHType::Image: {
    delete[] output.payload.imageValue.data;
    break;
  }
  case SHType::Audio: {
    delete[] output.payload.audioValue.samples;
    break;
  }
  case SHType::ShardRef: {
    auto blk = output.payload.shardValue;
    if (!blk->owned) {
      // destroy only if not owned
      blk->destroy(blk);
    }
    break;
  }
  case SHType::Wire: {
    SHWire::deleteRef(output.payload.wireValue);
    break;
  }
  case SHType::Object: {
    if ((output.flags & SHVAR_FLAGS_USES_OBJINFO) == SHVAR_FLAGS_USES_OBJINFO && output.objectInfo &&
        output.objectInfo->release) {
      output.objectInfo->release(output.payload.objectValue);
    }
    break;
  }
  }

  memset(&output, 0x0, sizeof(SHVar));
}

SHString getString(uint32_t crc) {
  assert(shards::GetGlobals().CompressedStrings);
  auto s = (*shards::GetGlobals().CompressedStrings)[crc].string;
  return s != nullptr ? s : "";
}

void setString(uint32_t crc, SHString str) {
  assert(shards::GetGlobals().CompressedStrings);
  (*shards::GetGlobals().CompressedStrings)[crc].string = str;
  (*shards::GetGlobals().CompressedStrings)[crc].crc = crc;
}
}; // namespace shards

// NO NAMESPACE here!

void SHWire::reset() {
  for (auto it = shards.rbegin(); it != shards.rend(); ++it) {
    (*it)->cleanup(*it);
  }
  for (auto it = shards.rbegin(); it != shards.rend(); ++it) {
    (*it)->destroy(*it);
    // blk is responsible to free itself, as they might use any allocation
    // strategy they wish!
  }
  shards.clear();

  // find dangling variables, notice but do not destroy
  for (auto var : variables) {
    if (var.second.refcount > 0) {
      SHLOG_ERROR("Found a dangling variable: {}, wire: {}", var.first, name);
    }
  }
  variables.clear();

  wireUsers.clear();
  composedHash = Var::Empty;
  inputType = SHTypeInfo();
  outputType = {};
  requiredVariables.clear();

  auto n = mesh.lock();
  if (n) {
    n->visitedWires.erase(this);
  }
  mesh.reset();

  if (stackMem) {
    ::operator delete[](stackMem, std::align_val_t{16});
    stackMem = nullptr;
  }

  resumer = nullptr;
}

void SHWire::warmup(SHContext *context) {
  if (!warmedUp) {
    SHLOG_DEBUG("Running warmup on wire: {}", name);

    // we likely need this early!
    mesh = context->main->mesh;
    warmedUp = true;

    context->wireStack.push_back(this);
    DEFER({ context->wireStack.pop_back(); });
    for (auto blk : shards) {
      try {
        if (blk->warmup)
          blk->warmup(blk, context);
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

    SHLOG_DEBUG("Ran warmup on wire: {}", name);
  }
}

void SHWire::cleanup(bool force) {
  if (warmedUp && (force || wireUsers.size() == 0)) {
    SHLOG_DEBUG("Running cleanup on wire: {} users count: {}", name, wireUsers.size());

    warmedUp = false;

    // Run cleanup on all shards, prepare them for a new start if necessary
    // Do this in reverse to allow a safer cleanup
    for (auto it = shards.rbegin(); it != shards.rend(); ++it) {
      auto blk = *it;
      try {
        blk->cleanup(blk);
      }
#ifndef __EMSCRIPTEN__
      catch (boost::context::detail::forced_unwind const &e) {
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
    auto n = mesh.lock();
    if (n) {
      n->visitedWires.erase(this);
    }
    mesh.reset();

    resumer = nullptr;

    SHLOG_DEBUG("Ran cleanup on wire: {}", name);
  }
}

#ifndef OVERRIDE_REGISTER_ALL_SHARDS
void shRegisterAllShards() { shards::registerCoreShards(); }
#endif

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
SHCore *__cdecl shardsInterface(uint32_t abi_version) {
  // for now we ignore abi_version
  if (sh_current_interface_loaded)
    return &sh_current_interface;

  // Load everything we know if we did not yet!
  try {
    shards::registerCoreShards();
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
    auto mem = ::operator new (size, std::align_val_t{16});
    memset(mem, 0, size);
    return mem;
  };

  result->free = [](void *ptr) { ::operator delete (ptr, std::align_val_t{16}); };

  result->registerShard = [](const char *fullName, SHShardConstructor constructor) noexcept {
    API_TRY_CALL(registerShard, shards::registerShard(fullName, constructor);)
  };

  result->registerObjectType = [](int32_t vendorId, int32_t typeId, SHObjectInfo info) noexcept {
    API_TRY_CALL(registerObjectType, shards::registerObjectType(vendorId, typeId, info);)
  };

  result->registerEnumType = [](int32_t vendorId, int32_t typeId, SHEnumInfo info) noexcept {
    API_TRY_CALL(registerEnumType, shards::registerEnumType(vendorId, typeId, info);)
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

  result->referenceVariable = [](SHContext *context, const char *name) noexcept {
    return shards::referenceVariable(context, name);
  };

  result->referenceWireVariable = [](SHWireRef wire, const char *name) noexcept {
    return shards::referenceWireVariable(wire, name);
  };

  result->releaseVariable = [](SHVar *variable) noexcept { return shards::releaseVariable(variable); };

  result->setExternalVariable = [](SHWireRef wire, const char *name, SHVar *pVar) noexcept {
    auto sc = SHWire::sharedFromRef(wire);
    sc->externalVariables[name] = pVar;
  };

  result->removeExternalVariable = [](SHWireRef wire, const char *name) noexcept {
    auto sc = SHWire::sharedFromRef(wire);
    sc->externalVariables.erase(name);
  };

  result->allocExternalVariable = [](SHWireRef wire, const char *name) noexcept {
    auto sc = SHWire::sharedFromRef(wire);
    auto res = new (std::align_val_t{16}) SHVar();
    sc->externalVariables[name] = res;
    return res;
  };

  result->freeExternalVariable = [](SHWireRef wire, const char *name) noexcept {
    auto sc = SHWire::sharedFromRef(wire);
    auto var = sc->externalVariables[name];
    if (var) {
      ::operator delete (var, std::align_val_t{16});
    }
    sc->externalVariables.erase(name);
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

  result->abortWire = [](SHContext *context, const char *message) noexcept { context->cancelFlow(message); };

  result->cloneVar = [](SHVar *dst, const SHVar *src) noexcept { shards::cloneVar(*dst, *src); };

  result->destroyVar = [](SHVar *var) noexcept { shards::destroyVar(*var); };

#define SH_ARRAY_IMPL(_arr_, _val_, _name_)                                                                         \
  result->_name_##Free = [](_arr_ *seq) noexcept { shards::arrayFree(*seq); };                                 \
                                                                                                                    \
  result->_name_##Resize = [](_arr_ *seq, uint32_t size) noexcept { shards::arrayResize(*seq, size); };        \
                                                                                                                    \
  result->_name_##Push = [](_arr_ *seq, const _val_ *value) noexcept { shards::arrayPush(*seq, *value); };     \
                                                                                                                    \
  result->_name_##Insert = [](_arr_ *seq, uint32_t index, const _val_ *value) noexcept {                            \
    shards::arrayInsert(*seq, index, *value);                                                                  \
  };                                                                                                                \
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
    auto sc = SHWire::sharedFromRef(wire);
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
    auto sc = SHWire::sharedFromRef(wire);
    return shards::runSubWire(sc.get(), context, *input);
  };

  result->composeShards = [](Shards shards, SHValidationCallback callback, void *userData, SHInstanceData data) noexcept {
    try {
      return shards::composeWire(shards, callback, userData, data);
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

  result->validateSetParam = [](Shard *shard, int index, const SHVar *param, SHValidationCallback callback,
                                void *userData) noexcept {
    return shards::validateSetParam(shard, index, *param, callback, userData);
  };

  result->runShards = [](Shards shards, SHContext *context, const SHVar *input, SHVar *output) noexcept {
    try {
      return shards::activateShards(shards, context, *input, *output);
    } catch (const std::exception &e) {
      context->cancelFlow(e.what());
      return SHWireState::Stop;
    } catch (...) {
      context->cancelFlow("foreign exception failure during runShards");
      return SHWireState::Stop;
    }
  };

  result->runShards2 = [](Shards shards, SHContext *context, const SHVar *input, SHVar *output) noexcept {
    try {
      return shards::activateShards2(shards, context, *input, *output);
    } catch (const std::exception &e) {
      context->cancelFlow(e.what());
      return SHWireState::Stop;
    } catch (...) {
      context->cancelFlow("foreign exception failure during runShards");
      return SHWireState::Stop;
    }
  };

  result->runShardsHashed = [](Shards shards, SHContext *context, const SHVar *input, SHVar *output, SHVar *outHash) noexcept {
    try {
      return shards::activateShards(shards, context, *input, *output, *outHash);
    } catch (const std::exception &e) {
      context->cancelFlow(e.what());
      return SHWireState::Stop;
    } catch (...) {
      context->cancelFlow("foreign exception failure during runShards");
      return SHWireState::Stop;
    }
  };

  result->runShardsHashed2 = [](Shards shards, SHContext *context, const SHVar *input, SHVar *output, SHVar *outHash) noexcept {
    try {
      return shards::activateShards2(shards, context, *input, *output, *outHash);
    } catch (const std::exception &e) {
      context->cancelFlow(e.what());
      return SHWireState::Stop;
    } catch (...) {
      context->cancelFlow("foreign exception failure during runShards");
      return SHWireState::Stop;
    }
  };

  result->getWireInfo = [](SHWireRef wireref) noexcept {
    auto sc = SHWire::sharedFromRef(wireref);
    auto wire = sc.get();
    SHWireInfo info{wire->name.c_str(),
                     wire->looped,
                     wire->unsafe,
                     wire,
                     {&wire->shards[0], uint32_t(wire->shards.size()), 0},
                     shards::isRunning(wire),
                     wire->state == SHWire::State::Failed,
                     wire->finishedError.c_str(),
                     &wire->finishedOutput};
    return info;
  };

  result->log = [](const char *msg) noexcept { SHLOG_INFO(msg); };

  result->logLevel = [](int level, const char *msg) noexcept {
    spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, (spdlog::level::level_enum)level,
                                      msg);
  };

  result->createShard = [](const char *name) noexcept { return shards::createShard(name); };

  result->createWire = []() noexcept {
    auto wire = SHWire::make();
    return wire->newRef();
  };

  result->setWireName = [](SHWireRef wireref, const char *name) noexcept {
    auto sc = SHWire::sharedFromRef(wireref);
    sc->name = name;
  };

  result->setWireLooped = [](SHWireRef wireref, SHBool looped) noexcept {
    auto sc = SHWire::sharedFromRef(wireref);
    sc->looped = looped;
  };

  result->setWireUnsafe = [](SHWireRef wireref, SHBool unsafe) noexcept {
    auto sc = SHWire::sharedFromRef(wireref);
    sc->unsafe = unsafe;
  };

  result->addShard = [](SHWireRef wireref, ShardPtr blk) noexcept {
    auto sc = SHWire::sharedFromRef(wireref);
    sc->addShard(blk);
  };

  result->removeShard = [](SHWireRef wireref, ShardPtr blk) noexcept {
    auto sc = SHWire::sharedFromRef(wireref);
    sc->removeShard(blk);
  };

  result->destroyWire = [](SHWireRef wire) noexcept { SHWire::deleteRef(wire); };

  result->stopWire = [](SHWireRef wire) {
    auto sc = SHWire::sharedFromRef(wire);
    SHVar output{};
    shards::stop(sc.get(), &output);
    return output;
  };

  result->destroyWire = [](SHWireRef wire) noexcept { SHWire::deleteRef(wire); };

  result->destroyWire = [](SHWireRef wire) noexcept { SHWire::deleteRef(wire); };

  result->getGlobalWire = [](SHString name) noexcept {
    auto it = shards::GetGlobals().GlobalWires.find(name);
    if (it != shards::GetGlobals().GlobalWires.end()) {
      return SHWire::weakRef(it->second);
    } else {
      return (SHWireRef) nullptr;
    }
  };

  result->setGlobalWire = [](SHString name, SHWireRef wire) noexcept {
    shards::GetGlobals().GlobalWires[name] = SHWire::sharedFromRef(wire);
  };

  result->unsetGlobalWire = [](SHString name) noexcept {
    auto it = shards::GetGlobals().GlobalWires.find(name);
    if (it != shards::GetGlobals().GlobalWires.end()) {
      shards::GetGlobals().GlobalWires.erase(it);
    }
  };

  result->createMesh = []() noexcept {
    SHLOG_TRACE("createMesh");
    return reinterpret_cast<SHMeshRef>(SHMesh::makePtr());
  };

  result->destroyMesh = [](SHMeshRef mesh) noexcept {
    SHLOG_TRACE("destroyMesh");
    auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
    delete smesh;
  };

  result->schedule = [](SHMeshRef mesh, SHWireRef wire) noexcept {
    try {
      auto smesh = reinterpret_cast<std::shared_ptr<SHMesh> *>(mesh);
      (*smesh)->schedule(SHWire::sharedFromRef(wire));
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
    (*smesh)->tick();
    if ((*smesh)->empty())
      return false;
    else
      return true;
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

  result->isEqualVar = [](const SHVar *v1, const SHVar *v2) -> SHBool { return *v1 == *v2; };

  result->isEqualType = [](const SHTypeInfo *t1, const SHTypeInfo *t2) -> SHBool { return *t1 == *t2; };

  result->deriveTypeInfo = [](const SHVar *v, const struct SHInstanceData *data) -> SHTypeInfo {
    return deriveTypeInfo(*v, *data);
  };

  result->freeDerivedTypeInfo = [](SHTypeInfo *t) { freeDerivedInfo(*t); };

  return result;
}
};
