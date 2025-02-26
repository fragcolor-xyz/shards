#define STB_RECT_PACK_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_fonts.hpp"
#include <stdlib.h>

int stbtt_PackFontRangesRenderIntoRects2(stbtt_pack_context *spc, const stbtt_fontinfo *info, stbtt_pack_range *ranges,
                                         int num_ranges, stbrp_rect *rects) {

  // Modified version of stbtt_PackFontRangesRenderIntoRects that doesn't fail on zero-sized glyphs
  int old_h_over = spc->h_oversample;
  int old_v_over = spc->v_oversample;
  int missing_glyph = -1;

  int i, j, k = 0;
  for (i = 0; i < num_ranges; ++i) {
    float fh = ranges[i].font_size;
    float scale = fh > 0 ? stbtt_ScaleForPixelHeight(info, fh) : stbtt_ScaleForMappingEmToPixels(info, -fh);
    float recip_h, recip_v, sub_x, sub_y;
    spc->h_oversample = ranges[i].h_oversample;
    spc->v_oversample = ranges[i].v_oversample;
    recip_h = 1.0f / spc->h_oversample;
    recip_v = 1.0f / spc->v_oversample;
    sub_x = stbtt__oversample_shift(spc->h_oversample);
    sub_y = stbtt__oversample_shift(spc->v_oversample);

    for (j = 0; j < ranges[i].num_chars; ++j) {
      stbrp_rect *r = &rects[k];
      if (r->was_packed) {
        stbtt_packedchar *bc = &ranges[i].chardata_for_range[j];
        int advance, lsb, x0, y0, x1, y1;
        int codepoint = ranges[i].array_of_unicode_codepoints == NULL ? ranges[i].first_unicode_codepoint_in_range + j
                                                                      : ranges[i].array_of_unicode_codepoints[j];
        int glyph = stbtt_FindGlyphIndex(info, codepoint);

        // Handle zero-sized glyphs
        if (r->w == 0 || r->h == 0) {
          if (glyph == 0)
            missing_glyph = j;
          else if (missing_glyph >= 0)
            ranges[i].chardata_for_range[j] = ranges[i].chardata_for_range[missing_glyph];
          ++k;
          continue;
        }

        stbrp_coord pad = (stbrp_coord)spc->padding;
        r->x += pad;
        r->y += pad;
        r->w -= pad;
        r->h -= pad;

        stbtt_GetGlyphHMetrics(info, glyph, &advance, &lsb);
        stbtt_GetGlyphBitmapBox(info, glyph, scale * spc->h_oversample, scale * spc->v_oversample, &x0, &y0, &x1, &y1);
        stbtt_MakeGlyphBitmapSubpixel(info, spc->pixels + r->x + r->y * spc->stride_in_bytes, r->w - spc->h_oversample + 1,
                                      r->h - spc->v_oversample + 1, spc->stride_in_bytes, scale * spc->h_oversample,
                                      scale * spc->v_oversample, 0, 0, glyph);

        if (spc->h_oversample > 1)
          stbtt__h_prefilter(spc->pixels + r->x + r->y * spc->stride_in_bytes, r->w, r->h, spc->stride_in_bytes,
                             spc->h_oversample);

        if (spc->v_oversample > 1)
          stbtt__v_prefilter(spc->pixels + r->x + r->y * spc->stride_in_bytes, r->w, r->h, spc->stride_in_bytes,
                             spc->v_oversample);

        bc->x0 = (stbtt_int16)r->x;
        bc->y0 = (stbtt_int16)r->y;
        bc->x1 = (stbtt_int16)(r->x + r->w);
        bc->y1 = (stbtt_int16)(r->y + r->h);
        bc->xadvance = scale * advance;
        bc->xoff = (float)x0 * recip_h + sub_x;
        bc->yoff = (float)y0 * recip_v + sub_y;
        bc->xoff2 = (x0 + r->w) * recip_h + sub_x;
        bc->yoff2 = (y0 + r->h) * recip_v + sub_y;

        if (glyph == 0)
          missing_glyph = j;
      }
      ++k;
    }
  }

  // restore original values
  spc->h_oversample = old_h_over;
  spc->v_oversample = old_v_over;

  return 1;
}

int stbtt_PackFontRanges2(stbtt_pack_context *spc, const stbtt_fontinfo *info, int font_index, stbtt_pack_range *ranges,
                          int num_ranges) {
  int i, j, n, return_value = 1;
  stbrp_rect *rects;

  // flag all characters as NOT packed
  for (i = 0; i < num_ranges; ++i)
    for (j = 0; j < ranges[i].num_chars; ++j)
      ranges[i].chardata_for_range[j].x0 = ranges[i].chardata_for_range[j].y0 = ranges[i].chardata_for_range[j].x1 =
          ranges[i].chardata_for_range[j].y1 = 0;

  n = 0;
  for (i = 0; i < num_ranges; ++i)
    n += ranges[i].num_chars;

  rects = (stbrp_rect *)STBTT_malloc(sizeof(*rects) * n, spc->user_allocator_context);
  if (rects == NULL)
    return 0;

  n = stbtt_PackFontRangesGatherRects(spc, info, ranges, num_ranges, rects);

  int packResult = stbrp_pack_rects((stbrp_context *)spc->pack_info, rects, n);
  if (packResult == 1) {
    return_value = stbtt_PackFontRangesRenderIntoRects2(spc, info, ranges, num_ranges, rects);
  } else {
    return_value = packResult;
  }

  STBTT_free(rects, spc->user_allocator_context);
  return return_value;
}