#include "stb_fonts.hpp"
#include "font.hpp"
#include <gfx/isb.hpp>
#include <gfx/texture.hpp>
#include <gfx/linalg.hpp>
#include <gfx/fmt.hpp>
#include <shards/core/assert.hpp>
#include <bit>

// Bundled font
#include <bundled/Px437_IBM_EGA_8x8.bin.h>
#include <brotli/decode.h>

namespace gfx::text {

FontMap::FontMap(int defaultPageSize, WGPUFilterMode filterMode) {
  shared = new FontMapShared();
  shared->defaultPageSize = defaultPageSize;
  shared->filterMode = filterMode;
}

FontMap::~FontMap() { delete shared; }

const FontSize &FontMap::getFontSize(uint32_t fontSize) { return getOrCreateFontSize(fontSize); }

const float HeurPackingEfficiency = 0.75f;

FontSize &FontMap::getOrCreateFontSize(uint32_t fontSize) {
  auto it = shared->fontSizes.find(fontSize);
  if (it != shared->fontSizes.end()) {
    return it->second;
  }

  FontSize newSize;
  newSize.fontSize = fontSize;
  newSize.shared = shared;

  // Initialize font data
  if (!stbtt_InitFont(&shared->fontInfo, shared->fontData.data(), stbtt_GetFontOffsetForIndex(shared->fontData.data(), 0))) {
    throw std::runtime_error("Failed to initialize font");
  }

  int spaceGlyphIndex = stbtt_FindGlyphIndex(&shared->fontInfo, ' ');
  int spaceAdvance = 0;
  stbtt_GetGlyphHMetrics(&shared->fontInfo, spaceGlyphIndex, &spaceAdvance, nullptr);

  int ascent = 0;
  int descent = 0;
  int lineGap = 0;
  stbtt_GetFontVMetrics(&shared->fontInfo, &ascent, &descent, &lineGap);
  float scale = stbtt_ScaleForPixelHeight(&shared->fontInfo, static_cast<float>(fontSize));
  newSize.ascent = ascent * scale;
  newSize.descent = descent * scale;

  int spaceNewline = ascent - descent;

  newSize.spaceSize.x = spaceAdvance * scale;
  newSize.spaceSize.y = spaceNewline * scale;
  newSize.glyphArea = newSize.spaceSize.x * newSize.spaceSize.y;

  shared->fontSizes[fontSize] = newSize;
  return shared->fontSizes[fontSize];
}

FontPage *FontSize::getPage(int codepoint) {
  // Guess the page size based on the font size and max dimension of 4096 x 4096
  uint32_t guessedPageSize = (1024 * 1024 * HeurPackingEfficiency) / glyphArea;
  guessedPageSize = std::max(1, 1 << (32 - std::countl_zero(guessedPageSize - 1) - 1));

  int pageIndex = codepoint / guessedPageSize;
  int pageOffset = pageIndex * guessedPageSize;
  auto it = pages.lower_bound(pageIndex);
  if (it != pages.end() && it->first == pageIndex && codepoint < it->second.firstChar + it->second.numChars) {
    return &it->second;
  }

  int currentPageSize = guessedPageSize;
  while (currentPageSize > 0) {
    try {
      pageOffset = (codepoint / currentPageSize) * currentPageSize;
      FontPage newPage = createPage(pageOffset, currentPageSize);
      auto [it2, _] = pages.emplace(pageIndex, std::move(newPage));
      return &it2->second;
    } catch (const std::runtime_error &) {
      SPDLOG_LOGGER_DEBUG(getLogger(), "Packing font ({}, size {}) codepoints {}-{} ({} chars) failed", (void *)shared, fontSize,
                          pageOffset, pageOffset + currentPageSize, currentPageSize);
      currentPageSize /= 2;
      continue;
    }
  }
  throw std::runtime_error("Failed to pack font - unable to create page with any size");
}

FontPage FontSize::createPage(int pageOffset, int pageSize) {
  FontPage newPage;
  newPage.firstChar = pageOffset;
  newPage.numChars = pageSize;

  stbtt_pack_range range{};
  range.first_unicode_codepoint_in_range = newPage.firstChar;
  range.array_of_unicode_codepoints = NULL;
  range.num_chars = newPage.numChars;
  newPage.charData.resize(range.num_chars);
  range.chardata_for_range = newPage.charData.data();
  range.font_size = static_cast<float>(fontSize);

  // Check how big one side of the image needs to be to fit all the glypths
  uint32_t guessedSide = std::ceil(std::sqrt(glyphArea * pageSize / HeurPackingEfficiency));
  guessedSide = std::min(4096u, 1u << (32 - std::countl_zero(guessedSide - 1)));

  int2 res{int32_t(guessedSide), int32_t(guessedSide)};
  bool packed = false;
  uint32_t imageDataRowStride{};
  std::vector<uint8_t> imageData;

  while (!packed && res.x <= 4096 && res.y <= 4096) {
    std::vector<uint8_t> singleChanMap(res.x * res.y);

    stbtt_pack_context pctx{};
    stbtt_PackBegin(&pctx, singleChanMap.data(), res.x, res.y, res.x, 0, nullptr);
    pctx.padding = 1;
    pctx.skip_missing = true;
    packed = stbtt_PackFontRanges2(&pctx, &shared->fontInfo, 0, &range, 1) != 0;
    stbtt_PackEnd(&pctx);

    if (!packed) {
      res.x *= 2;
      res.y *= 2;
      SPDLOG_LOGGER_DEBUG(getLogger(), "Packing font ({}, size {}) codepoints {}-{} ({} chars) failed, doubling resolution to {}",
                          (void *)shared, fontSize, pageOffset, pageOffset + pageSize, pageSize, res);
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

  SPDLOG_LOGGER_DEBUG(getLogger(), "Packed font ({}, size {}) codepoints {}-{} ({} chars) into {} texture", (void *)shared,
                      fontSize, pageOffset, pageOffset + pageSize, pageSize, res);

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
          .filterMode = shared->filterMode,
      });

  return newPage;
}

const FontPage *FontMap::getPage(int codepoint, uint32_t fontSize) {
  FontSize &fontSizeData = getOrCreateFontSize(fontSize);
  return fontSizeData.getPage(codepoint);
}

FontMap::Ptr FontMap::load(const uint8_t *data, size_t size, int pageSize, WGPUFilterMode filterMode) {
  auto result = std::make_shared<FontMap>(pageSize, filterMode);

  // Load the font data into a default size (e.g., 12) for initialization
  result->shared->fontData.resize(size);
  std::memcpy(result->shared->fontData.data(), data, size);

  return result;
}

static FontMap::Ptr loadDefaultFontmap() {
  static std::vector<uint8_t> decodedBuffer;
  if (decodedBuffer.size() == 0) {
    size_t decodedSize = *(uint32_t *)bundled_Px437_IBM_EGA_8x8Θbin_getData();
    decodedBuffer.resize(decodedSize);
    auto bres = BrotliDecoderDecompress(bundled_Px437_IBM_EGA_8x8Θbin_getLength() - 4,
                                        bundled_Px437_IBM_EGA_8x8Θbin_getData() + 4, &decodedSize, decodedBuffer.data());
    shassert(bres == BROTLI_DECODER_RESULT_SUCCESS);
  }

  return FontMap::load(decodedBuffer.data(), decodedBuffer.size(), 512, WGPUFilterMode_Nearest);
}

FontMap::Ptr FontMap::getDefault() {
  static FontMap::Ptr instance = loadDefaultFontmap();
  return instance;
}

} // namespace gfx::text
