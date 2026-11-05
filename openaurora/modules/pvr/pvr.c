/*
 * mod_pvr — recording-id PVR service backed by the offline persistent stub.
 *
 * Mirrors the vendor split between startRecording/stopRecording and
 * index-based recordingQuery. The store persists across reload/restart but is
 * project code, not the vendor SQLite metadata/timeshift engine.
 */
#include <errno.h>
#include <stdlib.h>

#include "openaurora/broadcast.h"
#include "openaurora/module.h"
#include "openaurora/pvr.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    const struct oa_pvr_backend *backend;
    int published;
};

static struct ctx *g_ctx;

static int pvr_list(struct oa_pvr_job *jobs, size_t cap, size_t *count)
{
    if (count) *count = 0;
    if (!g_ctx || !g_ctx->published || (cap && !jobs)) return -EINVAL;
    size_t total = g_ctx->backend->count();
    if (count) *count = total;
    size_t copy = total < cap ? total : cap;
    for (size_t i = 0; i < copy; i++) {
        int rc = g_ctx->backend->query(i, &jobs[i]);
        if (rc) return rc;
    }
    return 0;
}

static int pvr_add(const struct oa_pvr_request *request, uint32_t *id)
{
    if (!g_ctx || !g_ctx->published) return -EINVAL;
    return g_ctx->backend->start(request, id);
}

static int pvr_cancel(uint32_t id)
{
    if (!g_ctx || !g_ctx->published) return -EINVAL;
    return g_ctx->backend->stop(id);
}

static const oa_pvr_iface pvr_iface = { pvr_list, pvr_add, pvr_cancel };

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION ||
        !out)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api;
    c->backend = oa_pvr_stub_backend();
    *out = c;
    g_ctx = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    const char *path = c->api->config_get(c->api->priv, "pvr.path",
        "/tmp/openaurora-pvr.db");
    int rc = c->backend->open(path);
    if (rc) return -1;
    if (c->api->publish(c->api->priv, OA_SVC_PVR, 1, &pvr_iface)) {
        c->backend->close();
        return -1;
    }
    c->published = 1;
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->published) {
        c->api->unpublish(c->api->priv, OA_SVC_PVR);
        c->backend->close();
    }
    c->published = 0;
}

static int m_suspend(void *v) { (void)v; return 0; }
static int m_resume(void *v) { (void)v; return 0; }

static void m_destroy(void *v)
{
    if (g_ctx == v) g_ctx = NULL;
    free(v);
}

static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "pvr", .version = "1",
    .requires = { "", 0 }, .init = m_init, .start = m_start,
    .stop = m_stop, .destroy = m_destroy, .suspend = m_suspend,
    .resume = m_resume
};

OA_MODULE_EXPORT(descriptor)
