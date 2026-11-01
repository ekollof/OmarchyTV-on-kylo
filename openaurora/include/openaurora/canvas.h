#ifndef OPENAURORA_CANVAS_H
#define OPENAURORA_CANVAS_H
#include <stdint.h>
/* Top-down straight RGBA with source-over compositing.
 * No framebuffer format, hardware ownership or presentation is implied. */
typedef struct { uint8_t *pixels; int width, height; } oa_canvas;
int oa_canvas_clear(oa_canvas *canvas, int r, int g, int b, int a);
/* Byte-exact opaque-destination compatibility; preserve straight color when
 * the destination is transparent. Weights fit in uint32 even at full coverage. */
static inline void oa_canvas_over(uint8_t *d, const uint8_t *s)
{
    unsigned a = s[3], weight = a*255 + d[3]*(255-a);
    if (!a) return;
    for (unsigned k = 0; k < 3; k++)
        d[k] = (uint8_t)((s[k]*a*255 + d[k]*d[3]*(255-a))/weight);
    d[3] = (uint8_t)(weight/255);
}
int oa_canvas_fill(oa_canvas *canvas, int x, int y, int width, int height, int r, int g, int b);
int oa_canvas_blit(oa_canvas *canvas, int x, int y, int width, int height,
                   int source_width, int source_height, const uint8_t *rgba);
#endif
