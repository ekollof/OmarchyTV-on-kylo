#ifndef OPENAURORA_GLYPH_QUAD_H
#define OPENAURORA_GLYPH_QUAD_H
#include "openaurora/font_draw.h"
/* GPU glyph bounds are absolute top-down pixels, already including font/glyph
 * bearings. Vendor conversion uses integer half-screen dimensions, then float
 * subtraction/division, including the odd-dimension behavior. */
struct oa_glyph_quad { float vertex[24]; }; /* TL/TR/BR/BL: x,y,z,w,u,v */
/* No implicit viewport clipping (GL does that after interpolation). An optional
 * clip intersects the glyph and remaps its UVs, matching b34670. ENOENT means
 * fully clipped; EINVAL/EOVERFLOW reject unsafe input. All errors preserve out. */
int oa_glyph_quad_make(int screen_w, int screen_h, int x, int y, int w, int h,
                       const struct oa_font_clip *clip, struct oa_glyph_quad *out);
#endif
