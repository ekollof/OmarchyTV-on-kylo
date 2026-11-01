/* Vendor UI logical clock. No device, renderer or firmware function is called.
 * Sampling once per second keeps raw rollover tracking active while UI is idle.
 * See UI-ANIMATIONS for FROST source addresses and remaining clock-phase limits. */
#include <stdlib.h>
#include <string.h>
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/ui_time.h"

struct ctx {
    const oa_host_api *api;
    const oa_clock_iface *raw;
    oa_ui_time_state time;
    uint64_t timer;
    int published, suspended;
};
static struct ctx *g_ctx;
static uint32_t now_ms(void)
{
    return g_ctx && g_ctx->raw ? oa_ui_time_sample(&g_ctx->time, g_ctx->raw->now_ms()) : 0;
}
static const oa_ui_clock_iface iface = { now_ms, oa_ui_time_elapsed };
static void sample(void *v)
{
    struct ctx *c = v;
    if (c->raw && !c->suspended) (void)oa_ui_time_sample(&c->time, c->raw->now_ms());
}
static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof *api) return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api; *out = c; g_ctx = c;
    return 0;
}
static int m_start(void *v)
{
    struct ctx *c = v;
    c->raw = c->api->lookup(c->api->priv, OA_SVC_CLOCK, 1);
    if (!c->raw) return -1;
    c->timer = c->api->timer_add(c->api->priv, 1000, 1000, sample, c);
    if (!c->timer) return -1;
    c->suspended = 0;
    sample(c);
    int rc = c->api->publish(c->api->priv, OA_SVC_UI_CLOCK, 1, &iface);
    c->published = rc == 0;
    return rc;
}
static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->timer) c->api->timer_del(c->api->priv, c->timer);
    c->timer = 0;
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_UI_CLOCK);
    c->published = 0;
    c->api->release(c->api->priv, OA_SVC_CLOCK);
    c->raw = NULL;
}
static void m_destroy(void *v)
{
    if (g_ctx == v) g_ctx = NULL;
    free(v);
}
static int m_suspend(void *v)
{
    struct ctx *c = v;
    c->suspended = 1;
    if (c->timer) c->api->timer_del(c->api->priv, c->timer);
    c->timer = 0;
    return 0;
}
static int m_resume(void *v)
{
    struct ctx *c = v;
    if (!c->raw) return -1;
    uint64_t timer = c->api->timer_add(c->api->priv, 1000, 1000, sample, c);
    if (!timer) return -1;
    c->timer = timer; c->suspended = 0;
    sample(c);
    return 0;
}
static size_t m_save(void *v, void *buf, size_t cap)
{
    struct ctx *c = v;
    const uint32_t words[] = { 1, c->time.previous_raw, c->time.remainder, c->time.correction };
    if (buf && cap >= sizeof words) memcpy(buf, words, sizeof words);
    return sizeof words;
}
static int m_restore(void *v, const void *buf, size_t len)
{
    uint32_t words[4];
    if (len != sizeof words) return -1;
    memcpy(words, buf, sizeof words);
    if (words[0] != 1 || words[1] > OA_UI_TIME_PERIOD || words[2] > OA_UI_TIME_QUANTUM ||
        words[3] > OA_UI_TIME_PERIOD) return -1;
    struct ctx *c = v;
    c->time = (oa_ui_time_state){ words[1], words[2], words[3] };
    return 0;
}
static const oa_module module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "ui_clock", .version = "0.1", .requires = { OA_SVC_CLOCK, "" },
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
    .suspend = m_suspend, .resume = m_resume, .save = m_save, .restore = m_restore,
};
OA_MODULE_EXPORT(module)
