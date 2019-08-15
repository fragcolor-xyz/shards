#define runChainPOSTCHAIN                                                      \
  for (auto blk : chain->blocks) {                                             \
    if (blk->postChain) {                                                      \
      try {                                                                    \
        blk->postChain(blk, context);                                          \
      } catch (boost::context::detail::forced_unwind const &e) {               \
        throw;                                                                 \
      } catch (const std::exception &e) {                                      \
        LOG(ERROR) << "Post chain failure, failed block: "                     \
                   << std::string(blk->name(blk));                             \
        if (context->error.length() > 0)                                       \
          LOG(ERROR) << "Last error: " << std::string(context->error);         \
        LOG(ERROR) << e.what() << "\n";                                        \
        return {previousOutput, Failed};                                       \
      } catch (...) {                                                          \
        LOG(ERROR) << "Post chain failure, failed block: "                     \
                   << std::string(blk->name(blk));                             \
        if (context->error.length() > 0)                                       \
          LOG(ERROR) << "Last error: " << std::string(context->error);         \
        return {previousOutput, Failed};                                       \
      }                                                                        \
    }                                                                          \
  }

#define _runChainINLINECMATH(__cfunc, __cfuncf, __cfunc_str, __input,          \
                             __output)                                         \
  switch ((__input).valueType) {                                               \
  case Float:                                                                  \
    (__output).valueType = Float;                                              \
    (__output).payload.floatValue = __cfunc((__input).payload.floatValue);     \
    break;                                                                     \
  case Float2:                                                                 \
    (__output).valueType = Float2;                                             \
    (__output).payload.float2Value[0] =                                        \
        __cfunc((__input).payload.float2Value[0]);                             \
    (__output).payload.float2Value[1] =                                        \
        __cfunc((__input).payload.float2Value[1]);                             \
    break;                                                                     \
  case Float3:                                                                 \
    (__output).valueType = Float3;                                             \
    (__output).payload.float3Value[0] =                                        \
        __cfuncf((__input).payload.float3Value[0]);                            \
    (__output).payload.float3Value[1] =                                        \
        __cfuncf((__input).payload.float3Value[1]);                            \
    (__output).payload.float3Value[2] =                                        \
        __cfuncf((__input).payload.float3Value[2]);                            \
    break;                                                                     \
  case Float4:                                                                 \
    (__output).valueType = Float4;                                             \
    (__output).payload.float4Value[0] =                                        \
        __cfuncf((__input).payload.float4Value[0]);                            \
    (__output).payload.float4Value[1] =                                        \
        __cfuncf((__input).payload.float4Value[1]);                            \
    (__output).payload.float4Value[2] =                                        \
        __cfuncf((__input).payload.float4Value[2]);                            \
    (__output).payload.float4Value[3] =                                        \
        __cfuncf((__input).payload.float4Value[3]);                            \
    break;                                                                     \
  default:                                                                     \
    throw CBException(__cfunc_str                                              \
                      " operation not supported between given types!");        \
  }

#define runChainINLINECMATH(__cfunc, __cfuncf, __cfunc_str)                    \
  if (unlikely(input.valueType == Seq)) {                                      \
    stbds_arrsetlen(cblock->seqCache, 0);                                      \
    for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {          \
      CBVar tmp;                                                               \
      _runChainINLINECMATH(__cfunc, __cfuncf, __cfunc_str,                     \
                           input.payload.seqValue[i], tmp)                     \
          stbds_arrpush(cblock->seqCache, tmp);                                \
    }                                                                          \
    previousOutput.valueType = Seq;                                            \
    previousOutput.payload.seqValue = cblock->seqCache;                        \
    previousOutput.payload.seqLen = -1;                                        \
  } else {                                                                     \
    _runChainINLINECMATH(__cfunc, __cfuncf, __cfunc_str, input,                \
                         previousOutput)                                       \
  }

#define _runChainINLINEMATH(__op, __op_str, __input, __output)                 \
  auto operand =                                                               \
      cblock->operand.valueType == ContextVar                                  \
          ? cblock->ctxOperand                                                 \
                ? *cblock->ctxOperand                                          \
                : *(cblock->ctxOperand = contextVariable(                      \
                        context, cblock->operand.payload.stringValue))         \
          : cblock->operand;                                                   \
  if (unlikely((__input).valueType != operand.valueType)) {                    \
    throw CBException(__op_str " not supported between different types!");     \
  } else if (unlikely(operand.valueType == None)) {                            \
    throw CBException("Could not find the operand variable!");                 \
  } else {                                                                     \
    switch ((__input).valueType) {                                             \
    case Int:                                                                  \
      (__output).valueType = Int;                                              \
      (__output).payload.intValue =                                            \
          (__input).payload.intValue __op operand.payload.intValue;            \
      break;                                                                   \
    case Int2:                                                                 \
      (__output).valueType = Int2;                                             \
      (__output).payload.int2Value =                                           \
          (__input).payload.int2Value __op operand.payload.int2Value;          \
      break;                                                                   \
    case Int3:                                                                 \
      (__output).valueType = Int3;                                             \
      (__output).payload.int3Value =                                           \
          (__input).payload.int3Value __op operand.payload.int3Value;          \
      break;                                                                   \
    case Int4:                                                                 \
      (__output).valueType = Int4;                                             \
      (__output).payload.int4Value =                                           \
          (__input).payload.int4Value __op operand.payload.int4Value;          \
      break;                                                                   \
    case Int8:                                                                 \
      (__output).valueType = Int8;                                             \
      (__output).payload.int8Value =                                           \
          (__input).payload.int8Value __op operand.payload.int8Value;          \
      break;                                                                   \
    case Int16:                                                                \
      (__output).valueType = Int16;                                            \
      (__output).payload.int16Value =                                          \
          (__input).payload.int16Value __op operand.payload.int16Value;        \
      break;                                                                   \
    case Float:                                                                \
      (__output).valueType = Float;                                            \
      (__output).payload.floatValue =                                          \
          (__input).payload.floatValue __op operand.payload.floatValue;        \
      break;                                                                   \
    case Float2:                                                               \
      (__output).valueType = Float2;                                           \
      (__output).payload.float2Value =                                         \
          (__input).payload.float2Value __op operand.payload.float2Value;      \
      break;                                                                   \
    case Float3:                                                               \
      (__output).valueType = Float3;                                           \
      (__output).payload.float3Value =                                         \
          (__input).payload.float3Value __op operand.payload.float3Value;      \
      break;                                                                   \
    case Float4:                                                               \
      (__output).valueType = Float4;                                           \
      (__output).payload.float4Value =                                         \
          (__input).payload.float4Value __op operand.payload.float4Value;      \
      break;                                                                   \
    case Color:                                                                \
      (__output).valueType = Color;                                            \
      (__output).payload.colorValue.r =                                        \
          (__input).payload.colorValue.r __op operand.payload.colorValue.r;    \
      (__output).payload.colorValue.g =                                        \
          (__input).payload.colorValue.g __op operand.payload.colorValue.g;    \
      (__output).payload.colorValue.b =                                        \
          (__input).payload.colorValue.b __op operand.payload.colorValue.b;    \
      (__output).payload.colorValue.a =                                        \
          (__input).payload.colorValue.a __op operand.payload.colorValue.a;    \
      break;                                                                   \
    default:                                                                   \
      throw CBException(__op_str                                               \
                        " operation not supported between given types!");      \
    }                                                                          \
  }

#define runChainINLINEMATH(__op, __op_str)                                     \
  if (unlikely(input.valueType == Seq)) {                                      \
    stbds_arrsetlen(cblock->seqCache, 0);                                      \
    for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {          \
      CBVar tmp;                                                               \
      _runChainINLINEMATH(__op, __op_str, input.payload.seqValue[i], tmp)      \
          stbds_arrpush(cblock->seqCache, tmp);                                \
    }                                                                          \
    previousOutput.valueType = Seq;                                            \
    previousOutput.payload.seqValue = cblock->seqCache;                        \
    previousOutput.payload.seqLen = -1;                                        \
  } else {                                                                     \
    _runChainINLINEMATH(__op, __op_str, input, previousOutput)                 \
  }

#define __runChainINLINE_INT_MATH(__op, __op_str, __input, __output)           \
  auto operand =                                                               \
      cblock->operand.valueType == ContextVar                                  \
          ? cblock->ctxOperand                                                 \
                ? *cblock->ctxOperand                                          \
                : *(cblock->ctxOperand = contextVariable(                      \
                        context, cblock->operand.payload.stringValue))         \
          : cblock->operand;                                                   \
  if (unlikely((__input).valueType != operand.valueType)) {                    \
    throw CBException(__op_str " not supported between different types!");     \
  } else if (unlikely(operand.valueType == None)) {                            \
    throw CBException("Could not find the operand variable!");                 \
  } else {                                                                     \
    switch ((__input).valueType) {                                             \
    case Int:                                                                  \
      (__output).valueType = Int;                                              \
      (__output).payload.intValue =                                            \
          (__input).payload.intValue __op operand.payload.intValue;            \
      break;                                                                   \
    case Int2:                                                                 \
      (__output).valueType = Int2;                                             \
      (__output).payload.int2Value =                                           \
          (__input).payload.int2Value __op operand.payload.int2Value;          \
      break;                                                                   \
    case Int3:                                                                 \
      (__output).valueType = Int3;                                             \
      (__output).payload.int3Value =                                           \
          (__input).payload.int3Value __op operand.payload.int3Value;          \
      break;                                                                   \
    case Int4:                                                                 \
      (__output).valueType = Int4;                                             \
      (__output).payload.int4Value =                                           \
          (__input).payload.int4Value __op operand.payload.int4Value;          \
      break;                                                                   \
    case Int8:                                                                 \
      (__output).valueType = Int8;                                             \
      (__output).payload.int8Value =                                           \
          (__input).payload.int8Value __op operand.payload.int8Value;          \
      break;                                                                   \
    case Int16:                                                                \
      (__output).valueType = Int16;                                            \
      (__output).payload.int16Value =                                          \
          (__input).payload.int16Value __op operand.payload.int16Value;        \
      break;                                                                   \
    case Color:                                                                \
      (__output).valueType = Color;                                            \
      (__output).payload.colorValue.r =                                        \
          (__input).payload.colorValue.r __op operand.payload.colorValue.r;    \
      (__output).payload.colorValue.g =                                        \
          (__input).payload.colorValue.g __op operand.payload.colorValue.g;    \
      (__output).payload.colorValue.b =                                        \
          (__input).payload.colorValue.b __op operand.payload.colorValue.b;    \
      (__output).payload.colorValue.a =                                        \
          (__input).payload.colorValue.a __op operand.payload.colorValue.a;    \
      break;                                                                   \
    default:                                                                   \
      throw CBException(__op_str                                               \
                        " operation not supported between given types!");      \
    }                                                                          \
  }

#define runChainINLINE_INT_MATH(__op, __op_str)                                \
  if (unlikely(input.valueType == Seq)) {                                      \
    stbds_arrsetlen(cblock->seqCache, 0);                                      \
    for (auto i = 0; i < stbds_arrlen(input.payload.seqValue); i++) {          \
      CBVar tmp;                                                               \
      __runChainINLINE_INT_MATH(__op, __op_str, input.payload.seqValue[i],     \
                                tmp) stbds_arrpush(cblock->seqCache, tmp);     \
    }                                                                          \
    previousOutput.valueType = Seq;                                            \
    previousOutput.payload.seqValue = cblock->seqCache;                        \
    previousOutput.payload.seqLen = -1;                                        \
  } else {                                                                     \
    __runChainINLINE_INT_MATH(__op, __op_str, input, previousOutput)           \
  }

#define runChainQUICKRUN(__chain)                                              \
  auto chainRes = runChain(__chain, context, input);                           \
  if (unlikely(!std::get<0>(chainRes))) {                                      \
    previousOutput.valueType = None;                                           \
    previousOutput.payload.chainState = Stop;                                  \
  } else if (unlikely(context->restarted)) {                                   \
    previousOutput.valueType = None;                                           \
    previousOutput.payload.chainState = Restart;                               \
  }
