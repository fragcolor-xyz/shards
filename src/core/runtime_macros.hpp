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
