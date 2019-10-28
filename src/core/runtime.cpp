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
phmap::node_hash_map<std::string, CBBlockConstructor> BlocksRegister;
phmap::node_hash_map<std::tuple<int32_t, int32_t>, CBObjectInfo>
    ObjectTypesRegister;
phmap::node_hash_map<std::tuple<int32_t, int32_t>, CBEnumInfo>
    EnumTypesRegister;
phmap::node_hash_map<std::string, CBCallback> ExitHooks;
phmap::node_hash_map<std::string, CBChain *> GlobalChains;
std::map<std::string, CBCallback> RunLoopHooks;

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

namespace Math {
namespace LinAlg {
extern void registerBlocks();
};
}; // namespace Math

#ifdef CB_WITH_EXTRAS
extern void cbInitExtras();
#endif

void registerCoreBlocks() {
#ifdef USE_RPMALLOC
  rpmalloc_initialize();
#endif

  assert(sizeof(CBVarPayload) == 16);
  assert(sizeof(CBVar) == 32);

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

  // also enums
  initEnums();

#ifdef CB_WITH_EXTRAS
  cbInitExtras();
#endif
}

CBlock *createBlock(const char *name) {
  auto it = BlocksRegister.find(name);
  if (it == BlocksRegister.end()) {
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
  } else if (strcmp(name, "Take") == 0) {
    blkp->inlineBlockId = CBInlineBlocks::CoreTake;
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
  auto findIt = BlocksRegister.find(cname);
  if (findIt == BlocksRegister.end()) {
    BlocksRegister.insert(std::make_pair(cname, constructor));
    // DLOG(INFO) << "added block: " << cname;
  } else {
    BlocksRegister[cname] = constructor;
    LOG(INFO) << "overridden block: " << cname;
  }
}

void registerObjectType(int32_t vendorId, int32_t typeId, CBObjectInfo info) {
  auto tup = std::make_tuple(vendorId, typeId);
  auto typeName = std::string(info.name);
  auto findIt = ObjectTypesRegister.find(tup);
  if (findIt == ObjectTypesRegister.end()) {
    ObjectTypesRegister.insert(std::make_pair(tup, info));
    // DLOG(INFO) << "added object type: " << typeName;
  } else {
    ObjectTypesRegister[tup] = info;
    LOG(INFO) << "overridden object type: " << typeName;
  }
}

void registerEnumType(int32_t vendorId, int32_t typeId, CBEnumInfo info) {
  auto tup = std::make_tuple(vendorId, typeId);
  auto typeName = std::string(info.name);
  auto findIt = ObjectTypesRegister.find(tup);
  if (findIt == ObjectTypesRegister.end()) {
    EnumTypesRegister.insert(std::make_pair(tup, info));
    // DLOG(INFO) << "added enum type: " << typeName;
  } else {
    EnumTypesRegister[tup] = info;
    LOG(INFO) << "overridden enum type: " << typeName;
  }
}

void registerRunLoopCallback(const char *eventName, CBCallback callback) {
  chainblocks::RunLoopHooks[eventName] = callback;
}

void unregisterRunLoopCallback(const char *eventName) {
  auto findIt = chainblocks::RunLoopHooks.find(eventName);
  if (findIt != chainblocks::RunLoopHooks.end()) {
    chainblocks::RunLoopHooks.erase(findIt);
  }
}

void registerExitCallback(const char *eventName, CBCallback callback) {
  chainblocks::ExitHooks[eventName] = callback;
}

void unregisterExitCallback(const char *eventName) {
  auto findIt = chainblocks::ExitHooks.find(eventName);
  if (findIt != chainblocks::ExitHooks.end()) {
    chainblocks::ExitHooks.erase(findIt);
  }
}

void registerChain(CBChain *chain) {
  chainblocks::GlobalChains[chain->name] = chain;
}

void unregisterChain(CBChain *chain) {
  auto findIt = chainblocks::GlobalChains.find(chain->name);
  if (findIt != chainblocks::GlobalChains.end()) {
    chainblocks::GlobalChains.erase(findIt);
  }
}

void callExitCallbacks() {
  // Iterate backwards
  for (auto it = chainblocks::ExitHooks.begin();
       it != chainblocks::ExitHooks.end(); ++it) {
    it->second();
  }
}

CBVar *contextVariable(CBContext *ctx, const char *name, bool global) {
  if (!global) {
    CBVar &v = ctx->variables[name];
    return &v;
  } else {
    CBVar &v = ctx->chain->node->variables[name];
    return &v;
  }
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
  for (auto i = 0; i < nblocks; i++) {
    activateBlock(blocks[i], context, input, output);
    if (output.valueType == None) {
      switch (output.payload.chainState) {
      case CBChainState::Restart: {
        return Continuing;
      }
      case CBChainState::Stop: {
        return Stopping;
      }
      case CBChainState::Return: {
        output = input; // Invert them, we return previous output (input)
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
  return Continuing;
}

FlowState activateBlocks(CBSeq blocks, CBContext *context,
                         const CBVar &chainInput, CBVar &output) {
  auto input = chainInput;
  for (auto i = 0; i < stbds_arrlen(blocks); i++) {
    activateBlock(blocks[i].payload.blockValue, context, input, output);
    if (output.valueType == None) {
      switch (output.payload.chainState) {
      case CBChainState::Restart: {
        return Continuing;
      }
      case CBChainState::Stop: {
        return Stopping;
      }
      case CBChainState::Return: {
        output = input; // Invert them, we return previous output (input)
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

#ifdef __cplusplus
extern "C" {
#endif

EXPORTED void __cdecl cbRegisterBlock(const char *fullName,
                                      CBBlockConstructor constructor) {
  chainblocks::registerBlock(fullName, constructor);
}

EXPORTED void __cdecl cbRegisterObjectType(int32_t vendorId, int32_t typeId,
                                           CBObjectInfo info) {
  chainblocks::registerObjectType(vendorId, typeId, info);
}

EXPORTED void __cdecl cbRegisterEnumType(int32_t vendorId, int32_t typeId,
                                         CBEnumInfo info) {
  chainblocks::registerEnumType(vendorId, typeId, info);
}

EXPORTED void __cdecl cbRegisterRunLoopCallback(const char *eventName,
                                                CBCallback callback) {
  chainblocks::registerRunLoopCallback(eventName, callback);
}

EXPORTED void __cdecl cbUnregisterRunLoopCallback(const char *eventName) {
  chainblocks::unregisterRunLoopCallback(eventName);
}

EXPORTED void __cdecl cbRegisterExitCallback(const char *eventName,
                                             CBCallback callback) {
  chainblocks::registerExitCallback(eventName, callback);
}

EXPORTED void __cdecl cbUnregisterExitCallback(const char *eventName) {
  chainblocks::unregisterExitCallback(eventName);
}

EXPORTED CBVar *__cdecl cbContextVariable(CBContext *context,
                                          const char *name) {
  return chainblocks::contextVariable(context, name);
}

EXPORTED void __cdecl cbThrowException(const char *errorText) {
  throw chainblocks::CBException(errorText);
}

EXPORTED int __cdecl cbContextState(CBContext *context) {
  if (context->aborted)
    return 1;
  return 0;
}

EXPORTED CBVar __cdecl cbSuspend(CBContext *context, double seconds) {
  return chainblocks::suspend(context, seconds);
}

EXPORTED void __cdecl cbCloneVar(CBVar *dst, const CBVar *src) {
  chainblocks::cloneVar(*dst, *src);
}

EXPORTED void __cdecl cbDestroyVar(CBVar *var) {
  chainblocks::destroyVar(*var);
}

EXPORTED __cdecl CBRunChainOutput
cbRunSubChain(CBChain *chain, CBContext *context, CBVar input) {
  return chainblocks::runSubChain(chain, context, input);
}

EXPORTED CBValidationResult __cdecl cbValidateChain(
    CBChain *chain, CBValidationCallback callback, void *userData,
    CBTypeInfo inputType) {
  return validateConnections(chain, callback, userData, inputType);
}

EXPORTED void __cdecl cbActivateBlock(CBlock *block, CBContext *context,
                                      CBVar *input, CBVar *output) {
  chainblocks::activateBlock(block, context, *input, *output);
}

EXPORTED CBValidationResult __cdecl cbValidateConnections(
    const CBlocks chain, CBValidationCallback callback, void *userData,
    CBTypeInfo inputType) {
  return validateConnections(chain, callback, userData, inputType);
}

EXPORTED void __cdecl cbFreeValidationResult(CBValidationResult result) {
  stbds_arrfree(result.exposedInfo);
}

EXPORTED void __cdecl cbLog(int level, const char *format, ...) {
  auto temp = std::vector<char>{};
  auto length = std::size_t{63};
  std::va_list args;
  while (temp.size() <= length) {
    temp.resize(length + 1);
    va_start(args, format);
    const auto status = std::vsnprintf(temp.data(), temp.size(), format, args);
    va_end(args);
    if (status < 0)
      throw std::runtime_error{"string formatting error"};
    length = static_cast<std::size_t>(status);
  }
  std::string str(temp.data(), length);

  switch (level) {
  case CB_INFO:
    LOG(INFO) << "Info: " << str;
    break;
  case CB_DEBUG:
    LOG(INFO) << "Debug: " << str;
    break;
  case CB_TRACE:
    LOG(INFO) << "Trace: " << str;
    break;
  }
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

  CBlock *bottom{};

  CBValidationCallback cb{};
  void *userData{};
};

struct ConsumedParam {
  const char *key;
  CBParameterInfo value;
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
  if (ctx.bottom->inferTypes) {
    CBExposedTypesInfo consumables = nullptr;
    // Pass all we got in the context!
    for (auto &info : ctx.exposed) {
      for (auto &type : info.second) {
        stbds_arrpush(consumables, type);
      }
    }
    // this ensures e.g. SetVariable exposedVars have right type from the actual
    // input type (previousOutput)!
    ctx.previousOutputType =
        ctx.bottom->inferTypes(ctx.bottom, previousOutput, consumables);

    stbds_arrfree(consumables);
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
      if (ctx.variables.contains(name)) {
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
      if (ctx.references.contains(name)) {
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
      if (ctx.references.contains(name)) {
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
      if (ctx.references.contains(name)) {
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

  // Finally do checks on what we consume
  auto consumedVar = ctx.bottom->consumedVariables(ctx.bottom);

  phmap::node_hash_map<std::string, std::vector<CBExposedTypeInfo>>
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
      if (name.find(' ') != -1) { // take only the first part of variable name
        // the remaining should be a table key which we don't care here
        name = name.substr(0, name.find(' '));
      }
      auto findIt = ctx.exposed.find(name);
      if (findIt == ctx.exposed.end()) {
        std::string err("Required consumed variable not found: " + name);
        // Warning only, delegate inferTypes to decide
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
      for (auto info : ctx.exposed) {
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
                                       void *userData, CBTypeInfo inputType,
                                       CBExposedTypesInfo consumables) {
  auto ctx = ValidationContext();
  ctx.originalInputType = inputType;
  ctx.previousOutputType = inputType;
  ctx.cb = callback;
  ctx.userData = userData;

  if (consumables) {
    for (auto i = 0; i < stbds_arrlen(consumables); i++) {
      auto &info = consumables[i];
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
                                       void *userData, CBTypeInfo inputType,
                                       CBExposedTypesInfo consumables) {
  return validateConnections(chain->blocks, callback, userData, inputType,
                             consumables);
}

CBValidationResult validateConnections(const CBlocks chain,
                                       CBValidationCallback callback,
                                       void *userData, CBTypeInfo inputType,
                                       CBExposedTypesInfo consumables) {
  std::vector<CBlock *> blocks;
  for (auto i = 0; stbds_arrlen(chain) > i; i++) {
    blocks.push_back(chain[i]);
  }
  return validateConnections(blocks, callback, userData, inputType,
                             consumables);
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
    for (auto i = 0; stbds_arrlen(value.payload.tableValue) > i; i++) {
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
}; // namespace chainblocks

#ifdef TESTING
static CBChain mainChain("MainChain");

int main() {
  auto blk = chainblocks::createBlock("SetTableValue");
  LOG(INFO) << blk->name(blk);
  LOG(INFO) << blk->exposedVariables(blk);
}
#endif
