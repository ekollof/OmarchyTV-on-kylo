#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include "openaurora/canvas.h"
static int valid(const oa_canvas *c)
{ return c && c->pixels && c->width > 0 && c->height > 0 && (size_t)c->width <= SIZE_MAX/4/(size_t)c->height; }
int oa_canvas_clear(oa_canvas *c, int r, int g, int b, int a)
{
    if (!valid(c) || r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || a < 0 || a > 255) {
        errno = EINVAL; return -1;
    }
    for (size_t i = 0; i < (size_t)c->width*(size_t)c->height; i++) {
        uint8_t *p = c->pixels+i*4;
        p[0] = a ? (uint8_t)r : 0; p[1] = a ? (uint8_t)g : 0;
        p[2] = a ? (uint8_t)b : 0; p[3] = (uint8_t)a;
    }
    return 0;
}
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
        oa_canvas_over(d, p);
    }
    return 0;
}

#define OA_MIP_LEVELS 16
struct oa_mip_level {
    int width, height;
    const uint8_t *pixels;
    uint8_t *owned;
};

static void mip_release(struct oa_mip_level *levels, int count)
{
    for (int i = 1; i < count; i++) free(levels[i].owned);
}

/* glGenerateMipmap's filter is implementation-defined; Mali's UI textures are
 * box-downsampled. Level dimensions follow the GLES floor(w/2) rule. */
static int mip_build(struct oa_mip_level levels[OA_MIP_LEVELS],
                     const uint8_t *rgba, int sw, int sh, int need)
{
    levels[0].width = sw;
    levels[0].height = sh;
    levels[0].pixels = rgba;
    levels[0].owned = NULL;
    int count = 1;
    while (count < need) {
        int pw = levels[count-1].width, ph = levels[count-1].height;
        if (pw <= 1 && ph <= 1) break;
        int nw = pw > 1 ? pw/2 : 1, nh = ph > 1 ? ph/2 : 1;
        if ((size_t)nw > SIZE_MAX/4/(size_t)nh) { errno = EINVAL; return -1; }
        uint8_t *dst = malloc((size_t)nw*(size_t)nh*4);
        if (!dst) { mip_release(levels, count); errno = ENOMEM; return -1; }
        const uint8_t *src = levels[count-1].pixels;
        for (int y = 0; y < nh; y++) {
            int sy0 = y*2, sy1 = sy0+2;
            if (sy1 > ph) sy1 = ph;
            for (int x = 0; x < nw; x++) {
                int sx0 = x*2, sx1 = sx0+2;
                if (sx1 > pw) sx1 = pw;
                unsigned sum[4] = {0, 0, 0, 0};
                for (int yy = sy0; yy < sy1; yy++)
                    for (int xx = sx0; xx < sx1; xx++) {
                        const uint8_t *p = src+((size_t)yy*(size_t)pw+(size_t)xx)*4;
                        for (int k = 0; k < 4; k++) sum[k] += p[k];
                    }
                unsigned n = (unsigned)(sx1-sx0)*(unsigned)(sy1-sy0);
                uint8_t *d = dst+((size_t)y*(size_t)nw+(size_t)x)*4;
                for (int k = 0; k < 4; k++) d[k] = (uint8_t)((sum[k]+n/2)/n);
            }
        }
        levels[count].width = nw;
        levels[count].height = nh;
        levels[count].pixels = dst;
        levels[count].owned = dst;
        count++;
    }
    return count;
}

static void mip_sample_level(const struct oa_mip_level *level, int dw, int dh,
                             int dx, int dy, uint8_t out[4])
{
    int lw = level->width, lh = level->height;
    const uint8_t *p = level->pixels;
    float fx = ((float)dx+0.5f)*(float)lw/(float)dw-0.5f;
    float fy = ((float)dy+0.5f)*(float)lh/(float)dh-0.5f;
    int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
    float ax = fx-(float)x0, ay = fy-(float)y0;
    if (x0 < 0) { x0 = 0; ax = 0; }
    if (y0 < 0) { y0 = 0; ay = 0; }
    if (x0 >= lw) { x0 = lw-1; ax = 0; }
    if (y0 >= lh) { y0 = lh-1; ay = 0; }
    int x1 = x0+1 < lw ? x0+1 : x0;
    int y1 = y0+1 < lh ? y0+1 : y0;
    const uint8_t *p00 = p+((size_t)y0*(size_t)lw+(size_t)x0)*4;
    const uint8_t *p10 = p+((size_t)y0*(size_t)lw+(size_t)x1)*4;
    const uint8_t *p01 = p+((size_t)y1*(size_t)lw+(size_t)x0)*4;
    const uint8_t *p11 = p+((size_t)y1*(size_t)lw+(size_t)x1)*4;
    for (int k = 0; k < 4; k++) {
        float top = (float)p00[k]+((float)p10[k]-(float)p00[k])*ax;
        float bot = (float)p01[k]+((float)p11[k]-(float)p01[k])*ax;
        out[k] = (uint8_t)(top+(bot-top)*ay+0.5f);
    }
}

static void mip_sample(const struct oa_mip_level *levels, int count,
                       int dw, int dh, int dx, int dy, uint8_t out[4])
{
    float rx = (float)levels[0].width/(float)dw;
    float ry = (float)levels[0].height/(float)dh;
    float scale = rx > ry ? rx : ry;
    float lod = scale > 1.0f ? log2f(scale) : 0.0f;
    int last = count-1;
    if (lod > (float)last) lod = (float)last;
    int l0 = (int)floorf(lod);
    float f = lod-(float)l0;
    if (l0 >= last || f <= 0.0f) {
        mip_sample_level(&levels[l0], dw, dh, dx, dy, out);
        return;
    }
    uint8_t a[4], b[4];
    mip_sample_level(&levels[l0], dw, dh, dx, dy, a);
    mip_sample_level(&levels[l0+1], dw, dh, dx, dy, b);
    for (int k = 0; k < 4; k++)
        out[k] = (uint8_t)((float)a[k]+((float)b[k]-(float)a[k])*f+0.5f);
}

int oa_canvas_blit_mipmap(oa_canvas *c, int x, int y, int w, int h, int sw, int sh, const uint8_t *rgba)
{
    if (!valid(c) || !rgba || w <= 0 || h <= 0 || sw <= 0 || sh <= 0 ||
        (size_t)sw > SIZE_MAX/4/(size_t)sh) { errno = EINVAL; return -1; }
    struct oa_mip_level levels[OA_MIP_LEVELS];
    float rx = (float)sw/(float)w, ry = (float)sh/(float)h;
    float scale = rx > ry ? rx : ry;
    float lod = scale > 1.0f ? log2f(scale) : 0.0f;
    int l0 = (int)floorf(lod);
    int need = scale > 1.0f ? l0+2 : 1;
    if (need > OA_MIP_LEVELS) need = OA_MIP_LEVELS;
    int count = mip_build(levels, rgba, sw, sh, need);
    if (count < 0) return -1;
    int64_t right = (int64_t)x+w, bottom = (int64_t)y+h;
    int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    int end_x = right > c->width ? c->width : (int)right;
    int end_y = bottom > c->height ? c->height : (int)bottom;
    for (int py = top; py < end_y; py++) for (int px = left; px < end_x; px++) {
        uint8_t sample[4];
        mip_sample(levels, count, w, h, px-x, py-y, sample);
        uint8_t *d = c->pixels+((size_t)py*(size_t)c->width+(size_t)px)*4;
        oa_canvas_over(d, sample);
    }
    mip_release(levels, count);
    return 0;
}
