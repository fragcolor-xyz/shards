#ifndef GFX_MATH
#define GFX_MATH

#include <cassert>

namespace gfx {
constexpr float pi = 3.14159265359f;
constexpr float halfPi = pi / 2.0f;
constexpr float pi2 = 6.28318530718f;

constexpr float degToRadFactor = 1.0f / (180.0f / pi);
inline constexpr float degToRad(float v) { return v * degToRadFactor; }

constexpr float radToDegFactor = (180.0f / pi);
inline constexpr float radToDeg(float v) { return v * radToDegFactor; }

inline constexpr bool isPOT(size_t N) { return (N & (N - 1)) == 0; }

inline constexpr size_t alignTo(size_t size, size_t alignment) {
  if (alignment == 0)
    return size;

  size_t remainder = size % alignment;
  if (remainder > 0) {
    return size + (alignment - remainder);
  }
  return size;
}

inline constexpr size_t alignToPOT(size_t size, size_t alignment) {
  assert(isPOT(alignment) && "alignment needs to be a power of 2");
  return (size + alignment - 1) & ~(alignment - 1);
}

template <size_t Alignment> inline constexpr size_t alignTo(size_t size) {
  if constexpr (isPOT(Alignment)) {
    return alignToPOT(size, Alignment);
  } else {
    return alignTo(size, Alignment);
  }
}

} // namespace gfx

#endif // GFX_MATH
