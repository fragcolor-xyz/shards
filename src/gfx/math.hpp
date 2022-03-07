#pragma once

namespace gfx {
constexpr float pi = 3.14159265359f;
constexpr float halfPi = pi / 2.0f;
constexpr float pi2 = 6.28318530718f;

constexpr float degToRadFactor = 1.0f / (180.0f / pi);
inline float degToRad(float v) { return v * degToRadFactor; }

constexpr float radToDegFactor = (180.0f / pi);
inline float radToDeg(float v) { return v * radToDegFactor; }
} // namespace gfx
