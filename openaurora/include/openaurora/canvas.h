#ifndef OPENAURORA_CANVAS_H
#define OPENAURORA_CANVAS_H
#include <stdint.h>
/* Top-down straight RGBA, opaque compositing as the existing display canvas.
 * No framebuffer format, hardware ownership or presentation is implied. */
typedef struct { uint8_t *pixels; int width, height; } oa_canvas;
int oa_canvas_fill(oa_canvas *canvas, int x, int y, int width, int height, int r, int g, int b);
int oa_canvas_blit(oa_canvas *canvas, int x, int y, int width, int height,
                   int source_width, int source_height, const uint8_t *rgba);
#endif
