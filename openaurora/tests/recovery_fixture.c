/* Fake display plus a deliberately blocking callback, for recovery tests only. */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "openaurora/module.h"
#include "openaurora/services.h"

static const oa_host_api *api;
static int fail_present;
static int rgb(int r, int g, int b) { (void)r; (void)g; (void)b; return 0; }
static int fill(int x, int y, int w, int h, int r, int g, int b)
{ (void)x; (void)y; (void)w; (void)h; return rgb(r, g, b); }
static int blit(int x, int y, int w, int h, const unsigned char *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; return 0; }
static int scale(int x, int y, int w, int h, int sw, int sh, const unsigned char *p)
{ (void)sw; (void)sh; return blit(x, y, w, h, p); }
static int present(void) { return fail_present ? -1 : 0; }
static int info(int *w, int *h, int *bpp, int *frames)
{ if (w) *w = 1920; if (h) *h = 1080; if (bpp) *bpp = 32; if (frames) *frames = 2; return 0; }
static const oa_display_iface display = { rgb, fill, blit, scale, present, info };
static void stall(void *unused)
{
    (void)unused;
    const char *path = api->config_get(api->priv, "test.stall", "");
    if (*path && access(path, F_OK) == 0)
        for (;;) pause();
}
static int init(const oa_host_api *a, void **ctx) { api = a; *ctx = NULL; return 0; }
static int start(void *ctx)
{
    (void)ctx;
    if (!strcmp(api->config_get(api->priv, "test.start_stall", "0"), "1"))
        for (;;) pause();
    fail_present = !strcmp(api->config_get(api->priv, "test.present_fail", "0"), "1");
    api->publish(api->priv, OA_SVC_DISPLAY, 1, &display);
    api->timer_add(api->priv, 10, 10, stall, NULL);
    return 0;
}
static void stop(void *ctx) { (void)ctx; }
static const oa_module module = { .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "recovery_fixture", .version = "test", .init = init, .start = start,
    .stop = stop, .destroy = stop };
const oa_module *oa_module_entry(void) { return &module; }
