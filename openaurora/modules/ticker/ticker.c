/*
 * mod_ticker — the smallest useful module.
 *
 * Publishes the "clock" service, ticks once a second, and keeps a counter that
 * survives `reload` (see save/restore) so the state-handoff path is exercised.
 */
#include <errno.h>
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
    uint64_t *ticks;
    int published;
    /* ABI 3 quiesce: the counter is detached and freed by a sched slice. */
    uint32_t cleanup_task;
    uint64_t *cleanup_ticks;
    unsigned cleanup_delay_ms;
    uint64_t saved_ticks;
    int have_saved;
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
    if (!c->ticks) return;
    (*c->ticks)++;
    oa_tick_event ev = { clock_now_ms() };
    c->api->emit(c->api->priv, OA_TOPIC_TICK, &ev);
    if (*c->ticks % 30 == 0)
        c->api->log(c->api->priv, OA_LOG_DEBUG, "tick %llu",
                     (unsigned long long)*c->ticks);
}

static int cleanup_step(void *v, uint32_t *delay_ms)
{
    (void)delay_ms;
    struct ctx *c = v;
    free(c->cleanup_ticks);
    c->cleanup_ticks = NULL;
    c->cleanup_task = 0;
    return OA_SCHED_DONE;
}

static void cancel_cleanup(struct ctx *c)
{
    if (c->cleanup_task) {
        const oa_sched_iface *s =
            c->api->lookup(c->api->priv, OA_SVC_SCHED, 1);
        if (s) {
            (void)s->cancel(s->context, c->cleanup_task);
            c->api->release(c->api->priv, OA_SVC_SCHED);
        }
        c->cleanup_task = 0;
    }
    free(c->cleanup_ticks);
    c->cleanup_ticks = NULL;
}

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->ticks = calloc(1, sizeof *c->ticks);
    if (!c->ticks) { free(c); return -1; }
    c->api = api;
    *out = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->published || c->cleanup_task || c->cleanup_ticks) return -EBUSY;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "ticker.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    if (!c->ticks) {
        c->ticks = calloc(1, sizeof *c->ticks);
        if (!c->ticks) return -ENOMEM;
    }
    if (c->have_saved) *c->ticks = c->saved_ticks;
    if (c->api->publish(c->api->priv, OA_SVC_CLOCK, 1, &g_clock) != 0)
        return -1;
    c->published = 1;
    c->timer = c->api->timer_add(c->api->priv, 1000, 1000, on_tick, c);
    return c->timer ? 0 : -1;
}

/* ABI 3 gate: unpublish clock and remove the timer synchronously, snapshot
 * and detach the tick counter, then free it from a scheduler slice after
 * ticker.cleanup_delay_ms. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
    if (c->cleanup_task || c->cleanup_ticks) return -EAGAIN;
    if (c->timer) {
        c->api->timer_del(c->api->priv, c->timer);
        c->timer = 0;
    }
    int published = c->published;
    c->published = 0;
    if (published) c->api->unpublish(c->api->priv, OA_SVC_CLOCK);
    uint64_t *ticks = c->ticks;
    c->ticks = NULL;
    if (!ticks) return 0;
    c->saved_ticks = *ticks;
    c->have_saved = 1;
    if (!c->cleanup_delay_ms) {
        free(ticks);
        return 0;
    }
    c->cleanup_ticks = ticks;
    const oa_sched_iface *s =
        c->api->lookup(c->api->priv, OA_SVC_SCHED, 1);
    if (!s) {
        free(ticks);
        c->cleanup_ticks = NULL;
        return 0;
    }
    uint32_t id = 0;
    int rc = s->submit(s->context, cleanup_step, c, c->cleanup_delay_ms, &id);
    c->api->release(c->api->priv, OA_SVC_SCHED);
    if (rc) {
        free(ticks);
        c->cleanup_ticks = NULL;
        return -EIO;
    }
    c->cleanup_task = id;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->timer) {
        c->api->timer_del(c->api->priv, c->timer);
        c->timer = 0;
    }
    int published = c->published;
    c->published = 0;
    if (published) c->api->unpublish(c->api->priv, OA_SVC_CLOCK);
    cancel_cleanup(c);
    free(c->ticks);
    c->ticks = NULL;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    cancel_cleanup(c);
    free(c->ticks);
    free(c);
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
    uint64_t value = c->ticks ? *c->ticks : c->saved_ticks;
    if (!c->ticks && !c->have_saved) return 0;
    if (buf && cap >= sizeof value) memcpy(buf, &value, sizeof value);
    return sizeof value;
}

static int m_restore(void *v, const void *buf, size_t len)
{
    struct ctx *c = v;
    if (len != sizeof c->saved_ticks) return -1;
    memcpy(&c->saved_ticks, buf, sizeof c->saved_ticks);
    c->have_saved = 1;
    if (!c->ticks && !(c->ticks = calloc(1, sizeof *c->ticks))) return -1;
    *c->ticks = c->saved_ticks;
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
    .quiesce = m_quiesce,
};

OA_MODULE_EXPORT(g_module)
