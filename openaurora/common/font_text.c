#include "openaurora/font_text.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>

/* Firmware text is UTF-16 internally; the bitmap-font key table is packed
 * UTF-8 bytes (getPrintableCharacterFromUTF8 0xb4267c), e.g. C3 A9 => 0xc3a9. */
static int utf8_key(const unsigned char **cursor, uint32_t *key)
{
    const unsigned char *p = *cursor;
    unsigned char b0 = p[0];
    if (b0 < 0x80) {
        *key = b0;
        *cursor = p + 1;
        return 0;
    }
    int len;
    uint32_t k = b0;
    if (b0 >= 0xc2 && b0 <= 0xdf) {
        len = 2;
    } else if (b0 >= 0xe0 && b0 <= 0xef) {
        len = 3;
    } else if (b0 >= 0xf0 && b0 <= 0xf4) {
        len = 4;
    } else {
        return EILSEQ;
    }
    for (int i = 1; i < len; ++i) {
        if ((p[i] & 0xc0) != 0x80) return EILSEQ;
        k = k << 8 | p[i];
    }
    if (len == 3 && ((b0 == 0xe0 && p[1] < 0xa0) || (b0 == 0xed && p[1] >= 0xa0)))
        return EILSEQ;
    if (len == 4 && ((b0 == 0xf0 && p[1] < 0x90) || (b0 == 0xf4 && p[1] >= 0x90)))
        return EILSEQ;
    *key = k;
    *cursor = p + len;
    return 0;
}

static int walk(const struct oa_bitmap_font *font, const char *utf8,
                int spacing, int64_t *width)
{
    if (!font || !utf8 || !width) return EINVAL;
    int64_t cursor = 0;
    const unsigned char *p = (const unsigned char *)utf8;
    while (*p) {
        uint32_t key;
        int rc = utf8_key(&p, &key);
        if (rc) return rc;
        unsigned index;
        rc = oa_bitmap_font_lookup(font, key, &index);
        if (rc) return rc;
        struct oa_bitmap_glyph glyph;
        rc = oa_bitmap_font_glyph(font, index, &glyph);
        if (rc) return rc;
        cursor += glyph.advance;
        cursor += spacing;
        if (cursor < INT_MIN || cursor > INT_MAX) return EOVERFLOW;
    }
    *width = (int)cursor;
    return 0;
}

int oa_font_text_measure(const struct oa_bitmap_font *font, const char *utf8,
                         int spacing, int *advance)
{
    if (!advance) return EINVAL;
    int64_t width;
    int rc = walk(font, utf8, spacing, &width);
    if (rc) return rc;
    *advance = (int)width;
    return 0;
}

int oa_font_text_draw(oa_canvas *canvas, const struct oa_bitmap_font *font,
                      const char *utf8, int x, int y, const uint8_t rgba[4],
                      int spacing, const struct oa_font_clip *clip)
{
    if (!canvas || !rgba) return EINVAL;
    /* Validate the whole line before drawing so a missing glyph never leaves
     * a partially rendered string. */
    int64_t width;
    int rc = walk(font, utf8, spacing, &width);
    if (rc) return rc;
    int64_t cursor = x;
    const unsigned char *p = (const unsigned char *)utf8;
    while (*p) {
        uint32_t key;
        rc = utf8_key(&p, &key);
        if (rc) return rc;
        unsigned index;
        rc = oa_bitmap_font_lookup(font, key, &index);
        if (rc) return rc;
        struct oa_bitmap_glyph glyph;
        rc = oa_bitmap_font_glyph(font, index, &glyph);
        if (rc) return rc;
        if (cursor < INT_MIN || cursor > INT_MAX) return EOVERFLOW;
        rc = oa_bitmap_draw_glyph(canvas, font, index, (int)cursor, y, rgba, clip);
        if (rc) return rc;
        cursor += glyph.advance;
        cursor += spacing;
    }
    return 0;
}
