#include "openaurora/font_text.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

struct line_glyph {
    unsigned index;
    int left, right, advance, x;
    int64_t pen;
};
static int fits_int(int64_t n) { return n >= INT_MIN && n <= INT_MAX; }

/* BitmapFont physical width b42b78: exclude final spacing, adjust first
 * negative bearing unless alignment1, replace final advance with right ink. */
static int line_width(const struct line_glyph *g, size_t begin, size_t end,
                       const struct oa_font_line_style *s, int *out)
{
    int64_t first = s->alignment != 1 && g[begin].left < 0 ? -g[begin].left : 0;
    int64_t width = g[end].pen - g[begin].pen - s->spacing + first +
                    g[end-1].right - g[end-1].advance;
    if (!fits_int(width * 64)) return EOVERFLOW;
    *out = (int)width;
    return 0;
}

static int prepare_line(const struct oa_bitmap_font *font, const char *text,
                         const struct oa_font_line_style *s, struct line_glyph **owned,
                         struct oa_font_line_layout *out)
{
    if (!font || !font->pixels || !font->metrics || !text || !s || !owned || !out) return EINVAL;
    if (s->alignment < 1 || s->alignment > 3) return ENOTSUP;
    if (s->right < s->left) return EINVAL;
    size_t bytes = strlen(text);
    if (bytes > INT_MAX || bytes >= SIZE_MAX / sizeof(struct line_glyph)) return EOVERFLOW;
    struct line_glyph *g = calloc(bytes + 1, sizeof *g);
    if (!g) return ENOMEM;
    int rc = 0;
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        uint32_t key;
        unsigned index;
        struct oa_bitmap_glyph glyph;
        if ((rc = utf8_key(&p,&key)) || (rc = oa_bitmap_font_lookup(font,key,&index)) ||
            (rc = oa_bitmap_font_glyph(font,index,&glyph))) goto fail;
        g[n].index=index; g[n].left=glyph.left; g[n].right=glyph.right; g[n].advance=glyph.advance;
        g[n+1].pen = g[n].pen + glyph.advance + (int64_t)s->spacing;
        if (!fits_int(g[n+1].pen)) { rc=EOVERFLOW; goto fail; }
        ++n;
    }
    struct oa_font_line_layout result={.count=n,.origin=s->x,.reverse=s->alignment==2};
    size_t begin=0, end=n;
    if (n) {
        int width;
        if ((rc = line_width(g,begin,end,s,&width))) goto fail;
        int64_t origin=s->x;
        if (s->alignment==2) origin=(int64_t)s->right-g[end-1].right;
        if (s->alignment==3) {
            int64_t available=(int64_t)s->right-s->left;
            if (!fits_int(available*64)) { rc=EOVERFLOW; goto fail; }
            if (width > available) {
                /* centreOverhangingLine113452c creates progressively trimmed
                 * slices; createSlice11356f4 does not copy old X into the new
                 * LineObj (constructor1135110 initializes it to0). */
                for (size_t trim=1; n-trim>trim; ++trim) {
                    int candidate;
                    if ((rc = line_width(g,trim,n-trim,s,&candidate))) goto fail;
                    if (candidate <= available) {
                        begin=trim; end=n-trim; width=candidate;
                        origin=(available-width)/2;
                        break;
                    }
                }
                /* Failure keeps the original range and original X. */
            } else {
                int first=g[0].left < 0 ? -g[0].left : 0;
                origin += (available-width)/2 + first;
            }
        }
        result.first=begin; result.count=end-begin; result.physical_width=width;
        if (!fits_int(origin)) { rc=EOVERFLOW; goto fail; }
        result.origin=(int)origin;
        int64_t cursor=origin;
        for (size_t k=0;k<result.count;++k) {
            size_t i=result.reverse ? end-1-k : begin+k;
            if (!fits_int(cursor)) { rc=EOVERFLOW; goto fail; }
            g[i].x=(int)cursor;
            if (k+1<result.count) {
                if (result.reverse) cursor -= (int64_t)g[i-1].advance+s->spacing;
                else cursor += (int64_t)g[i].advance+s->spacing;
            }
        }
    }
    *owned=g; *out=result;
    return 0;
fail:
    free(g);
    return rc;
}

int oa_font_text_layout_line(const struct oa_bitmap_font *font, const char *text,
                             const struct oa_font_line_style *s, struct oa_font_line_layout *out)
{
    if (!out) return EINVAL;
    struct line_glyph *g;
    struct oa_font_line_layout result;
    int rc=prepare_line(font,text,s,&g,&result);
    if (rc) return rc;
    free(g); *out=result;
    return 0;
}

int oa_font_text_draw_line(oa_canvas *c, const struct oa_bitmap_font *font, const char *text,
                           const struct oa_font_line_style *s, const uint8_t rgba[4],
                           const struct oa_font_clip *clip, struct oa_font_line_layout *out)
{
    if (!c || !c->pixels || c->width<=0 || c->height<=0 || !rgba ||
        (size_t)c->width>SIZE_MAX/4/(size_t)c->height) return EINVAL;
    struct line_glyph *g;
    struct oa_font_line_layout result;
    int rc=prepare_line(font,text,s,&g,&result);
    if (rc) return rc;
    uint8_t color[4];
    memcpy(color,rgba,sizeof color);
    for (size_t k=0;k<result.count;++k) {
        size_t i=result.reverse ? result.first+result.count-1-k : result.first+k;
        rc=oa_bitmap_draw_glyph(c,font,g[i].index,g[i].x,s->y,color,clip);
        if (rc) break;
    }
    free(g);
    if (!rc && out) *out=result;
    return rc;
}
