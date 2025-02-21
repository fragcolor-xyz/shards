#ifndef A814977B_BC08_4DA3_B60B_0300555CE92A
#define A814977B_BC08_4DA3_B60B_0300555CE92A

#include <shards/gfx/fwd.hpp>
#include <stb_truetype.h>
#include <gfx/linalg.hpp>
#include <gfx/gfx_wgpu.hpp>
#include <optional>
#include <map>

namespace gfx::text {

struct FontPage {
  TexturePtr image;
  std::vector<stbtt_packedchar> charData;
  int firstChar;
  int numChars;
};

struct FontMapShared;
struct FontSize {
  // Pages mapped by starting codepoint
  std::map<uint32_t, FontPage> pages;
  // Font metrics
  float ascent;
  float descent;
  int2 spaceSize;
  uint32_t glyphArea;
  uint32_t fontSize;
  // Shared data
  FontMapShared *shared;

  const FontPage *getPage(int codepoint) const { return const_cast<FontSize *>(this)->getPage(codepoint); }
  FontPage *getPage(int codepoint);

private:
  FontPage createPage(int pageOffset, int pageSize);
};

struct FontMapShared {
  // Source data for the font file
  std::vector<uint8_t> fontData;
  // Loaded stb fontinfo
  stbtt_fontinfo fontInfo;
  // Pages
  std::unordered_map<uint32_t, FontSize> fontSizes;
  // Default size of character pages
  size_t defaultPageSize;
  WGPUFilterMode filterMode;
};

struct FontMap {
  using Ptr = std::shared_ptr<FontMap>;

  FontMap(int defaultPageSize = 512, WGPUFilterMode filterMode = WGPUFilterMode_Linear);
  ~FontMap();

  const FontPage *getPage(int codepoint, uint32_t fontSize);
  const FontSize &getFontSize(uint32_t fontSize);

  static FontMap::Ptr getDefault();
  static FontMap::Ptr load(const uint8_t *data, size_t size, int pageSize = 512, WGPUFilterMode filterMode = WGPUFilterMode_Linear);

private:
  FontSize &getOrCreateFontSize(uint32_t fontSize);

  FontMapShared *shared;
};

} // namespace gfx::text

#endif /* A814977B_BC08_4DA3_B60B_0300555CE92A */
