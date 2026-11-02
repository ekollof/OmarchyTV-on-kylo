#include "openaurora/font_bitmap.h"

#include <errno.h>
#include <string.h>

static unsigned be16(const unsigned char *p)
{ return (unsigned)p[0] * 256u + p[1]; }
static uint32_t be32(const unsigned char *p)
{ return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
         (uint32_t)p[2] << 8 | p[3]; }
static int metric(unsigned char b)
{ return b < 128 ? b : (int)b - 256; }

int oa_bitmap_font_height(const struct oa_bitmap_font *f, int *height)
{
    if (!f || !f->extremes || !height) return EINVAL;
    *height = metric(f->extremes[2]) - metric(f->extremes[0]);
    return 0;
}

int oa_bitmap_font_x_offset(const struct oa_bitmap_font *f, int *offset)
{
    if (!f || !f->extremes || !offset) return EINVAL;
    *offset = -metric(f->extremes[1]);
    return 0;
}

int oa_bitmap_font_glyph(const struct oa_bitmap_font *f, unsigned index,
                         struct oa_bitmap_glyph *out)
{
    if (!f || !out || !f->metrics || !f->pixels || index >= f->glyph_count) return EINVAL;
    const unsigned char *p = f->metrics + (size_t)index * 9;
    struct oa_bitmap_glyph g = {
        .top = metric(p[0]), .left = metric(p[1]),
        .bottom = metric(p[2]), .right = metric(p[3]),
        .advance = metric(p[4])
    };
    /* Some shipped oversized glyphs wrap signed metrics. Vendor's signed
     * loop bounds draw no pixels for nonpositive dimensions (b43f44/f62).
     * Preserve the metrics, without turning a negative extent into size_t. */
    size_t width = g.right > g.left ? (size_t)(g.right - g.left) : 0;
    size_t height = g.bottom > g.top ? (size_t)(g.bottom - g.top) : 0;
    size_t area = width * height;
    size_t offset = be32(p + 5);
    if (offset > f->pixel_bytes || area > f->pixel_bytes - offset) return EINVAL;
    g.coverage = f->pixels + offset;
    g.stride = width;
    *out = g;
    return 0;
}

int oa_bitmap_font_open(struct oa_bitmap_font *out, const void *data, size_t size)
{
    if (!out) return EINVAL;
    memset(out, 0, sizeof *out);
    if (!data || size < 8) return EINVAL;
    const unsigned char *p = data;
    if (memcmp(p, "EF41", 4)) return ENOTSUP;
    struct oa_bitmap_font f = { .pixels = p + 8, .pixel_bytes = be32(p + 4) };
    if (f.pixel_bytes > size - 8) return EINVAL;
    size_t at = 8 + f.pixel_bytes;
    if (size - at < 5 || memcmp(p + at, "UTF", 3)) return EINVAL;
    f.key_count = be16(p + at + 3);
    at += 5;
    if ((size_t)f.key_count > (size - at) / 4) return EINVAL;
    f.keys = p + at;
    at += (size_t)f.key_count * 4;
    if (size - at < 5 || memcmp(p + at, "DIM", 3)) return EINVAL;
    f.glyph_count = be16(p + at + 3);
    at += 5;
    if (f.glyph_count != f.key_count + 95u ||
        (size_t)f.glyph_count + 1 != (size - at) / 9 ||
        (size - at) % 9) return EINVAL;
    f.metrics = p + at;
    f.extremes = f.metrics + (size_t)f.glyph_count * 9;
    /* Firmware bsearch requires sorted keys. Reject unsafe/corrupt input. */
    for (unsigned i = 1; i < f.key_count; ++i)
        if (be32(f.keys + (size_t)(i - 1) * 4) >=
            be32(f.keys + (size_t)i * 4)) return EINVAL;
    for (unsigned i = 0; i < f.glyph_count; ++i) {
        struct oa_bitmap_glyph g;
        if (oa_bitmap_font_glyph(&f, i, &g)) return EINVAL;
    }
    *out = f;
    return 0;
}

int oa_bitmap_font_lookup(const struct oa_bitmap_font *f, uint32_t key,
                          unsigned *index)
{
    if (!f || !index || !f->metrics) return EINVAL;
    if (key >= 32 && key <= 126) { *index = key - 32; return 0; }
    unsigned lo = 0, hi = f->key_count;
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2;
        uint32_t found = be32(f->keys + (size_t)mid * 4);
        if (found < key) lo = mid + 1;
        else if (found > key) hi = mid;
        else { *index = mid + 95; return 0; }
    }
    return ENOENT;
}
