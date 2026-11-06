/* Reloadable firmware-derived menu tree. Loads the JSON model at start and
 * publishes ui.menu; a reload replaces the whole document. No display or
 * device access. Consumers hold the service while using any returned view. */
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#include "openaurora/menu_model.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    struct oa_menu_doc *doc;
    int published;
    /* ABI 3 quiesce: the parsed document is detached and freed on a worker. */
    pthread_t cleanup_thread;
    int cleanup_running;
    struct oa_menu_doc *cleanup_doc;
    unsigned cleanup_delay_ms;
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

/* No host API calls after this returns. */
static void *cleanup_worker(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_delay_ms) {
        const struct timespec pause = {
            .tv_sec = c->cleanup_delay_ms / 1000u,
            .tv_nsec = (long)(c->cleanup_delay_ms % 1000u) * 1000000L
        };
        (void)nanosleep(&pause, NULL);
    }
    struct oa_menu_doc *doc = c->cleanup_doc;
    c->cleanup_doc = NULL;
    oa_menu_free(doc);
    return NULL;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->doc || c->published || c->cleanup_running) return -EBUSY;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "ui_menu.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
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

/* ABI 3 gate: unpublish ui.menu and detach the document, then free it on a
 * worker. A stale iface call sees published=0/document NULL. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running) {
        for (int i = 0; i < 10; i++) {
            int rc = pthread_tryjoin_np(c->cleanup_thread, NULL);
            if (rc == 0) {
                c->cleanup_running = 0;
                return 0;
            }
            if (rc != EBUSY) {
                c->cleanup_running = 0;
                return -EIO;
            }
            const struct timespec pause = { .tv_nsec = 5000000 };
            (void)nanosleep(&pause, NULL);
        }
        return -EAGAIN;
    }
    int published = c->published;
    c->published = 0;
    if (published) c->api->unpublish(c->api->priv, OA_SVC_UI_MENU);
    struct oa_menu_doc *doc = c->doc;
    c->doc = NULL;
    if (!doc) return 0;
    if (!c->cleanup_delay_ms) {
        oa_menu_free(doc);
        return 0;
    }
    c->cleanup_doc = doc;
    int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
    if (rc) {
        oa_menu_free(c->cleanup_doc);
        c->cleanup_doc = NULL;
        return -EIO;
    }
    c->cleanup_running = 1;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    int published = c->published;
    c->published = 0;
    if (published) c->api->unpublish(c->api->priv, OA_SVC_UI_MENU);
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->cleanup_doc) {
        oa_menu_free(c->cleanup_doc);
        c->cleanup_doc = NULL;
    }
    oa_menu_free(c->doc);
    c->doc = NULL;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->cleanup_doc) oa_menu_free(c->cleanup_doc);
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
    .suspend = m_suspend, .resume = m_resume, .quiesce = m_quiesce,
};
OA_MODULE_EXPORT(module)
