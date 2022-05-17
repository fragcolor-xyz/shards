#ifndef GFX_MATH
#define GFX_MATH

namespace gfx {
constexpr float pi = 3.14159265359f;
constexpr float halfPi = pi / 2.0f;
constexpr float pi2 = 6.28318530718f;

constexpr float degToRadFactor = 1.0f / (180.0f / pi);
inline float degToRad(float v) { return v * degToRadFactor; }

constexpr float radToDegFactor = (180.0f / pi);
inline float radToDeg(float v) { return v * radToDegFactor; }

inline size_t alignTo(size_t size, size_t alignment) {
  if (alignment == 0)
    return size;

  size_t remainder = size % alignment;
  if (remainder > 0) {
    return size + (alignment - remainder);
  }
  return size;
}
} // namespace gfx

#endif // GFX_MATH
