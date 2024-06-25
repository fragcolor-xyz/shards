#ifndef FE9BA2C2_C78A_4D68_B01F_99AC5628BA55
#define FE9BA2C2_C78A_4D68_B01F_99AC5628BA55

#include <shards/core/hasherxxh3.hpp>

namespace gfx {
using HasherDefaultVisitor = shards::HasherDefaultVisitor;
template <typename T = HasherDefaultVisitor> using HasherXXH3 = shards::HasherXXH3<T>;
using Hash128 = shards::Hash128;
}

#endif /* FE9BA2C2_C78A_4D68_B01F_99AC5628BA55 */
