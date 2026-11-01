#ifndef OPENAURORA_FONT_DRAW_H
#define OPENAURORA_FONT_DRAW_H
#include "openaurora/canvas.h"
#include "openaurora/font_bitmap.h"

/* Render one already-selected glyph at a supplied font origin (not baseline),
 * adding its signed left/top bearings. No text shaping, advance, wrapping,
 * alignment or font selection is implied. Canvas is top-down straight RGBA;
 * framebuffer BGRA conversion belongs to display, as with every other draw.
 * Optional clip is intersected with the canvas. Nonpositive clip is empty.
 * Font storage must remain immutable and not overlap the writable canvas.
 * Returns errno value, without touching the canvas on invalid input. */
struct oa_font_clip { int x, y, width, height; };
int oa_bitmap_draw_glyph(oa_canvas *canvas, const struct oa_bitmap_font *font,
                         unsigned index, int x, int y, const uint8_t rgba[4],
                         const struct oa_font_clip *clip);
#endif
