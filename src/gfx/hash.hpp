#pragma once

#include <functional>
#include <stdint.h>

namespace gfx {
struct Hash128 {
  uint64_t low = 0, high = 0;
  Hash128() = default;
  Hash128(uint64_t low, uint64_t high) : low(low), high(high) {}
  bool operator==(const Hash128 &other) const { return low == other.low && high == other.high; }
  bool operator!=(const Hash128 &other) const { return low != other.low && high != other.high; }
};
} // namespace gfx

namespace std {
template <> struct hash<gfx::Hash128> {
  size_t operator()(const gfx::Hash128 &h) const { return size_t(h.low); }
};
} // namespace std
