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
};

struct TextPlacer {
  float2 origin{};
  float2 pos{};
  float2 max{};
  int numLines{};
  std::vector<TextQuad> textQuads;

  void appendChar(FontMap::Ptr fontMap, uint32_t c, float scale = 1.0f);
  void appendString(FontMap::Ptr fontMap, std::string_view text, float scale = 1.0f);

  inline float2 getSize() const { return max - origin; }
};

} // namespace gfx::text

#endif // TEXT_PLACER_HPP