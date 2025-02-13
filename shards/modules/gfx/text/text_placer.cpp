#include "text_placer.hpp"
#include <stb_rect_pack.h>
#include <stb_truetype.h>
#include <gfx/texture.hpp>

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

void TextPlacer::appendChar(text::FontMap::Ptr fontMap, uint32_t c, float scale) {
  if (c == U' ') {
    pos.x += fontMap->spaceSize.x * scale;
  } else if (c == U'\n') {
    ++numLines;
    pos.x = origin.x;
    pos.y += fontMap->spaceSize.y * scale;
  } else {
    const FontPage *page = fontMap->getPage(c);
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
        .texture = page->image // Annotate with texture
    };
    textQuads.push_back(tq);
  }

  max.x = std::max(max.x, pos.x);
  max.y = std::max(max.y, pos.y);
}

void TextPlacer::appendString(FontMap::Ptr fontMap, std::string_view text, float scale) {
  for (auto c : text) {
    appendChar(fontMap, c, scale);
  }
}

} // namespace gfx::text