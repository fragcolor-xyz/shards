/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#define STB_DS_IMPLEMENTATION 1

#include "runtime.hpp"
#include "blocks/shared.hpp"
#include <boost/stacktrace.hpp>
#include <csignal>
#include <cstdarg>
#include <string.h>

#ifdef USE_RPMALLOC
void *operator new(std::size_t s) {
  rpmalloc_initialize();
  return rpmalloc(s);
}
void *operator new[](std::size_t s) {
  rpmalloc_initialize();
  return rpmalloc(s);
}
void *operator new(std::size_t s, const std::nothrow_t &tag) noexcept {
  rpmalloc_initialize();
  return rpmalloc(s);
}
void *operator new[](std::size_t s, const std::nothrow_t &tag) noexcept {
  rpmalloc_initialize();
  return rpmalloc(s);
}

void operator delete(void *ptr) noexcept { rpfree(ptr); }
void operator delete[](void *ptr) noexcept { rpfree(ptr); }
void operator delete(void *ptr, const std::nothrow_t &tag) noexcept {
  rpfree(ptr);
}
void operator delete[](void *ptr, const std::nothrow_t &tag) noexcept {
  rpfree(ptr);
}
void operator delete(void *ptr, std::size_t sz) noexcept { rpfree(ptr); }
void operator delete[](void *ptr, std::size_t sz) noexcept { rpfree(ptr); }
#endif

INITIALIZE_EASYLOGGINGPP

namespace chainblocks {
extern void registerAssertBlocks();
extern void registerChainsBlocks();
extern void registerLoggingBlocks();
extern void registerFlowBlocks();
extern void registerProcessBlocks();
extern void registerSeqsBlocks();
extern void registerCastingBlocks();
extern void registerBlocksCoreBlocks();
extern void registerSerializationBlocks();
extern void registerFSBlocks();
extern void registerJsonBlocks();
extern void registerNetworkBlocks();
extern void registerStructBlocks();
extern void registerTimeBlocks();
extern void registerOSBlocks();

namespace Math {
namespace LinAlg {
extern void registerBlocks();
}
} // namespace Math

namespace Regex {
extern void registerBlocks();
}

#ifdef CB_WITH_EXTRAS
extern void cbInitExtras();
#endif

void registerCoreBlocks() {
#ifdef USE_RPMALLOC
  rpmalloc_initialize();
#endif

  static_assert(sizeof(uint48_t) == 48 / 8);
  static_assert(sizeof(CBVarPayload) == 16);
  static_assert(sizeof(CBVar) == 32);

  registerBlocksCoreBlocks();
  registerAssertBlocks();
  registerChainsBlocks();
  registerLoggingBlocks();
  registerFlowBlocks();
  registerProcessBlocks();
  registerSeqsBlocks();
  registerCastingBlocks();
  registerSerializationBlocks();
  Math::LinAlg::registerBlocks();
  registerFSBlocks();
  registerJsonBlocks();
  registerNetworkBlocks();
  registerStructBlocks();
  registerTimeBlocks();
  registerOSBlocks();
  Regex::registerBlocks();

  // also enums
  SharedTypes::initEnums();

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

#ifndef NDEBUG
  // TODO remove when we have better tests/samples
  auto chain1 = std::unique_ptr<CBChain>(Chain("test-chain")
                                             .looped(true)
                                             .let(1)
                                             .block("Log")
                                             .block("Math.Add", 2)
                                             .block("Assert.Is", 3, true));
  assert(chain1->blocks.size() == 4);
#endif
}

CBlock *createBlock(const char *name) {
  auto it = Globals::BlocksRegister.find(name);
  if (it == Globals::BlocksRegister.end()) {
    return nullptr;
  }

  auto blkp = it->second();

  // Hook inline blocks to override activation in runChain
  if (strcmp(name, "Const") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreConst;
  } else if (strcmp(name, "Stop") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreStop;
  } else if (strcmp(name, "Input") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreInput;
  } else if (strcmp(name, "Restart") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreRestart;
  } else if (strcmp(name, "Sleep") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreSleep;
  } else if (strcmp(name, "Repeat") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreRepeat;
  } else if (strcmp(name, "Get") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreGet;
  } else if (strcmp(name, "Set") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreSet;
  } else if (strcmp(name, "Update") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreUpdate;
  } else if (strcmp(name, "Swap") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreSwap;
  } else if (strcmp(name, "Push") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CorePush;
  } else if (strcmp(name, "Is") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreIs;
  } else if (strcmp(name, "IsNot") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreIsNot;
  } else if (strcmp(name, "IsMore") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreIsMore;
  } else if (strcmp(name, "IsLess") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreIsLess;
  } else if (strcmp(name, "IsMoreEqual") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreIsMoreEqual;
  } else if (strcmp(name, "IsLessEqual") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreIsLessEqual;
  } else if (strcmp(name, "And") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreAnd;
  } else if (strcmp(name, "Or") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreOr;
  } else if (strcmp(name, "Not") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreNot;
  } else if (strcmp(name, "Math.Add") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAdd;
  } else if (strcmp(name, "Math.Subtract") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathSubtract;
  } else if (strcmp(name, "Math.Multiply") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathMultiply;
  } else if (strcmp(name, "Math.Divide") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathDivide;
  } else if (strcmp(name, "Math.Xor") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathXor;
  } else if (strcmp(name, "Math.And") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAnd;
  } else if (strcmp(name, "Math.Or") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathOr;
  } else if (strcmp(name, "Math.Mod") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathMod;
  } else if (strcmp(name, "Math.LShift") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathLShift;
  } else if (strcmp(name, "Math.RShift") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathRShift;
  } else if (strcmp(name, "Math.Abs") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAbs;
  } else if (strcmp(name, "Math.Exp") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathExp;
  } else if (strcmp(name, "Math.Exp2") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathExp2;
  } else if (strcmp(name, "Math.Expm1") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathExpm1;
  } else if (strcmp(name, "Math.Log") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathLog;
  } else if (strcmp(name, "Math.Log10") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathLog10;
  } else if (strcmp(name, "Math.Log2") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathLog2;
  } else if (strcmp(name, "Math.Log1p") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathLog1p;
  } else if (strcmp(name, "Math.Sqrt") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathSqrt;
  } else if (strcmp(name, "Math.Cbrt") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathCbrt;
  } else if (strcmp(name, "Math.Sin") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathSin;
  } else if (strcmp(name, "Math.Cos") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathCos;
  } else if (strcmp(name, "Math.Tan") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathTan;
  } else if (strcmp(name, "Math.Asin") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAsin;
  } else if (strcmp(name, "Math.Acos") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAcos;
  } else if (strcmp(name, "Math.Atan") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAtan;
  } else if (strcmp(name, "Math.Sinh") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathSinh;
  } else if (strcmp(name, "Math.Cosh") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathCosh;
  } else if (strcmp(name, "Math.Tanh") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathTanh;
  } else if (strcmp(name, "Math.Asinh") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAsinh;
  } else if (strcmp(name, "Math.Acosh") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAcosh;
  } else if (strcmp(name, "Math.Atanh") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathAtanh;
  } else if (strcmp(name, "Math.Erf") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathErf;
  } else if (strcmp(name, "Math.Erfc") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathErfc;
  } else if (strcmp(name, "Math.TGamma") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathTGamma;
  } else if (strcmp(name, "Math.LGamma") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathLGamma;
  } else if (strcmp(name, "Math.Ceil") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathCeil;
  } else if (strcmp(name, "Math.Floor") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathFloor;
  } else if (strcmp(name, "Math.Trunc") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathTrunc;
  } else if (strcmp(name, "Math.Round") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::MathRound;
  }

  return blkp;
}

void registerBlock(const char *fullName, CBBlockConstructor constructor) {
  auto cname = std::string(fullName);
  auto findIt = Globals::BlocksRegister.find(cname);
  if (findIt == Globals::BlocksRegister.end()) {
    Globals::BlocksRegister.insert(std::make_pair(cname, constructor));
    // DLOG(INFO) << "added block: " << cname;
  } else {
    Globals::BlocksRegister[cname] = constructor;
    LOG(INFO) << "overridden block: " << cname;
  }

  for (auto &pobs : Globals::Observers) {
    if (pobs.expired())
      continue;
    auto obs = pobs.lock();
    obs->registerBlock(fullName, constructor);
  }
}

void registerObjectType(int32_t vendorId, int32_t typeId, CBObjectInfo info) {
  int64_t id = (int64_t)vendorId << 32 | typeId;
  auto typeName = std::string(info.name);
  auto findIt = Globals::ObjectTypesRegister.find(id);
  if (findIt == Globals::ObjectTypesRegister.end()) {
    Globals::ObjectTypesRegister.insert(std::make_pair(id, info));
    // DLOG(INFO) << "added object type: " << typeName;
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
    // DLOG(INFO) << "added enum type: " << typeName;
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

void registerRunLoopCallback(const char *eventName, CBCallback callback) {
  chainblocks::Globals::RunLoopHooks[eventName] = callback;
}

void unregisterRunLoopCallback(const char *eventName) {
  auto findIt = chainblocks::Globals::RunLoopHooks.find(eventName);
  if (findIt != chainblocks::Globals::RunLoopHooks.end()) {
    chainblocks::Globals::RunLoopHooks.erase(findIt);
  }
}

void registerExitCallback(const char *eventName, CBCallback callback) {
  chainblocks::Globals::ExitHooks[eventName] = callback;
}

void unregisterExitCallback(const char *eventName) {
  auto findIt = chainblocks::Globals::ExitHooks.find(eventName);
  if (findIt != chainblocks::Globals::ExitHooks.end()) {
    chainblocks::Globals::ExitHooks.erase(findIt);
  }
}

void registerChain(CBChain *chain) {
  chainblocks::Globals::GlobalChains[chain->name] = chain;
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

CBVar *findVariable(CBContext *ctx, const char *name) {
  cbassert(ctx->chain->node);
  CBVar &v = ctx->chain->node->variables[name];
  return &v;
}

CBVar suspend(CBContext *context, double seconds) {
  if (seconds <= 0) {
    context->next = Duration(0);
  } else {
    context->next = Clock::now().time_since_epoch() + Duration(seconds);
  }
  context->continuation = context->continuation.resume();
  if (context->restarted) {
    CBVar restart = {};
    restart.valueType = None;
    restart.payload.chainState = CBChainState::Restart;
    return restart;
  } else if (context->aborted) {
    CBVar stop = {};
    stop.valueType = None;
    stop.payload.chainState = CBChainState::Stop;
    return stop;
  }
  CBVar cont = {};
  cont.valueType = None;
  cont.payload.chainState = Continue;
  return cont;
}

FlowState activateBlocks(CBlocks blocks, int nblocks, CBContext *context,
                         const CBVar &chainInput, CBVar &output) {
  auto input = chainInput;
  // validation prevents extra pops so this should be safe
  auto sidx = stbds_arrlenu(context->stack);
  for (auto i = 0; i < nblocks; i++) {
    output = activateBlock(blocks[i], context, input);
    if (output.valueType == None) {
      switch (output.payload.chainState) {
      case CBChainState::Restart: {
        stbds_arrsetlen(context->stack, sidx);
        return Continuing;
      }
      case CBChainState::Stop: {
        stbds_arrsetlen(context->stack, sidx);
        return Stopping;
      }
      case CBChainState::Return: {
        output = input; // Invert them, we return previous output (input)
        stbds_arrsetlen(context->stack, sidx);
        return Returning;
      }
      case CBChainState::Rebase: {
        input = chainInput;
        continue;
      }
      case Continue:
        break;
      }
    }
    input = output;
  }
  stbds_arrsetlen(context->stack, sidx);
  return Continuing;
}

FlowState activateBlocks(CBSeq blocks, CBContext *context,
                         const CBVar &chainInput, CBVar &output) {
  auto input = chainInput;
  // validation prevents extra pops so this should be safe
  auto sidx = stbds_arrlenu(context->stack);
  for (auto i = 0; i < stbds_arrlen(blocks); i++) {
    output = activateBlock(blocks[i].payload.blockValue, context, input);
    if (output.valueType == None) {
      switch (output.payload.chainState) {
      case CBChainState::Restart: {
        stbds_arrsetlen(context->stack, sidx);
        return Continuing;
      }
      case CBChainState::Stop: {
        stbds_arrsetlen(context->stack, sidx);
        return Stopping;
      }
      case CBChainState::Return: {
        output = input; // Invert them, we return previous output (input)
        stbds_arrsetlen(context->stack, sidx);
        return Returning;
      }
      case CBChainState::Rebase: {
        input = chainInput;
        continue;
      }
      case Continue:
        break;
      }
    }
    input = output;
  }
  stbds_arrsetlen(context->stack, sidx);
  return Continuing;
}
}; // namespace chainblocks

#ifndef OVERRIDE_REGISTER_ALL_BLOCKS
void cbRegisterAllBlocks() { chainblocks::registerCoreBlocks(); }
#endif

// Utility
void dealloc(CBImage &self) {
  if (self.data) {
    delete[] self.data;
    self.data = nullptr;
  }
}

// Utility
void alloc(CBImage &self) {
  if (self.data)
    dealloc(self);

  auto binsize = self.width * self.height * self.channels;
  self.data = new uint8_t[binsize];
}

void releaseMemory(CBVar &self) {
  if ((self.valueType == String || self.valueType == ContextVar) &&
      self.payload.stringValue != nullptr) {
    delete[] self.payload.stringValue;
    self.payload.stringValue = nullptr;
  } else if (self.valueType == Seq && self.payload.seqValue) {
    for (auto i = 0; i < stbds_arrlen(self.payload.seqValue); i++) {
      releaseMemory(self.payload.seqValue[i]);
    }
    stbds_arrfree(self.payload.seqValue);
    self.payload.seqValue = nullptr;
  } else if (self.valueType == Table && self.payload.tableValue) {
    for (auto i = 0; i < stbds_shlen(self.payload.tableValue); i++) {
      delete[] self.payload.tableValue[i].key;
      releaseMemory(self.payload.tableValue[i].value);
    }
    stbds_shfree(self.payload.tableValue);
    self.payload.tableValue = nullptr;
  } else if (self.valueType == Image) {
    dealloc(self.payload.imageValue);
  } else if (self.valueType == Bytes) {
    delete[] self.payload.bytesValue;
    self.payload.bytesValue = nullptr;
    self.payload.bytesSize = 0;
  }
}

thread_local std::vector<char> gLogCache;

#ifdef __cplusplus
extern "C" {
#endif

EXPORTED struct CBCore __cdecl chainblocksInterface(uint32_t abi_version) {
  CBCore result{};

  if (CHAINBLOCKS_CURRENT_ABI != abi_version) {
    LOG(ERROR) << "A plugin requested an invalid ABI version.";
    return result;
  }

  result.registerBlock = [](const char *fullName,
                            CBBlockConstructor constructor) {
    chainblocks::registerBlock(fullName, constructor);
  };

  result.registerObjectType = [](int32_t vendorId, int32_t typeId,
                                 CBObjectInfo info) {
    chainblocks::registerObjectType(vendorId, typeId, info);
  };

  result.registerEnumType = [](int32_t vendorId, int32_t typeId,
                               CBEnumInfo info) {
    chainblocks::registerEnumType(vendorId, typeId, info);
  };

  result.registerRunLoopCallback = [](const char *eventName,
                                      CBCallback callback) {
    chainblocks::registerRunLoopCallback(eventName, callback);
  };

  result.registerExitCallback = [](const char *eventName, CBCallback callback) {
    chainblocks::registerExitCallback(eventName, callback);
  };

  result.unregisterRunLoopCallback = [](const char *eventName) {
    chainblocks::unregisterRunLoopCallback(eventName);
  };

  result.unregisterExitCallback = [](const char *eventName) {
    chainblocks::unregisterExitCallback(eventName);
  };

  result.findVariable = [](CBContext *context, const char *name) {
    return chainblocks::findVariable(context, name);
  };

  result.throwException = [](const char *errorText) {
    throw chainblocks::CBException(errorText);
  };

  result.suspend = [](CBContext *context, double seconds) {
    return chainblocks::suspend(context, seconds);
  };

  result.cloneVar = [](CBVar *dst, const CBVar *src) {
    chainblocks::cloneVar(*dst, *src);
  };

  result.destroyVar = [](CBVar *var) { chainblocks::destroyVar(*var); };

  result.arrayFree = [](void *arr) { stbds_arrfree(arr); };

  result.arrayLength = [](CBArray seq) { return (uint64_t)stbds_arrlenu(seq); };

  result.seqResize = [](CBSeq seq, uint64_t size) {
    stbds_arrsetlen(seq, size);
    return seq;
  };

  result.seqPush = [](CBSeq seq, const CBVar *value) {
    stbds_arrpush(seq, *value);
    return seq;
  };

  result.seqInsert = [](CBSeq seq, uint64_t index, const CBVar *value) {
    stbds_arrins(seq, index, *value);
    return seq;
  };

  result.seqPop = [](CBSeq seq) { return stbds_arrpop(seq); };

  result.seqFastDelete = [](CBSeq seq, uint64_t index) {
    stbds_arrdelswap(seq, index);
  };

  result.seqSlowDelete = [](CBSeq seq, uint64_t index) {
    stbds_arrdel(seq, index);
  };

  result.typesResize = [](CBTypesInfo types, uint64_t size) {
    stbds_arrsetlen(types, size);
    return types;
  };

  result.typesPush = [](CBTypesInfo types, const CBTypeInfo *value) {
    stbds_arrpush(types, *value);
    return types;
  };

  result.typesInsert = [](CBTypesInfo types, uint64_t index,
                          const CBTypeInfo *value) {
    stbds_arrins(types, index, *value);
    return types;
  };

  result.typesPop = [](CBTypesInfo types) { return stbds_arrpop(types); };

  result.typesFastDelete = [](CBTypesInfo types, uint64_t index) {
    stbds_arrdelswap(types, index);
  };

  result.typesSlowDelete = [](CBTypesInfo types, uint64_t index) {
    stbds_arrdel(types, index);
  };

  result.validateChain = [](CBChain *chain, CBValidationCallback callback,
                            void *userData, CBInstanceData data) {
    return validateConnections(chain, callback, userData, data);
  };

  result.runChain = [](CBChain *chain, CBContext *context, CBVar input) {
    return chainblocks::runSubChain(chain, context, input);
  };

  result.validateBlocks = [](CBlocks blocks, CBValidationCallback callback,
                             void *userData, CBInstanceData data) {
    return validateConnections(blocks, callback, userData, data);
  };

  result.runBlocks = [](CBlocks blocks, CBContext *context, CBVar input) {
    CBVar output{};
    chainblocks::activateBlocks(blocks, stbds_arrlen(blocks), context, input,
                                output);
    return output;
  };

  result.log = [](const char *msg) { LOG(INFO) << msg; };

  result.createBlock = [](const char *name) {
    return chainblocks::createBlock(name);
  };

  result.createChain = [](const char *name, CBlocks blocks, bool looped,
                          bool unsafe) {
    auto chain = new CBChain(name);
    chain->looped = looped;
    chain->unsafe = unsafe;
    for (size_t i = 0; i < stbds_arrlenu(blocks); i++) {
      chain->addBlock(blocks[i]);
    }
    return chain;
  };

  result.destroyChain = [](CBChain *chain) { delete chain; };

  result.createNode = []() { return new CBNode(); };

  result.destroyNode = [](CBNode *node) { delete node; };

  result.schedule = [](CBNode *node, CBChain *chain) { node->schedule(chain); };

  result.tick = [](CBNode *node) { node->tick(); };

  result.sleep = [](double seconds, bool runCallbacks) {
    chainblocks::sleep(seconds, runCallbacks);
  };

  result.getRootPath = []() { return chainblocks::Globals::RootPath.c_str(); };

  result.setRootPath = [](const char *p) {
    chainblocks::Globals::RootPath = p;
  };

  return result;
}

#ifdef __cplusplus
};
#endif

bool matchTypes(const CBTypeInfo &exposedType, const CBTypeInfo &consumedType,
                bool isParameter, bool strict) {
  if (consumedType.basicType == Any ||
      (!isParameter && consumedType.basicType == None))
    return true;

  if (exposedType.basicType != consumedType.basicType) {
    // Fail if basic type differs
    return false;
  }

  switch (exposedType.basicType) {
  case Object: {
    if (exposedType.objectVendorId != consumedType.objectVendorId ||
        exposedType.objectTypeId != consumedType.objectTypeId) {
      return false;
    }
    break;
  }
  case Enum: {
    if (exposedType.enumVendorId != consumedType.enumVendorId ||
        exposedType.enumTypeId != consumedType.enumTypeId) {
      return false;
    }
    break;
  }
  case Seq: {
    if (strict) {
      if (exposedType.seqType && consumedType.seqType) {
        if (!matchTypes(*exposedType.seqType, *consumedType.seqType,
                        isParameter, strict)) {
          return false;
        }
      } else if (exposedType.seqType == nullptr ||
                 consumedType.seqType == nullptr) {
        return false;
      } else if (exposedType.seqType && consumedType.seqType == nullptr &&
                 !isParameter) {
        // Assume consumer is not strict
        return true;
      }
    }
    break;
  }
  case Table: {
    if (strict) {
      auto atypes = stbds_arrlen(exposedType.tableTypes);
      auto btypes = stbds_arrlen(consumedType.tableTypes);
      //  btypes != 0 assume consumer is not strict
      for (auto i = 0; i < atypes && (isParameter || btypes != 0); i++) {
        // Go thru all exposed types and make sure we get a positive match with
        // the consumer
        auto atype = exposedType.tableTypes[i];
        auto matched = false;
        for (auto y = 0; y < btypes; y++) {
          auto btype = consumedType.tableTypes[y];
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
    break;
  }
  default:
    return true;
  }
  return true;
}

namespace std {
template <> struct hash<CBExposedTypeInfo> {
  std::size_t operator()(const CBExposedTypeInfo &typeInfo) const {
    using std::hash;
    using std::size_t;
    using std::string;
    auto res = hash<string>()(typeInfo.name);
    res = res ^ hash<int>()(typeInfo.exposedType.basicType);
    res = res ^ hash<int>()(typeInfo.isMutable);
    if (typeInfo.exposedType.basicType == Table &&
        typeInfo.exposedType.tableTypes && typeInfo.exposedType.tableKeys) {
      for (auto i = 0; i < stbds_arrlen(typeInfo.exposedType.tableKeys); i++) {
        res = res ^ hash<string>()(typeInfo.exposedType.tableKeys[i]);
      }
    } else if (typeInfo.exposedType.basicType == Seq &&
               typeInfo.exposedType.seqType) {
      res = res ^ hash<int>()(typeInfo.exposedType.seqType->basicType);
    }
    return res;
  }
};
} // namespace std

struct ValidationContext {
  phmap::flat_hash_map<std::string, phmap::flat_hash_set<CBExposedTypeInfo>>
      exposed;
  phmap::flat_hash_set<std::string> variables;
  phmap::flat_hash_set<std::string> references;

  CBTypeInfo previousOutputType{};
  CBTypeInfo originalInputType{};
  std::vector<CBTypeInfo> stackTypes;

  CBlock *bottom{};

  CBValidationCallback cb{};
  void *userData{};
};

void validateConnection(ValidationContext &ctx) {
  auto previousOutput = ctx.previousOutputType;

  auto inputInfos = ctx.bottom->inputTypes(ctx.bottom);
  auto inputMatches = false;
  // validate our generic input
  for (auto i = 0; stbds_arrlen(inputInfos) > i; i++) {
    auto &inputInfo = inputInfos[i];
    if (matchTypes(previousOutput, inputInfo, false, false)) {
      inputMatches = true;
      break;
    }
  }

  if (!inputMatches) {
    std::string err("Could not find a matching input type, block: " +
                    std::string(ctx.bottom->name(ctx.bottom)));
    ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
  }

  // infer and specialize types if we need to
  // If we don't we assume our output will be of the same type of the previous!
  if (ctx.bottom->compose) {
    CBInstanceData data{};
    data.block = ctx.bottom;
    data.inputType = previousOutput;
    // Pass all we got in the context!
    for (auto &info : ctx.exposed) {
      for (auto &type : info.second) {
        stbds_arrpush(data.consumables, type);
      }
    }
    for (auto &info : ctx.stackTypes) {
      stbds_arrpush(data.stack, info);
    }

    // this ensures e.g. SetVariable exposedVars have right type from the actual
    // input type (previousOutput)!
    ctx.previousOutputType = ctx.bottom->compose(ctx.bottom, data);

    stbds_arrfree(data.stack);
    stbds_arrfree(data.consumables);
  } else {
    // Short-cut if it's just one type and not any type
    // Any type tho means keep previous output type!
    auto outputTypes = ctx.bottom->outputTypes(ctx.bottom);
    if (stbds_arrlen(outputTypes) == 1 && outputTypes[0].basicType != Any) {
      ctx.previousOutputType = outputTypes[0];
    }
  }

  // Grab those after type inference!
  auto exposedVars = ctx.bottom->exposedVariables(ctx.bottom);
  // Add the vars we expose
  for (auto i = 0; stbds_arrlen(exposedVars) > i; i++) {
    auto &exposed_param = exposedVars[i];
    std::string name(exposed_param.name);
    ctx.exposed[name].insert(exposed_param);

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
  if (strcmp(ctx.bottom->name(ctx.bottom), "Take") == 0) {
    if (previousOutput.basicType == Seq) {
      ctx.bottom->inlineBlockId = CBInlineBlocks::CoreTakeSeq;
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
  auto consumedVar = ctx.bottom->consumedVariables(ctx.bottom);

  phmap::flat_hash_map<std::string, std::vector<CBExposedTypeInfo>>
      consumedVars;
  for (auto i = 0; stbds_arrlen(consumedVar) > i; i++) {
    auto &consumed_param = consumedVar[i];
    std::string name(consumed_param.name);
    consumedVars[name].push_back(consumed_param);
  }

  // make sure we have the vars we need, collect first
  for (const auto &consumed : consumedVars) {
    auto matching = false;

    for (const auto &consumed_param : consumed.second) {
      std::string name(consumed_param.name);
      if (name.find(' ') !=
          std::string::npos) { // take only the first part of variable name
        // the remaining should be a table key which we don't care here
        name = name.substr(0, name.find(' '));
      }
      auto findIt = ctx.exposed.find(name);
      if (findIt == ctx.exposed.end()) {
        std::string err("Required consumed variable not found: " + name);
        // Warning only, delegate compose to decide
        ctx.cb(ctx.bottom, err.c_str(), true, ctx.userData);
      } else {

        for (auto type : findIt->second) {
          auto exposedType = type.exposedType;
          auto requiredType = consumed_param.exposedType;
          // Finally deep compare types
          if (matchTypes(exposedType, requiredType, false, true)) {
            matching = true;
            break;
          }
        }
      }
      if (matching)
        break;
    }

    if (!matching) {
      std::string err(
          "Required consumed types do not match currently exposed ones: " +
          consumed.first);
      err += " exposed types:";
      for (const auto &info : ctx.exposed) {
        err += " (" + info.first + " [";

        for (auto type : info.second) {
          if (type.exposedType.basicType == Table &&
              type.exposedType.tableTypes && type.exposedType.tableKeys) {
            err += "(" + type2Name(type.exposedType.basicType) + " [" +
                   type2Name(type.exposedType.tableTypes[0].basicType) + " " +
                   type.exposedType.tableKeys[0] + "]) ";
          } else if (type.exposedType.basicType == Seq &&
                     type.exposedType.seqType) {
            err += "(" + type2Name(type.exposedType.basicType) + " [" +
                   type2Name(type.exposedType.seqType->basicType) + "]) ";
          } else {
            err += type2Name(type.exposedType.basicType) + " ";
          }
        }
        err.erase(err.end() - 1);

        err += "])";
      }
      ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
    }
  }
}

CBValidationResult validateConnections(const std::vector<CBlock *> &chain,
                                       CBValidationCallback callback,
                                       void *userData, CBInstanceData data) {
  auto ctx = ValidationContext();
  ctx.originalInputType = data.inputType;
  ctx.previousOutputType = data.inputType;
  ctx.cb = callback;
  ctx.userData = userData;
  for (auto i = 0; i < stbds_arrlen(data.stack); i++) {
    ctx.stackTypes.push_back(data.stack[i]);
  }

  if (data.consumables) {
    for (auto i = 0; i < stbds_arrlen(data.consumables); i++) {
      auto &info = data.consumables[i];
      ctx.exposed[info.name].insert(info);
    }
  }

  for (auto blk : chain) {
    if (strcmp(blk->name(blk), "Input") == 0 ||
        strcmp(blk->name(blk), "And") == 0 ||
        strcmp(blk->name(blk), "Or") == 0) {
      // Hard code behavior for Input block and And and Or, in order to validate
      // with actual chain input the followup
      ctx.previousOutputType = ctx.originalInputType;
    } else if (strcmp(blk->name(blk), "SetInput") == 0) {
      if (ctx.previousOutputType != ctx.originalInputType) {
        throw chainblocks::CBException(
            "SetInput input type must be the same original chain input type.");
      }
    } else if (strcmp(blk->name(blk), "Push") == 0) {
      // Check first param see if empty/null
      auto seqName = blk->getParam(blk, 0);
      if (seqName.payload.stringValue == nullptr ||
          seqName.payload.stringValue[0] == 0) {
        blk->inlineBlockId = StackPush;
        // keep previous output type
        // push stack type
        ctx.stackTypes.push_back(ctx.previousOutputType);
      } else {
        ctx.bottom = blk;
        validateConnection(ctx);
      }
    } else if (strcmp(blk->name(blk), "Pop") == 0) {
      // Check first param see if empty/null
      auto seqName = blk->getParam(blk, 0);
      if (seqName.payload.stringValue == nullptr ||
          seqName.payload.stringValue[0] == 0) {
        blk->inlineBlockId = StackPop;
        if (ctx.stackTypes.empty()) {
          throw chainblocks::CBException("Stack Pop, but stack was empty!");
        }
        ctx.previousOutputType = ctx.stackTypes.back();
        ctx.stackTypes.pop_back();
      } else {
        ctx.bottom = blk;
        validateConnection(ctx);
      }
    } else if (strcmp(blk->name(blk), "Drop") == 0) {
      // Check first param see if empty/null
      auto seqName = blk->getParam(blk, 0);
      if (seqName.payload.stringValue == nullptr ||
          seqName.payload.stringValue[0] == 0) {
        blk->inlineBlockId = StackDrop;
        if (ctx.stackTypes.empty()) {
          throw chainblocks::CBException("Stack Drop, but stack was empty!");
        }
        ctx.stackTypes.pop_back();
      } else {
        ctx.bottom = blk;
        validateConnection(ctx);
      }
    } else if (strcmp(blk->name(blk), "Swap") == 0) {
      // Check first param see if empty/null
      auto aname = blk->getParam(blk, 0);
      auto bname = blk->getParam(blk, 1);
      if ((aname.payload.stringValue == nullptr ||
           aname.payload.stringValue[0] == 0) &&
          (bname.payload.stringValue == nullptr ||
           bname.payload.stringValue[0] == 0)) {
        blk->inlineBlockId = StackSwap;
        if (ctx.stackTypes.size() < 2) {
          throw chainblocks::CBException("Stack Swap, but stack size < 2!");
        }
        std::swap(ctx.stackTypes[ctx.stackTypes.size() - 1],
                  ctx.stackTypes[ctx.stackTypes.size() - 2]);
        ctx.previousOutputType = ctx.stackTypes.back();
      } else {
        ctx.bottom = blk;
        validateConnection(ctx);
      }
    } else {
      ctx.bottom = blk;
      validateConnection(ctx);
    }
  }

  CBValidationResult result = {ctx.previousOutputType};
  for (auto &exposed : ctx.exposed) {
    for (auto &type : exposed.second) {
      stbds_arrpush(result.exposedInfo, type);
    }
  }
  return result;
}

CBValidationResult validateConnections(const CBChain *chain,
                                       CBValidationCallback callback,
                                       void *userData, CBInstanceData data) {
  return validateConnections(chain->blocks, callback, userData, data);
}

CBValidationResult validateConnections(const CBlocks chain,
                                       CBValidationCallback callback,
                                       void *userData, CBInstanceData data) {
  std::vector<CBlock *> blocks;
  for (auto i = 0; stbds_arrlen(chain) > i; i++) {
    blocks.push_back(chain[i]);
  }
  return validateConnections(blocks, callback, userData, data);
}

void freeDerivedInfo(CBTypeInfo info) {
  switch (info.basicType) {
  case Seq: {
    freeDerivedInfo(*info.seqType);
    delete info.seqType;
  }
  case Table: {
    for (auto i = 0; stbds_arrlen(info.tableTypes) > i; i++) {
      freeDerivedInfo(info.tableTypes[i]);
    }
    stbds_arrfree(info.tableTypes);
  }
  default:
    break;
  };
}

CBTypeInfo deriveTypeInfo(CBVar &value) {
  // We need to guess a valid CBTypeInfo for this var in order to validate
  // Build a CBTypeInfo for the var
  auto varType = CBTypeInfo();
  varType.basicType = value.valueType;
  varType.seqType = nullptr;
  varType.tableTypes = nullptr;
  switch (value.valueType) {
  case Object: {
    varType.objectVendorId = value.payload.objectVendorId;
    varType.objectTypeId = value.payload.objectTypeId;
    break;
  }
  case Enum: {
    varType.enumVendorId = value.payload.enumVendorId;
    varType.enumTypeId = value.payload.enumTypeId;
    break;
  }
  case Seq: {
    varType.seqType = new CBTypeInfo();
    if (stbds_arrlen(value.payload.seqValue) > 0) {
      *varType.seqType = deriveTypeInfo(value.payload.seqValue[0]);
    }
    break;
  }
  case Table: {
    for (auto i = 0; stbds_shlen(value.payload.tableValue) > i; i++) {
      stbds_arrpush(varType.tableTypes,
                    deriveTypeInfo(value.payload.tableValue[i].value));
    }
    break;
  }
  default:
    break;
  };
  return varType;
}

bool validateSetParam(CBlock *block, int index, CBVar &value,
                      CBValidationCallback callback, void *userData) {
  auto params = block->parameters(block);
  if (stbds_arrlen(params) <= index) {
    std::string err("Parameter index out of range");
    callback(block, err.c_str(), false, userData);
    return false;
  }

  auto param = params[index];

  // Build a CBTypeInfo for the var
  auto varType = deriveTypeInfo(value);

  for (auto i = 0; stbds_arrlen(param.valueTypes) > i; i++) {
    if (matchTypes(varType, param.valueTypes[i], true, true)) {
      freeDerivedInfo(varType);
      return true; // we are good just exit
    }
  }

  // Failed until now but let's check if the type is a sequenced too
  if (value.valueType == Seq) {
    // Validate each type in the seq
    for (auto i = 0; stbds_arrlen(value.payload.seqValue) > i; i++) {
      if (validateSetParam(block, index, value.payload.seqValue[i], callback,
                           userData)) {
        freeDerivedInfo(varType);
        return true;
      }
    }
  }

  std::string err("Parameter not accepting this kind of variable");
  callback(block, err.c_str(), false, userData);

  freeDerivedInfo(varType);

  return false;
}

void CBChain::cleanup() {
  if (node) {
    node->remove(this);
    node = nullptr;
  }

  for (auto blk : blocks) {
    blk->cleanup(blk);
    blk->destroy(blk);
    // blk is responsible to free itself, as they might use any allocation
    // strategy they wish!
  }
  blocks.clear();

  if (ownedOutput) {
    chainblocks::destroyVar(finishedOutput);
    ownedOutput = false;
  }
}

namespace chainblocks {
void error_handler(int err_sig) {
  std::signal(err_sig, SIG_DFL);

  auto printTrace = false;

  switch (err_sig) {
  case SIGINT:
  case SIGTERM:
    LOG(INFO) << "Exiting due to INT/TERM signal";
    std::exit(0);
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
    std::signal(SIGABRT, SIG_DFL); // also make sure to remove this due to
                                   // logger double report on FATAL
    LOG(ERROR) << boost::stacktrace::stacktrace();
  }

  std::raise(err_sig);
}

void installSignalHandlers() {
  std::signal(SIGFPE, &error_handler);
  std::signal(SIGILL, &error_handler);
  std::signal(SIGABRT, &error_handler);
  std::signal(SIGSEGV, &error_handler);
}

Chain::operator CBChain *() {
  auto chain = new CBChain(_name.c_str());
  chain->looped = _looped;
  for (auto blk : _blocks) {
    chain->addBlock(blk);
  }
  // blocks are unique so drain them here
  _blocks.clear();
  return chain;
}

CBRunChainOutput runChain(CBChain *chain, CBContext *context,
                          const CBVar &chainInput) {
  chain->previousOutput = CBVar();
  chain->started = true;
  chain->context = context;

  // store stack index
  auto sidx = stbds_arrlenu(context->stack);

  auto input = chainInput;
  for (auto blk : chain->blocks) {
    try {
      input = chain->previousOutput = activateBlock(blk, context, input);
      if (chain->previousOutput.valueType == None) {
        switch (chain->previousOutput.payload.chainState) {
        case CBChainState::Restart: {
          stbds_arrsetlen(context->stack, sidx);
          return {chain->previousOutput, Restarted};
        }
        case CBChainState::Stop: {
          stbds_arrsetlen(context->stack, sidx);
          return {chain->previousOutput, Stopped};
        }
        case CBChainState::Return: {
          stbds_arrsetlen(context->stack, sidx);
          // Use input as output, return previous block result
          return {input, Restarted};
        }
        case CBChainState::Rebase:
          // Rebase means we need to put back main input
          input = chainInput;
          break;
        case CBChainState::Continue:
          break;
        }
      }
    } catch (boost::context::detail::forced_unwind const &e) {
      throw; // required for Boost Coroutine!
    } catch (const std::exception &e) {
      LOG(ERROR) << "Block activation error, failed block: "
                 << std::string(blk->name(blk));
      LOG(ERROR) << e.what();
      stbds_arrsetlen(context->stack, sidx);
      return {chain->previousOutput, Failed};
    } catch (...) {
      LOG(ERROR) << "Block activation error, failed block: "
                 << std::string(blk->name(blk));
      stbds_arrsetlen(context->stack, sidx);
      return {chain->previousOutput, Failed};
    }
  }

  stbds_arrsetlen(context->stack, sidx);
  return {chain->previousOutput, Running};
}

boost::context::continuation run(CBChain *chain,
                                 boost::context::continuation &&sink) {
  auto running = true;
  // Reset return state
  chain->returned = false;
  // Clean previous output if we had one
  if (chain->ownedOutput) {
    destroyVar(chain->finishedOutput);
    chain->ownedOutput = false;
  }
  // Reset error
  chain->failed = false;
  // Create a new context and copy the sink in
  CBContext context(std::move(sink), chain);

  // We prerolled our coro, suspend here before actually starting.
  // This allows us to allocate the stack ahead of time.
  context.continuation = context.continuation.resume();
  if (context.aborted) // We might have stopped before even starting!
    goto endOfChain;

  while (running) {
    running = chain->looped;
    context.restarted = false; // Remove restarted flag

    chain->finished = false; // Reset finished flag (atomic)
    auto runRes = runChain(chain, &context, chain->rootTickInput);
    chain->finishedOutput = runRes.output; // Write result before setting flag
    chain->finished = true;                // Set finished flag (atomic)
    context.iterationCount++;              // increatse iteration counter
    stbds_arrsetlen(context.stack, 0);     // clear the stack
    if (unlikely(runRes.state == Failed)) {
      chain->failed = true;
      context.aborted = true;
      break;
    } else if (unlikely(runRes.state == Stopped)) {
      context.aborted = true;
      break;
    }

    if (!chain->unsafe && chain->looped) {
      // Ensure no while(true), yield anyway every run
      context.next = Duration(0);
      context.continuation = context.continuation.resume();
      // This is delayed upon continuation!!
      if (context.aborted)
        break;
    }
  }

endOfChain:
  // Copy the output variable since the next call might wipe it
  auto tmp = chain->finishedOutput;
  // Reset it, we are not sure on the internal state
  chain->finishedOutput = {};
  chain->ownedOutput = true;
  cloneVar(chain->finishedOutput, tmp);

  // run cleanup on all the blocks
  cleanup(chain);

  // Need to take care that we might have stopped the chain very early due to
  // errors and the next eventual stop() should avoid resuming
  chain->returned = true;
  return std::move(context.continuation);
}
}; // namespace chainblocks

#ifdef TESTING
static CBChain mainChain("MainChain");

int main() {
  auto blk = chainblocks::createBlock("SetTableValue");
  LOG(INFO) << blk->name(blk);
  LOG(INFO) << blk->exposedVariables(blk);
}
#endif
