#ifndef B7638520_BA4D_4989_BDC2_7F3533FE84B5
#define B7638520_BA4D_4989_BDC2_7F3533FE84B5

#include <shards/shards.h>

namespace shards {
void freeTraitVariable(SHTraitVariable &ivar);
SHTraitVariable cloneTraitVariable(const SHTraitVariable &other);
void freeTrait(SHTrait &trait_);
SHTrait cloneTrait(const SHTrait &other);
} // namespace shards

#endif /* B7638520_BA4D_4989_BDC2_7F3533FE84B5 */
