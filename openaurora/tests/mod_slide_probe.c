/* Live diagnostic fixture: publishes a recording `display` and a `ui.clock` backed by the TV's
 * CLOCK_MONOTONIC (folded into the vendor 90,000,000 ms period), so mod_ui_vendor's slide can be
 * observed with real timers on the set. It draws nothing and opens no device.
 *
 * Every title blit (y == 80) prints: wall ms, dest x/w, and the source column (red channel).
 * Build for the TV toolchain and stage only in /tmp.
 */
#include <stdio.h>
#include <time.h>

#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/ui_time.h"

static const oa_host_api *g_api;

static uint64_t wall_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static uint32_t clock_now(void)
{
    return (uint32_t)(wall_ms() % OA_UI_TIME_PERIOD);
}
static uint32_t clock_elapsed(uint32_t now, uint32_t before)
{
    return oa_ui_time_elapsed(now, before);
}
static const oa_ui_clock_iface g_clock = { clock_now, clock_elapsed };

static int d_info(int *w, int *h, int *bpp, int *frames)
{
    if (w) *w = 1920;
    if (h) *h = 1080;
    if (bpp) *bpp = 32;
    if (frames) *frames = 1;
    return 0;
}
static int d_clear(int r, int g, int b) { (void)r; (void)g; (void)b; return 0; }
static int d_fill(int x, int y, int w, int h, int r, int g, int b)
{
    (void)x; (void)y; (void)w; (void)h; (void)r; (void)g; (void)b;
    return 0;
}
static int d_blit(int x, int y, int w, int h, const unsigned char *rgba)
{
    (void)h;
    if (y == 80)
        fprintf(stderr, "[probe] wall=%llu x=%d w=%d shift=%u\n",
                (unsigned long long)wall_ms(), x, w, rgba ? rgba[0] : 0);
    return 0;
}
static int d_blit_scaled(int x, int y, int w, int h, int sw, int sh, const unsigned char *rgba)
{
    (void)sw; (void)sh;
    if (w == 160 && h == 160)
        fprintf(stderr, "[probe] hl wall=%llu x=%d y=%d frame=%u\n",
                (unsigned long long)wall_ms(), x, y, rgba ? rgba[0] : 0);
    return 0;
}
static int d_present(void) { return 0; }
static const oa_display_iface g_disp = {
    .clear = d_clear, .fill = d_fill, .blit = d_blit, .blit_scaled = d_blit_scaled,
    .present = d_present, .info = d_info,
};

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof(oa_host_api))
        return -1;
    g_api = api;
    *out = NULL;
    return 0;
}

static int m_start(void *v)
{
    (void)v;
    if (g_api->publish(g_api->priv, OA_SVC_DISPLAY, 1, &g_disp) != 0) return -1;
    if (g_api->publish(g_api->priv, OA_SVC_UI_CLOCK, 1, &g_clock) != 0) {
        g_api->unpublish(g_api->priv, OA_SVC_DISPLAY);
        return -1;
    }
    fprintf(stderr, "[probe] started at wall=%llu\n", (unsigned long long)wall_ms());
    return 0;
}

static void m_stop(void *v)
{
    (void)v;
    g_api->unpublish(g_api->priv, OA_SVC_UI_CLOCK);
    g_api->unpublish(g_api->priv, OA_SVC_DISPLAY);
}

static void m_destroy(void *v) { (void)v; }

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "slide_probe", .version = "1",
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
};

OA_MODULE_EXPORT(g_module)
