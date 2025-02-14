#include "stb_fonts.hpp"
#include "font.hpp"
#include <gfx/isb.hpp>
#include <gfx/texture.hpp>
#include <gfx/linalg.hpp>
#include <shards/core/assert.hpp>

#include <brotli/decode.h>

namespace gfx::text {

FontMap::FontMap(int pageSize, float fontSize) {
  impl = new FontMapImpl();
  impl->pageSize = pageSize;
  impl->fontSize = fontSize;
}

FontMap::~FontMap() {
  delete impl; // Ensure the FontMapImpl is properly deleted
}

const FontPage *FontMap::getPage(int codepoint) {
  int pageIndex = codepoint / impl->pageSize;
  int pageOffset = pageIndex * impl->pageSize;
  auto it = impl->pages.lower_bound(pageIndex);
  if (it != impl->pages.end() && it->first == pageIndex && codepoint < it->second.firstChar + it->second.numChars) {
    return &it->second;
  }

  int currentPageSize = impl->pageSize;
  while (currentPageSize > 0) {
    try {
      // Adjust pageOffset to ensure codepoint falls within the page
      pageOffset = (codepoint / currentPageSize) * currentPageSize;
      FontPage newPage = createPage(pageOffset, currentPageSize);
      auto [it2, _] = impl->pages.emplace(pageIndex, std::move(newPage));
      return &it2->second;
    } catch (const std::runtime_error &) {
      currentPageSize /= 2;
      continue;
    }
  }
  throw std::runtime_error("Failed to pack font - unable to create page with any size");
}

FontPage FontMap::createPage(int pageOffset, int pageSize) {
  FontPage newPage;
  newPage.firstChar = pageOffset;
  newPage.numChars = pageSize;

  stbtt_pack_range range{};
  range.first_unicode_codepoint_in_range = newPage.firstChar;
  range.array_of_unicode_codepoints = NULL;
  range.num_chars = newPage.numChars;
  newPage.charData.resize(range.num_chars);
  range.chardata_for_range = newPage.charData.data();
  range.font_size = impl->fontSize;

  int2 res{256, 256};
  bool packed = false;
  uint32_t imageDataRowStride{};
  std::vector<uint8_t> imageData;

  while (!packed && res.x < 4096 && res.y < 4096) {
    std::vector<uint8_t> singleChanMap(res.x * res.y);

    stbtt_pack_context pctx{};
    stbtt_PackBegin(&pctx, singleChanMap.data(), res.x, res.y, res.x, 0, nullptr);
    pctx.skip_missing = true;
    packed = stbtt_PackFontRanges2(&pctx, &impl->fontInfo, 0, &range, 1) != 0;
    stbtt_PackEnd(&pctx);

    if (!packed) {
      res.x *= 2;
      res.y *= 2;
      continue;
    }

    imageData.resize(res.x * res.y * sizeof(uint32_t));
    imageDataRowStride = res.x * sizeof(uint32_t);

    for (size_t y = 0; y < res.y; ++y) {
      for (size_t x = 0; x < res.x; ++x) {
        uint8_t *src = &singleChanMap[x + y * res.x];
        uint32_t *dst = (uint32_t *)((uint8_t *)imageData.data() + (y * imageDataRowStride) + x * sizeof(uint32_t));
        *dst = (uint32_t(*src) << 24) | 0x00FFFFFF;
      }
    }
  }

  if (!packed) {
    throw std::runtime_error("Failed to pack font - texture too large");
  }

  newPage.image = std::make_shared<Texture>();
  TextureDesc textureDesc{.format = TextureFormat{.pixelFormat = WGPUTextureFormat_RGBA8Unorm},
                          .resolution = res,
                          .source = TextureSource{
                              .numChannels = 4,
                              .rowStride = imageDataRowStride,
                              .data = ImmutableSharedBuffer(std::move(imageData)),
                          }};

  newPage.image->init(textureDesc)
      .initWithSamplerState(SamplerState{
          .addressModeU = WGPUAddressMode_ClampToEdge,
          .addressModeV = WGPUAddressMode_ClampToEdge,
          .addressModeW = WGPUAddressMode_ClampToEdge,
          .filterMode = WGPUFilterMode_Nearest,
      });

  return newPage;
}

FontMap::Ptr FontMap::load(const uint8_t *data, size_t size, int pageSize, float fontSize) {
  auto result = std::make_shared<FontMap>(pageSize, fontSize);
  auto &impl = *result->impl;

  impl.fontData.resize(size);
  std::memcpy(impl.fontData.data(), data, size);

  // Initialize the font info once and store it
  if (!stbtt_InitFont(&impl.fontInfo, impl.fontData.data(), stbtt_GetFontOffsetForIndex(impl.fontData.data(), 0))) {
    throw std::runtime_error("Failed to initialize font");
  }

  int spaceGlyphIndex = stbtt_FindGlyphIndex(&impl.fontInfo, ' ');
  int spaceAdvance = 0;
  stbtt_GetGlyphHMetrics(&impl.fontInfo, spaceGlyphIndex, &spaceAdvance, nullptr);

  int ascent = 0;
  int descent = 0;
  int lineGap = 0;
  stbtt_GetFontVMetrics(&impl.fontInfo, &ascent, &descent, &lineGap);
  float scale = stbtt_ScaleForPixelHeight(&impl.fontInfo, fontSize);
  result->ascent = ascent * scale;
  result->descent = descent * scale;

  int spaceNewline = ascent - descent;

  result->spaceSize.x = spaceAdvance * scale;
  result->spaceSize.y = spaceNewline * scale;

  return result;
}

static FontMap::Ptr loadDefaultFontmap() { return nullptr; }

FontMap::Ptr FontMap::getDefault() {
  static FontMap::Ptr instance = loadDefaultFontmap();
  return instance;
}

} // namespace gfx::text
