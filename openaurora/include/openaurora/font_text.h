#ifndef OPENAURORA_FONT_TEXT_H
#define OPENAURORA_FONT_TEXT_H
#include "openaurora/canvas.h"
#include "openaurora/font_bitmap.h"
#include "openaurora/font_draw.h"
#include <stddef.h>

/* One left-to-right line of packed-UTF8 text. Origin is caller-supplied and
 * passed to each glyph (see font_draw), not an established vendor baseline
 * or text-box top-left. Advance comes from the glyph metric and
 * spacing is added after every glyph. No shaping, kerning, wrapping, ellipsis
 * or alignment is implied. Missing glyphs make the whole call fail before any
 * pixel is drawn. */
int oa_font_text_measure(const struct oa_bitmap_font *font, const char *utf8,
                         int spacing, int *advance);
int oa_font_text_draw(oa_canvas *canvas, const struct oa_bitmap_font *font,
                      const char *utf8, int x, int y, const uint8_t rgba[4],
                      int spacing, const struct oa_font_clip *clip);
/* Single unshaped EF41 line, unit horizontal ratio, no markup/style runs.
 * Numeric alignment1=start,2=end (reverse glyph paint order),3=center.
 * x is the existing LineObj origin; left/right define its alignment viewport.
 * Center overhang follows firmware symmetric character trimming. A successful
 * trimmed slice starts from X0; if no slice fits, original text/X are retained.
 * Missing glyph/invalid UTF8/range failures precede all drawing. Unsupported
 * word justification4 returns ENOTSUP. Does not supply baseline, BiDi shaping,
 * wrapping, ellipsis, horizontal scaling or implicit clipping to the viewport.
 * Use the explicit clip argument for draw clipping. Font/storage immutable. */
struct oa_font_line_style { int x, y, left, right, spacing; unsigned alignment; };
struct oa_font_line_layout {
    size_t first, count; /* selected decoded glyph range, not byte offsets */
    int origin, physical_width, reverse; /* origin is last-glyph X when reverse */
};
int oa_font_text_layout_line(const struct oa_bitmap_font *, const char *,
                             const struct oa_font_line_style *, struct oa_font_line_layout *);
int oa_font_text_draw_line(oa_canvas *, const struct oa_bitmap_font *, const char *,
                           const struct oa_font_line_style *, const uint8_t rgba[4],
                           const struct oa_font_clip *, struct oa_font_line_layout *optional_result);
#endif
