/* mod_ui_omarchy — OmarchyTV skin skeleton over ui.engine.
 *
 * This is the second skin for the engine seam: it owns its own screens and key
 * semantics, draws only through ui.engine, and does not use the vendor assets.
 * The real OmarchyTV UI replaces paint()/key() here; the vendor skin remains the
 * fallback. It deliberately publishes no service yet (no portal contract).
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/events.h"
#include "openaurora/lua_skin.h"
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/ui_engine.h"
#include "openaurora/ui_omarchy.h"

struct ctx {
    const oa_host_api *api;
    const oa_ui_engine_iface *d;
    const oa_lua_iface *lua;
    oa_omarchy_state *state;
    int active;
    int suspended;
    uint32_t removed, changed;
    uint64_t rebind;
    /* ABI 3 quiesce: cached skin state is detached and freed by a sched slice. */
    uint32_t cleanup_task;
    oa_omarchy_state *cleanup_state;
    unsigned cleanup_delay_ms;
    oa_omarchy_state saved;
    int have_saved;
};
static const oa_ui_skin g_skin;
static void dispatch(void *ud)
{
    struct ctx *c = ud;
    if (c && c->active && c->d && !c->suspended) c->d->dispatch_skin();
}

static void take_lua_paint(struct ctx *c, int *br, int *bg, int *bb, int *ba,
                           int *ar, int *ag, int *ab, int *x, int *y, int *w, int *h)
{
    int bgc[4], acc[4];
    oa_lua_layout lay;
    oa_omarchy_state *st = c->state;
    if (!st || !c->lua || c->lua->call("paint", "omarchy", 0, NULL) != 0)
        return;
    if (c->lua->theme(bgc, acc) == 0) {
        *br = bgc[0];
        *bg = bgc[1];
        *bb = bgc[2];
        *ba = bgc[3];
        *ar = acc[0];
        *ag = acc[1];
        *ab = acc[2];
        st->bg[0] = *br;
        st->bg[1] = *bg;
        st->bg[2] = *bb;
        st->bg[3] = *ba;
        st->accent[0] = *ar;
        st->accent[1] = *ag;
        st->accent[2] = *ab;
        st->have_colors = 1;
    }
    if (c->lua->layout_at(0, &lay) == 0 && lay.w > 0 && lay.h > 0) {
        *x = lay.x;
        *y = lay.y;
        *w = lay.w;
        *h = lay.h;
        st->layout[0] = lay.x;
        st->layout[1] = lay.y;
        st->layout[2] = lay.w;
        st->layout[3] = lay.h;
        st->have_layout = 1;
    }
}

static void paint(struct ctx *c)
{
    const oa_ui_engine_iface *d = c ? c->d : NULL;
    int W = 0, H = 0;
    int br, bg, bb, ba, ar, ag, ab, x, y, w, h;
    if (!c || !d || !c->active || !c->state || c->suspended)
        return;
    if (d->info(&W, &H, NULL, NULL) != 0 || W <= 0 || H <= 0)
        return;
    oa_omarchy_state *st = c->state;
    st->screen[0] = W;
    st->screen[1] = H;
    st->have_screen = 1;
    br = st->have_colors ? st->bg[0] : 12;
    bg = st->have_colors ? st->bg[1] : 14;
    bb = st->have_colors ? st->bg[2] : 18;
    ba = st->have_colors ? st->bg[3] : 255;
    ar = st->have_colors ? st->accent[0] : 0;
    ag = st->have_colors ? st->accent[1] : 122;
    ab = st->have_colors ? st->accent[2] : 204;
    x = st->have_layout ? st->layout[0] : 0;
    y = st->have_layout ? st->layout[1] : 0;
    w = st->have_layout ? st->layout[2] : 14;
    h = st->have_layout ? st->layout[3] : H;
    take_lua_paint(c, &br, &bg, &bb, &ba, &ar, &ag, &ab, &x, &y, &w, &h);
    d->clear_rgba(br, bg, bb, ba);
    d->fill(x, y, w, h, ar, ag, ab);
    d->fill(40, 40, W - 80, 8, ar, ag, ab);
    d->fill(40, 88, 300, 180, 24, 28, 36);
    d->fill(360, 88, 300, 180, 24, 28, 36);
    d->fill(40, 288, 620, 80, 24, 28, 36);
    (void)d->present();
}

static int enter(void *ud)
{
    struct ctx *c = ud;
    if (!c || !c->active) return 0;
    if (c->lua)
        (void)c->lua->call("enter", "omarchy", 0, NULL);
    paint(ud);
    return 0;
}

static void leave(void *ud)
{
    struct ctx *c = ud;
    if (!c || !c->active) return;
    if (c->lua)
        (void)c->lua->call("leave", "omarchy", 0, NULL);
}

static void skin_paint(void *ud)
{
    paint(ud);
}

static int skin_key(void *ud, int id)
{
    struct ctx *c = ud;
    int handled = 0;
    if (!c || !c->active)
        return 1;
    /* The skeleton has no navigation yet; every press repaints so the seam is
     * observable. A loaded script may claim the key. Exit/Menu are ignored. */
    if (c->lua && c->lua->call("key", "omarchy", id, &handled) == 0 && handled)
        paint(c);
    else
        paint(c);
    return 0;
}

static const oa_ui_skin g_skin = { "omarchy", enter, leave, skin_paint, skin_key };

static void rebind(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    c->rebind = 0;
    if (!c->active || c->suspended || c->d) return;
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 2);
    if (!c->d) return;
    if (c->d->register_skin_dispatch(&g_skin, c, dispatch)) {
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
        return;
    }
    if (!c->lua)
        c->lua = c->api->lookup(c->api->priv, OA_SVC_UI_LUA, OA_LUA_API_VERSION);
}
static void changed(const void *ev, void *ud)
{
    (void)ev;
    struct ctx *c = ud;
    if (!c || !c->active) return;
    if (!c->suspended && !c->d && !c->rebind)
        c->rebind = c->api->timer_add(c->api->priv, 0, 0, rebind, c);
}
static void removed(const void *ev, void *ud)
{
    const oa_service_removed_event *e = ev;
    struct ctx *c = ud;
    if (!c || !c->active || !e || !e->name)
        return;
    if (!strcmp(e->name, OA_SVC_UI_LUA)) {
        c->lua = NULL;
        return;
    }
    if (!strcmp(e->name, OA_SVC_UI_ENGINE)) {
        c->d = NULL;
        changed(NULL, c);
    }
}

static oa_omarchy_state *state_new(void)
{
    oa_omarchy_state *st = calloc(1, sizeof *st);
    if (st) {
        st->magic = OA_OMARCHY_STATE_MAGIC;
        st->version = OA_OMARCHY_STATE_VERSION;
    }
    return st;
}

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->state = state_new();
    if (!c->state) { free(c); return -1; }
    c->api = api;
    *out = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->active || c->cleanup_task || c->cleanup_state) return -EBUSY;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "ui_omarchy.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    if (!c->state && !(c->state = state_new())) return -ENOMEM;
    if (c->have_saved) *c->state = c->saved;
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 2);
    if (!c->d)
        return -1;
    c->active = 1;
    if (c->d->register_skin_dispatch(&g_skin, c, dispatch) != 0) {
        c->active = 0;
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
        return -1;
    }
    c->removed = c->api->subscribe(c->api->priv, OA_TOPIC_SERVICE_REMOVED, removed, c);
    c->changed = c->api->subscribe(c->api->priv, OA_TOPIC_MODULE_CHANGED, changed, c);
    if (!c->removed || !c->changed) return -1;
    c->lua = c->api->lookup(c->api->priv, OA_SVC_UI_LUA, OA_LUA_API_VERSION);
    if (atoi(c->api->config_get(c->api->priv, "ui.omarchy.lua", "0")) == 1) {
        oa_lua_status st;
        if (!c->lua || c->lua->status(&st) != 0 || !st.loaded)
            return -1;
    }
    if (c->api->config_get(c->api->priv, "ui.paint", "1")[0] == '1')
        paint(c);
    return 0;
}

static int cleanup_step(void *v, uint32_t *delay_ms)
{
    (void)delay_ms;
    struct ctx *c = v;
    free(c->cleanup_state);
    c->cleanup_state = NULL;
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
    free(c->cleanup_state);
    c->cleanup_state = NULL;
}

/* ABI 3 gate: invalidate callbacks and tear down the engine/lua registrations
 * synchronously, snapshot/detach the cached skin state, then free it from a
 * scheduler slice after ui_omarchy.cleanup_delay_ms. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
    if (c->cleanup_task || c->cleanup_state) return -EAGAIN;
    c->active = 0;
    if (c->rebind) {
        c->api->timer_del(c->api->priv, c->rebind);
        c->rebind = 0;
    }
    if (c->removed) {
        c->api->unsubscribe(c->api->priv, c->removed);
        c->removed = 0;
    }
    if (c->changed) {
        c->api->unsubscribe(c->api->priv, c->changed);
        c->changed = 0;
    }
    if (c->d) {
        c->d->unregister_skin(&g_skin);
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
    }
    if (c->lua) {
        c->api->release(c->api->priv, OA_SVC_UI_LUA);
        c->lua = NULL;
    }
    c->suspended = 0;
    oa_omarchy_state *st = c->state;
    c->state = NULL;
    if (!st) return 0;
    c->saved = *st;
    c->have_saved = 1;
    if (!c->cleanup_delay_ms) {
        free(st);
        return 0;
    }
    c->cleanup_state = st;
    const oa_sched_iface *s =
        c->api->lookup(c->api->priv, OA_SVC_SCHED, 1);
    if (!s) {
        free(st);
        c->cleanup_state = NULL;
        return 0;
    }
    uint32_t id = 0;
    int rc = s->submit(s->context, cleanup_step, c, c->cleanup_delay_ms, &id);
    c->api->release(c->api->priv, OA_SVC_SCHED);
    if (rc) {
        free(st);
        c->cleanup_state = NULL;
        return -EIO;
    }
    c->cleanup_task = id;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    c->active = 0;
    if (c->removed) c->api->unsubscribe(c->api->priv, c->removed);
    if (c->changed) c->api->unsubscribe(c->api->priv, c->changed);
    c->removed = c->changed = 0;
    if (c->rebind) c->api->timer_del(c->api->priv, c->rebind);
    c->rebind = 0;
    if (c->d) {
        c->d->unregister_skin(&g_skin);
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
    }
    if (c->lua) {
        c->api->release(c->api->priv, OA_SVC_UI_LUA);
        c->lua = NULL;
    }
    c->suspended = 0;
    cancel_cleanup(c);
    free(c->state);
    c->state = NULL;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    cancel_cleanup(c);
    free(c->state);
    free(c);
}

static size_t m_save(void *v, void *buf, size_t cap)
{
    struct ctx *c = v;
    oa_omarchy_state *st = c->state;
    if (!st) {
        if (!c->have_saved) return 0;
        st = &c->saved;
    }
    if (buf && cap >= sizeof *st) memcpy(buf, st, sizeof *st);
    return sizeof *st;
}

static int m_restore(void *v, const void *buf, size_t len)
{
    struct ctx *c = v;
    if (!buf || len != sizeof(oa_omarchy_state)) return -1;
    oa_omarchy_state in;
    memcpy(&in, buf, sizeof in);
    if (in.magic != OA_OMARCHY_STATE_MAGIC ||
        in.version != OA_OMARCHY_STATE_VERSION)
        return -1;
    c->saved = in;
    c->have_saved = 1;
    if (!c->state && !(c->state = state_new())) return -1;
    *c->state = in;
    return 0;
}

static int m_suspend(void *v)
{
    struct ctx *c = v;
    if (!c->active) return 0;
    c->suspended = 1;
    if (c->rebind) c->api->timer_del(c->api->priv, c->rebind);
    c->rebind = 0;
    if (c->d) {
        c->d->unregister_skin(&g_skin);
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
    }
    return 0;
}

static int m_resume(void *v)
{
    struct ctx *c = v;
    if (!c->active || !c->state || c->d) return -1;
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 2);
    if (!c->d)
        return -1;
    if (c->d->register_skin_dispatch(&g_skin, c, dispatch) != 0) {
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
        return -1;
    }
    c->suspended = 0;
    paint(c);
    return 0;
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "ui_omarchy",
    .version = "0.1",
    .requires = { OA_SVC_UI_ENGINE, "" },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .suspend = m_suspend,
    .resume = m_resume,
    .destroy = m_destroy,
    .save = m_save,
    .restore = m_restore,
    .quiesce = m_quiesce,
};

OA_MODULE_EXPORT(g_module)
