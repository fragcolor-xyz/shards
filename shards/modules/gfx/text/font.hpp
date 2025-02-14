#ifndef A814977B_BC08_4DA3_B60B_0300555CE92A
#define A814977B_BC08_4DA3_B60B_0300555CE92A

#include <shards/gfx/fwd.hpp>
#include <stb_truetype.h>
#include <gfx/linalg.hpp>
#include <optional>

namespace gfx::text {

struct FontPage {
  TexturePtr image;
  std::vector<stbtt_packedchar> charData;
  int firstChar;
  int numChars;
};

struct FontMapImpl {
  std::map<int, FontPage> pages;
  std::vector<uint8_t> fontData;
  float fontSize;
  int pageSize;
  stbtt_fontinfo fontInfo;
};

struct FontMap {
  using Ptr = std::shared_ptr<FontMap>;

  FontMapImpl *impl;
  int2 spaceSize;
  float ascent,descent;

  FontMap(int pageSize = 512, float fontSize = 12.0f);
  ~FontMap();

  const FontPage *getPage(int codepoint);
  static FontMap::Ptr getDefault();
  static FontMap::Ptr load(const uint8_t *data, size_t size, int pageSize = 512, float fontSize = 12.0f);

private:
  FontPage createPage(int pageOffset, int pageSize);
};

} // namespace gfx::text

#endif /* A814977B_BC08_4DA3_B60B_0300555CE92A */
