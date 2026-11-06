/*
 * mod_channels — runtime active-list provider.
 *
 * Accepts the recovered ACTIVECHANNELLIST XML and preserves its row order.
 * The list carries no LCN/visibility fields, so it is explicitly marked
 * incomplete; the UI falls back to stored order when no runtime list exists.
 */
#include <errno.h>
#include <stdlib.h>

#include "openaurora/channels.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    struct oa_channel_list *list;
    int published;
    /* ABI 3 quiesce: the list is detached and destroyed by a scheduler
     * slice on the host loop. */
    uint32_t cleanup_task;
    struct oa_channel_list *cleanup_list;
    unsigned cleanup_delay_ms;
};

static struct ctx *g_ctx;

/* The published iface callbacks have no user pointer, so they bind to the
 * module's single instance. published=0 makes a stale iface fail closed. */
static const struct oa_channel_snapshot *module_snapshot(void)
{
    if (!g_ctx || !g_ctx->published || !g_ctx->list) return NULL;
    return oa_channel_list_snapshot(g_ctx->list);
}

static int module_ingest_xml(const char *xml, size_t len)
{
    if (!g_ctx || !g_ctx->published || !g_ctx->list) return -ENODEV;
    return oa_channel_list_ingest_xml(g_ctx->list, xml, len);
}

static void module_clear(void)
{
    if (g_ctx && g_ctx->published && g_ctx->list)
        oa_channel_list_clear(g_ctx->list);
}

static const oa_channel_iface module_iface = {
    module_snapshot, module_ingest_xml, module_clear
};

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION ||
        !out)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api;
    *out = c;
    g_ctx = c;
    return 0;
}

/* One nonblocking scheduler slice frees the detached list. */
static int cleanup_step(void *v, uint32_t *delay_ms)
{
    (void)delay_ms;
    struct ctx *c = v;
    struct oa_channel_list *list = c->cleanup_list;
    c->cleanup_list = NULL;
    c->cleanup_task = 0;
    oa_channel_list_destroy(list);
    return OA_SCHED_DONE;
}

/* Cancel a queued free and release any list the step did not consume. */
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
    if (c->cleanup_list) {
        oa_channel_list_destroy(c->cleanup_list);
        c->cleanup_list = NULL;
    }
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->published || c->cleanup_task || c->cleanup_list) return -EBUSY;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "channels.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    if (!c->list) {
        c->list = oa_channel_list_create();
        if (!c->list) return -1;
    }
    if (c->api->publish(c->api->priv, OA_SVC_CHANNELS, 1, &module_iface))
        return -1;
    c->published = 1;
    return 0;
}

/* ABI 3 gate: unpublish channels synchronously and detach the runtime list,
 * then destroy it synchronously or from one scheduler slice after
 * channels.cleanup_delay_ms. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
    if (c->cleanup_task || c->cleanup_list) return -EAGAIN;
    int published = c->published;
    c->published = 0;
    if (published) c->api->unpublish(c->api->priv, OA_SVC_CHANNELS);
    struct oa_channel_list *list = c->list;
    c->list = NULL;
    if (!list) return 0;
    if (!c->cleanup_delay_ms) {
        oa_channel_list_destroy(list);
        return 0;
    }
    c->cleanup_list = list;
    const oa_sched_iface *s =
        c->api->lookup(c->api->priv, OA_SVC_SCHED, 1);
    if (!s) {
        c->cleanup_list = NULL;
        oa_channel_list_destroy(list);
        return 0;
    }
    uint32_t id = 0;
    int rc = s->submit(s->context, cleanup_step, c, c->cleanup_delay_ms, &id);
    c->api->release(c->api->priv, OA_SVC_SCHED);
    if (rc) {
        c->cleanup_list = NULL;
        oa_channel_list_destroy(list);
        return -EIO;
    }
    c->cleanup_task = id;
    return -EAGAIN;
}


static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_CHANNELS);
    c->published = 0;
    cancel_cleanup(c);
    oa_channel_list_destroy(c->list);
    c->list = NULL;
}

static int m_suspend(void *v) { (void)v; return 0; }
static int m_resume(void *v) { (void)v; return 0; }

static void m_destroy(void *v)
{
    struct ctx *c = v;
    cancel_cleanup(c);
    oa_channel_list_destroy(c->list);
    if (g_ctx == c) g_ctx = NULL;
    free(c);
}

static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "channels", .version = "1",
    .requires = { "", 0 }, .init = m_init, .start = m_start,
    .stop = m_stop, .destroy = m_destroy, .suspend = m_suspend,
    .resume = m_resume, .quiesce = m_quiesce
};

OA_MODULE_EXPORT(descriptor)
