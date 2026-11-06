/* Reloadable animation controller. The timer is the recovered 0 ms kick then
 * 10 ms cadence; sample() is the same update the timer runs. No framebuffer. */
#include <errno.h>
#include <stdlib.h>
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/ui_anim.h"

struct ctx {
    const oa_host_api *api;
    const oa_ui_clock_iface *clock;
    oa_ui_anim *anim;
    uint64_t timer;
    uint32_t id;
    int kick_pending;
    int suspended;
    int published;
    int timer_fault; /* EAGAIN once the owned kick cannot arm the 10 ms timer */
};
static struct ctx *g_ctx;
static void on_timer(void *ud);

static int set_enabled(int on)
{
    return g_ctx ? oa_ui_anim_set_enabled(g_ctx->anim, on) : -1;
}
static int enabled(void)
{
    return g_ctx ? oa_ui_anim_enabled(g_ctx->anim) : 0;
}
static void arm(struct ctx *c)
{
    if (c->timer) return;
    c->timer = c->api->timer_add(c->api->priv, 0, 10, on_timer, c);
}
static void kick(void *ud)
{
    struct ctx *c = ud;
    c->kick_pending = 0;
    if (c->published && !c->suspended) {
        arm(c); /* owned post dispatch attributes this timer to ui_anim */
        if (!c->timer) {
            c->timer_fault = EAGAIN;
            c->api->log(c->api->priv, OA_LOG_ERROR, "animation timer allocation failed");
        } else {
            c->timer_fault = 0;
        }
    }
}
static int fault(void)
{
    if (!g_ctx) return -1;
    return g_ctx->timer_fault;
}
static int start(int id, int kind, const oa_anim_rect *from, const oa_anim_rect *to,
                 uint32_t duration, uint32_t *started)
{
    if (!g_ctx || !g_ctx->clock) return -1;
    /* Service entry points execute under the consumer's owner token. Queue
     * the timer registration under our token; never borrow the caller's life. */
    if (!g_ctx->timer && !g_ctx->kick_pending) {
        if (g_ctx->api->post(g_ctx->api->priv, g_ctx->id, kick, g_ctx) != 0) return -1;
        g_ctx->kick_pending = 1;
    }
    uint32_t now = g_ctx->clock->now_ms();
    if (oa_ui_anim_start(g_ctx->anim, id, kind, from, to, duration, now) != 0) return -1;
    if (started) *started = now;
    return 0;
}
static int cancel(int id)
{
    if (!g_ctx) return -1;
    return oa_ui_anim_cancel(g_ctx->anim, id);
}
static int finalise(int id)
{
    if (!g_ctx) return -1;
    return oa_ui_anim_finalise(g_ctx->anim, id);
}
static int sample(uint32_t now)
{
    if (!g_ctx) return -1;
    return oa_ui_anim_sample(g_ctx->anim, now);
}
static int get(int id, int *kind, oa_anim_rect *rect, float *eased, uint32_t *started,
               uint32_t *duration)
{
    if (!g_ctx) return -1;
    return oa_ui_anim_get(g_ctx->anim, id, kind, rect, eased, started, duration);
}
static int bind(oa_anim_fn fn, void *ud)
{
    if (!g_ctx) return -1;
    oa_ui_anim_bind(g_ctx->anim, fn, ud);
    return 0;
}
static const oa_ui_anim_iface g_iface = {
    set_enabled, enabled, start, cancel, finalise, sample, get, bind,
    oa_anim_slide_duration, oa_anim_slide_margin, fault,
};

static void on_timer(void *ud)
{
    struct ctx *c = ud;
    if (!c->clock) return;
    int running = oa_ui_anim_sample(c->anim, c->clock->now_ms());
    if (running <= 0 && c->timer) {
        c->api->timer_del(c->api->priv, c->timer);
        c->timer = 0;
    }
}
static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof *api) return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->anim = oa_ui_anim_create();
    if (!c->anim) { free(c); return -1; }
    c->api = api;
    c->id = api->owner_id(api->priv);
    g_ctx = c;
    *out = c;
    return 0;
}
static int m_start(void *v)
{
    struct ctx *c = v;
    g_ctx = c;
    c->clock = c->api->lookup(c->api->priv, OA_SVC_UI_CLOCK, 1);
    if (!c->clock) return -1;
    if (c->api->publish(c->api->priv, OA_SVC_UI_ANIM, 3, &g_iface) != 0) return -1;
    c->published = 1;
    c->suspended = 0;
    return 0;
}
static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->timer) c->api->timer_del(c->api->priv, c->timer);
    c->timer = 0;
    c->kick_pending = 0;
    c->timer_fault = 0;
    oa_ui_anim_bind(c->anim, NULL, NULL);
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_UI_ANIM);
    c->published = 0;
    if (c->clock) c->api->release(c->api->priv, OA_SVC_UI_CLOCK);
    c->clock = NULL;
}
static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (g_ctx == c) g_ctx = NULL;
    oa_ui_anim_destroy(c->anim);
    free(c);
}
static int m_suspend(void *v)
{
    struct ctx *c = v;
    if (c->timer) c->api->timer_del(c->api->priv, c->timer);
    c->timer = 0;
    c->suspended = 1;
    return 0;
}
static int m_resume(void *v)
{
    struct ctx *c = v;
    if (!c->clock) return -1;
    /* Suspend removed the cadence timer, not the animations it drives. An
     * empty controller disarms this timer on its first sample. */
    arm(c);
    if (!c->timer) return -1;
    c->suspended = 0;
    return 0;
}
static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "ui_anim", .version = "1",
    .requires = { OA_SVC_UI_CLOCK, "" },
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
    .suspend = m_suspend, .resume = m_resume,
};
OA_MODULE_EXPORT(g_module)
