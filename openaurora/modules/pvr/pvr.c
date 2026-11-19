/*
 * mod_pvr — recording-id PVR service backed by the offline persistent stub.
 *
 * Mirrors the vendor split between startRecording/stopRecording and
 * index-based recordingQuery. The store persists across reload/restart but is
 * project code, not the vendor SQLite metadata/timeshift engine.
 */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "openaurora/broadcast.h"
#include "openaurora/dvr.h"
#include "openaurora/module.h"
#include "openaurora/pvr.h"
#include "openaurora/pvr_ts_source.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    const struct oa_pvr_backend *backend;
    int published;
    int opened;
    /* ABI 3 quiesce: the backend is closed on a worker after the service is
     * already unpublished. */
    pthread_t cleanup_thread;
    int cleanup_running;
    unsigned cleanup_delay_ms;
    /* Optional live DVR recording source (default off). */
    int ts_gate, ts_adapter, ts_index;
    unsigned ts_max;
    char ts_path[256];
    FILE *ts_file;
};

static long ts_read(void *context, void *buf, size_t len)
{
    struct ctx *c = context;
    if (!c || !c->ts_file)
        return -EBADF;
    size_t n = fread(buf, 1, len, c->ts_file);
    if (n == 0 && ferror(c->ts_file))
        return -EIO;
    return (long)n;
}

static void ts_stop(struct ctx *c)
{
    int active = 0;
    oa_pvr_ts_info(&active, NULL, NULL, NULL);
    if (active)
        (void)oa_pvr_ts_end();
    if (c->ts_file) {
        fclose(c->ts_file);
        c->ts_file = NULL;
    }
}

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

static int pvr_record(uint32_t id)
{
    if (!g_ctx || !g_ctx->published) return -EINVAL;
    int rc = g_ctx->backend->record(id);
    if (rc || !g_ctx->ts_gate)
        return rc;
    struct ctx *c = g_ctx;
    int active = 0;
    oa_pvr_ts_info(&active, NULL, NULL, NULL);
    if (!active) {
        c->ts_file = fopen(c->ts_path, "rb");
        if (!c->ts_file)
            return -EIO;
        rc = oa_pvr_ts_begin(c->ts_adapter, c->ts_index,
                             oa_dvr_linux_ops(), c);
        if (rc) {
            fclose(c->ts_file);
            c->ts_file = NULL;
            return rc;
        }
    }
    const struct oa_pvr_ts_io io = { c, ts_read };
    long packets = oa_pvr_ts_pump(&io, c->ts_max);
    if (packets < 0)
        return (int)packets;
    if (packets == 0)              /* source exhausted */
        ts_stop(c);
    return 0;
}

static int pvr_finish(uint32_t id)
{
    if (!g_ctx || !g_ctx->published) return -EINVAL;
    if (g_ctx->ts_gate)
        ts_stop(g_ctx);
    return g_ctx->backend->finish(id);
}

static const oa_pvr_iface pvr_iface = {
    pvr_list, pvr_add, pvr_cancel, pvr_record, pvr_finish
};

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

/* No host API calls after this returns. The host serializes a replacement
 * start until this worker has joined. */
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
    ts_stop(c);
    c->backend->close();
    c->opened = 0;
    return NULL;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->published || c->cleanup_running || c->opened) return -EBUSY;
    c->cleanup_running = 0;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "pvr.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    /* Live DVR is opt-in: pvr.dvr_ts=<adapter>:<index>, pvr.ts_path=<file>.
     * Without both, recording stays the offline persistent stub. */
    const char *dvr = c->api->config_get(c->api->priv, "pvr.dvr_ts", "");
    const char *ts_path = c->api->config_get(c->api->priv, "pvr.ts_path", "");
    int a = -1, i = -1;
    if (dvr && dvr[0] && ts_path && ts_path[0] &&
        sscanf(dvr, "%d:%d", &a, &i) == 2 && a >= 0 && i >= 0) {
        c->ts_gate = 1;
        c->ts_adapter = a;
        c->ts_index = i;
        snprintf(c->ts_path, sizeof c->ts_path, "%s", ts_path);
        c->ts_max = OA_PVR_TS_MAX_PACKETS;
        long max = atol(c->api->config_get(c->api->priv,
                                           "pvr.ts_max_packets", "64"));
        if (max > 0 && max <= (long)OA_PVR_TS_MAX_PACKETS)
            c->ts_max = (unsigned)max;
    }
    const char *path = c->api->config_get(c->api->priv, "pvr.path",
        "/tmp/openaurora-pvr.db");
    int rc = c->backend->open(path);
    if (rc) return -1;
    c->opened = 1;
    if (c->api->publish(c->api->priv, OA_SVC_PVR, 2, &pvr_iface)) {
        c->backend->close();
        c->opened = 0;
        return -1;
    }
    c->published = 1;
    return 0;
}

/* ABI 3 gate: unpublish pvr synchronously, detach the backend open state, then
 * close it synchronously or on a worker after pvr.cleanup_delay_ms. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
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
    if (published) c->api->unpublish(c->api->priv, OA_SVC_PVR);
    if (!c->opened) return 0;
    if (!c->cleanup_delay_ms) {
        c->backend->close();
        c->opened = 0;
        return 0;
    }
    int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
    if (rc) {
        c->backend->close();
        c->opened = 0;
        return -EIO;
    }
    c->cleanup_running = 1;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_PVR);
    c->published = 0;
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->opened) {
        c->backend->close();
        c->opened = 0;
    }
}

static int m_suspend(void *v) { (void)v; return 0; }
static int m_resume(void *v) { (void)v; return 0; }

static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->opened) c->backend->close();
    if (g_ctx == v) g_ctx = NULL;
    free(v);
}

static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "pvr", .version = "1",
    .requires = { "", 0 }, .init = m_init, .start = m_start,
    .stop = m_stop, .destroy = m_destroy, .suspend = m_suspend,
    .resume = m_resume, .quiesce = m_quiesce
};

OA_MODULE_EXPORT(descriptor)
