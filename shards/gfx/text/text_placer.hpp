#ifndef TEXT_PLACER_HPP
#define TEXT_PLACER_HPP

#include <gfx/linalg.hpp>
#include <vector>
#include <optional>
#include "font.hpp"

namespace gfx::text {

struct TextQuad {
  float4 quad;
  float4 uv;
  TexturePtr texture; // Add texture annotation
  uint32_t codepoint;
  // Placement coordinate:
  //  x increments by 1 every character
  //  y increments by 1 every line
  int2 coord;
};

struct TextPlacer {
  float2 origin{};
  float2 pos{};
  float2 max{};
  int numLines{};
  int2 coord{};
  std::vector<TextQuad> textQuads;

  // Vertically align (0 = bottom-left, 1 = top-left)
  void verticalAlignOrigin(const FontSize& fontSize, float alignment);
  void appendChar(const FontSize& fontSize, uint32_t c, float scale = 1.0f);
  void appendString(const FontSize& fontSize, std::string_view text, float scale = 1.0f);

  void clear();
  inline float2 getSize() const { return max - origin; }
};

} // namespace gfx::text

#endif // TEXT_PLACER_HPP