#define runChainPOSTCHAIN for(auto blk : chain->blocks)\
{\
  try\
  {\
    blk->postChain(blk, context);\
  }\
  catch(const std::exception& e)\
  {\
    std::cerr << "Post chain failure, failed block: " << std::string(blk->name(blk)) << "\n";\
    if(context->error.length() > 0)\
      std::cerr << "Last error: " << std::string(context->error) << "\n";\
    std::cerr << e.what() << "\n";\
    return { false, previousOutput };\
  }\
  catch(...)\
  {\
    std::cerr << "Post chain failure, failed block: " << std::string(blk->name(blk)) << "\n";\
    if(context->error.length() > 0)\
      std::cerr << "Last error: " << std::string(context->error) << "\n";\
    return { false, previousOutput };\
  }\
}

#define _runChainINLINECMATH(__cfunc, __cfuncf, __cfunc_str, __input, __output)\
switch(__input.valueType)\
{\
  case Float:\
    __output.valueType = Float;\
    __output.floatValue = __cfunc(__input.floatValue);\
    break;\
  case Float2:\
    __output.valueType = Float2;\
    __output.float2Value[0] = __cfunc(__input.float2Value[0]);\
    __output.float2Value[1] = __cfunc(__input.float2Value[1]);\
    break;\
  case Float3:\
    __output.valueType = Float3;\
    __output.float3Value[0] = __cfuncf(__input.float3Value[0]);\
    __output.float3Value[1] = __cfuncf(__input.float3Value[1]);\
    __output.float3Value[2] = __cfuncf(__input.float3Value[2]);\
    break;\
  case Float4:\
    __output.valueType = Float4;\
    __output.float4Value[0] = __cfuncf(__input.float4Value[0]);\
    __output.float4Value[1] = __cfuncf(__input.float4Value[1]);\
    __output.float4Value[2] = __cfuncf(__input.float4Value[2]);\
    __output.float4Value[3] = __cfuncf(__input.float4Value[3]);\
    break;\
  default:\
    throw CBException(__cfunc_str " operation not supported between given types!");\
}

#define runChainINLINECMATH(__cfunc, __cfuncf, __cfunc_str)\
if(unlikely(input.valueType == Seq))\
{\
  stbds_arrsetlen(cblock->seqCache, 0);\
  for(auto i = 0; i < stbds_arrlen(input.seqValue); i++)\
  {\
    CBVar tmp;\
    _runChainINLINECMATH(__cfunc, __cfuncf, __cfunc_str, input.seqValue[i], tmp)\
    stbds_arrpush(cblock->seqCache, tmp);\
  }\
  previousOutput.valueType = Seq;\
  previousOutput.seqValue = cblock->seqCache;\
}\
else\
{\
  _runChainINLINECMATH(__cfunc, __cfuncf, __cfunc_str, input, previousOutput)\
}

#define _runChainINLINEMATH(__op, __op_str, __input, __output)\
auto operand = cblock->operand.valueType == ContextVar ?\
  cblock->ctxOperand ? *cblock->ctxOperand : *(cblock->ctxOperand = contextVariable(context, cblock->operand.stringValue)) :\
    cblock->operand;\
if(unlikely(__input.valueType != operand.valueType))\
{\
  throw CBException(__op_str " not supported between different types!");\
}\
else if(unlikely(operand.valueType == None))\
{\
  throw CBException("Could not find the operand variable!");\
}\
else\
{\
  switch(__input.valueType)\
  {\
    case Int:\
      __output.valueType = Int;\
      __output.intValue = __input.intValue __op operand.intValue;\
      break;\
    case Int2:\
      __output.valueType = Int2;\
      __output.int2Value = __input.int2Value __op operand.int2Value;\
      break;\
    case Int3:\
      __output.valueType = Int3;\
      __output.int3Value = __input.int3Value __op operand.int3Value;\
      break;\
    case Int4:\
      __output.valueType = Int4;\
      __output.int4Value = __input.int4Value __op operand.int4Value;\
      break;\
    case Float:\
      __output.valueType = Float;\
      __output.floatValue = __input.floatValue __op operand.floatValue;\
      break;\
    case Float2:\
      __output.valueType = Float2;\
      __output.float2Value = __input.float2Value __op operand.float2Value;\
      break;\
    case Float3:\
      __output.valueType = Float3;\
      __output.float3Value = __input.float3Value __op operand.float3Value;\
      break;\
    case Float4:\
      __output.valueType = Float4;\
      __output.float4Value = __input.float4Value __op operand.float4Value;\
      break;\
    case Color:\
      __output.valueType = Color;\
      __output.colorValue.r = __input.colorValue.r __op operand.colorValue.r;\
      __output.colorValue.g = __input.colorValue.g __op operand.colorValue.g;\
      __output.colorValue.b = __input.colorValue.b __op operand.colorValue.b;\
      __output.colorValue.a = __input.colorValue.a __op operand.colorValue.a;\
      break;\
    default:\
      throw CBException(__op_str " operation not supported between given types!");\
  }\
}

#define runChainINLINEMATH(__op, __op_str)\
if(unlikely(input.valueType == Seq))\
{\
  stbds_arrsetlen(cblock->seqCache, 0);\
  for(auto i = 0; i < stbds_arrlen(input.seqValue); i++)\
  {\
    CBVar tmp;\
    _runChainINLINEMATH(__op, __op_str, input.seqValue[i], tmp)\
    stbds_arrpush(cblock->seqCache, tmp);\
  }\
  previousOutput.valueType = Seq;\
  previousOutput.seqValue = cblock->seqCache;\
}\
else\
{\
  _runChainINLINEMATH(__op, __op_str, input, previousOutput)\
}

#define __runChainINLINE_INT_MATH(__op, __op_str, __input, __output)\
auto operand = cblock->operand.valueType == ContextVar ?\
  cblock->ctxOperand ? *cblock->ctxOperand : *(cblock->ctxOperand = contextVariable(context, cblock->operand.stringValue)) :\
    cblock->operand;\
if(unlikely(__input.valueType != operand.valueType))\
{\
  throw CBException(__op_str " not supported between different types!");\
}\
else if(unlikely(operand.valueType == None))\
{\
  throw CBException("Could not find the operand variable!");\
}\
else\
{\
  switch(__input.valueType)\
  {\
    case Int:\
      __output.valueType = Int;\
      __output.intValue = __input.intValue __op operand.intValue;\
      break;\
    case Int2:\
      __output.valueType = Int2;\
      __output.int2Value = __input.int2Value __op operand.int2Value;\
      break;\
    case Int3:\
      __output.valueType = Int3;\
      __output.int3Value = __input.int3Value __op operand.int3Value;\
      break;\
    case Int4:\
      __output.valueType = Int4;\
      __output.int4Value = __input.int4Value __op operand.int4Value;\
      break;\
    case Color:\
      __output.valueType = Color;\
      __output.colorValue.r = __input.colorValue.r __op operand.colorValue.r;\
      __output.colorValue.g = __input.colorValue.g __op operand.colorValue.g;\
      __output.colorValue.b = __input.colorValue.b __op operand.colorValue.b;\
      __output.colorValue.a = __input.colorValue.a __op operand.colorValue.a;\
      break;\
    default:\
      throw CBException(__op_str " operation not supported between given types!");\
  }\
}

#define runChainINLINE_INT_MATH(__op, __op_str)\
if(unlikely(input.valueType == Seq))\
{\
  stbds_arrsetlen(cblock->seqCache, 0);\
  for(auto i = 0; i < stbds_arrlen(input.seqValue); i++)\
  {\
    CBVar tmp;\
    __runChainINLINE_INT_MATH(__op, __op_str, input.seqValue[i], tmp)\
    stbds_arrpush(cblock->seqCache, tmp);\
  }\
  previousOutput.valueType = Seq;\
  previousOutput.seqValue = cblock->seqCache;\
}\
else\
{\
  __runChainINLINE_INT_MATH(__op, __op_str, input, previousOutput)\
}

#define runChainQUICKRUN(__chain)\
auto chainRes = runChain(__chain, context, input);\
if(unlikely(!std::get<0>(chainRes)))\
{ \
  previousOutput.valueType = None;\
  previousOutput.chainState = Stop;\
}\
else if(unlikely(context->restarted))\
{\
  previousOutput.valueType = None;\
  previousOutput.chainState = Restart;\
}
