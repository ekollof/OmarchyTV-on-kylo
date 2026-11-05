/* Reloadable firmware-derived menu tree. Loads the JSON model at start and
 * publishes ui.menu; a reload replaces the whole document. No display or
 * device access. Consumers hold the service while using any returned view. */
#include <stdlib.h>

#include "openaurora/menu_model.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    struct oa_menu_doc *doc;
    int published;
};

static struct ctx *g_ctx;

static const struct oa_menu_doc *document(void)
{
    return g_ctx && g_ctx->published ? g_ctx->doc : NULL;
}

static const struct oa_menu_dialog *dialog(const char *id)
{
    const struct oa_menu_doc *doc = document();
    return doc ? oa_menu_find(doc, id) : NULL;
}

static const struct oa_menu_branch *branch(const struct oa_menu_dialog *d,
                                           int launcher_type)
{
    return document() ? oa_menu_branch(d, launcher_type) : NULL;
}

static const oa_ui_menu_iface g_iface = { document, dialog, branch };

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof *api) return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api;
    g_ctx = c;
    *out = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    const char *path = c->api->config_get(c->api->priv, "ui_menu.file",
        "/buffer/openaurora/menus/sub_launcher.json");
    int rc = oa_menu_load(path, &c->doc);
    if (rc) {
        c->api->log(c->api->priv, OA_LOG_ERROR,
                    "ui_menu: cannot load %s (%d)", path, rc);
        return -1;
    }
    rc = c->api->publish(c->api->priv, OA_SVC_UI_MENU, 1, &g_iface);
    if (rc) {
        oa_menu_free(c->doc);
        c->doc = NULL;
        return rc;
    }
    c->published = 1;
    c->api->log(c->api->priv, OA_LOG_INFO, "ui_menu: loaded %s (%u dialogs)",
                path, c->doc->dialog_count);
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_UI_MENU);
    c->published = 0;
    oa_menu_free(c->doc);
    c->doc = NULL;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    oa_menu_free(c->doc);
    if (g_ctx == c) g_ctx = NULL;
    free(c);
}

static int m_suspend(void *v) { (void)v; return 0; }
static int m_resume(void *v) { (void)v; return 0; }

static const oa_module module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "ui_menu", .version = "0.1",
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
    .suspend = m_suspend, .resume = m_resume,
};
OA_MODULE_EXPORT(module)
