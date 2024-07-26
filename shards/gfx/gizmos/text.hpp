#ifndef A1907F3D_44DB_4C56_B073_4DF4445AC6D4
#define A1907F3D_44DB_4C56_B073_4DF4445AC6D4

#include "../fwd.hpp"
#include "linalg.hpp"
#include <optional>

namespace gfx::gizmos {

struct FontMapImpl;
struct FontMap {
  using Ptr = std::shared_ptr<FontMap>;

  FontMapImpl *impl;
  gfx::TexturePtr image;
  int2 spaceSize;

  FontMap();
  ~FontMap();

  static FontMap::Ptr getDefault();
};

struct TextQuad {
  float4 quad;
  float4 uv;
};

struct TextPlacement {
  std::optional<TextQuad> quad;
  float2 offset;
};

struct TextPlacer {
  float2 origin{};
  float2 pos{};
  float2 max{};
  int numLines{};
  std::vector<TextQuad> textQuads;

  void appendChar(FontMap::Ptr fontMap, uint32_t c, float scale = 1.0f);
  void appendString(FontMap::Ptr fontMap, std::string_view text, float scale = 1.0f);

  inline float2 getSize() { return max - origin; }
  // static TextPlacement generateChar(FontMap::Ptr fontMap, char c, float2 position, float scale = 1.0f);
};

} // namespace gfx::gizmos

#endif /* A1907F3D_44DB_4C56_B073_4DF4445AC6D4 */
