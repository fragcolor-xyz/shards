#ifndef B7638520_BA4D_4989_BDC2_7F3533FE84B5
#define B7638520_BA4D_4989_BDC2_7F3533FE84B5

#include <shards/shards.h>

namespace shards {
void freeInterfaceVariable(SHInterfaceVariable &ivar);
SHInterfaceVariable cloneInterfaceVariable(const SHInterfaceVariable &other);
void freeInterface(SHInterface &interface_);
SHInterface cloneInterface(const SHInterface &other);
} // namespace shards

#endif /* B7638520_BA4D_4989_BDC2_7F3533FE84B5 */
