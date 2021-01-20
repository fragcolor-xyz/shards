/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

// must go first
#include <boost/asio.hpp>

#include "blocks/shared.hpp"
#include "runtime.hpp"
#include "utility.hpp"
#include <boost/stacktrace.hpp>
#include <csignal>
#include <cstdarg>
#include <filesystem>
#include <string.h>
#include <unordered_set>

#define XXH_INLINE_ALL
#include <xxhash.h>

#ifndef CUSTOM_XXH3_kSecret
// Applications embedding chainblocks can override this and should.
// TODO add our secret
#define CUSTOM_XXH3_kSecret XXH3_kSecret
#endif

INITIALIZE_EASYLOGGINGPP

namespace chainblocks {
#ifdef CB_COMPRESSED_STRINGS
CBOptionalString getCompiledCompressedString(uint32_t id) {
  static std::unordered_map<uint32_t, CBOptionalString>
      CompiledCompressedStrings;
  if (Globals::CompressedStrings == nullptr)
    Globals::CompressedStrings = &CompiledCompressedStrings;
  auto &val = CompiledCompressedStrings[id];
  val.crc = id; // make sure we return with crc to allow later lookups!
  return val;
}
#else
CBOptionalString setCompiledCompressedString(uint32_t id, const char *str) {
  static std::unordered_map<uint32_t, CBOptionalString>
      CompiledCompressedStrings;
  if (Globals::CompressedStrings == nullptr)
    Globals::CompressedStrings = &CompiledCompressedStrings;
  CBOptionalString ls{str, id};
  CompiledCompressedStrings[id] = ls;
  return ls;
}
#endif

extern void registerChainsBlocks();
extern void registerLoggingBlocks();
extern void registerFlowBlocks();
extern void registerSeqsBlocks();
extern void registerCastingBlocks();
extern void registerBlocksCoreBlocks();
extern void registerSerializationBlocks();
extern void registerFSBlocks();
extern void registerJsonBlocks();
extern void registerNetworkBlocks();
extern void registerStructBlocks();
extern void registerTimeBlocks();
// extern void registerOSBlocks();
extern void registerProcessBlocks();

namespace Math {
namespace LinAlg {
extern void registerBlocks();
}
} // namespace Math

namespace Regex {
extern void registerBlocks();
}

namespace channels {
extern void registerBlocks();
}

namespace Assert {
extern void registerBlocks();
}

namespace Python {
extern void registerBlocks();
}

namespace Genetic {
extern void registerBlocks();
}

namespace Random {
extern void registerBlocks();
}

namespace Imaging {
extern void registerBlocks();
}

namespace Http {
extern void registerBlocks();
}

namespace WS {
extern void registerBlocks();
}

namespace BigInt {
extern void registerBlocks();
}

namespace Wasm {
extern void registerBlocks();
}

#ifdef CB_WITH_EXTRAS
extern void cbInitExtras();
#endif

static bool globalRegisterDone = false;

void loadExternalBlocks(std::string from) {
  namespace fs = std::filesystem;
  auto root = fs::path(from);
  auto pluginPath = root / "cblocks";
  if (!fs::exists(pluginPath))
    return;

  for (auto &p : fs::recursive_directory_iterator(pluginPath)) {
    if (p.is_regular_file()) {
      auto ext = p.path().extension();
      if (ext == ".dll" || ext == ".so" || ext == ".dylib") {
        auto filename = p.path().filename();
        auto dllstr = p.path().string();
        LOG(INFO) << "Loading external dll: " << filename
                  << " path: " << dllstr;
#if _WIN32
        auto handle = LoadLibraryExA(dllstr.c_str(), NULL,
                                     LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!handle) {
          LOG(ERROR) << "LoadLibrary failed, error: " << GetLastError();
        }
#elif defined(__linux__) || defined(__APPLE__)
        auto handle = dlopen(dllstr.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
          LOG(ERROR) << "dlerror: " << dlerror();
        }
#endif
      }
    }
  }
}

el::Configurations LogsDefaultConf;

void registerCoreBlocks() {
  if (globalRegisterDone)
    return;

  globalRegisterDone = true;

// UTF8 on windows
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  namespace fs = std::filesystem;
  if (Globals::ExePath.size() > 0) {
    auto pluginPath = fs::absolute(Globals::ExePath) / "cblocks";
    auto pluginPathStr = pluginPath.wstring();
    LOG(DEBUG) << "Adding dll path: " << pluginPathStr;
    AddDllDirectory(pluginPathStr.c_str());
  }
  if (Globals::RootPath.size() > 0) {
    auto pluginPath = fs::absolute(Globals::RootPath) / "cblocks";
    auto pluginPathStr = pluginPath.wstring();
    LOG(DEBUG) << "Adding dll path: " << pluginPathStr;
    AddDllDirectory(pluginPathStr.c_str());
  }
#endif

  LogsDefaultConf.setToDefault();
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  LogsDefaultConf.setGlobally(el::ConfigurationType::Format,
                              "[%datetime %level] %msg");
#else
  LogsDefaultConf.setGlobally(el::ConfigurationType::Format,
                              "[%datetime %level %thread] %msg");
#endif
  el::Loggers::reconfigureAllLoggers(LogsDefaultConf);

  // at this point we might have some auto magical static linked block already
  // keep them stored here and re-register them
  // as we assume the observers were setup in this call caller so too late for
  // them
  std::vector<std::pair<std::string_view, CBBlockConstructor>> earlyblocks;
  for (auto &pair : Globals::BlocksRegister) {
    earlyblocks.push_back(pair);
  }
  Globals::BlocksRegister.clear();

  static_assert(sizeof(CBVarPayload) == 16);
  static_assert(sizeof(CBVar) == 32);
  static_assert(sizeof(CBMapIt) <= sizeof(CBTableIterator));

  registerBlocksCoreBlocks();
  Assert::registerBlocks();
  registerChainsBlocks();
  registerLoggingBlocks();
  registerFlowBlocks();
  registerSeqsBlocks();
  registerCastingBlocks();
  registerSerializationBlocks();
  Math::LinAlg::registerBlocks();
  registerJsonBlocks();
  registerStructBlocks();
  registerTimeBlocks();
  Regex::registerBlocks();
  channels::registerBlocks();
  Random::registerBlocks();
  Imaging::registerBlocks();
  BigInt::registerBlocks();
  registerFSBlocks();
  Wasm::registerBlocks();

#ifndef __EMSCRIPTEN__
  // registerOSBlocks();
  registerProcessBlocks();
  Genetic::registerBlocks();
  registerNetworkBlocks();
  Python::registerBlocks();
  Http::registerBlocks();
  WS::registerBlocks();
#endif

  // Enums are auto registered we need to propagate them to observers
  for (auto &einfo : Globals::EnumTypesRegister) {
    int32_t vendorId = (int32_t)((einfo.first & 0xFFFFFFFF00000000) >> 32);
    int32_t enumId = (int32_t)(einfo.first & 0x00000000FFFFFFFF);
    for (auto &pobs : Globals::Observers) {
      if (pobs.expired())
        continue;
      auto obs = pobs.lock();
      obs->registerEnumType(vendorId, enumId, einfo.second);
    }
  }

#ifdef CB_WITH_EXTRAS
  cbInitExtras();
#endif

  // re run early blocks registration!
  for (auto &pair : earlyblocks) {
    registerBlock(pair.first, pair.second);
  }

  // finally iterate cblock directory and load external dlls
  loadExternalBlocks(Globals::ExePath);
  if (Globals::RootPath != Globals::ExePath) {
    loadExternalBlocks(Globals::RootPath);
  }

  // set root path as current directory
  std::filesystem::current_path(Globals::RootPath);
}

CBlock *createBlock(std::string_view name) {
  auto it = Globals::BlocksRegister.find(name);
  if (it == Globals::BlocksRegister.end()) {
    return nullptr;
  }

  auto blkp = it->second();

  // Hook inline blocks to override activation in runChain
  if (name == "Const") {
    blkp->inlineBlockId = CBInlineBlocks::CoreConst;
  } else if (name == "Pass") {
    blkp->inlineBlockId = CBInlineBlocks::NoopBlock;
  } else if (name == "OnCleanup") {
    blkp->inlineBlockId = CBInlineBlocks::NoopBlock;
  } else if (name == "Comment") {
    blkp->inlineBlockId = CBInlineBlocks::NoopBlock;
  } else if (name == "Input") {
    blkp->inlineBlockId = CBInlineBlocks::CoreInput;
  } else if (name == "Pause") {
    blkp->inlineBlockId = CBInlineBlocks::CoreSleep;
  } else if (name == "Repeat") {
    blkp->inlineBlockId = CBInlineBlocks::CoreRepeat;
  } else if (name == "Once") {
    blkp->inlineBlockId = CBInlineBlocks::CoreOnce;
  } else if (name == "Get") {
    blkp->inlineBlockId = CBInlineBlocks::CoreGet;
  } else if (name == "Set") {
    blkp->inlineBlockId = CBInlineBlocks::CoreSet;
  } else if (name == "Ref") {
    blkp->inlineBlockId = CBInlineBlocks::CoreRef;
  } else if (name == "Update") {
    blkp->inlineBlockId = CBInlineBlocks::CoreUpdate;
  } else if (name == "Swap") {
    blkp->inlineBlockId = CBInlineBlocks::CoreSwap;
  } else if (name == "Push") {
    blkp->inlineBlockId = CBInlineBlocks::CorePush;
  } else if (name == "Is") {
    blkp->inlineBlockId = CBInlineBlocks::CoreIs;
  } else if (name == "IsNot") {
    blkp->inlineBlockId = CBInlineBlocks::CoreIsNot;
  } else if (name == "IsMore") {
    blkp->inlineBlockId = CBInlineBlocks::CoreIsMore;
  } else if (name == "IsLess") {
    blkp->inlineBlockId = CBInlineBlocks::CoreIsLess;
  } else if (name == "IsMoreEqual") {
    blkp->inlineBlockId = CBInlineBlocks::CoreIsMoreEqual;
  } else if (name == "IsLessEqual") {
    blkp->inlineBlockId = CBInlineBlocks::CoreIsLessEqual;
  } else if (name == "And") {
    blkp->inlineBlockId = CBInlineBlocks::CoreAnd;
  } else if (name == "Or") {
    blkp->inlineBlockId = CBInlineBlocks::CoreOr;
  } else if (name == "Not") {
    blkp->inlineBlockId = CBInlineBlocks::CoreNot;
  } else if (name == "Math.Add") {
    blkp->inlineBlockId = CBInlineBlocks::MathAdd;
  } else if (name == "Math.Subtract") {
    blkp->inlineBlockId = CBInlineBlocks::MathSubtract;
  } else if (name == "Math.Multiply") {
    blkp->inlineBlockId = CBInlineBlocks::MathMultiply;
  } else if (name == "Math.Divide") {
    blkp->inlineBlockId = CBInlineBlocks::MathDivide;
  } else if (name == "Math.Xor") {
    blkp->inlineBlockId = CBInlineBlocks::MathXor;
  } else if (name == "Math.And") {
    blkp->inlineBlockId = CBInlineBlocks::MathAnd;
  } else if (name == "Math.Or") {
    blkp->inlineBlockId = CBInlineBlocks::MathOr;
  } else if (name == "Math.Mod") {
    blkp->inlineBlockId = CBInlineBlocks::MathMod;
  } else if (name == "Math.LShift") {
    blkp->inlineBlockId = CBInlineBlocks::MathLShift;
  } else if (name == "Math.RShift") {
    blkp->inlineBlockId = CBInlineBlocks::MathRShift;
  } else if (name == "Math.Abs") {
    blkp->inlineBlockId = CBInlineBlocks::MathAbs;
  } else if (name == "Math.Exp") {
    blkp->inlineBlockId = CBInlineBlocks::MathExp;
  } else if (name == "Math.Exp2") {
    blkp->inlineBlockId = CBInlineBlocks::MathExp2;
  } else if (name == "Math.Expm1") {
    blkp->inlineBlockId = CBInlineBlocks::MathExpm1;
  } else if (name == "Math.Log") {
    blkp->inlineBlockId = CBInlineBlocks::MathLog;
  } else if (name == "Math.Log10") {
    blkp->inlineBlockId = CBInlineBlocks::MathLog10;
  } else if (name == "Math.Log2") {
    blkp->inlineBlockId = CBInlineBlocks::MathLog2;
  } else if (name == "Math.Log1p") {
    blkp->inlineBlockId = CBInlineBlocks::MathLog1p;
  } else if (name == "Math.Sqrt") {
    blkp->inlineBlockId = CBInlineBlocks::MathSqrt;
  } else if (name == "Math.Cbrt") {
    blkp->inlineBlockId = CBInlineBlocks::MathCbrt;
  } else if (name == "Math.Sin") {
    blkp->inlineBlockId = CBInlineBlocks::MathSin;
  } else if (name == "Math.Cos") {
    blkp->inlineBlockId = CBInlineBlocks::MathCos;
  } else if (name == "Math.Tan") {
    blkp->inlineBlockId = CBInlineBlocks::MathTan;
  } else if (name == "Math.Asin") {
    blkp->inlineBlockId = CBInlineBlocks::MathAsin;
  } else if (name == "Math.Acos") {
    blkp->inlineBlockId = CBInlineBlocks::MathAcos;
  } else if (name == "Math.Atan") {
    blkp->inlineBlockId = CBInlineBlocks::MathAtan;
  } else if (name == "Math.Sinh") {
    blkp->inlineBlockId = CBInlineBlocks::MathSinh;
  } else if (name == "Math.Cosh") {
    blkp->inlineBlockId = CBInlineBlocks::MathCosh;
  } else if (name == "Math.Tanh") {
    blkp->inlineBlockId = CBInlineBlocks::MathTanh;
  } else if (name == "Math.Asinh") {
    blkp->inlineBlockId = CBInlineBlocks::MathAsinh;
  } else if (name == "Math.Acosh") {
    blkp->inlineBlockId = CBInlineBlocks::MathAcosh;
  } else if (name == "Math.Atanh") {
    blkp->inlineBlockId = CBInlineBlocks::MathAtanh;
  } else if (name == "Math.Erf") {
    blkp->inlineBlockId = CBInlineBlocks::MathErf;
  } else if (name == "Math.Erfc") {
    blkp->inlineBlockId = CBInlineBlocks::MathErfc;
  } else if (name == "Math.TGamma") {
    blkp->inlineBlockId = CBInlineBlocks::MathTGamma;
  } else if (name == "Math.LGamma") {
    blkp->inlineBlockId = CBInlineBlocks::MathLGamma;
  } else if (name == "Math.Ceil") {
    blkp->inlineBlockId = CBInlineBlocks::MathCeil;
  } else if (name == "Math.Floor") {
    blkp->inlineBlockId = CBInlineBlocks::MathFloor;
  } else if (name == "Math.Trunc") {
    blkp->inlineBlockId = CBInlineBlocks::MathTrunc;
  } else if (name == "Math.Round") {
    blkp->inlineBlockId = CBInlineBlocks::MathRound;
  }

  return blkp;
}

void registerBlock(std::string_view name, CBBlockConstructor constructor,
                   std::string_view fullTypeName) {
  auto findIt = Globals::BlocksRegister.find(name);
  if (findIt == Globals::BlocksRegister.end()) {
    Globals::BlocksRegister.insert(std::make_pair(name, constructor));
    // LOG(TRACE) << "added block: " << cname;
  } else {
    Globals::BlocksRegister[name] = constructor;
    LOG(INFO) << "overridden block: " << name;
  }

  Globals::BlockNamesToFullTypeNames[name] = fullTypeName;

  for (auto &pobs : Globals::Observers) {
    if (pobs.expired())
      continue;
    auto obs = pobs.lock();
    obs->registerBlock(name.data(), constructor);
  }
}

void registerObjectType(int32_t vendorId, int32_t typeId, CBObjectInfo info) {
  int64_t id = (int64_t)vendorId << 32 | typeId;
  auto typeName = std::string(info.name);
  auto findIt = Globals::ObjectTypesRegister.find(id);
  if (findIt == Globals::ObjectTypesRegister.end()) {
    Globals::ObjectTypesRegister.insert(std::make_pair(id, info));
    // LOG(TRACE) << "added object type: " << typeName;
  } else {
    Globals::ObjectTypesRegister[id] = info;
    LOG(INFO) << "overridden object type: " << typeName;
  }

  for (auto &pobs : Globals::Observers) {
    if (pobs.expired())
      continue;
    auto obs = pobs.lock();
    obs->registerObjectType(vendorId, typeId, info);
  }
}

void registerEnumType(int32_t vendorId, int32_t typeId, CBEnumInfo info) {
  int64_t id = (int64_t)vendorId << 32 | typeId;
  auto typeName = std::string(info.name);
  auto findIt = Globals::ObjectTypesRegister.find(id);
  if (findIt == Globals::ObjectTypesRegister.end()) {
    Globals::EnumTypesRegister.insert(std::make_pair(id, info));
    // LOG(TRACE) << "added enum type: " << typeName;
  } else {
    Globals::EnumTypesRegister[id] = info;
    LOG(INFO) << "overridden enum type: " << typeName;
  }

  for (auto &pobs : Globals::Observers) {
    if (pobs.expired())
      continue;
    auto obs = pobs.lock();
    obs->registerEnumType(vendorId, typeId, info);
  }
}

void registerRunLoopCallback(std::string_view eventName, CBCallback callback) {
  chainblocks::Globals::RunLoopHooks[eventName] = callback;
}

void unregisterRunLoopCallback(std::string_view eventName) {
  auto findIt = chainblocks::Globals::RunLoopHooks.find(eventName);
  if (findIt != chainblocks::Globals::RunLoopHooks.end()) {
    chainblocks::Globals::RunLoopHooks.erase(findIt);
  }
}

void registerExitCallback(std::string_view eventName, CBCallback callback) {
  chainblocks::Globals::ExitHooks[eventName] = callback;
}

void unregisterExitCallback(std::string_view eventName) {
  auto findIt = chainblocks::Globals::ExitHooks.find(eventName);
  if (findIt != chainblocks::Globals::ExitHooks.end()) {
    chainblocks::Globals::ExitHooks.erase(findIt);
  }
}

void registerChain(CBChain *chain) {
  std::shared_ptr<CBChain> sc(chain);
  chainblocks::Globals::GlobalChains[chain->name] = sc;
}

void unregisterChain(CBChain *chain) {
  auto findIt = chainblocks::Globals::GlobalChains.find(chain->name);
  if (findIt != chainblocks::Globals::GlobalChains.end()) {
    chainblocks::Globals::GlobalChains.erase(findIt);
  }
}

void callExitCallbacks() {
  // Iterate backwards
  for (auto it = chainblocks::Globals::ExitHooks.begin();
       it != chainblocks::Globals::ExitHooks.end(); ++it) {
    it->second();
  }
}

CBVar *referenceChainVariable(CBChainRef chain, const char *name) {
  auto schain = CBChain::sharedFromRef(chain);
  CBVar &v = schain->variables[name];
  v.refcount++;
  v.flags |= CBVAR_FLAGS_REF_COUNTED;
  return &v;
}

CBVar *referenceGlobalVariable(CBContext *ctx, const char *name) {
  auto node = ctx->main->node.lock();
  assert(node);
  CBVar &v = node->variables[name];
  v.refcount++;
  if (v.refcount == 1) {
    LOG(TRACE) << "Creating a global variable, chain: "
               << ctx->chainStack.back()->name << " name: " << name;
  }
  v.flags |= CBVAR_FLAGS_REF_COUNTED;
  return &v;
}

CBVar *referenceVariable(CBContext *ctx, const char *name) {
  auto node = ctx->main->node.lock();
  assert(node);

  // try find a chain variable
  // from top to bottom of chain stack
  auto rit = ctx->chainStack.rbegin();
  for (; rit != ctx->chainStack.rend(); ++rit) {
    auto it = (*rit)->variables.find(name);
    if (it != (*rit)->variables.end()) {
      // found, lets get out here
      CBVar &cv = it->second;
      cv.refcount++;
      cv.flags |= CBVAR_FLAGS_REF_COUNTED;
      return &cv;
    }
  }

  // Was not in chains.. find in global node,
  // if fails create on top chain
  auto it = node->variables.find(name);
  if (it != node->variables.end()) {
    // found, lets get out here
    CBVar &cv = it->second;
    cv.refcount++;
    cv.flags |= CBVAR_FLAGS_REF_COUNTED;
    return &cv;
  }

  // worst case create in current top chain!
  LOG(TRACE) << "Creating a variable, chain: " << ctx->chainStack.back()->name
             << " name: " << name;
  CBVar &cv = ctx->chainStack.back()->variables[name];
  cv.refcount++;
  cv.flags |= CBVAR_FLAGS_REF_COUNTED;
  return &cv;
}

void releaseVariable(CBVar *variable) {
  if (!variable)
    return;

  assert((variable->flags & CBVAR_FLAGS_REF_COUNTED) ==
         CBVAR_FLAGS_REF_COUNTED);
  assert(variable->refcount > 0);

  variable->refcount--;
  if (variable->refcount == 0) {
    LOG(TRACE) << "Destroying a variable (0 ref count), type: "
               << type2Name(variable->valueType);
    destroyVar(*variable);
  }
}

CBChainState suspend(CBContext *context, double seconds) {
  if (unlikely(!context->shouldContinue() || context->onCleanup)) {
    throw ActivationError("Trying to suspend a terminated context!");
  } else if (unlikely(!context->continuation)) {
    throw ActivationError("Trying to suspend a context without coroutine!");
  }

  if (seconds <= 0) {
    context->next = CBDuration(0);
  } else {
    context->next = CBClock::now().time_since_epoch() + CBDuration(seconds);
  }

#ifdef CB_USE_TSAN
  auto curr = __tsan_get_current_fiber();
  __tsan_switch_to_fiber(context->tsan_handle, 0);
#endif

#ifndef __EMSCRIPTEN__
  context->continuation = context->continuation.resume();
#else
  context->continuation->yield();
#endif

#ifdef CB_USE_TSAN
  __tsan_switch_to_fiber(curr, 0);
#endif

  return context->getState();
}

template <typename T>
ALWAYS_INLINE CBChainState blocksActivation(T blocks, CBContext *context,
                                            const CBVar &chainInput,
                                            CBVar &output,
                                            const bool handlesReturn) {
  auto input = chainInput;
  size_t len;
  if constexpr (std::is_same<T, CBlocks>::value ||
                std::is_same<T, CBSeq>::value) {
    len = blocks.len;
  } else if constexpr (std::is_same<T, std::vector<CBlockPtr>>::value) {
    len = blocks.size();
  } else {
    len = 0;
    LOG(FATAL) << "Unreachable blocksActivation case";
  }
  for (size_t i = 0; i < len; i++) {
    CBlockPtr blk;
    if constexpr (std::is_same<T, CBlocks>::value) {
      blk = blocks.elements[i];
    } else if constexpr (std::is_same<T, CBSeq>::value) {
      blk = blocks.elements[i].payload.blockValue;
    } else if constexpr (std::is_same<T, std::vector<CBlockPtr>>::value) {
      blk = blocks[i];
    } else {
      blk = nullptr;
      LOG(FATAL) << "Unreachable blocksActivation case";
    }
    try {
      output = activateBlock(blk, context, input);
      if (unlikely(!context->shouldContinue())) {
        switch (context->getState()) {
        case CBChainState::Return:
          if (handlesReturn)
            context->continueFlow();
          return CBChainState::Return;
        case CBChainState::Stop:
          if (context->failed()) {
            throw ActivationError(context->getErrorMessage());
          }
        case CBChainState::Restart:
          return context->getState();
        case CBChainState::Rebase:
          // reset input to chain one
          // and reset state
          input = chainInput;
          context->continueFlow();
          continue;
        case CBChainState::Continue:
          break;
        }
      }
    } catch (const std::exception &e) {
      LOG(ERROR) << "Block activation error, failed block: "
                 << std::string(blk->name(blk));
      LOG(ERROR) << e.what();
      // failure from exceptions need uptdate on context
      if (!context->failed()) {
        context->cancelFlow(e.what());
      }
      throw; // bubble up
    } catch (...) {
      LOG(ERROR) << "Block activation error (...), failed block: "
                 << std::string(blk->name(blk));
      if (!context->failed()) {
        context->cancelFlow("foreign exception failure, check logs");
      }
      throw; // bubble up
    }
    input = output;
  }
  return CBChainState::Continue;
}

CBChainState activateBlocks(CBlocks blocks, CBContext *context,
                            const CBVar &chainInput, CBVar &output,
                            const bool handlesReturn) {
  return blocksActivation(blocks, context, chainInput, output, handlesReturn);
}

CBChainState activateBlocks(CBSeq blocks, CBContext *context,
                            const CBVar &chainInput, CBVar &output,
                            const bool handlesReturn) {
  return blocksActivation(blocks, context, chainInput, output, handlesReturn);
}

// Lazy and also avoid windows Loader (Dead)Lock
// https://docs.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices?redirectedfrom=MSDN
Shared<boost::asio::thread_pool> SharedThreadPool{};

CBVar awaitne(CBContext *context, std::function<CBVar()> func) noexcept {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  return func();
#else
  std::exception_ptr exp = nullptr;
  CBVar res{};
  std::atomic_bool complete = false;

  boost::asio::dispatch(chainblocks::SharedThreadPool(), [&]() {
    try {
      res = func();
    } catch (...) {
      exp = std::current_exception();
    }
    complete = true;
  });

  while (!complete && context->shouldContinue()) {
    if (chainblocks::suspend(context, 0) != CBChainState::Continue)
      break;
  }

  // TODO figure out cancellations inside parallel tasks...
  while (!complete) {
    std::this_thread::yield();
  }

  if (exp) {
    try {
      std::rethrow_exception(exp);
    } catch (const std::exception &e) {
      context->cancelFlow(e.what());
    } catch (...) {
      context->cancelFlow("foreign exception failure");
    }
  }

  return res;
#endif
}

void await(CBContext *context, std::function<void()> func) {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  func();
#else
  std::exception_ptr exp = nullptr;
  std::atomic_bool complete = false;

  boost::asio::dispatch(chainblocks::SharedThreadPool(), [&]() {
    try {
      func();
    } catch (...) {
      exp = std::current_exception();
    }
    complete = true;
  });

  while (!complete && context->shouldContinue()) {
    if (chainblocks::suspend(context, 0) != CBChainState::Continue)
      break;
  }

  // TODO figure out cancellations inside parallel tasks...
  while (!complete) {
    std::this_thread::yield();
  }

  if (exp) {
    std::rethrow_exception(exp);
  }
#endif
}

bool matchTypes(const CBTypeInfo &inputType, const CBTypeInfo &receiverType,
                bool isParameter, bool strict) {
  if (receiverType.basicType == CBType::Any)
    return true;

  if (inputType.basicType != receiverType.basicType) {
    // Fail if basic type differs
    return false;
  }

  switch (inputType.basicType) {
  case Object: {
    if (inputType.object.vendorId != receiverType.object.vendorId ||
        inputType.object.typeId != receiverType.object.typeId) {
      return false;
    }
    break;
  }
  case Enum: {
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
            if (receiverType.seqTypes.elements[j].basicType == CBType::Any ||
                inputType.seqTypes.elements[i] ==
                    receiverType.seqTypes.elements[j])
              goto matched;
          }
          return false;
        matched:
          continue;
        }
      } else if (inputType.seqTypes.len == 0 && receiverType.seqTypes.len > 0) {
        // find Any
        for (uint32_t j = 0; j < receiverType.seqTypes.len; j++) {
          if (receiverType.seqTypes.elements[j].basicType == CBType::Any)
            return true;
        }
        return false;
      } else if (inputType.seqTypes.len == 0 ||
                 receiverType.seqTypes.len == 0) {
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
      if (receiverType.table.keys.len == 0) {
        // case 1, consumer is not strict, match types if avail
        // ignore input keys information
        if (atypes == 0) {
          // assume this as an Any
          auto matched = false;
          CBTypeInfo anyType{CBType::Any};
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
        const auto akeys = inputType.table.keys.len;
        const auto bkeys = receiverType.table.keys.len;
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
  std::unordered_map<std::string, std::unordered_set<CBExposedTypeInfo>>
      exposed;
  std::unordered_set<std::string> variables;
  std::unordered_set<std::string> references;
  std::unordered_set<CBExposedTypeInfo> required;

  CBTypeInfo previousOutputType{};
  CBTypeInfo originalInputType{};

  CBlock *bottom{};
  CBlock *next{};
  CBChain *chain{};

  CBValidationCallback cb{};
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
    ss << "Could not find a matching input type, block: "
       << ctx.bottom->name(ctx.bottom) << " expected: " << inputInfos
       << " found instead: " << ctx.previousOutputType;
    const auto sss = ss.str();
    ctx.cb(ctx.bottom, sss.c_str(), false, ctx.userData);
  }

  // infer and specialize types if we need to
  // If we don't we assume our output will be of the same type of the previous!
  if (ctx.bottom->compose) {
    CBInstanceData data{};
    data.block = ctx.bottom;
    data.chain = ctx.chain;
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
    // notice that blocks might add new records to this array
    for (auto &info : ctx.exposed) {
      for (auto &type : info.second) {
        chainblocks::arrayPush(data.shared, type);
      }
    }
    DEFER(chainblocks::arrayFree(data.shared));

    // this ensures e.g. SetVariable exposedVars have right type from the actual
    // input type (previousOutput)!
    ctx.previousOutputType = ctx.bottom->compose(ctx.bottom, data);
#ifndef NDEBUG
    // do some sanity checks that also provide coverage on outputTypes
    auto outputTypes = ctx.bottom->outputTypes(ctx.bottom);
    chainblocks::IterableTypesInfo otypes(outputTypes);
    auto flowStopper = [&]() {
      if (strcmp(ctx.bottom->name(ctx.bottom), "Restart") == 0 ||
          strcmp(ctx.bottom->name(ctx.bottom), "Stop") == 0 ||
          strcmp(ctx.bottom->name(ctx.bottom), "Return") == 0 ||
          strcmp(ctx.bottom->name(ctx.bottom), "Fail") == 0) {
        return true;
      } else {
        return false;
      }
    }();
    auto valid_block_outputTypes =
        flowStopper ||
        std::any_of(otypes.begin(), otypes.end(), [&](const auto &t) {
          return t.basicType == CBType::Any ||
                 (t.basicType == Seq && t.seqTypes.len == 1 &&
                  t.seqTypes.elements[0].basicType == CBType::Any &&
                  ctx.previousOutputType.basicType == Seq) || // any seq
                 (t.basicType == Seq && t.seqTypes.len == 1 &&
                  t.seqTypes.elements[0].basicType == CBType::Any &&
                  ctx.previousOutputType.basicType == Table) || // any table
                 t == ctx.previousOutputType;
        });
    assert(valid_block_outputTypes);
#endif

    if (externalCtx.externalFailure) {
      if (externalCtx.warning) {
        externalCtx.externalError.insert(0, "Chain compose warning: ");
        ctx.cb(ctx.bottom, externalCtx.externalError.c_str(), true,
               ctx.userData);
      } else {
        externalCtx.externalError.insert(0,
                                         "Chain compose failed with error: ");
        ctx.cb(ctx.bottom, externalCtx.externalError.c_str(), false,
               ctx.userData);
      }
    }
  } else {
    // Short-cut if it's just one type and not any type
    auto outputTypes = ctx.bottom->outputTypes(ctx.bottom);
    if (outputTypes.len == 1) {
      if (outputTypes.elements[0].basicType != CBType::Any) {
        ctx.previousOutputType = outputTypes.elements[0];
      } else {
        // Any type tho means keep previous output type!
        // Unless we require a specific input type, in that case
        // We assume we are not a passthru block
        auto inputTypes = ctx.bottom->inputTypes(ctx.bottom);
        if (inputTypes.len == 1 &&
            inputTypes.elements[0].basicType != CBType::Any) {
          ctx.previousOutputType = outputTypes.elements[0];
        }
      }
    }
  }

  // Grab those after type inference!
  auto exposedVars = ctx.bottom->exposedVariables(ctx.bottom);
  // Add the vars we expose
  for (uint32_t i = 0; exposedVars.len > i; i++) {
    auto exposed_param = exposedVars.elements[i];
    std::string name(exposed_param.name);
    exposed_param.scope = exposed_param.global ? nullptr : ctx.chain;
    ctx.exposed[name].emplace(exposed_param);

    // Reference mutability checks
    if (strcmp(ctx.bottom->name(ctx.bottom), "Ref") == 0) {
      // make sure we are not Ref-ing a Set
      // meaning target would be overwritten, yet Set will try to deallocate
      // it/manage it
      if (ctx.variables.count(name)) {
        // Error
        std::string err(
            "Ref variable name already used as Set. Overwriting a previously "
            "Set variable with Ref is not allowed, name: " +
            name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
      ctx.references.insert(name);
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Set") == 0) {
      // make sure we are not Set-ing a Ref
      // meaning target memory could be any block temporary buffer, yet Set will
      // try to deallocate it/manage it
      if (ctx.references.count(name)) {
        // Error
        std::string err(
            "Set variable name already used as Ref. Overwriting a previously "
            "Ref variable with Set is not allowed, name: " +
            name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
      ctx.variables.insert(name);
    } else if (strcmp(ctx.bottom->name(ctx.bottom), "Update") == 0) {
      // make sure we are not Set-ing a Ref
      // meaning target memory could be any block temporary buffer, yet Set will
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
      // meaning target memory could be any block temporary buffer, yet Push
      // will try to deallocate it/manage it
      if (ctx.references.count(name)) {
        // Error
        std::string err(
            "Push variable name already used as Ref. Overwriting a previously "
            "Ref variable with Push is not allowed, name: " +
            name);
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
      ctx.variables.insert(name);
    }
  }

  // Take selector checks
  // TODO move into Take compose, we know have block variable
  if (strcmp(ctx.bottom->name(ctx.bottom), "Take") == 0) {
    if (previousOutput.basicType == Seq) {
      ctx.bottom->inlineBlockId = CBInlineBlocks::CoreTakeSeq;
    } else if (previousOutput.basicType == Table) {
      ctx.bottom->inlineBlockId = CBInlineBlocks::CoreTakeTable;
    } else if (previousOutput.basicType >= Int2 &&
               previousOutput.basicType <= Int16) {
      ctx.bottom->inlineBlockId = CBInlineBlocks::CoreTakeInts;
    } else if (previousOutput.basicType >= Float2 &&
               previousOutput.basicType <= Float4) {
      ctx.bottom->inlineBlockId = CBInlineBlocks::CoreTakeFloats;
    } else if (previousOutput.basicType == Color) {
      ctx.bottom->inlineBlockId = CBInlineBlocks::CoreTakeColor;
    } else if (previousOutput.basicType == Bytes) {
      ctx.bottom->inlineBlockId = CBInlineBlocks::CoreTakeBytes;
    } else if (previousOutput.basicType == String) {
      ctx.bottom->inlineBlockId = CBInlineBlocks::CoreTakeString;
    }
  }

  // Finally do checks on what we consume
  auto requiredVar = ctx.bottom->requiredVariables(ctx.bottom);

  std::unordered_map<std::string, std::vector<CBExposedTypeInfo>> requiredVars;
  for (uint32_t i = 0; requiredVar.len > i; i++) {
    auto &required_param = requiredVar.elements[i];
    std::string name(required_param.name);
    requiredVars[name].push_back(required_param);
  }

  // make sure we have the vars we need, collect first
  for (const auto &required : requiredVars) {
    auto matching = false;
    CBExposedTypeInfo match{};

    for (const auto &required_param : required.second) {
      std::string name(required_param.name);
      if (name.find(' ') !=
          std::string::npos) { // take only the first part of variable name
        // the remaining should be a table key which we don't care here
        name = name.substr(0, name.find(' '));
      }
      auto findIt = ctx.exposed.find(name);
      if (findIt == ctx.exposed.end()) {
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
      ss << "Required types do not match currently exposed ones for variable '"
         << required.first << "' required possible types: ";
      for (auto type : required.second) {
        ss << "{\"" << type.name << "\" (" << type.exposedType << ")} ";
      }
      ss << "exposed types: ";
      for (const auto &info : ctx.exposed) {
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

CBComposeResult composeChain(const std::vector<CBlock *> &chain,
                             CBValidationCallback callback, void *userData,
                             CBInstanceData data) {
  ValidationContext ctx{};
  ctx.originalInputType = data.inputType;
  ctx.previousOutputType = data.inputType;
  ctx.cb = callback;
  ctx.chain = data.chain;
  ctx.userData = userData;
  ctx.onWorkerThread = data.onWorkerThread;

  if (data.shared.elements) {
    for (uint32_t i = 0; i < data.shared.len; i++) {
      auto &info = data.shared.elements[i];
      ctx.exposed[info.name].insert(info);
    }
  }

  size_t chsize = chain.size();
  for (size_t i = 0; i < chsize; i++) {
    CBlock *blk = chain[i];
    ctx.next = nullptr;
    if (i < chsize - 1)
      ctx.next = chain[i + 1];

    if (strcmp(blk->name(blk), "Input") == 0) {
      // Hard code behavior for Input block and And and Or, in order to validate
      // with actual chain input the followup
      ctx.previousOutputType = ctx.chain->inputType;
    } else if (strcmp(blk->name(blk), "And") == 0 ||
               strcmp(blk->name(blk), "Or") == 0) {
      // Hard code behavior for Input block and And and Or, in order to validate
      // with actual chain input the followup
      ctx.previousOutputType = ctx.originalInputType;
    } else {
      ctx.bottom = blk;
      validateConnection(ctx);
    }
  }

  CBComposeResult result = {ctx.previousOutputType};

  for (auto &exposed : ctx.exposed) {
    for (auto &type : exposed.second) {
      chainblocks::arrayPush(result.exposedInfo, type);
    }
  }

  for (auto &req : ctx.required) {
    chainblocks::arrayPush(result.requiredInfo, req);
  }

  if (chain.size() > 0) {
    auto &last = chain.back();
    if (strcmp(last->name(last), "Restart") == 0 ||
        strcmp(last->name(last), "Stop") == 0 ||
        strcmp(last->name(last), "Return") == 0 ||
        strcmp(last->name(last), "Fail") == 0) {
      result.flowStopper = true;
    }
  }

  return result;
}

CBComposeResult composeChain(const CBChain *chain,
                             CBValidationCallback callback, void *userData,
                             CBInstanceData data) {
  // settle input type of chain before compose
  if (chain->blocks.size() > 0 &&
      !std::any_of(chain->blocks.begin(), chain->blocks.end(),
                   [&](const auto &block) {
                     return strcmp(block->name(block), "Input") == 0;
                   })) {
    // If first block is a plain None, mark this chain has None input
    // But make sure we have no (Input) blocks
    auto inTypes = chain->blocks[0]->inputTypes(chain->blocks[0]);
    if (inTypes.len == 1 && inTypes.elements[0].basicType == None)
      chain->inputType = CBTypeInfo{};
    else
      chain->inputType = data.inputType;
  } else {
    chain->inputType = data.inputType;
  }

  auto res = composeChain(chain->blocks, callback, userData, data);

  // set outputtype
  chain->outputType = res.outputType;

  std::vector<chainblocks::CBlockInfo> allBlocks;
  chainblocks::gatherBlocks(chain, allBlocks);
  // call composed on all blocks if they have it
  for (auto &blk : allBlocks) {
    if (blk.block->composed)
      blk.block->composed(const_cast<CBlock *>(blk.block), chain, &res);
  }

  // add variables
  chainblocks::IterableExposedInfo reqs(res.requiredInfo);
  for (auto req : reqs) {
    chain->requiredVariables.emplace_back(req.name);
  }

  return res;
}

CBComposeResult composeChain(const CBlocks chain, CBValidationCallback callback,
                             void *userData, CBInstanceData data) {
  std::vector<CBlock *> blocks;
  for (uint32_t i = 0; chain.len > i; i++) {
    blocks.push_back(chain.elements[i]);
  }
  return composeChain(blocks, callback, userData, data);
}

CBComposeResult composeChain(const CBSeq chain, CBValidationCallback callback,
                             void *userData, CBInstanceData data) {
  std::vector<CBlock *> blocks;
  for (uint32_t i = 0; chain.len > i; i++) {
    blocks.push_back(chain.elements[i].payload.blockValue);
  }
  return composeChain(blocks, callback, userData, data);
}

void freeDerivedInfo(CBTypeInfo info) {
  switch (info.basicType) {
  case Seq: {
    for (uint32_t i = 0; info.seqTypes.len > i; i++) {
      freeDerivedInfo(info.seqTypes.elements[i]);
    }
    chainblocks::arrayFree(info.seqTypes);
  }
  case Table: {
    for (uint32_t i = 0; info.table.types.len > i; i++) {
      freeDerivedInfo(info.table.types.elements[i]);
    }
    chainblocks::arrayFree(info.table.types);
    chainblocks::arrayFree(info.table.keys);
  }
  default:
    break;
  };
}

CBTypeInfo deriveTypeInfo(const CBVar &value) {
  // We need to guess a valid CBTypeInfo for this var in order to validate
  // Build a CBTypeInfo for the var
  // this is not complete at all, missing Array and ContextVar for example
  CBTypeInfo varType{};
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
    std::unordered_set<CBTypeInfo> types;
    for (uint32_t i = 0; i < value.payload.seqValue.len; i++) {
      auto derived = deriveTypeInfo(value.payload.seqValue.elements[i]);
      if (!types.count(derived)) {
        chainblocks::arrayPush(varType.seqTypes, derived);
        types.insert(derived);
      }
    }
    varType.fixedSize = value.payload.seqValue.len;
    break;
  }
  case Table: {
    auto &ta = value.payload.tableValue;
    struct iterdata {
      CBTypeInfo *varType;
    } data;
    data.varType = &varType;
    ta.api->tableForEach(
        ta,
        [](const char *key, CBVar *value, void *_data) {
          auto data = (iterdata *)_data;
          auto derived = deriveTypeInfo(*value);
          chainblocks::arrayPush(data->varType->table.types, derived);
          chainblocks::arrayPush(data->varType->table.keys, key);
          return true;
        },
        &data);
    break;
  }
  default:
    break;
  };
  return varType;
}

CBTypeInfo cloneTypeInfo(const CBTypeInfo &other) {
  // We need to guess a valid CBTypeInfo for this var in order to validate
  // Build a CBTypeInfo for the var
  // this is not complete at all, missing Array and ContextVar for example
  CBTypeInfo varType;
  memcpy(&varType, &other, sizeof(CBTypeInfo));
  switch (varType.basicType) {
  case Seq: {
    varType.seqTypes = {};
    for (uint32_t i = 0; i < other.seqTypes.len; i++) {
      auto cloned = cloneTypeInfo(other.seqTypes.elements[i]);
      chainblocks::arrayPush(varType.seqTypes, cloned);
    }
    break;
  }
  case Table: {
    varType.table = {};
    for (uint32_t i = 0; i < other.table.types.len; i++) {
      auto cloned = cloneTypeInfo(other.table.types.elements[i]);
      chainblocks::arrayPush(varType.table.types, cloned);
    }
    for (uint32_t i = 0; i < other.table.keys.len; i++) {
      chainblocks::arrayPush(varType.table.keys, other.table.keys.elements[i]);
    }
    break;
  }
  default:
    break;
  };
  return varType;
} // namespace chainblocks

// this is potentially called from unsafe code (e.g. networking)
// let's do some crude stack protection here
static thread_local int deriveTypeHashRecursionCounter;
static constexpr int MAX_DERIVED_TYPE_HASH_RECURSION = 100;

uint64_t _deriveTypeHash(const CBVar &value);

void updateTypeHash(const CBVar &var, XXH3_state_s *state) {
  // this is not complete at all, missing Array and ContextVar for example
  XXH3_64bits_update(state, &var.valueType, sizeof(var.valueType));

  switch (var.valueType) {
  case Object: {
    XXH3_64bits_update(state, &var.payload.objectVendorId,
                       sizeof(var.payload.objectVendorId));
    XXH3_64bits_update(state, &var.payload.objectTypeId,
                       sizeof(var.payload.objectTypeId));
    break;
  }
  case Enum: {
    XXH3_64bits_update(state, &var.payload.enumVendorId,
                       sizeof(var.payload.enumVendorId));
    XXH3_64bits_update(state, &var.payload.enumTypeId,
                       sizeof(var.payload.enumTypeId));
    break;
  }
  case Seq: {
    std::unordered_set<uint64_t, std::hash<uint64_t>, std::equal_to<uint64_t>,
                       stack_allocator<uint64_t>>
        types;
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      // first derive the hash of the full type, mix only once per type
      auto typeHash = _deriveTypeHash(var.payload.seqValue.elements[i]);
      if (!types.count(typeHash)) {
        XXH3_64bits_update(state, &typeHash, sizeof(typeHash));
        types.insert(typeHash);
      }
    }
    break;
  }
  case Table: {
    auto &ta = var.payload.tableValue;
    struct iterdata {
      XXH3_state_s *state;
    } data;
    data.state = state;
    ta.api->tableForEach(
        ta,
        [](const char *key, CBVar *value, void *_data) {
          auto data = (iterdata *)_data;
          auto typeHash = _deriveTypeHash(*value);
          XXH3_64bits_update(data->state, &typeHash, sizeof(typeHash));
          XXH3_64bits_update(data->state, key, strlen(key));
          return true;
        },
        &data);
    break;
  }
  default:
    break;
  };
}

uint64_t _deriveTypeHash(const CBVar &value) {
  if (deriveTypeHashRecursionCounter >= MAX_DERIVED_TYPE_HASH_RECURSION)
    throw CBException("deriveTypeHash maximum recursion exceeded");
  deriveTypeHashRecursionCounter++;
  DEFER(deriveTypeHashRecursionCounter--);

  XXH3_state_s hashState;
  XXH3_INITSTATE(&hashState);

  XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret,
                               XXH_SECRET_DEFAULT_SIZE);

  updateTypeHash(value, &hashState);

  return XXH3_64bits_digest(&hashState);
}

uint64_t deriveTypeHash(const CBVar &value) {
  deriveTypeHashRecursionCounter = 0;
  return _deriveTypeHash(value);
}

uint64_t deriveTypeHash(const CBTypeInfo &value);

void updateTypeHash(const CBTypeInfo &t, XXH3_state_s *state) {
  XXH3_64bits_update(state, &t.basicType, sizeof(t.basicType));

  switch (t.basicType) {
  case Object: {
    XXH3_64bits_update(state, &t.object.vendorId, sizeof(t.object.vendorId));
    XXH3_64bits_update(state, &t.object.typeId, sizeof(t.object.typeId));
    break;
  }
  case Enum: {
    XXH3_64bits_update(state, &t.enumeration.vendorId,
                       sizeof(t.enumeration.vendorId));
    XXH3_64bits_update(state, &t.enumeration.typeId,
                       sizeof(t.enumeration.typeId));
    break;
  }
  case Seq: {
    // this is unsafe because allocates on the stack, but faster...
    std::unordered_set<uint64_t, std::hash<uint64_t>, std::equal_to<uint64_t>,
                       stack_allocator<uint64_t>>
        types;
    for (uint32_t i = 0; i < t.seqTypes.len; i++) {
      if (t.seqTypes.elements[i].recursiveSelf) {
        // just mix a integer to flag recursive self
        uint32_t selfRecursive = UINT32_MAX;
        XXH3_64bits_update(state, &selfRecursive, sizeof(uint32_t));
      } else {
        // first derive the hash of the full type, mix only once per type
        auto typeHash = deriveTypeHash(t.seqTypes.elements[i]);
        if (!types.count(typeHash)) {
          XXH3_64bits_update(state, &typeHash, sizeof(typeHash));
          types.insert(typeHash);
        }
      }
    }
    break;
  }
  case Table: {
    if (t.table.types.len != t.table.keys.len)
      throw CBException("updateTypeHash for tables needs both types and keys "
                        "populated, the other cases are undefined for now");
    for (uint32_t i = 0; i < t.table.types.len; i++) {
      auto typeHash = deriveTypeHash(t.table.types.elements[i]);
      XXH3_64bits_update(state, &typeHash, sizeof(typeHash));
      const char *key = t.table.keys.elements[i];
      XXH3_64bits_update(state, key, strlen(key));
    }
    break;
  }
  default:
    break;
  };
}

uint64_t deriveTypeHash(const CBTypeInfo &value) {
  XXH3_state_s hashState;
  XXH3_INITSTATE(&hashState);

  XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret,
                               XXH_SECRET_DEFAULT_SIZE);

  updateTypeHash(value, &hashState);

  return XXH3_64bits_digest(&hashState);
}

bool validateSetParam(CBlock *block, int index, const CBVar &value,
                      CBValidationCallback callback, void *userData) {
  auto params = block->parameters(block);
  if (params.len <= (uint32_t)index) {
    std::string err("Parameter index out of range");
    callback(block, err.c_str(), false, userData);
    return false;
  }

  auto param = params.elements[index];

  // Build a CBTypeInfo for the var
  auto varType = deriveTypeInfo(value);
  DEFER({ freeDerivedInfo(varType); });

  for (uint32_t i = 0; param.valueTypes.len > i; i++) {
    if (matchTypes(varType, param.valueTypes.elements[i], true, true)) {
      return true; // we are good just exit
    }
  }

  std::string err("Parameter not accepting this kind of variable (" +
                  type2Name(value.valueType) + ")");
  callback(block, err.c_str(), false, userData);

  return false;
}

void error_handler(int err_sig) {
  std::signal(err_sig, SIG_DFL);

  auto printTrace = false;

  switch (err_sig) {
  case SIGINT:
  case SIGTERM:
    LOG(INFO) << "Exiting due to INT/TERM signal";
    chainblocks::Globals::SigIntTerm++;
    if (chainblocks::Globals::SigIntTerm > 5)
      std::exit(-1);
    break;
  case SIGFPE:
    LOG(ERROR) << "Fatal SIGFPE";
    printTrace = true;
    break;
  case SIGILL:
    LOG(ERROR) << "Fatal SIGILL";
    printTrace = true;
    break;
  case SIGABRT:
    LOG(ERROR) << "Fatal SIGABRT";
    printTrace = true;
    break;
  case SIGSEGV:
    LOG(ERROR) << "Fatal SIGSEGV";
    printTrace = true;
    break;
  }

  if (printTrace) {
#ifndef __EMSCRIPTEN__
    LOG(ERROR) << boost::stacktrace::stacktrace();
#endif
  }

  std::raise(err_sig);
}

void installSignalHandlers() {
  std::signal(SIGINT, &error_handler);
  std::signal(SIGTERM, &error_handler);
  std::signal(SIGFPE, &error_handler);
  std::signal(SIGILL, &error_handler);
  std::signal(SIGABRT, &error_handler);
  std::signal(SIGSEGV, &error_handler);
}

Blocks &Blocks::block(std::string_view name, std::vector<Var> params) {
  auto blk = createBlock(name.data());
  if (!blk) {
    LOG(ERROR) << "The block " << name << " was not found.";
    throw CBException("Block not found");
  }

  blk->setup(blk);

  const auto psize = params.size();
  for (size_t i = 0; i < psize; i++) {
    // skip Any, as they mean default value
    if (params[i] != Var::Any)
      blk->setParam(blk, int(i), &params[i]);
  }

  _blocks.emplace_back(blk);
  return *this;
}

Blocks &Blocks::let(Var value) {
  auto blk = createBlock("Const");
  blk->setup(blk);
  blk->setParam(blk, 0, &value);
  _blocks.emplace_back(blk);
  return *this;
}

Chain::Chain(std::string_view name) : _chain(CBChain::make(name)) {}

Chain &Chain::looped(bool looped) {
  _chain->looped = looped;
  return *this;
}

Chain &Chain::unsafe(bool unsafe) {
  _chain->unsafe = unsafe;
  return *this;
}

Chain &Chain::name(std::string_view name) {
  _chain->name = name;
  return *this;
}

Chain &Chain::block(std::string_view name, std::vector<Var> params) {
  auto blk = createBlock(name.data());
  if (!blk) {
    LOG(ERROR) << "The block " << name << " was not found.";
    throw CBException("Block not found");
  }

  blk->setup(blk);

  const auto psize = params.size();
  for (size_t i = 0; i < psize; i++) {
    // skip Any, as they mean default value
    if (params[i] != Var::Any)
      blk->setParam(blk, int(i), &params[i]);
  }

  _chain->addBlock(blk);
  return *this;
}

Chain &Chain::let(Var value) {
  auto blk = createBlock("Const");
  blk->setup(blk);
  blk->setParam(blk, 0, &value);
  _chain->addBlock(blk);
  return *this;
}

CBChainRef Chain::weakRef() const { return CBChain::weakRef(_chain); }

Var::Var(const Chain &chain) : Var(chain.weakRef()) {}

CBRunChainOutput runChain(CBChain *chain, CBContext *context,
                          const CBVar &chainInput) {
  memset(&chain->previousOutput, 0x0, sizeof(CBVar));
  chain->currentInput = chainInput;
  chain->state = CBChain::State::Iterating;
  chain->context = context;
  DEFER({ chain->state = CBChain::State::IterationEnded; });

  try {
    auto state = blocksActivation(chain->blocks, context, chainInput,
                                  chain->previousOutput, false);
    switch (state) {
    case CBChainState::Return:
    case CBChainState::Restart:
      return {context->getFlowStorage(), Restarted};
    case CBChainState::Stop:
      // On failure blocksActivation throws!
      assert(!context->failed());
      return {context->getFlowStorage(), Stopped};
    case CBChainState::Rebase:
      // Handled inside blocksActivation
      assert(false);
    case CBChainState::Continue:
      break;
    }
  }
#ifndef __EMSCRIPTEN__
  catch (boost::context::detail::forced_unwind const &e) {
    throw; // required for Boost Coroutine!
  }
#endif
  catch (...) {
    // blocksActivation handles error logging and such
    return {chain->previousOutput, Failed};
  }

  return {chain->previousOutput, Running};
}

#ifndef __EMSCRIPTEN__
boost::context::continuation run(CBChain *chain, CBFlow *flow,
                                 boost::context::continuation &&sink)
#else
void run(CBChain *chain, CBFlow *flow, CBCoro *coro)
#endif
{
  LOG(TRACE) << "chain " << chain->name << " rolling.";

  auto running = true;

  // Reset state
  chain->state = CBChain::State::Prepared;
  chain->finishedOutput = Var::Empty;
  chain->finishedError.clear();

  // Create a new context and copy the sink in
  CBFlow anonFlow{chain};
#ifndef __EMSCRIPTEN__
  CBContext context(std::move(sink), chain, flow ? flow : &anonFlow);
#else
  CBContext context(coro, chain, flow ? flow : &anonFlow);
#endif

#ifdef CB_USE_TSAN
  context.tsan_handle = chain->tsan_coro;
#endif
  // also pupulate context in chain
  chain->context = &context;

  // We prerolled our coro, suspend here before actually starting.
  // This allows us to allocate the stack ahead of time.
  // And call warmup on all the blocks!
  try {
    chain->warmup(&context);
  } catch (...) {
    // inside warmup we re-throw, we handle logging and such there
    chain->state = CBChain::State::Failed;
    LOG(ERROR) << "chain " << chain->name << " warmup failed.";
    goto endOfChain;
  }

// yield after warming up
#ifndef __EMSCRIPTEN__
  context.continuation = context.continuation.resume();
#else
  context.continuation->yield();
#endif

  LOG(TRACE) << "chain " << chain->name << " starting.";

  if (context.shouldStop()) {
    // We might have stopped before even starting!
    LOG(ERROR) << "chain " << chain->name << " stopped before starting.";
    goto endOfChain;
  }

  while (running) {
    running = chain->looped;
    // reset context state
    context.continueFlow();

    auto runRes = runChain(chain, &context, chain->rootTickInput);
    if (unlikely(runRes.state == Failed)) {
      LOG(DEBUG) << "chain " << chain->name << " failed.";
      chain->state = CBChain::State::Failed;
      context.stopFlow(runRes.output);
      break;
    } else if (unlikely(runRes.state == Stopped)) {
      LOG(DEBUG) << "chain " << chain->name << " stopped.";
      context.stopFlow(runRes.output);
      // also replace the previous output with actual output
      // as it's likely coming from flowStorage of context!
      chain->previousOutput = runRes.output;
      break;
    } else if (unlikely(runRes.state == Restarted)) {
      // must clone over rootTickInput!
      // restart overwrites rootTickInput on purpose
      cloneVar(chain->rootTickInput, context.getFlowStorage());
    }

    if (!chain->unsafe && chain->looped) {
      // Ensure no while(true), yield anyway every run
      context.next = CBDuration(0);
#ifndef __EMSCRIPTEN__
      context.continuation = context.continuation.resume();
#else
      context.continuation->yield();
#endif
      // This is delayed upon continuation!!
      if (context.shouldStop()) {
        LOG(DEBUG) << "chain " << chain->name << " aborted on resume.";
        break;
      }
    }
  }

endOfChain:
  chain->finishedOutput = chain->previousOutput;
  if (context.failed())
    chain->finishedError = context.getErrorMessage();

  // run cleanup on all the blocks
  // ensure stop state is set
  context.stopFlow(chain->previousOutput);
  chain->cleanup(true);

  // Need to take care that we might have stopped the chain very early due to
  // errors and the next eventual stop() should avoid resuming
  if (chain->state != CBChain::State::Failed)
    chain->state = CBChain::State::Ended;

  LOG(TRACE) << "chain " << chain->name << " ended.";

#ifndef __EMSCRIPTEN__
  return std::move(context.continuation);
#else
  context.continuation->yield();
#endif
}

template <typename T>
NO_INLINE void arrayGrow(T &arr, size_t addlen, size_t min_cap) {
  // safety check to make sure this is not a borrowed foreign array!
  assert((arr.cap == 0 && arr.elements == nullptr) ||
         (arr.cap > 0 && arr.elements != nullptr));

  size_t min_len = arr.len + addlen;

  // compute the minimum capacity needed
  if (min_len > min_cap)
    min_cap = min_len;

  if (min_cap <= arr.cap)
    return;

  // increase needed capacity to guarantee O(1) amortized
  if (min_cap < 2 * arr.cap)
    min_cap = 2 * arr.cap;

  // TODO investigate realloc
  auto newbuf =
      new (std::align_val_t{16}) uint8_t[sizeof(arr.elements[0]) * min_cap];
  if (arr.elements) {
    memcpy(newbuf, arr.elements, sizeof(arr.elements[0]) * arr.len);
    ::operator delete[](arr.elements, std::align_val_t{16});
  }
  arr.elements = (decltype(arr.elements))newbuf;

  // also memset to 0 new memory in order to make cloneVars valid on new items
  size_t size = sizeof(arr.elements[0]) * (min_cap - arr.len);
  memset(arr.elements + arr.len, 0x0, size);

  if (min_cap > UINT32_MAX) {
    // this is the case for now for many reasons, but should be just fine
    LOG(FATAL) << "Int array overflow, we don't support more then UINT32_MAX.";
    abort();
  }
  arr.cap = uint32_t(min_cap);
}

NO_INLINE void _destroyVarSlow(CBVar &var) {
  switch (var.valueType) {
  case Seq: {
    // notice we use .cap! because we make sure to 0 new empty elements
    for (size_t i = var.payload.seqValue.cap; i > 0; i--) {
      destroyVar(var.payload.seqValue.elements[i - 1]);
    }
    chainblocks::arrayFree(var.payload.seqValue);
  } break;
  case Table: {
    assert(var.payload.tableValue.api == &Globals::TableInterface);
    assert(var.payload.tableValue.opaque);
    auto map = (CBMap *)var.payload.tableValue.opaque;
    delete map;
  } break;
  default:
    break;
  };
}

NO_INLINE void _cloneVarSlow(CBVar &dst, const CBVar &src) {
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

    chainblocks::arrayResize(dst.payload.seqValue, srcLen);
    for (uint32_t i = 0; i < srcLen; i++) {
      const auto &subsrc = src.payload.seqValue.elements[i];
      cloneVar(dst.payload.seqValue.elements[i], subsrc);
    }
  } break;
  case Path:
  case ContextVar:
  case String: {
    auto srcSize = src.payload.stringLen > 0
                       ? src.payload.stringLen
                       : uint32_t(strlen(src.payload.stringValue));
    if ((dst.valueType != String && dst.valueType != ContextVar) ||
        dst.payload.stringCapacity < srcSize) {
      destroyVar(dst);
      // allocate a 0 terminator too
      dst.payload.stringValue = new char[srcSize + 1];
      dst.payload.stringCapacity = srcSize;
    } else {
      if (src.payload.stringValue == dst.payload.stringValue)
        return;
    }

    dst.valueType = src.valueType;
    memcpy((void *)dst.payload.stringValue, (void *)src.payload.stringValue,
           srcSize);
    ((char *)dst.payload.stringValue)[srcSize] = 0;
    // fill the optional len field
    dst.payload.stringLen = srcSize;
  } break;
  case Image: {
    auto spixsize = 1;
    auto dpixsize = 1;
    if ((src.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
        CBIMAGE_FLAGS_16BITS_INT)
      spixsize = 2;
    else if ((src.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
             CBIMAGE_FLAGS_32BITS_FLOAT)
      spixsize = 4;
    if ((dst.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
        CBIMAGE_FLAGS_16BITS_INT)
      dpixsize = 2;
    else if ((dst.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
             CBIMAGE_FLAGS_32BITS_FLOAT)
      dpixsize = 4;

    size_t srcImgSize = src.payload.imageValue.height *
                        src.payload.imageValue.width *
                        src.payload.imageValue.channels * spixsize;
    size_t dstCapacity = dst.payload.imageValue.height *
                         dst.payload.imageValue.width *
                         dst.payload.imageValue.channels * dpixsize;
    if (dst.valueType != Image || srcImgSize > dstCapacity) {
      destroyVar(dst);
      dst.valueType = Image;
      dst.payload.imageValue.data = new uint8_t[srcImgSize];
    }

    dst.payload.imageValue.flags = dst.payload.imageValue.flags;
    dst.payload.imageValue.height = src.payload.imageValue.height;
    dst.payload.imageValue.width = src.payload.imageValue.width;
    dst.payload.imageValue.channels = src.payload.imageValue.channels;

    if (src.payload.imageValue.data == dst.payload.imageValue.data)
      return;

    memcpy(dst.payload.imageValue.data, src.payload.imageValue.data,
           srcImgSize);
  } break;
  case Table: {
    CBMap *map;
    if (dst.valueType == Table) {
      if (src.payload.tableValue.opaque == dst.payload.tableValue.opaque)
        return;
      map = (CBMap *)dst.payload.tableValue.opaque;
      map->clear();
    } else {
      destroyVar(dst);
      dst.valueType = Table;
      dst.payload.tableValue.api = &Globals::TableInterface;
      map = new CBMap();
      dst.payload.tableValue.opaque = map;
    }

    auto &ta = src.payload.tableValue;
    ta.api->tableForEach(
        ta,
        [](const char *key, CBVar *value, void *data) {
          auto map = (CBMap *)data;
          (*map)[key] = *value;
          return true;
        },
        map);
  } break;
  case Bytes: {
    if (dst.valueType != Bytes ||
        dst.payload.bytesCapacity < src.payload.bytesSize) {
      destroyVar(dst);
      dst.valueType = Bytes;
      dst.payload.bytesValue = new uint8_t[src.payload.bytesSize];
      dst.payload.bytesCapacity = src.payload.bytesSize;
    }

    dst.payload.bytesSize = src.payload.bytesSize;

    if (src.payload.bytesValue == dst.payload.bytesValue)
      return;

    memcpy((void *)dst.payload.bytesValue, (void *)src.payload.bytesValue,
           src.payload.bytesSize);
  } break;
  case CBType::Array: {
    auto srcLen = src.payload.arrayValue.len;

    // try our best to re-use memory
    if (dst.valueType != Array) {
      destroyVar(dst);
      dst.valueType = Array;
    }

    if (src.payload.arrayValue.elements == dst.payload.arrayValue.elements)
      return;

    dst.innerType = src.innerType;
    chainblocks::arrayResize(dst.payload.arrayValue, srcLen);
    // array holds only blittables and is packed so a single memcpy is enough
    memcpy(&dst.payload.arrayValue.elements[0],
           &src.payload.arrayValue.elements[0], sizeof(CBVarPayload) * srcLen);
  } break;
  case CBType::Chain:
    if (dst != src) {
      destroyVar(dst);
    }
    dst.valueType = CBType::Chain;
    dst.payload.chainValue = CBChain::addRef(src.payload.chainValue);
    break;
  case CBType::Object:
    if (dst != src) {
      destroyVar(dst);
    }

    dst.valueType = CBType::Object;
    dst.payload.objectValue = src.payload.objectValue;
    dst.payload.objectVendorId = src.payload.objectVendorId;
    dst.payload.objectTypeId = src.payload.objectTypeId;

    if ((src.flags & CBVAR_FLAGS_USES_OBJINFO) == CBVAR_FLAGS_USES_OBJINFO &&
        src.objectInfo) {
      // in this case the custom object needs actual destruction
      dst.flags |= CBVAR_FLAGS_USES_OBJINFO;
      dst.objectInfo = src.objectInfo;
      if (src.objectInfo->reference)
        dst.objectInfo->reference(dst.payload.objectValue);
    }
  default:
    break;
  };
}

void gatherBlocks(const BlocksCollection &coll, std::vector<CBlockInfo> &out) {
  // TODO out should be a set?
  switch (coll.index()) {
  case 0: {
    // chain
    auto chain = std::get<const CBChain *>(coll);
    for (auto blk : chain->blocks) {
      gatherBlocks(blk, out);
    }
  } break;
  case 1: {
    // Single block
    auto blk = std::get<CBlockPtr>(coll);
    std::string_view name(blk->name(blk));
    out.emplace_back(name, blk);
    // Also find nested blocks
    const auto params = blk->parameters(blk);
    for (uint32_t i = 0; i < params.len; i++) {
      const auto &param = params.elements[i];
      const auto &types = param.valueTypes;
      for (uint32_t j = 0; j < types.len; j++) {
        const auto &type = types.elements[j];
        if (type.basicType == Block) {
          gatherBlocks(blk->getParam(blk, i), out);
          break;
        } else if (type.basicType == Seq) {
          const auto &stypes = type.seqTypes;
          for (uint32_t k = 0; k < stypes.len; k++) {
            if (stypes.elements[k].basicType == Block) {
              gatherBlocks(blk->getParam(blk, i), out);
              break;
            }
          }
          break;
        }
      }
    }
  } break;
  case 2: {
    // Blocks seq
    auto bs = std::get<CBlocks>(coll);
    for (uint32_t i = 0; i < bs.len; i++) {
      gatherBlocks(bs.elements[i], out);
    }
  } break;
  case 3: {
    // Var
    auto var = std::get<CBVar>(coll);
    if (var.valueType == Block) {
      gatherBlocks(var.payload.blockValue, out);
    } else if (var.valueType == CBType::Chain) {
      // TODO test this, how do we deal with multiple references of a chain??
      auto chain = CBChain::sharedFromRef(var.payload.chainValue);
      gatherBlocks(chain.get(), out);
    } else if (var.valueType == Seq) {
      auto bs = var.payload.seqValue;
      for (uint32_t i = 0; i < bs.len; i++) {
        gatherBlocks(bs.elements[i], out);
      }
    }
  } break;
  default:
    assert(false);
  }
}

static thread_local std::unordered_set<CBChain *> tHashingChains;

void hash_update(const CBVar &var, void *state);

uint64_t hash(const CBVar &var) {
  static_assert(std::is_same<uint64_t, XXH64_hash_t>::value,
                "XXH64_hash_t is not uint64_t");

  tHashingChains.clear();

  XXH3_state_s hashState;
  XXH3_INITSTATE(&hashState);
  XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret,
                               XXH_SECRET_DEFAULT_SIZE);

  hash_update(var, &hashState);

  return XXH3_64bits_digest(&hashState);
}

void hash_update(const CBVar &var, void *state) {
  auto hashState = reinterpret_cast<XXH3_state_s *>(state);

  auto error =
      XXH3_64bits_update(hashState, &var.valueType, sizeof(var.valueType));
  assert(error == XXH_OK);

  switch (var.valueType) {
  case CBType::Bytes: {
    error = XXH3_64bits_update(hashState, var.payload.bytesValue,
                               size_t(var.payload.bytesSize));
    assert(error == XXH_OK);
  } break;
  case CBType::Path:
  case CBType::ContextVar:
  case CBType::String: {
    error = XXH3_64bits_update(hashState, var.payload.stringValue,
                               size_t(var.payload.stringLen > 0
                                          ? var.payload.stringLen
                                          : strlen(var.payload.stringValue)));
    assert(error == XXH_OK);
  } break;
  case CBType::Image: {
    error = XXH3_64bits_update(hashState, &var.payload, sizeof(CBVarPayload));
    assert(error == XXH_OK);
    auto pixsize = 1;
    if ((var.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) ==
        CBIMAGE_FLAGS_16BITS_INT)
      pixsize = 2;
    else if ((var.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) ==
             CBIMAGE_FLAGS_32BITS_FLOAT)
      pixsize = 4;
    error = XXH3_64bits_update(
        hashState, var.payload.imageValue.data,
        size_t(var.payload.imageValue.channels * var.payload.imageValue.width *
               var.payload.imageValue.height * pixsize));
    assert(error == XXH_OK);
  } break;
  case CBType::Seq: {
    for (uint32_t i = 0; i < var.payload.seqValue.len; i++) {
      hash_update(var.payload.seqValue.elements[i], state);
    }
  } break;
  case CBType::Array: {
    for (uint32_t i = 0; i < var.payload.arrayValue.len; i++) {
      CBVar tmp; // only of blittable types and hash uses just type, so no init
                 // needed
      tmp.valueType = var.innerType;
      tmp.payload = var.payload.arrayValue.elements[i];
      hash_update(tmp, state);
    }
  } break;
  case CBType::Table: {
    CBTableIterator it;
    var.payload.tableValue.api->tableGetIterator(var.payload.tableValue, &it);
    CBString key;
    CBVar value;
    while (true) {
      auto valid = var.payload.tableValue.api->tableNext(var.payload.tableValue,
                                                         &it, &key, &value);
      if (!valid)
        break;
      error = XXH3_64bits_update(hashState, key, strlen(key));
      assert(error == XXH_OK);
      hash_update(value, state);
    }
  } break;
  case CBType::Block: {
    auto blk = var.payload.blockValue;
    auto name = blk->name(blk);
    auto error = XXH3_64bits_update(hashState, name, strlen(name));
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
  case CBType::Chain: {
    auto chain = CBChain::sharedFromRef(var.payload.chainValue);
    if (tHashingChains.count(chain.get()) == 0) {
      tHashingChains.insert(chain.get());

      error = XXH3_64bits_update(hashState, chain->name.c_str(),
                                 chain->name.length());
      assert(error == XXH_OK);

      error =
          XXH3_64bits_update(hashState, &chain->looped, sizeof(chain->looped));
      assert(error == XXH_OK);

      error =
          XXH3_64bits_update(hashState, &chain->unsafe, sizeof(chain->unsafe));
      assert(error == XXH_OK);

      for (auto &blk : chain->blocks) {
        CBVar tmp;
        tmp.valueType = CBType::Block;
        tmp.payload.blockValue = blk;
        hash_update(tmp, state);
      }

      for (auto &chainVar : chain->variables) {
        error = XXH3_64bits_update(hashState, chainVar.first.c_str(),
                                   chainVar.first.length());
        assert(error == XXH_OK);
        hash_update(chainVar.second, state);
      }
    }
  } break;
  case CBType::Object: {
    error = XXH3_64bits_update(hashState, &var.payload.objectVendorId,
                               sizeof(var.payload.objectVendorId));
    assert(error == XXH_OK);
    error = XXH3_64bits_update(hashState, &var.payload.objectTypeId,
                               sizeof(var.payload.objectTypeId));
    assert(error == XXH_OK);
    if ((var.flags & CBVAR_FLAGS_USES_OBJINFO) == CBVAR_FLAGS_USES_OBJINFO &&
        var.objectInfo && var.objectInfo->hash) {
      // hash of the hash...
      auto objHash = var.objectInfo->hash(var.payload.objectValue);
      error = XXH3_64bits_update(hashState, &objHash, sizeof(uint64_t));
      assert(error == XXH_OK);
    } else {
      // this will be valid only within this process memory space
      // better then nothing
      error = XXH3_64bits_update(hashState, &var.payload.objectValue,
                                 sizeof(var.payload.objectValue));
      assert(error == XXH_OK);
    }
  } break;
  case CBType::None:
  case CBType::Any:
    break;
  default: {
    error = XXH3_64bits_update(hashState, &var.payload, sizeof(CBVarPayload));
    assert(error == XXH_OK);
  }
  }
}

CBString getString(uint32_t crc) {
  assert(chainblocks::Globals::CompressedStrings);
  auto s = (*chainblocks::Globals::CompressedStrings)[crc].string;
  return s != nullptr ? s : "";
}

void setString(uint32_t crc, CBString str) {
  assert(chainblocks::Globals::CompressedStrings);
  (*chainblocks::Globals::CompressedStrings)[crc].string = str;
  (*chainblocks::Globals::CompressedStrings)[crc].crc = crc;
}
}; // namespace chainblocks

// NO NAMESPACE here!

void CBChain::reset() {
  for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
    (*it)->cleanup(*it);
  }
  for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
    (*it)->destroy(*it);
    // blk is responsible to free itself, as they might use any allocation
    // strategy they wish!
  }
  blocks.clear();

  // find dangling variables, notice but do not destroy
  for (auto var : variables) {
    if (var.second.refcount > 0) {
      LOG(ERROR) << "Found a dangling variable: " << var.first
                 << " in chain: " << name;
    }
  }
  variables.clear();

  chainUsers.clear();
  composedHash = 0;
  inputType = CBTypeInfo();
  outputType = {};
  requiredVariables.clear();

  auto n = node.lock();
  if (n) {
    n->visitedChains.erase(this);
  }
  node.reset();

  if (stack_mem) {
    ::operator delete[](stack_mem, std::align_val_t{16});
    stack_mem = nullptr;
  }

  resumer = nullptr;
}

void CBChain::warmup(CBContext *context) {
  if (!warmedUp) {
    LOG(DEBUG) << "Running warmup on chain: " << name;

    // we likely need this early!
    node = context->main->node;
    warmedUp = true;

    context->chainStack.push_back(this);
    DEFER({ context->chainStack.pop_back(); });
    for (auto blk : blocks) {
      try {
        if (blk->warmup)
          blk->warmup(blk, context);
        if (context->failed()) {
          throw chainblocks::WarmupError(context->getErrorMessage());
        }
      } catch (const std::exception &e) {
        LOG(ERROR) << "Block warmup error, failed block: "
                   << std::string(blk->name(blk));
        LOG(ERROR) << e.what();
        // if the failure is from an exception context might not be uptodate
        if (!context->failed()) {
          context->cancelFlow(e.what());
        }
        throw;
      } catch (...) {
        LOG(ERROR) << "Block warmup error, failed block: "
                   << std::string(blk->name(blk));
        if (!context->failed()) {
          context->cancelFlow("foreign exception failure, check logs");
        }
        throw;
      }
    }

    LOG(DEBUG) << "Ran warmup on chain: " << name;
  }
}

void CBChain::cleanup(bool force) {
  if (warmedUp && (force || chainUsers.size() == 0)) {
    LOG(DEBUG) << "Running cleanup on chain: " << name
               << " users count: " << chainUsers.size();

    warmedUp = false;

    // Run cleanup on all blocks, prepare them for a new start if necessary
    // Do this in reverse to allow a safer cleanup
    for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
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
        LOG(ERROR) << "Block cleanup error, failed block: "
                   << std::string(blk->name(blk));
        LOG(ERROR) << e.what();
      } catch (...) {
        LOG(ERROR) << "Block cleanup error, failed block: "
                   << std::string(blk->name(blk));
      }
    }

    // Also clear all variables reporting dangling ones
    for (auto var : variables) {
      if (var.second.refcount > 0) {
        LOG(ERROR) << "Found a dangling variable: " << var.first
                   << " in chain: " << name;
      }
    }
    variables.clear();

    // finally reset the node
    auto n = node.lock();
    if (n) {
      n->visitedChains.erase(this);
    }
    node.reset();

    resumer = nullptr;

    LOG(DEBUG) << "Ran cleanup on chain: " << name;
  }
}

#ifndef OVERRIDE_REGISTER_ALL_BLOCKS
void cbRegisterAllBlocks() { chainblocks::registerCoreBlocks(); }
#endif

#define API_TRY_CALL(_name_, _block_)                                          \
  {                                                                            \
    try {                                                                      \
      _block_                                                                  \
    } catch (const std::exception &ex) {                                       \
      LOG(ERROR) << #_name_ " failed, error: " << ex.what();                   \
    }                                                                          \
  }

bool cb_current_interface_loaded{false};
CBCore cb_current_interface{};

extern "C" {
EXPORTED CBCore *__cdecl chainblocksInterface(uint32_t abi_version) {
  // for now we ignore abi_version
  if (cb_current_interface_loaded)
    return &cb_current_interface;

  // Load everything we know if we did not yet!
  try {
    chainblocks::registerCoreBlocks();
  } catch (const std::exception &ex) {
    LOG(ERROR) << "Failed to register core blocks, error: " << ex.what();
    return nullptr;
  }

  if (CHAINBLOCKS_CURRENT_ABI != abi_version) {
    LOG(ERROR) << "A plugin requested an invalid ABI version.";
    return nullptr;
  }

  auto result = &cb_current_interface;
  cb_current_interface_loaded = true;

  result->registerBlock = [](const char *fullName,
                             CBBlockConstructor constructor) noexcept {
    API_TRY_CALL(registerBlock,
                 chainblocks::registerBlock(fullName, constructor);)
  };

  result->registerObjectType = [](int32_t vendorId, int32_t typeId,
                                  CBObjectInfo info) noexcept {
    API_TRY_CALL(registerObjectType,
                 chainblocks::registerObjectType(vendorId, typeId, info);)
  };

  result->registerEnumType = [](int32_t vendorId, int32_t typeId,
                                CBEnumInfo info) noexcept {
    API_TRY_CALL(registerEnumType,
                 chainblocks::registerEnumType(vendorId, typeId, info);)
  };

  result->registerRunLoopCallback = [](const char *eventName,
                                       CBCallback callback) noexcept {
    API_TRY_CALL(registerRunLoopCallback,
                 chainblocks::registerRunLoopCallback(eventName, callback);)
  };

  result->registerExitCallback = [](const char *eventName,
                                    CBCallback callback) noexcept {
    API_TRY_CALL(registerExitCallback,
                 chainblocks::registerExitCallback(eventName, callback);)
  };

  result->unregisterRunLoopCallback = [](const char *eventName) noexcept {
    API_TRY_CALL(unregisterRunLoopCallback,
                 chainblocks::unregisterRunLoopCallback(eventName);)
  };

  result->unregisterExitCallback = [](const char *eventName) noexcept {
    API_TRY_CALL(unregisterExitCallback,
                 chainblocks::unregisterExitCallback(eventName);)
  };

  result->referenceVariable = [](CBContext *context,
                                 const char *name) noexcept {
    return chainblocks::referenceVariable(context, name);
  };

  result->referenceChainVariable = [](CBChainRef chain,
                                      const char *name) noexcept {
    return chainblocks::referenceChainVariable(chain, name);
  };

  result->releaseVariable = [](CBVar *variable) noexcept {
    return chainblocks::releaseVariable(variable);
  };

  result->suspend = [](CBContext *context, double seconds) noexcept {
    try {
      return chainblocks::suspend(context, seconds);
    } catch (const chainblocks::ActivationError &ex) {
      LOG(ERROR) << ex.what();
      return CBChainState::Stop;
    }
  };

  result->getState = [](CBContext *context) noexcept {
    return context->getState();
  };

  result->abortChain = [](CBContext *context, const char *message) noexcept {
    context->cancelFlow(message);
  };

  result->cloneVar = [](CBVar *dst, const CBVar *src) noexcept {
    chainblocks::cloneVar(*dst, *src);
  };

  result->destroyVar = [](CBVar *var) noexcept {
    chainblocks::destroyVar(*var);
  };

#define CB_ARRAY_IMPL(_arr_, _val_, _name_)                                    \
  result->_name_##Free = [](_arr_ *seq) noexcept {                             \
    chainblocks::arrayFree(*seq);                                              \
  };                                                                           \
                                                                               \
  result->_name_##Resize = [](_arr_ *seq, uint32_t size) noexcept {            \
    chainblocks::arrayResize(*seq, size);                                      \
  };                                                                           \
                                                                               \
  result->_name_##Push = [](_arr_ *seq, const _val_ *value) noexcept {         \
    chainblocks::arrayPush(*seq, *value);                                      \
  };                                                                           \
                                                                               \
  result->_name_##Insert = [](_arr_ *seq, uint32_t index,                      \
                              const _val_ *value) noexcept {                   \
    chainblocks::arrayInsert(*seq, index, *value);                             \
  };                                                                           \
                                                                               \
  result->_name_##Pop = [](_arr_ *seq) noexcept {                              \
    return chainblocks::arrayPop<_arr_, _val_>(*seq);                          \
  };                                                                           \
                                                                               \
  result->_name_##FastDelete = [](_arr_ *seq, uint32_t index) noexcept {       \
    chainblocks::arrayDelFast(*seq, index);                                    \
  };                                                                           \
                                                                               \
  result->_name_##SlowDelete = [](_arr_ *seq, uint32_t index) noexcept {       \
    chainblocks::arrayDel(*seq, index);                                        \
  }

  CB_ARRAY_IMPL(CBSeq, CBVar, seq);
  CB_ARRAY_IMPL(CBTypesInfo, CBTypeInfo, types);
  CB_ARRAY_IMPL(CBParametersInfo, CBParameterInfo, params);
  CB_ARRAY_IMPL(CBlocks, CBlockPtr, blocks);
  CB_ARRAY_IMPL(CBExposedTypesInfo, CBExposedTypeInfo, expTypes);
  CB_ARRAY_IMPL(CBStrings, CBString, strings);

  result->tableNew = []() noexcept {
    CBTable res;
    res.api = &chainblocks::Globals::TableInterface;
    res.opaque = new chainblocks::CBMap();
    return res;
  };

  result->composeChain = [](CBChainRef chain, CBValidationCallback callback,
                            void *userData, CBInstanceData data) noexcept {
    auto sc = CBChain::sharedFromRef(chain);
    try {
      return composeChain(sc.get(), callback, userData, data);
    } catch (const std::exception &e) {
      CBComposeResult res{};
      res.failed = true;
      auto msgTmp = chainblocks::Var(e.what());
      chainblocks::cloneVar(res.failureMessage, msgTmp);
      return res;
    } catch (...) {
      CBComposeResult res{};
      res.failed = true;
      auto msgTmp =
          chainblocks::Var("foreign exception failure during composeChain");
      chainblocks::cloneVar(res.failureMessage, msgTmp);
      return res;
    }
  };

  result->runChain = [](CBChainRef chain, CBContext *context,
                        const CBVar *input) noexcept {
    auto sc = CBChain::sharedFromRef(chain);
    return chainblocks::runSubChain(sc.get(), context, *input);
  };

  result->composeBlocks = [](CBlocks blocks, CBValidationCallback callback,
                             void *userData, CBInstanceData data) noexcept {
    try {
      return chainblocks::composeChain(blocks, callback, userData, data);
    } catch (const std::exception &e) {
      CBComposeResult res{};
      res.failed = true;
      auto msgTmp = chainblocks::Var(e.what());
      chainblocks::cloneVar(res.failureMessage, msgTmp);
      return res;
    } catch (...) {
      CBComposeResult res{};
      res.failed = true;
      auto msgTmp =
          chainblocks::Var("foreign exception failure during composeChain");
      chainblocks::cloneVar(res.failureMessage, msgTmp);
      return res;
    }
  };

  result->validateSetParam = [](CBlock *block, int index, const CBVar *param,
                                CBValidationCallback callback,
                                void *userData) noexcept {
    return chainblocks::validateSetParam(block, index, *param, callback,
                                         userData);
  };

  result->runBlocks = [](CBlocks blocks, CBContext *context, const CBVar *input,
                         CBVar *output, const CBBool handleReturn) noexcept {
    try {
      return chainblocks::activateBlocks(blocks, context, *input, *output,
                                         handleReturn);
    } catch (const std::exception &e) {
      context->cancelFlow(e.what());
      return CBChainState::Stop;
    } catch (...) {
      context->cancelFlow("foreign exception failure during runBlocks");
      return CBChainState::Stop;
    }
  };

  result->getChainInfo = [](CBChainRef chainref) noexcept {
    auto sc = CBChain::sharedFromRef(chainref);
    auto chain = sc.get();
    CBChainInfo info{chain->name.c_str(),
                     chain->looped,
                     chain->unsafe,
                     chain,
                     {&chain->blocks[0], uint32_t(chain->blocks.size()), 0},
                     chainblocks::isRunning(chain)};
    return info;
  };

  result->log = [](const char *msg) noexcept { LOG(INFO) << msg; };

  result->setLoggingOptions = [](CBLoggingOptions options) noexcept {
    chainblocks::LogsDefaultConf.setGlobally(
        el::ConfigurationType::MaxLogFileSize, std::to_string(options.maxSize));
    el::Loggers::reconfigureAllLoggers(chainblocks::LogsDefaultConf);
  };

  result->createBlock = [](const char *name) noexcept {
    return chainblocks::createBlock(name);
  };

  result->createChain = []() noexcept {
    auto chain = CBChain::make();
    return chain->newRef();
  };

  result->setChainName = [](CBChainRef chainref, const char *name) noexcept {
    auto sc = CBChain::sharedFromRef(chainref);
    sc->name = name;
  };

  result->setChainLooped = [](CBChainRef chainref, CBBool looped) noexcept {
    auto sc = CBChain::sharedFromRef(chainref);
    sc->looped = looped;
  };

  result->setChainUnsafe = [](CBChainRef chainref, CBBool unsafe) noexcept {
    auto sc = CBChain::sharedFromRef(chainref);
    sc->unsafe = unsafe;
  };

  result->addBlock = [](CBChainRef chainref, CBlockPtr blk) noexcept {
    auto sc = CBChain::sharedFromRef(chainref);
    sc->addBlock(blk);
  };

  result->removeBlock = [](CBChainRef chainref, CBlockPtr blk) noexcept {
    auto sc = CBChain::sharedFromRef(chainref);
    sc->removeBlock(blk);
  };

  result->destroyChain = [](CBChainRef chain) noexcept {
    CBChain::deleteRef(chain);
  };

  result->stopChain = [](CBChainRef chain) {
    auto sc = CBChain::sharedFromRef(chain);
    CBVar output{};
    chainblocks::stop(sc.get(), &output);
    return output;
  };

  result->destroyChain = [](CBChainRef chain) noexcept {
    CBChain::deleteRef(chain);
  };

  result->destroyChain = [](CBChainRef chain) noexcept {
    CBChain::deleteRef(chain);
  };

  result->createNode = []() noexcept {
    return reinterpret_cast<CBNodeRef>(CBNode::makePtr());
  };

  result->destroyNode = [](CBNodeRef node) noexcept {
    auto snode = reinterpret_cast<std::shared_ptr<CBNode> *>(node);
    delete snode;
  };

  result->schedule = [](CBNodeRef node, CBChainRef chain) noexcept {
    auto snode = reinterpret_cast<std::shared_ptr<CBNode> *>(node);
    (*snode)->schedule(CBChain::sharedFromRef(chain));
  };

  result->unschedule = [](CBNodeRef node, CBChainRef chain) noexcept {
    auto snode = reinterpret_cast<std::shared_ptr<CBNode> *>(node);
    (*snode)->remove(CBChain::sharedFromRef(chain));
  };

  result->tick = [](CBNodeRef node) noexcept {
    auto snode = reinterpret_cast<std::shared_ptr<CBNode> *>(node);
    (*snode)->tick();
    if ((*snode)->empty())
      return false;
    else
      return true;
  };

  result->sleep = [](double seconds, bool runCallbacks) noexcept {
    chainblocks::sleep(seconds, runCallbacks);
  };

  result->getRootPath = []() noexcept {
    return chainblocks::Globals::RootPath.c_str();
  };

  result->setRootPath = [](const char *p) noexcept {
    chainblocks::Globals::RootPath = p;
    chainblocks::loadExternalBlocks(p);
    std::filesystem::current_path(p);
  };

  result->asyncActivate = [](auto context, auto data, auto call) {
    return chainblocks::awaitne(context, [&] { return call(context, data); });
  };

  result->getBlocks = []() {
    CBStrings s{};
    for (auto [name, _] : chainblocks::Globals::BlocksRegister) {
      chainblocks::arrayPush(s, name.data());
    }
    return s;
  };

  result->getString = [](uint32_t crc) { return chainblocks::getString(crc); };

  result->setString = [](uint32_t crc, CBString str) {
    chainblocks::setString(crc, str);
  };

  return result;
}
};
