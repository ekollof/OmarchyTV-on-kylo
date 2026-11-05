/*
 * mod_channels — runtime active-list provider.
 *
 * Accepts the recovered ACTIVECHANNELLIST XML and preserves its row order.
 * The list carries no LCN/visibility fields, so it is explicitly marked
 * incomplete; the UI falls back to stored order when no runtime list exists.
 */
#include <stdlib.h>

#include "openaurora/channels.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    int published;
};

static struct ctx *g_ctx;

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

static int m_start(void *v)
{
    struct ctx *c = v;
    oa_channel_runtime_iface.clear();
    if (c->api->publish(c->api->priv, OA_SVC_CHANNELS, 1,
            &oa_channel_runtime_iface))
        return -1;
    c->published = 1;
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_CHANNELS);
    c->published = 0;
    oa_channel_runtime_iface.clear();
}

static int m_suspend(void *v) { (void)v; return 0; }
static int m_resume(void *v) { (void)v; return 0; }

static void m_destroy(void *v)
{
    if (g_ctx == v) g_ctx = NULL;
    oa_channel_runtime_iface.clear();
    free(v);
}

static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "channels", .version = "1",
    .requires = { "", 0 }, .init = m_init, .start = m_start,
    .stop = m_stop, .destroy = m_destroy, .suspend = m_suspend,
    .resume = m_resume
};

OA_MODULE_EXPORT(descriptor)
