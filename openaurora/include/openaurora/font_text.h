#ifndef OPENAURORA_FONT_TEXT_H
#define OPENAURORA_FONT_TEXT_H
#include "openaurora/canvas.h"
#include "openaurora/font_bitmap.h"
#include "openaurora/font_draw.h"

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
#endif
