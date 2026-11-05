/* Test-only display.patch provider. It records the region requested by the
 * rail-highlight path so the recorder can prove that the animation used a
 * partial strip rather than a full-screen clear. No pixels/device access. */
#include "openaurora/module.h"
#include "openaurora/services.h"

#define TEST_PATCH_SVC "test.patch"
typedef struct {
    unsigned (*calls)(void);
    int (*last)(int *x, int *y, int *w, int *h);
} test_patch_iface;

static const oa_host_api *g_api;
static unsigned g_calls;
static int g_x, g_y, g_w, g_h;

static int fill_rgba(int x, int y, int w, int h, int r, int g, int b, int a)
{
    (void)r; (void)g; (void)b; (void)a;
    g_calls++;
    g_x = x; g_y = y; g_w = w; g_h = h;
    return 0;
}
static unsigned calls(void) { return g_calls; }
static int last(int *x, int *y, int *w, int *h)
{
    if (x) *x = g_x;
    if (y) *y = g_y;
    if (w) *w = g_w;
    if (h) *h = g_h;
    return 0;
}
static const oa_display_patch_iface g_patch = {fill_rgba};
static const test_patch_iface g_test = {calls, last};

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof(oa_host_api))
        return -1;
    g_api = api; *out = NULL; return 0;
}
static int m_start(void *v)
{
    (void)v;
    if (g_api->publish(g_api->priv, OA_SVC_DISPLAY_PATCH, 1, &g_patch)) return -1;
    if (g_api->publish(g_api->priv, TEST_PATCH_SVC, 1, &g_test)) {
        g_api->unpublish(g_api->priv, OA_SVC_DISPLAY_PATCH);
        return -1;
    }
    return 0;
}
static void m_stop(void *v)
{
    (void)v;
    g_api->unpublish(g_api->priv, TEST_PATCH_SVC);
    g_api->unpublish(g_api->priv, OA_SVC_DISPLAY_PATCH);
}
static void m_destroy(void *v) { (void)v; }

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "patch_fixture", .version = "1",
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
};
OA_MODULE_EXPORT(g_module)
