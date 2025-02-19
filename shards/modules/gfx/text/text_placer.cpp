#include "text_placer.hpp"
#include <stb_rect_pack.h>
#include <stb_truetype.h>
#include <gfx/texture.hpp>
#include <utf8.h/utf8.h>

namespace gfx::text {

void stbtt_GetPackedQuadScaled(const stbtt_packedchar *chardata, int pw, int ph, int char_index, float scale, float *xpos,
                               float *ypos, stbtt_aligned_quad *q, int align_to_integer) {
  float ipw = 1.0f / pw, iph = 1.0f / ph;
  const stbtt_packedchar *b = chardata + char_index;

  if (align_to_integer) {
    float x = (float)std::floor((*xpos + b->xoff * scale) + 0.5f);
    float y = (float)std::floor((*ypos + b->yoff * scale) + 0.5f);
    q->x0 = x;
    q->y0 = y;
    q->x1 = x + (b->xoff2 - b->xoff) * scale;
    q->y1 = y + (b->yoff2 - b->yoff) * scale;
  } else {
    q->x0 = *xpos + b->xoff * scale;
    q->y0 = *ypos + b->yoff * scale;
    q->x1 = *xpos + b->xoff2 * scale;
    q->y1 = *ypos + b->yoff2 * scale;
  }

  q->s0 = b->x0 * ipw;
  q->t0 = b->y0 * iph;
  q->s1 = b->x1 * ipw;
  q->t1 = b->y1 * iph;

  *xpos += b->xadvance * scale;
}

void TextPlacer::verticalAlignOrigin(const FontSize& fontSize, float alignment) {
  if (alignment < -0.5f) {
    // -1 to -0.5 interpolates from top to baseline
    float t = (alignment + 0.5f) / 0.5f; // t goes from 0 to 1
    origin.y = -fontSize.ascent * t;
  } else if (alignment < 0) {
    // -0.5 to 0 is baseline to bottom
    origin.y = fontSize.descent * (1.0f - alignment / -0.5f);
  } else {
    // 0 to 1 interpolates from bottom to top
    origin.y = fontSize.descent;
    origin.y += alignment * fontSize.spaceSize.y;
  }
  pos.y = origin.y;
}

void TextPlacer::appendChar(const FontSize& fontSize, uint32_t c, float scale) {
  if (c == U' ') {
    pos.x += fontSize.spaceSize.x * scale;
    coord.x += 1;
  } else if (c == U'\n') {
    ++numLines;
    pos.x = origin.x;
    pos.y += fontSize.spaceSize.y * scale;
    coord.y = 1;
    coord.x = 0;
  } else {
    const FontPage *page = fontSize.getPage(c);
    if (!page)
      return;

    int charIdx = c - page->firstChar;
    if (charIdx >= page->numChars)
      return;

    int2 res = page->image->getResolution();
    float2 tmpPos = pos;

    stbtt_aligned_quad quad;
    stbtt_GetPackedQuadScaled(page->charData.data(), res.x, res.y, charIdx, scale, &tmpPos.x, &tmpPos.y, &quad, 0);
    float2 posDelta = tmpPos - pos;
    pos += posDelta;

    TextQuad tq{
        .quad = float4{quad.x0, quad.y0, quad.x1, quad.y1},
        .uv = float4{quad.s0, quad.t0, quad.s1, quad.t1},
        .texture = page->image, // Annotate with texture
        .codepoint = c,
        .coord = coord,
    };
    textQuads.push_back(tq);
    coord.x += 1;
  }

  max.x = std::max(max.x, pos.x);
  max.y = std::max(max.y, pos.y);
}

void TextPlacer::appendString(const FontSize& fontSize, std::string_view text, float scale) {
  const char *str = text.data();
  while (*str) {
    utf8_int32_t codepoint;
    str = (const char *)utf8codepoint(str, &codepoint);
    appendChar(fontSize, codepoint, scale);
  }
}

void TextPlacer::clear() {
  origin = float2(0, 0);
  pos = float2(0, 0);
  max = float2(0, 0);
  numLines = 0;
  textQuads.clear();
  coord = int2(0, 0);
}

} // namespace gfx::text
