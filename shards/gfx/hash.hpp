#ifndef GFX_HASH
#define GFX_HASH

#include <functional>
#include <stdint.h>

namespace gfx {
struct Hash128 {
  uint64_t low = 0, high = 0;
  Hash128() = default;
  Hash128(uint64_t low, uint64_t high) : low(low), high(high) {}
  std::strong_ordering operator<=>(const Hash128 &) const = default;
};
} // namespace gfx

template <> struct std::hash<gfx::Hash128> {
  size_t operator()(const gfx::Hash128 &h) const { return size_t(h.low); }
};

#endif // GFX_HASH
