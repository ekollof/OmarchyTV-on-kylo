#include <string.h>
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "ui_nav_trace.h"

static int g_clears, g_presents, g_w, g_h;
static int g_ax, g_ay, g_aw, g_ah, g_accent;
enum { FILL_MAX = 32 };
static struct { int x, y, w, h, r, g, b; } g_fill[FILL_MAX];
static int g_nf;
enum { BLIT_MAX = 64 };
static struct { int x, y, w, h, r, g, b, a; } g_blit[BLIT_MAX];
static int g_nb;
enum { SCALED_MAX = 32 };
static struct { int x, y, w, h, sw, sh, r, g, b, a; } g_scaled[SCALED_MAX];
static int g_ns;

static int d_info(int *w, int *h, int *bpp, int *frames)
{
    g_w = 1920;
    g_h = 1080;
    if (w) *w = g_w;
    if (h) *h = g_h;
    if (bpp) *bpp = 32;
    if (frames) *frames = 1;
    return 0;
}
static int d_clear(int r, int g, int b)
{
    (void)r; (void)g; (void)b;
    g_clears++;
    g_nf = 0;
    g_nb = 0;
    g_ns = 0;
    return 0;
}
static int d_fill(int x, int y, int w, int h, int r, int g, int b)
{
    if (g_nf < FILL_MAX) {
        g_fill[g_nf].x = x;
        g_fill[g_nf].y = y;
        g_fill[g_nf].w = w;
        g_fill[g_nf].h = h;
        g_fill[g_nf].r = r;
        g_fill[g_nf].g = g;
        g_fill[g_nf].b = b;
        g_nf++;
    }
    if (r == 247 && g == 148 && b == 30) {
        g_ax = x;
        g_ay = y;
        g_aw = w;
        g_ah = h;
        g_accent++;
    }
    return 0;
}
static int d_blit(int x, int y, int w, int h, const unsigned char *rgba)
{
    if (g_nb < BLIT_MAX) {
        g_blit[g_nb].x = x; g_blit[g_nb].y = y; g_blit[g_nb].w = w; g_blit[g_nb].h = h;
        g_blit[g_nb].r = rgba ? rgba[0] : -1;
        g_blit[g_nb].g = rgba ? rgba[1] : -1;
        g_blit[g_nb].b = rgba ? rgba[2] : -1;
        g_blit[g_nb].a = rgba ? rgba[3] : -1;
        g_nb++;
    }
    return 0;
}
static int d_blit_scaled(int x, int y, int w, int h, int sw, int sh, const unsigned char *rgba)
{
    if (g_ns < SCALED_MAX) {
        g_scaled[g_ns].x = x; g_scaled[g_ns].y = y; g_scaled[g_ns].w = w; g_scaled[g_ns].h = h;
        g_scaled[g_ns].sw = sw; g_scaled[g_ns].sh = sh;
        g_scaled[g_ns].r = rgba ? rgba[0] : -1;
        g_scaled[g_ns].g = rgba ? rgba[1] : -1;
        g_scaled[g_ns].b = rgba ? rgba[2] : -1;
        g_scaled[g_ns].a = rgba ? rgba[3] : -1;
        g_ns++;
    }
    return 0;
}
static int d_present(void)
{
    g_presents++;
    return 0;
}
static const oa_display_iface g_disp = {
    .clear = d_clear, .fill = d_fill, .blit = d_blit, .blit_scaled = d_blit_scaled,
    .present = d_present, .info = d_info,
};
static int t_accent(int *x, int *y, int *w, int *h)
{
    if (!g_accent) return -1;
    if (x) *x = g_ax;
    if (y) *y = g_ay;
    if (w) *w = g_aw;
    if (h) *h = g_ah;
    return 0;
}
static int t_counts(int *clears, int *presents, int *surface_w, int *surface_h)
{
    if (clears) *clears = g_clears;
    if (presents) *presents = g_presents;
    if (surface_w) *surface_w = g_w;
    if (surface_h) *surface_h = g_h;
    return 0;
}
static int t_fill_count(void) { return g_nf; }
static int t_fill_at(int i, int *x, int *y, int *w, int *h, int *r, int *g, int *b)
{
    if (i < 0 || i >= g_nf) return -1;
    if (x) *x = g_fill[i].x;
    if (y) *y = g_fill[i].y;
    if (w) *w = g_fill[i].w;
    if (h) *h = g_fill[i].h;
    if (r) *r = g_fill[i].r;
    if (g) *g = g_fill[i].g;
    if (b) *b = g_fill[i].b;
    return 0;
}
static int t_blit_count(void) { return g_nb; }
static int t_blit_at(int i, int *x, int *y, int *w, int *h)
{
    if (i < 0 || i >= g_nb) return -1;
    if (x) *x = g_blit[i].x;
    if (y) *y = g_blit[i].y;
    if (w) *w = g_blit[i].w;
    if (h) *h = g_blit[i].h;
    return 0;
}
static int t_blit_first(int i, int *r, int *g, int *b, int *a)
{
    if (i < 0 || i >= g_nb) return -1;
    if (r) *r = g_blit[i].r;
    if (g) *g = g_blit[i].g;
    if (b) *b = g_blit[i].b;
    if (a) *a = g_blit[i].a;
    return 0;
}
static int t_scaled_count(void) { return g_ns; }
static int t_scaled_at(int i, int *x, int *y, int *w, int *h, int *sw, int *sh)
{
    if (i < 0 || i >= g_ns) return -1;
    if (x) *x = g_scaled[i].x;
    if (y) *y = g_scaled[i].y;
    if (w) *w = g_scaled[i].w;
    if (h) *h = g_scaled[i].h;
    if (sw) *sw = g_scaled[i].sw;
    if (sh) *sh = g_scaled[i].sh;
    return 0;
}
static int t_scaled_first(int i, int *r, int *g, int *b, int *a)
{
    if (i < 0 || i >= g_ns) return -1;
    if (r) *r = g_scaled[i].r;
    if (g) *g = g_scaled[i].g;
    if (b) *b = g_scaled[i].b;
    if (a) *a = g_scaled[i].a;
    return 0;
}
static const oa_nav_trace g_trace = {
    .accent = t_accent, .counts = t_counts, .fill_count = t_fill_count, .fill_at = t_fill_at,
    .blit_count = t_blit_count, .blit_at = t_blit_at, .blit_first = t_blit_first,
    .scaled_count = t_scaled_count, .scaled_at = t_scaled_at, .scaled_first = t_scaled_first,
};

static const oa_host_api *g_api;

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION)
        return -1;
    g_api = api;
    *out = (void *)&g_trace;
    return 0;
}
static int m_start(void *v)
{
    (void)v;
    if (g_api->publish(g_api->priv, OA_SVC_DISPLAY, 1, &g_disp) != 0)
        return -1;
    if (g_api->publish(g_api->priv, OA_SVC_NAV_TRACE, 1, &g_trace) != 0)
        return -1;
    return 0;
}
static void m_stop(void *v)
{
    (void)v;
    g_api->unpublish(g_api->priv, OA_SVC_NAV_TRACE);
    g_api->unpublish(g_api->priv, OA_SVC_DISPLAY);
}
static void m_destroy(void *v)
{
    (void)v;
}
static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "nav_display", .version = "1",
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
};

OA_MODULE_EXPORT(g_module)
