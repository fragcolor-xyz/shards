#pragma once

#include "linalg.hpp"

namespace gfx {
using Color = byte4;
using Colorf = float4;
inline Colorf colorToFloat(const Color &color) { return Colorf(color) / Colorf(255.0f); }
inline Color colorFromFloat(const Colorf &color) { return Color(color * Colorf(255.0f)); }

inline uint32_t colorToRGBA(const Color &in) {
  uint32_t result = 0;
  result |= (uint32_t(in.x) << 24);
  result |= (uint32_t(in.y) << 16);
  result |= (uint32_t(in.z) << 8);
  result |= (uint32_t(in.w) << 0);
  return result;
}

inline Color colorFromRGBA(uint32_t in) {
  Color result;
  result.x = uint8_t(in >> 24);
  result.y = uint8_t(in >> 16);
  result.z = uint8_t(in >> 8);
  result.w = uint8_t(in >> 0);
  return result;
}

inline uint32_t colorToARGB(const Color &in) {
  uint32_t result = 0;
  result |= (uint32_t(in.x) << 16);
  result |= (uint32_t(in.y) << 8);
  result |= (uint32_t(in.z) << 0);
  result |= (uint32_t(in.w) << 24);
  return result;
}

inline Color colorFromARGB(uint32_t in) {
  Color result;
  result.x = uint8_t(in >> 16);
  result.y = uint8_t(in >> 8);
  result.z = uint8_t(in >> 0);
  result.w = uint8_t(in >> 24);
  return result;
}

struct Rect {
  int x{0};
  int y{0};
  int width{0};
  int height{0};

  Rect() = default;
  Rect(int width, int height) : width(width), height(height) {}
  Rect(int x, int y, int width, int height) : x(x), y(y), width(width), height(height) {}

  int getX1() const { return x + width; }
  int getY1() const { return x + height; }
  int2 getSize() const { return int2(width, height); }

  static Rect fromCorners(int x0, int y0, int x1, int y1) { return Rect(x0, y0, x1 - x0, y1 - y0); }
};
} // namespace gfx
