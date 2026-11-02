#ifndef OPENAURORA_FONT_BOX_H
#define OPENAURORA_FONT_BOX_H
#include "openaurora/font_text.h"

/* Plain single EF41 line in a zero-origin text surface, unit X/Y ratios,
 * positive logical font height, no markup/shaping/mirroring/overrides.
 * Numeric widget alignment follows SVGSurface::TEXT_ALIGN (1..9; default4).
 * Results are LOCAL to the surface; the caller owns absolute placement.
 * Center trimming/end reverse paint use the maintained line renderer. */
struct oa_font_box_layout {
    struct oa_font_line_layout line;
    int y;
    unsigned horizontal, vertical;
};
int oa_font_text_layout_box(const struct oa_bitmap_font *, const char *text,
                            int width, int height, unsigned alignment, int spacing,
                            struct oa_font_box_layout *out);
/* Canvas IS the text surface: its bounds provide clipping like the separate
 * vendor text surface. No extra temporary raster/compositing layer is added.
 * Existing pixels use normal glyph source-over. Caller chooses RGBA explicitly.
 * Invalid input/glyph/range failures precede painting and preserve result. */
int oa_font_text_draw_box(oa_canvas *, const struct oa_bitmap_font *, const char *text,
                          unsigned alignment, int spacing, const uint8_t rgba[4],
                          struct oa_font_box_layout *optional_result);
#endif
