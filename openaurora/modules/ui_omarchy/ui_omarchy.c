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
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/ui_engine.h"

struct ctx {
    const oa_host_api *api;
    const oa_ui_engine_iface *d;
    int suspended;
};

static void paint(struct ctx *c)
{
    const oa_ui_engine_iface *d = c->d;
    int W = 0, H = 0;
    if (!c || !d || c->suspended)
        return;
    if (d->info(&W, &H, NULL, NULL) != 0 || W <= 0 || H <= 0)
        return;
    d->clear_rgba(12, 14, 18, 255);
    d->fill(0, 0, 14, H, 0, 122, 204);
    d->fill(40, 40, W - 80, 8, 0, 122, 204);
    d->fill(40, 88, 300, 180, 24, 28, 36);
    d->fill(360, 88, 300, 180, 24, 28, 36);
    d->fill(40, 288, 620, 80, 24, 28, 36);
    (void)d->present();
}

static int enter(void *ud)
{
    paint(ud);
    return 0;
}

static void leave(void *ud)
{
    (void)ud;
}

static void skin_paint(void *ud)
{
    paint(ud);
}

static int skin_key(void *ud, int id)
{
    struct ctx *c = ud;
    if (!c)
        return 1;
    /* The skeleton has no navigation yet; every press repaints so the seam is
     * observable. Exit/Menu are ignored for now. */
    (void)id;
    paint(c);
    return 0;
}

static const oa_ui_skin g_skin = { "omarchy", enter, leave, skin_paint, skin_key };

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
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 1);
    if (!c->d)
        return -1;
    c->d->reset_ready();
    if (c->d->register_skin(&g_skin, c) != 0) {
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
        return -1;
    }
    if (c->api->config_get(c->api->priv, "ui.paint", "1")[0] == '1')
        paint(c);
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->d) {
        c->d->unregister_skin(&g_skin);
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
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
    c->suspended = 0;
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 1);
    if (!c->d)
        return -1;
    if (c->d->register_skin(&g_skin, c) != 0) {
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
        return -1;
    }
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
