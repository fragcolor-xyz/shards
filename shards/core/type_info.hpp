#ifndef E46E13C8_B6C4_45A0_92E1_48F26F3C80BC
#define E46E13C8_B6C4_45A0_92E1_48F26F3C80BC

#include <shards/shards.h>
#include <vector>

namespace shards {
void freeDerivedInfo(SHTypeInfo info);
inline void freeTypeInfo(SHTypeInfo info) { freeDerivedInfo(info); }
SHTypeInfo deriveTypeInfo(const SHVar &value, const SHInstanceData &data, std::vector<SHExposedTypeInfo> *expInfo = nullptr,
                          bool resolveContextVariables = true);
SHTypeInfo cloneTypeInfo(const SHTypeInfo &other);
} // namespace shards

#endif /* E46E13C8_B6C4_45A0_92E1_48F26F3C80BC */
