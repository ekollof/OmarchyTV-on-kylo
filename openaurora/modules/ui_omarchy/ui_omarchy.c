/* mod_ui_omarchy — OmarchyTV skin skeleton over ui.engine.
 *
 * This is the second skin for the engine seam: it owns its own screens and key
 * semantics, draws only through ui.engine, and does not use the vendor assets.
 * The real OmarchyTV UI replaces paint()/key() here; the vendor skin remains the
 * fallback. It deliberately publishes no service yet (no portal contract).
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/events.h"
#include "openaurora/lua_skin.h"
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/ui_engine.h"

struct ctx {
    const oa_host_api *api;
    const oa_ui_engine_iface *d;
    const oa_lua_iface *lua;
    int suspended;
    int have_colors;
    int have_layout;
    int br, bg, bb, ba, ar, ag, ab;
    int lx, ly, lw, lh;
    uint32_t removed, changed;
    uint64_t rebind;
};
static const oa_ui_skin g_skin;
static void dispatch(void *ud)
{
    struct ctx *c = ud;
    if (c->d && !c->suspended) c->d->dispatch_skin();
}

static void take_lua_paint(struct ctx *c, int *br, int *bg, int *bb, int *ba,
                           int *ar, int *ag, int *ab, int *x, int *y, int *w, int *h)
{
    int bgc[4], acc[4];
    oa_lua_layout lay;
    if (!c->lua || c->lua->call("paint", "omarchy", 0, NULL) != 0)
        return;
    if (c->lua->theme(bgc, acc) == 0) {
        *br = bgc[0];
        *bg = bgc[1];
        *bb = bgc[2];
        *ba = bgc[3];
        *ar = acc[0];
        *ag = acc[1];
        *ab = acc[2];
        c->br = *br;
        c->bg = *bg;
        c->bb = *bb;
        c->ba = *ba;
        c->ar = *ar;
        c->ag = *ag;
        c->ab = *ab;
        c->have_colors = 1;
    }
    if (c->lua->layout_at(0, &lay) == 0 && lay.w > 0 && lay.h > 0) {
        *x = lay.x;
        *y = lay.y;
        *w = lay.w;
        *h = lay.h;
        c->lx = lay.x;
        c->ly = lay.y;
        c->lw = lay.w;
        c->lh = lay.h;
        c->have_layout = 1;
    }
}

static void paint(struct ctx *c)
{
    const oa_ui_engine_iface *d = c ? c->d : NULL;
    int W = 0, H = 0;
    int br, bg, bb, ba, ar, ag, ab, x, y, w, h;
    if (!c || !d || c->suspended)
        return;
    if (d->info(&W, &H, NULL, NULL) != 0 || W <= 0 || H <= 0)
        return;
    br = c->have_colors ? c->br : 12;
    bg = c->have_colors ? c->bg : 14;
    bb = c->have_colors ? c->bb : 18;
    ba = c->have_colors ? c->ba : 255;
    ar = c->have_colors ? c->ar : 0;
    ag = c->have_colors ? c->ag : 122;
    ab = c->have_colors ? c->ab : 204;
    x = c->have_layout ? c->lx : 0;
    y = c->have_layout ? c->ly : 0;
    w = c->have_layout ? c->lw : 14;
    h = c->have_layout ? c->lh : H;
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
    if (c && c->lua)
        (void)c->lua->call("enter", "omarchy", 0, NULL);
    paint(ud);
    return 0;
}

static void leave(void *ud)
{
    struct ctx *c = ud;
    if (c && c->lua)
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
    if (!c)
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
    c->rebind = 0;
    if (c->suspended || c->d) return;
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
    if (!c->suspended && !c->d && !c->rebind)
        c->rebind = c->api->timer_add(c->api->priv, 0, 0, rebind, c);
}
static void removed(const void *ev, void *ud)
{
    const oa_service_removed_event *e = ev;
    struct ctx *c = ud;
    if (!e || !e->name)
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
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 2);
    if (!c->d)
        return -1;
    if (c->d->register_skin_dispatch(&g_skin, c, dispatch) != 0) {
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

static void m_stop(void *v)
{
    struct ctx *c = v;
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
}

static void m_destroy(void *v)
{
    free(v);
}

static int m_suspend(void *v)
{
    struct ctx *c = v;
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
};

OA_MODULE_EXPORT(g_module)
