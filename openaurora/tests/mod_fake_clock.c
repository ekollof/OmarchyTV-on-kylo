/* Test-only clock provider: publishes ui.clock v1 with a settable now, plus fake.clock so the test
 * can drive time deterministically. Lets the text-slide/repeat path be tested without sleeping. */
#include <stdlib.h>

#include "openaurora/module.h"
#include "openaurora/services.h"

#define OA_SVC_FAKE_CLOCK "fake.clock"
typedef struct {
    void (*set)(uint32_t now_ms);
    uint32_t (*get)(void);
} oa_fake_clock_iface;

static uint32_t g_now;

static uint32_t clock_now(void) { return g_now; }
static uint32_t clock_elapsed(uint32_t now, uint32_t before) { return now - before; }
static void fake_set(uint32_t now) { g_now = now; }
static uint32_t fake_get(void) { return g_now; }

static const oa_ui_clock_iface g_clock = { clock_now, clock_elapsed };
static const oa_fake_clock_iface g_fake = { fake_set, fake_get };

static const oa_host_api *g_api;

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof(oa_host_api))
        return -1;
    g_api = api;
    *out = (void *)&g_clock;
    return 0;
}

static int m_start(void *v)
{
    (void)v;
    if (g_api->publish(g_api->priv, OA_SVC_UI_CLOCK, 1, &g_clock) != 0) return -1;
    if (g_api->publish(g_api->priv, OA_SVC_FAKE_CLOCK, 1, &g_fake) != 0) {
        g_api->unpublish(g_api->priv, OA_SVC_UI_CLOCK);
        return -1;
    }
    return 0;
}

static void m_stop(void *v)
{
    (void)v;
    g_api->unpublish(g_api->priv, OA_SVC_FAKE_CLOCK);
    g_api->unpublish(g_api->priv, OA_SVC_UI_CLOCK);
}

static void m_destroy(void *v) { (void)v; }

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "fake_clock", .version = "1",
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
};

OA_MODULE_EXPORT(g_module)
