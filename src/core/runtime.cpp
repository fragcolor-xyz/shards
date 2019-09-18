#define STB_DS_IMPLEMENTATION 1

#include "runtime.hpp"
#include "blocks/shared.hpp"
#include <boost/stacktrace.hpp>
#include <csignal>
#include <cstdarg>
#include <string.h>

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

void registerCoreBlocks() {
  registerBlocksCoreBlocks();
  registerAssertBlocks();
  registerChainsBlocks();
  registerLoggingBlocks();
  registerFlowBlocks();
  registerProcessBlocks();
  registerSeqsBlocks();
  registerCastingBlocks();
  registerSerializationBlocks();

  // also enums
  initEnums();
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

EXPORTED void __cdecl cbSetError(CBContext *context, const char *errorText) {
  context->setError(errorText);
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

void to_json(json &j, const CBVar &var) {
  auto valType = int(var.valueType);
  switch (var.valueType) {
  case Any:
  case EndOfBlittableTypes: {
    j = json{{"type", 0}, {"value", int(Continue)}};
    break;
  }
  case None: {
    j = json{{"type", 0}, {"value", int(var.payload.chainState)}};
    break;
  }
  case Object: {
    auto findIt = chainblocks::ObjectTypesRegister.find(
        std::make_tuple(var.payload.objectVendorId, var.payload.objectTypeId));
    if (findIt != chainblocks::ObjectTypesRegister.end() &&
        findIt->second.serialize) {
      j = json{{"type", valType},
               {"vendorId", var.payload.objectVendorId},
               {"typeId", var.payload.objectTypeId},
               {"value", findIt->second.serialize(var.payload.objectValue)}};
    } else {
      j = json{{"type", valType},
               {"vendorId", var.payload.objectVendorId},
               {"typeId", var.payload.objectTypeId},
               {"value", nullptr}};
    }
    break;
  }
  case Bool: {
    j = json{{"type", valType}, {"value", var.payload.boolValue}};
    break;
  }
  case Int: {
    j = json{{"type", valType}, {"value", var.payload.intValue}};
    break;
  }
  case Int2: {
    auto vec = {var.payload.int2Value[0], var.payload.int2Value[1]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Int3: {
    auto vec = {var.payload.int3Value[0], var.payload.int3Value[1],
                var.payload.int3Value[2]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Int4: {
    auto vec = {var.payload.int4Value[0], var.payload.int4Value[1],
                var.payload.int4Value[2], var.payload.int4Value[3]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Int8: {
    auto vec = {var.payload.int8Value[0], var.payload.int8Value[1],
                var.payload.int8Value[2], var.payload.int8Value[3],
                var.payload.int8Value[4], var.payload.int8Value[5],
                var.payload.int8Value[6], var.payload.int8Value[7]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Int16: {
    auto vec = {var.payload.int16Value[0],  var.payload.int16Value[1],
                var.payload.int16Value[2],  var.payload.int16Value[3],
                var.payload.int16Value[4],  var.payload.int16Value[5],
                var.payload.int16Value[6],  var.payload.int16Value[7],
                var.payload.int16Value[8],  var.payload.int16Value[9],
                var.payload.int16Value[10], var.payload.int16Value[11],
                var.payload.int16Value[12], var.payload.int16Value[13],
                var.payload.int16Value[14], var.payload.int16Value[15]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Float: {
    j = json{{"type", valType}, {"value", var.payload.floatValue}};
    break;
  }
  case Float2: {
    auto vec = {var.payload.float2Value[0], var.payload.float2Value[1]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Float3: {
    auto vec = {var.payload.float3Value[0], var.payload.float3Value[1],
                var.payload.float3Value[2]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case Float4: {
    auto vec = {var.payload.float4Value[0], var.payload.float4Value[1],
                var.payload.float4Value[2], var.payload.float4Value[3]};
    j = json{{"type", valType}, {"value", vec}};
    break;
  }
  case ContextVar:
  case String: {
    j = json{{"type", valType}, {"value", var.payload.stringValue}};
    break;
  }
  case Color: {
    j = json{{"type", valType},
             {"value",
              {var.payload.colorValue.r, var.payload.colorValue.g,
               var.payload.colorValue.b, var.payload.colorValue.a}}};
    break;
  }
  case Image: {
    if (var.payload.imageValue.data) {
      auto binsize = var.payload.imageValue.width *
                     var.payload.imageValue.height *
                     var.payload.imageValue.channels;
      std::vector<uint8_t> buffer(binsize);
      memcpy(&buffer[0], var.payload.imageValue.data, binsize);
      j = json{{"type", valType},
               {"width", var.payload.imageValue.width},
               {"height", var.payload.imageValue.height},
               {"channels", var.payload.imageValue.channels},
               {"data", buffer}};
    } else {
      j = json{{"type", 0}, {"value", int(Continue)}};
    }
    break;
  }
  case Enum: {
    j = json{{"type", valType},
             {"value", int32_t(var.payload.enumValue)},
             {"vendorId", var.payload.enumVendorId},
             {"typeId", var.payload.enumTypeId}};
    break;
  }
  case Seq: {
    std::vector<json> items;
    for (int i = 0; i < stbds_arrlen(var.payload.seqValue); i++) {
      auto &v = var.payload.seqValue[i];
      items.emplace_back(v);
    }
    j = json{{"type", valType}, {"values", items}};
    break;
  }
  case Table: {
    std::vector<json> items;
    for (int i = 0; i < stbds_arrlen(var.payload.tableValue); i++) {
      auto &v = var.payload.tableValue[i];
      items.push_back(json{{"key", v.key}, {"value", v.value}});
    }
    j = json{{"type", valType}, {"values", items}};
    break;
  }
  case Chain: {
    j = json{{"type", valType}, {"name", (var.payload.chainValue)->name}};
    break;
  }
  case Block: {
    auto blk = var.payload.blockValue;
    std::vector<json> params;
    auto paramsDesc = blk->parameters(blk);
    for (int i = 0; i < stbds_arrlen(paramsDesc); i++) {
      auto &desc = paramsDesc[i];
      auto value = blk->getParam(blk, i);

      json param_obj = {{"name", desc.name}, {"value", value}};

      params.push_back(param_obj);
    }

    j = {{"type", valType}, {"name", blk->name(blk)}, {"params", params}};
    break;
  }
  };
}

void from_json(const json &j, CBVar &var) {
  auto valType = CBType(j.at("type").get<int>());
  switch (valType) {
  case Any:
  case EndOfBlittableTypes: {
    var = {};
    break;
  }
  case None: {
    var.valueType = None;
    var.payload.chainState = CBChainState(j.at("value").get<int>());
    break;
  }
  case Object: {
    auto vendorId = j.at("vendorId").get<int32_t>();
    auto typeId = j.at("typeId").get<int32_t>();
    if (!j.at("value").is_null()) {
      auto value = j.at("value").get<std::string>();
      auto findIt = chainblocks::ObjectTypesRegister.find(
          std::make_tuple(vendorId, typeId));
      if (findIt != chainblocks::ObjectTypesRegister.end() &&
          findIt->second.deserialize) {
        var.payload.objectValue =
            findIt->second.deserialize(const_cast<CBString>(value.c_str()));
      } else {
        throw chainblocks::CBException(
            "Failed to find a custom object deserializer.");
      }
    } else {
      var.payload.objectValue = nullptr;
    }
    var.payload.objectVendorId = vendorId;
    var.payload.objectTypeId = typeId;
    break;
  }
  case Bool: {
    var.valueType = Bool;
    var.payload.boolValue = j.at("value").get<bool>();
    break;
  }
  case Int: {
    var.valueType = Int;
    var.payload.intValue = j.at("value").get<int64_t>();
    break;
  }
  case Int2: {
    var.valueType = Int2;
    var.payload.int2Value[0] = j.at("value")[0].get<int64_t>();
    var.payload.int2Value[1] = j.at("value")[1].get<int64_t>();
    break;
  }
  case Int3: {
    var.valueType = Int3;
    var.payload.int3Value[0] = j.at("value")[0].get<int32_t>();
    var.payload.int3Value[1] = j.at("value")[1].get<int32_t>();
    var.payload.int3Value[2] = j.at("value")[2].get<int32_t>();
    break;
  }
  case Int4: {
    var.valueType = Int4;
    var.payload.int4Value[0] = j.at("value")[0].get<int32_t>();
    var.payload.int4Value[1] = j.at("value")[1].get<int32_t>();
    var.payload.int4Value[2] = j.at("value")[2].get<int32_t>();
    var.payload.int4Value[3] = j.at("value")[3].get<int32_t>();
    break;
  }
  case Int8: {
    var.valueType = Int8;
    for (auto i = 0; i < 8; i++) {
      var.payload.int8Value[i] = j.at("value")[i].get<int16_t>();
    }
    break;
  }
  case Int16: {
    var.valueType = Int16;
    for (auto i = 0; i < 16; i++) {
      var.payload.int16Value[i] = j.at("value")[i].get<int8_t>();
    }
    break;
  }
  case Float: {
    var.valueType = Float;
    var.payload.floatValue = j.at("value").get<double>();
    break;
  }
  case Float2: {
    var.valueType = Float2;
    var.payload.float2Value[0] = j.at("value")[0].get<double>();
    var.payload.float2Value[1] = j.at("value")[1].get<double>();
    break;
  }
  case Float3: {
    var.valueType = Float3;
    var.payload.float3Value[0] = j.at("value")[0].get<float>();
    var.payload.float3Value[1] = j.at("value")[1].get<float>();
    var.payload.float3Value[2] = j.at("value")[2].get<float>();
    break;
  }
  case Float4: {
    var.valueType = Float4;
    var.payload.float4Value[0] = j.at("value")[0].get<float>();
    var.payload.float4Value[1] = j.at("value")[1].get<float>();
    var.payload.float4Value[2] = j.at("value")[2].get<float>();
    var.payload.float4Value[3] = j.at("value")[3].get<float>();
    break;
  }
  case ContextVar: {
    var.valueType = ContextVar;
    auto strVal = j.at("value").get<std::string>();
    var.payload.stringValue = new char[strVal.length() + 1];
    memset((void *)var.payload.stringValue, 0x0, strVal.length() + 1);
    memcpy((void *)var.payload.stringValue, strVal.c_str(), strVal.length());
    break;
  }
  case String: {
    var.valueType = String;
    auto strVal = j.at("value").get<std::string>();
    var.payload.stringValue = new char[strVal.length() + 1];
    memset((void *)var.payload.stringValue, 0x0, strVal.length() + 1);
    memcpy((void *)var.payload.stringValue, strVal.c_str(), strVal.length());
    break;
  }
  case Color: {
    var.valueType = Color;
    var.payload.colorValue.r = j.at("value")[0].get<uint8_t>();
    var.payload.colorValue.g = j.at("value")[1].get<uint8_t>();
    var.payload.colorValue.b = j.at("value")[2].get<uint8_t>();
    var.payload.colorValue.a = j.at("value")[3].get<uint8_t>();
    break;
  }
  case Image: {
    var.valueType = Image;
    var.payload.imageValue.width = j.at("width").get<int32_t>();
    var.payload.imageValue.height = j.at("height").get<int32_t>();
    var.payload.imageValue.channels = j.at("channels").get<int32_t>();
    var.payload.imageValue.data = nullptr;
    alloc(var.payload.imageValue);
    auto buffer = j.at("data").get<std::vector<uint8_t>>();
    auto binsize = var.payload.imageValue.width *
                   var.payload.imageValue.height *
                   var.payload.imageValue.channels;
    memcpy(var.payload.imageValue.data, &buffer[0], binsize);
    break;
  }
  case Enum: {
    var.valueType = Enum;
    var.payload.enumValue = CBEnum(j.at("value").get<int32_t>());
    var.payload.enumVendorId = CBEnum(j.at("vendorId").get<int32_t>());
    var.payload.enumTypeId = CBEnum(j.at("typeId").get<int32_t>());
    break;
  }
  case Seq: {
    var.valueType = Seq;
    auto items = j.at("values").get<std::vector<json>>();
    var.payload.seqValue = nullptr;
    for (const auto &item : items) {
      stbds_arrpush(var.payload.seqValue, item.get<CBVar>());
    }
    break;
  }
  case Table: {
    var.valueType = Seq;
    auto items = j.at("values").get<std::vector<json>>();
    var.payload.tableValue = nullptr;
    stbds_shdefault(var.payload.tableValue, CBVar());
    for (const auto &item : items) {
      auto key = item.at("key").get<std::string>();
      auto value = item.at("value").get<CBVar>();
      CBNamedVar named{};
      named.key = new char[key.length() + 1];
      memset((void *)named.key, 0x0, key.length() + 1);
      memcpy((char *)named.key, key.c_str(), key.length());
      named.value = value;
      stbds_shputs(var.payload.tableValue, named);
    }
    break;
  }
  case Chain: {
    var.valueType = Chain;
    auto chainName = j.at("name").get<std::string>();
    auto findIt = chainblocks::GlobalChains.find(chainName);
    if (findIt != chainblocks::GlobalChains.end()) {
      var.payload.chainValue = findIt->second;
    } else {
      // create it anyway, deserialize when we can
      var.payload.chainValue = new CBChain(chainName.c_str());
      chainblocks::GlobalChains[chainName] = var.payload.chainValue;
    }
    break;
  }
  case Block: {
    var.valueType = Block;
    auto blkname = j.at("name").get<std::string>();
    auto blk = chainblocks::createBlock(blkname.c_str());
    if (!blk) {
      auto errmsg = "Failed to create block of type: " + std::string("blkname");
      throw chainblocks::CBException(errmsg.c_str());
    }
    var.payload.blockValue = blk;

    // Setup
    blk->setup(blk);

    // Set params
    auto jparams = j.at("params");
    auto blkParams = blk->parameters(blk);
    for (auto jparam : jparams) {
      auto paramName = jparam.at("name").get<std::string>();
      auto value = jparam.at("value").get<CBVar>();

      if (value.valueType != None) {
        for (auto i = 0; stbds_arrlen(blkParams) > i; i++) {
          auto &paramInfo = blkParams[i];
          if (paramName == paramInfo.name) {
            blk->setParam(blk, i, value);
            break;
          }
        }
      }

      // Assume block copied memory internally so we can clean up here!!!
      releaseMemory(value);
    }
    break;
  }
  }
}

void to_json(json &j, const CBChainPtr &chain) {
  std::vector<json> blocks;
  for (auto blk : chain->blocks) {
    std::vector<json> params;
    auto paramsDesc = blk->parameters(blk);
    for (int i = 0; stbds_arrlen(paramsDesc) > i; i++) {
      auto &desc = paramsDesc[i];
      auto value = blk->getParam(blk, i);

      json param_obj = {{"name", desc.name}, {"value", value}};

      params.push_back(param_obj);
    }

    json block_obj = {{"name", blk->name(blk)}, {"params", params}};

    blocks.push_back(block_obj);
  }

  j = {
      {"blocks", blocks},        {"name", chain->name},
      {"looped", chain->looped}, {"unsafe", chain->unsafe},
      {"version", 0.1},
  };
}

void from_json(const json &j, CBChainPtr &chain) {
  auto chainName = j.at("name").get<std::string>();
  auto findIt = chainblocks::GlobalChains.find(chainName);
  if (findIt != chainblocks::GlobalChains.end()) {
    chain = findIt->second;
    // Need to clean it up for rewrite!
    chain->cleanup();
  } else {
    chain = new CBChain(chainName.c_str());
    chainblocks::GlobalChains["chainName"] = chain;
  }

  chain->looped = j.at("looped").get<bool>();
  chain->unsafe = j.at("unsafe").get<bool>();

  auto jblocks = j.at("blocks");
  for (auto jblock : jblocks) {
    auto blkname = jblock.at("name").get<std::string>();
    auto blk = chainblocks::createBlock(blkname.c_str());
    if (!blk) {
      auto errmsg = "Failed to create block of type: " + std::string(blkname);
      throw chainblocks::CBException(errmsg.c_str());
    }

    // Setup
    blk->setup(blk);

    // Set params
    auto jparams = jblock.at("params");
    auto blkParams = blk->parameters(blk);
    for (auto jparam : jparams) {
      auto paramName = jparam.at("name").get<std::string>();
      auto value = jparam.at("value").get<CBVar>();

      if (value.valueType != None) {
        for (auto i = 0; stbds_arrlen(blkParams) > i; i++) {
          auto &paramInfo = blkParams[i];
          if (paramName == paramInfo.name) {
            blk->setParam(blk, i, value);
            break;
          }
        }
      }

      // Assume block copied memory internally so we can clean up here!!!
      releaseMemory(value);
    }

    // From now on this chain owns the block
    chain->addBlock(blk);
  }
}

bool matchTypes(const CBTypeInfo &exposedType, const CBTypeInfo &consumedType,
                bool isParameter, bool strict = true) {
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
                        isParameter)) {
          return false;
        }
      } else if (exposedType.seqType == nullptr ||
                 consumedType.seqType == nullptr) {
        return false;
      }
    }
    break;
  }
  case Table: {
    if (strict) {
      auto atypes = stbds_arrlen(exposedType.tableTypes);
      auto btypes = stbds_arrlen(consumedType.tableTypes);
      for (auto i = 0; i < atypes; i++) {
        // Go thru all exposed types and make sure we get a positive match with
        // the consumer
        auto atype = exposedType.tableTypes[i];
        auto matched = false;
        for (auto y = 0; y < btypes; y++) {
          auto btype = consumedType.tableTypes[y];
          if (matchTypes(atype, btype, isParameter)) {
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

struct ValidationContext {
  phmap::flat_hash_map<std::string, std::vector<CBExposedTypeInfo>> exposed;

  CBTypeInfo previousOutputType{};

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
    if (matchTypes(previousOutput, inputInfo, false)) {
      inputMatches = true;
      break;
    }
  }

  if (!inputMatches) {
    std::string err("Could not find a matching input type");
    ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
  }

  auto consumedVar = ctx.bottom->consumedVariables(ctx.bottom);

  // make sure we have the vars we need, collect first
  for (auto i = 0; stbds_arrlen(consumedVar) > i; i++) {
    auto &consumed_param = consumedVar[i];
    std::string name(consumed_param.name);
    auto findIt = ctx.exposed.find(name);
    if (findIt == ctx.exposed.end()) {
      std::string err("Required consumed variable not found: " + name);
      // Warning only, delegate inferTypes to decide
      ctx.cb(ctx.bottom, err.c_str(), true, ctx.userData);
    } else {
      auto matching = false;
      for (auto type : findIt->second) {
        auto exposedType = type.exposedType;
        auto requiredType = consumedVar[i].exposedType;
        // Finally deep compare types
        if (matchTypes(exposedType, requiredType, false, false)) {
          // We matched in non strict mode, meaning seq and table inner types
          // are ignored At this point this is ok, strict checking should happen
          // inside infer anyway
          matching = true;
          break;
        }
      }
      if (!matching) {
        std::string err(
            "Required consumed types do not match currently exposed ones: " +
            name);
        err += " exposed types:";
        for (auto info : ctx.exposed) {
          err += " (" + info.first + " [";

          for (auto type : info.second) {
            err += type2Name(type.exposedType.basicType) + " ";
          }
          err.erase(err.end() - 1);

          err += "])";
        }
        ctx.cb(ctx.bottom, err.c_str(), false, ctx.userData);
      }
    }
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
    auto findIt = ctx.exposed.find(name);
#if 0 // Well not sure.. but for now silent this...
    if (findIt != ctx.exposed.end()) {
      // Ignore tables and seqs, this behavior is a bit hard coded but makes
      // sense as they support "adding" items
      if (exposed_param.exposedType.basicType != Table &&
          exposed_param.exposedType.basicType != Seq) {
        // do we want to override it?, warn at least
        std::string err("Overriding already exposed variable: " + name);
        ctx.cb(ctx.bottom, err.c_str(), true, ctx.userData);
      }
    }
#endif
    // Tables expose multiple types
    if (exposed_param.exposedType.basicType == Table) {
      if (ctx.exposed[name].size() == 1 &&
          ctx.exposed[name][0].exposedType.basicType != Table)
        ctx.exposed[name].clear(); // reset in this case!
      ctx.exposed[name].push_back(exposed_param);
    } else {
      ctx.exposed[name] = {exposed_param};
    }
  }
}

CBValidationResult validateConnections(const std::vector<CBlock *> &chain,
                                       CBValidationCallback callback,
                                       void *userData, CBTypeInfo inputType,
                                       CBExposedTypesInfo consumables) {
  auto ctx = ValidationContext();
  ctx.previousOutputType = inputType;
  ctx.cb = callback;
  ctx.userData = userData;

  if (consumables) {
    for (auto i = 0; i < stbds_arrlen(consumables); i++) {
      auto &info = consumables[i];
      ctx.exposed[info.name].push_back(info);
    }
  }

  for (auto blk : chain) {
    ctx.bottom = blk;
    validateConnection(ctx);
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
    if (matchTypes(varType, param.valueTypes[i], true)) {
      return true; // we are good just exit
    }
  }

  // Failed until now but let's check if the type is a sequenced too
  if (value.valueType == Seq) {
    // Validate each type in the seq
    for (auto i = 0; stbds_arrlen(value.payload.seqValue) > i; i++) {
      if (validateSetParam(block, index, value.payload.seqValue[i], callback,
                           userData))
        return true;
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