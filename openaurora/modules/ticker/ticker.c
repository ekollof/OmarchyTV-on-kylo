/*
 * mod_ticker — the smallest useful module.
 *
 * Publishes the "clock" service, ticks once a second, and keeps a counter that
 * survives `reload` (see save/restore) so the state-handoff path is exercised.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    uint64_t timer;
    uint64_t ticks;
};

/* Modules never link against host symbols: the only contract is the API table.
 * Each module keeps its own monotonic clock helper. */
static uint64_t clock_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static const oa_clock_iface g_clock = { clock_now_ms };

static void on_tick(void *ud)
{
    struct ctx *c = ud;
    c->ticks++;
    oa_tick_event ev = { clock_now_ms() };
    c->api->emit(c->api->priv, OA_TOPIC_TICK, &ev);
    if (c->ticks % 30 == 0)
        c->api->log(c->api->priv, OA_LOG_DEBUG, "tick %llu", (unsigned long long)c->ticks);
}

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    *out = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->api->publish(c->api->priv, OA_SVC_CLOCK, 1, &g_clock) != 0)
        return -1;
    c->timer = c->api->timer_add(c->api->priv, 1000, 1000, on_tick, c);
    return c->timer ? 0 : -1;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->timer) {
        c->api->timer_del(c->api->priv, c->timer);
        c->timer = 0;
    }
    c->api->unpublish(c->api->priv, OA_SVC_CLOCK);
}

static void m_destroy(void *v)
{
    free(v);
}

/* Keep the read-only clock interface alive while stopping event dispatch. */
static int m_suspend(void *v)
{
    struct ctx *c = v;
    if (c->timer) c->api->timer_del(c->api->priv, c->timer);
    c->timer = 0;
    return 0;
}
static int m_resume(void *v)
{
    struct ctx *c = v;
    c->timer = c->api->timer_add(c->api->priv, 1000, 1000, on_tick, c);
    return c->timer ? 0 : -1;
}

static size_t m_save(void *v, void *buf, size_t cap)
{
    struct ctx *c = v;
    if (!buf || cap < sizeof c->ticks)
        return sizeof c->ticks;
    memcpy(buf, &c->ticks, sizeof c->ticks);
    return sizeof c->ticks;
}

static int m_restore(void *v, const void *buf, size_t len)
{
    struct ctx *c = v;
    if (len != sizeof c->ticks)
        return -1;
    memcpy(&c->ticks, buf, sizeof c->ticks);
    return 0;
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "ticker",
    .version = "1.0",
    .requires = { "", 0 },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
    .suspend = m_suspend,
    .resume = m_resume,
    .save = m_save,
    .restore = m_restore,
};

OA_MODULE_EXPORT(g_module)
