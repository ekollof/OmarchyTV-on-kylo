#include "openaurora/font_draw.h"
#include <errno.h>
#include <stddef.h>

static int64_t greater(int64_t a, int64_t b) { return a > b ? a : b; }
static int64_t lesser(int64_t a, int64_t b) { return a < b ? a : b; }

int oa_bitmap_draw_glyph(oa_canvas *c, const struct oa_bitmap_font *f,
                         unsigned index, int x, int y, const uint8_t rgba[4],
                         const struct oa_font_clip *clip)
{
    if (!c || !c->pixels || c->width <= 0 || c->height <= 0 || !rgba ||
        (size_t)c->width > SIZE_MAX / 4 / (size_t)c->height) return EINVAL;
    struct oa_bitmap_glyph g;
    int rc = oa_bitmap_font_glyph(f, index, &g);
    if (rc) return rc;
    if (g.right <= g.left || g.bottom <= g.top || !rgba[3]) return 0;
    int64_t gx = (int64_t)x + g.left, gy = (int64_t)y + g.top;
    int64_t left = greater(0, gx), top = greater(0, gy);
    int64_t right = lesser(c->width, (int64_t)x + g.right);
    int64_t bottom = lesser(c->height, (int64_t)y + g.bottom);
    if (clip) {
        if (clip->width <= 0 || clip->height <= 0) return 0;
        left = greater(left, clip->x); top = greater(top, clip->y);
        right = lesser(right, (int64_t)clip->x + clip->width);
        bottom = lesser(bottom, (int64_t)clip->y + clip->height);
    }
    /* Copy color before writing, so an aliased color remains stable. */
    uint8_t source[4] = {rgba[0], rgba[1], rgba[2], rgba[3]};
    unsigned alpha = rgba[3];
    for (int64_t py = top; py < bottom; ++py) {
        for (int64_t px = left; px < right; ++px) {
            unsigned coverage = g.coverage[(size_t)(py - gy) * g.stride + (size_t)(px - gx)];
            /* ARGB8888ColourAPI::compositePixel b370c0: floor(A*coverage/255),
             * straight source-over with truncated output alpha/components.
             * DrawContext::drawGreyScaleGlyph b43ea0 skips zero coverage. */
            if (!coverage) continue;
            source[3] = (uint8_t)(alpha * coverage / 255u);
            uint8_t *dst = c->pixels + ((size_t)py * (size_t)c->width + (size_t)px) * 4;
            if (!source[3] && !dst[3]) {
                /* Vendor zeroes the whole pixel when combined alpha is0,
                 * even if transparent destination RGB was noncanonical. */
                dst[0] = dst[1] = dst[2] = dst[3] = 0;
            } else oa_canvas_over(dst, source);
        }
    }
    return 0;
}
