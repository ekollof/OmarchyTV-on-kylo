/* Vendor UI logical clock. No device, renderer or firmware function is called.
 * Sampling once per second keeps raw rollover tracking active while UI is idle.
 * See UI-ANIMATIONS for FROST source addresses and remaining clock-phase limits. */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/ui_time.h"

struct ctx {
    const oa_host_api *api;
    const oa_clock_iface *raw;
    oa_ui_time_state *time;
    uint64_t timer;
    int published, suspended;
    /* ABI 3 quiesce: the time state is detached and freed by a sched slice. */
    uint32_t cleanup_task;
    oa_ui_time_state *cleanup_time;
    unsigned cleanup_delay_ms;
    uint32_t saved[4];
    int have_saved;
};
static struct ctx *g_ctx;
static uint32_t now_ms(void)
{
    return g_ctx && g_ctx->raw && g_ctx->time ?
        oa_ui_time_sample(g_ctx->time, g_ctx->raw->now_ms()) : 0;
}
static const oa_ui_clock_iface iface = { now_ms, oa_ui_time_elapsed };
static void sample(void *v)
{
    struct ctx *c = v;
    if (c->raw && c->time && !c->suspended)
        (void)oa_ui_time_sample(c->time, c->raw->now_ms());
}
static size_t state_save(const oa_ui_time_state *t, void *buf, size_t cap)
{
    const uint32_t words[] = { 1, t->previous_raw, t->remainder, t->correction };
    if (buf && cap >= sizeof words) memcpy(buf, words, sizeof words);
    return sizeof words;
}

static int state_restore(oa_ui_time_state *t, const uint32_t *words)
{
    if (words[0] != 1 || words[1] > OA_UI_TIME_PERIOD ||
        words[2] > OA_UI_TIME_QUANTUM || words[3] > OA_UI_TIME_PERIOD)
        return -1;
    *t = (oa_ui_time_state){ words[1], words[2], words[3] };
    return 0;
}

static int cleanup_step(void *v, uint32_t *delay_ms)
{
    (void)delay_ms;
    struct ctx *c = v;
    free(c->cleanup_time);
    c->cleanup_time = NULL;
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
    free(c->cleanup_time);
    c->cleanup_time = NULL;
}

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof *api) return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->time = calloc(1, sizeof *c->time);
    if (!c->time) { free(c); return -1; }
    c->api = api; *out = c; g_ctx = c;
    return 0;
}
static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->published || c->cleanup_task || c->cleanup_time) return -EBUSY;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "ui_clock.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    if (!c->time && !(c->time = calloc(1, sizeof *c->time))) return -ENOMEM;
    if (c->have_saved) (void)state_restore(c->time, c->saved);
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
/* ABI 3 gate: unpublish ui.clock, remove its timer and release the raw clock
 * synchronously, snapshot/detach the time state, then free it from one
 * scheduler slice after ui_clock.cleanup_delay_ms. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
    if (c->cleanup_task || c->cleanup_time) return -EAGAIN;
    int published = c->published;
    c->published = 0;
    if (published) c->api->unpublish(c->api->priv, OA_SVC_UI_CLOCK);
    if (c->timer) c->api->timer_del(c->api->priv, c->timer);
    c->timer = 0;
    if (c->raw) c->api->release(c->api->priv, OA_SVC_CLOCK);
    c->raw = NULL;
    oa_ui_time_state *time = c->time;
    c->time = NULL;
    if (!time) return 0;
    (void)state_save(time, c->saved, sizeof c->saved);
    c->have_saved = 1;
    if (!c->cleanup_delay_ms) {
        free(time);
        return 0;
    }
    c->cleanup_time = time;
    const oa_sched_iface *s =
        c->api->lookup(c->api->priv, OA_SVC_SCHED, 1);
    if (!s) {
        free(time);
        c->cleanup_time = NULL;
        return 0;
    }
    uint32_t id = 0;
    int rc = s->submit(s->context, cleanup_step, c, c->cleanup_delay_ms, &id);
    c->api->release(c->api->priv, OA_SVC_SCHED);
    if (rc) {
        free(time);
        c->cleanup_time = NULL;
        return -EIO;
    }
    c->cleanup_task = id;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    c->published = 0;
    if (c->timer) c->api->timer_del(c->api->priv, c->timer);
    c->timer = 0;
    if (c->raw) c->api->release(c->api->priv, OA_SVC_CLOCK);
    c->raw = NULL;
    cancel_cleanup(c);
    free(c->time);
    c->time = NULL;
}
static void m_destroy(void *v)
{
    struct ctx *c = v;
    cancel_cleanup(c);
    free(c->time);
    if (g_ctx == c) g_ctx = NULL;
    free(c);
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
    if (c->time) return state_save(c->time, buf, cap);
    if (!c->have_saved) return 0;
    if (buf && cap >= sizeof c->saved) memcpy(buf, c->saved, sizeof c->saved);
    return sizeof c->saved;
}
static int m_restore(void *v, const void *buf, size_t len)
{
    uint32_t words[4];
    if (len != sizeof words) return -1;
    memcpy(words, buf, sizeof words);
    struct ctx *c = v;
    int allocated = 0;
    if (!c->time) {
        c->time = calloc(1, sizeof *c->time);
        if (!c->time) return -1;
        allocated = 1;
    }
    if (state_restore(c->time, words)) {
        if (allocated) { free(c->time); c->time = NULL; }
        return -1;
    }
    return 0;
}
static const oa_module module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "ui_clock", .version = "0.1", .requires = { OA_SVC_CLOCK, "" },
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
    .suspend = m_suspend, .resume = m_resume, .save = m_save, .restore = m_restore,
    .quiesce = m_quiesce,
};
OA_MODULE_EXPORT(module)
