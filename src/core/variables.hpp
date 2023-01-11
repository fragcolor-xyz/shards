#ifndef A19120D7_FA47_4F4A_9240_ABB869FB7034
#define A19120D7_FA47_4F4A_9240_ABB869FB7034

#include "runtime.hpp"

namespace shards {
inline void mergeIntoRequiredVariables(ExposedInfo &outInfo, const ParamVar &paramVar, const SHTypeInfo &type,
                                       bool isMutable = false) {
  if (paramVar.isVariable())
    outInfo.push_back(ExposedInfo::Variable(paramVar.variableName(), SHCCSTR("The required variable"), type, isMutable));
}

inline void mergeIntoRequiredVariables(ExposedInfo &outInfo, const ShardsVar &shardsVar) {
  for (auto &info : shardsVar.composeResult().requiredInfo)
    outInfo.push_back(info);
}
} // namespace shards

#endif /* A19120D7_FA47_4F4A_9240_ABB869FB7034 */
