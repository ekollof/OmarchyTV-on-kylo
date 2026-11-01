#include <errno.h>
#include <stddef.h>
#include "openaurora/canvas.h"
static int valid(const oa_canvas *c)
{ return c && c->pixels && c->width > 0 && c->height > 0 && (size_t)c->width <= SIZE_MAX/4/(size_t)c->height; }
int oa_canvas_fill(oa_canvas *c, int x, int y, int w, int h, int r, int g, int b)
{
    if (!valid(c) || w <= 0 || h <= 0 || r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) { errno = EINVAL; return -1; }
    int64_t right = (int64_t)x+w, bottom = (int64_t)y+h;
    int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    int end_x = right > c->width ? c->width : (int)right;
    int end_y = bottom > c->height ? c->height : (int)bottom;
    for (int py = top; py < end_y; py++) for (int px = left; px < end_x; px++) {
        uint8_t *p = c->pixels+((size_t)py*(size_t)c->width+(size_t)px)*4;
        p[0] = (uint8_t)r; p[1] = (uint8_t)g; p[2] = (uint8_t)b; p[3] = 255;
    }
    return 0;
}
int oa_canvas_blit(oa_canvas *c, int x, int y, int w, int h, int sw, int sh, const uint8_t *rgba)
{
    if (!valid(c) || !rgba || w <= 0 || h <= 0 || sw <= 0 || sh <= 0 ||
        (size_t)sw > SIZE_MAX/4/(size_t)sh) { errno = EINVAL; return -1; }
    int64_t right = (int64_t)x+w, bottom = (int64_t)y+h;
    int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    int end_x = right > c->width ? c->width : (int)right;
    int end_y = bottom > c->height ? c->height : (int)bottom;
    for (int py = top; py < end_y; py++) for (int px = left; px < end_x; px++) {
        int sx = (int)(((int64_t)px-x)*sw/w), sy = (int)(((int64_t)py-y)*sh/h);
        const uint8_t *p = rgba+((size_t)sy*(size_t)sw+(size_t)sx)*4;
        uint8_t *d = c->pixels+((size_t)py*(size_t)c->width+(size_t)px)*4;
        unsigned a = p[3];
        if (!a) continue;
        for (unsigned k = 0; k < 3; k++) d[k] = (uint8_t)((p[k]*a+d[k]*(255-a))/255);
        d[3] = 255;
    }
    return 0;
}
