#ifndef OPENAURORA_FONT_BITMAP_H
#define OPENAURORA_FONT_BITMAP_H

#include <stddef.h>
#include <stdint.h>

/* Borrowed immutable EF41 record. Its storage must outlive this view/glyphs.
 * Firmware: BitmapFont::decodeBitmapFont 0xb428d4, drawGreyScaleGlyph
 * 0xb43ea0. Other encodings deliberately remain unsupported. */
struct oa_bitmap_font {
    const unsigned char *pixels, *keys, *metrics, *extremes;
    size_t pixel_bytes;
    unsigned glyph_count, key_count;
};
struct oa_bitmap_glyph {
    /* Nonpositive signed extent is blank, as in the vendor draw loop. */
    int top, left, bottom, right, advance;
    const unsigned char *coverage;
    size_t stride;
};
int oa_bitmap_font_open(struct oa_bitmap_font *out, const void *data, size_t size);
int oa_bitmap_font_glyph(const struct oa_bitmap_font *font, unsigned index,
                         struct oa_bitmap_glyph *out);
/* key is packed UTF-8 bytes, e.g. 0xc3a9, NOT a Unicode code point.
 * Missing keys return ENOENT; no invented substitution/fallback policy. */
int oa_bitmap_font_lookup(const struct oa_bitmap_font *font, uint32_t key,
                          unsigned *index);

#endif
