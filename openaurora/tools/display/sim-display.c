/* Offline QEMU framebuffer display provider.
 *
 * Publishes the `display` service and writes every presented 1920x1080 RGBA
 * canvas to $OA_NAV_CAPTURE.<sequence>. It uses only integer alpha blending
 * and nearest-neighbour scaling, so the ARM build has no libm dependency and
 * runs against the TV's glibc 2.23 rootfs. This is a layout oracle, not
 * scanout or vendor-render proof.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/module.h"
#include "openaurora/services.h"

#define WIDTH 1920
#define HEIGHT 1080

static unsigned char *g_pixels;
static unsigned g_seq;

static int ensure_canvas(void)
{
    if (!g_pixels) g_pixels = calloc((size_t)WIDTH * HEIGHT, 4);
    return g_pixels ? 0 : -1;
}

static void blend(unsigned char *dst, const unsigned char *src)
{
    unsigned a = src[3];
    if (!a) return;
    if (a == 255) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = 255;
        return;
    }
    unsigned inv = 255u - a;
    dst[0] = (unsigned char)((src[0] * a + dst[0] * inv) / 255u);
    dst[1] = (unsigned char)((src[1] * a + dst[1] * inv) / 255u);
    dst[2] = (unsigned char)((src[2] * a + dst[2] * inv) / 255u);
    dst[3] = (unsigned char)(a + (dst[3] * inv) / 255u);
}

static int d_info(int *w, int *h, int *bpp, int *frames)
{
    if (w) *w = WIDTH;
    if (h) *h = HEIGHT;
    if (bpp) *bpp = 32;
    if (frames) *frames = 1;
    return 0;
}

static int d_clear_rgba(int r, int g, int b, int a)
{
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 ||
        a < 0 || a > 255)
        return -1;
    if (ensure_canvas()) return -1;
    for (size_t i = 0; i < (size_t)WIDTH * HEIGHT; i++) {
        g_pixels[i * 4 + 0] = a ? (unsigned char)r : 0;
        g_pixels[i * 4 + 1] = a ? (unsigned char)g : 0;
        g_pixels[i * 4 + 2] = a ? (unsigned char)b : 0;
        g_pixels[i * 4 + 3] = (unsigned char)a;
    }
    return 0;
}

static int d_clear(int r, int g, int b) { return d_clear_rgba(r, g, b, 255); }

static int d_fill(int x, int y, int w, int h, int r, int g, int b)
{
    if (x < 0 || y < 0 || w < 0 || h < 0 || x + w > WIDTH || y + h > HEIGHT)
        return -1;
    if (ensure_canvas()) return -1;
    unsigned char src[4] = { (unsigned char)r, (unsigned char)g,
                             (unsigned char)b, 255 };
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            blend(g_pixels + ((size_t)yy * WIDTH + xx) * 4, src);
    return 0;
}

static int d_blit(int x, int y, int w, int h, const unsigned char *rgba)
{
    if (!rgba || x < 0 || y < 0 || w < 0 || h < 0 ||
        x + w > WIDTH || y + h > HEIGHT)
        return -1;
    if (ensure_canvas()) return -1;
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
            blend(g_pixels + ((size_t)(y + yy) * WIDTH + (x + xx)) * 4,
                  rgba + ((size_t)yy * w + xx) * 4);
    return 0;
}

static int d_blit_scaled(int x, int y, int w, int h, int sw, int sh,
                         const unsigned char *rgba)
{
    if (!rgba || sw <= 0 || sh <= 0 || w < 0 || h < 0 ||
        x < 0 || y < 0 || x + w > WIDTH || y + h > HEIGHT)
        return -1;
    if (ensure_canvas()) return -1;
    for (int yy = 0; yy < h; yy++) {
        int sy = sh == h ? yy : (int)((long long)yy * sh / h);
        if (sy >= sh) sy = sh - 1;
        for (int xx = 0; xx < w; xx++) {
            int sx = sw == w ? xx : (int)((long long)xx * sw / w);
            if (sx >= sw) sx = sw - 1;
            blend(g_pixels + ((size_t)(y + yy) * WIDTH + (x + xx)) * 4,
                  rgba + ((size_t)sy * sw + sx) * 4);
        }
    }
    return 0;
}

static int d_present(void)
{
    const char *prefix = getenv("OA_NAV_CAPTURE");
    if (!prefix || ensure_canvas()) return 0;
    char path[512];
    if (snprintf(path, sizeof path, "%s.%u", prefix, g_seq++) >= (int)sizeof path)
        return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t n = fwrite(g_pixels, 4, (size_t)WIDTH * HEIGHT, f);
    return fclose(f) == 0 && n == (size_t)WIDTH * HEIGHT ? 0 : -1;
}

static int d_set_scene_transform(int type, float progress)
{
    (void)type;
    (void)progress;
    return 0;
}

static const oa_display_iface g_display = {
    .clear = d_clear,
    .fill = d_fill,
    .blit = d_blit,
    .blit_scaled = d_blit_scaled,
    .present = d_present,
    .info = d_info,
    .set_scene_transform = d_set_scene_transform,
};
static const oa_display_alpha_iface g_alpha = { .clear_rgba = d_clear_rgba };

struct ctx {
    const oa_host_api *api;
    int published;
};

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof *api) return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api;
    *out = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (ensure_canvas()) return -1;
    int rc = c->api->publish(c->api->priv, OA_SVC_DISPLAY, 1, &g_display);
    if (rc) return rc;
    rc = c->api->publish(c->api->priv, OA_SVC_DISPLAY_ALPHA, 1, &g_alpha);
    if (rc) c->api->unpublish(c->api->priv, OA_SVC_DISPLAY);
    c->published = 1;
    return rc;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (!c->published) return;
    c->api->unpublish(c->api->priv, OA_SVC_DISPLAY_ALPHA);
    c->api->unpublish(c->api->priv, OA_SVC_DISPLAY);
    c->published = 0;
}

static void m_destroy(void *v)
{
    free(g_pixels);
    g_pixels = NULL;
    free(v);
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "sim_display",
    .version = "0.1",
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
};
OA_MODULE_EXPORT(g_module)
