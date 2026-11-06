/*
 * mod_tuner — vendor-shaped tuner service with selectable offline/live-capable
 * sources.
 *
 * The scan state machine and status fields follow the recovered
 * DEV_NIM_DTV_StartTune/MonitorTune/signal contracts. The default backend is
 * the offline stub; tuner.ts_path selects the TS replay EIT source and
 * tuner.backend=ndal selects the NDAL frontend adapter (optionally with
 * tuner.eit_demux for the DVB section-filter EIT source). Hardware acceptance
 * remains owner-gated.
 */
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "openaurora/broadcast.h"
#include "openaurora/module.h"
#include "openaurora/nim.h"
#include "openaurora/services.h"

#define SCAN_SERVICES 12

struct ctx {
    const oa_host_api *api;
    const struct oa_tuner_backend *backend;
    uint8_t network;
    uint8_t state;          /* enum oa_tuner_scan_state */
    uint8_t progress;
    uint8_t locked;
    uint8_t path;
    uint32_t services_found;
    uint32_t selected_handle;
    uint32_t signal_strength;
    uint32_t signal_quality;
    uint32_t unc;
    float ber;
    float snr;
    int error;
    int active;
    int published;
    int ts_source;
    int ts_open;
    char ts_path[512];
    /* ABI 3 quiesce: the TS FILE* is detached and closed on a worker. */
    pthread_t cleanup_thread;
    int cleanup_running;
    unsigned cleanup_delay_ms;
};

static struct ctx *g_ctx;

static void metrics_default(struct ctx *c)
{
    c->locked = 0;
    c->signal_strength = 0;
    c->signal_quality = 0;
    c->unc = OA_TUNER_UNC_UNKNOWN;
    c->ber = 0.0f;
    c->snr = 0.0f;
}

static void apply_signal(struct ctx *c, const struct oa_nim_signal *signal)
{
    c->locked = signal->locked;
    c->signal_strength = signal->strength;
    c->signal_quality = signal->quality;
    c->unc = signal->unc;
    c->ber = signal->ber;
    c->snr = signal->snr;
}

static int path_for_network(uint8_t network, uint8_t *path)
{
    switch (network) {
    case OA_TUNER_NET_DVB_T:
        *path = OA_NIM_PATH_TERRESTRIAL;
        return 0;
    case OA_TUNER_NET_DVB_C:
        *path = OA_NIM_PATH_CABLE;
        return 0;
    case OA_TUNER_NET_DVB_S:
        *path = OA_NIM_PATH_QPSK;
        return 0;
    case OA_TUNER_NET_IP:
        *path = 0; /* offline synthetic path */
        return 0;
    case OA_TUNER_NET_ANALOG:
        return -ENOTSUP; /* ATV path was not recovered for this interface */
    default:
        return -EINVAL;
    }
}

static void feed_eit(struct ctx *c)
{
    if (!c->backend->next_eit) return;
    const oa_epg_ingest_iface *ingest =
        c->api->lookup(c->api->priv, OA_SVC_EPG_INGEST, 1);
    if (!ingest) return;
    uint8_t section[4096];
    for (int i = 0; i < 64; i++) {
        size_t len = 0;
        int rc = c->backend->next_eit(section, sizeof section, &len);
        if (rc <= 0) break;
        int fed = ingest->feed_section(section, len);
        if (fed < 0 && fed != -ENODEV) {
            c->error = -fed;
            break;
        }
    }
    c->api->release(c->api->priv, OA_SVC_EPG_INGEST);
}

static int tuner_status(struct oa_tuner_status *out)
{
    if (!g_ctx || !out) return -EINVAL;
    struct ctx *c = g_ctx;
    if (c->active && c->state == OA_TUNER_SCANNING) {
        struct oa_nim_tune_result result;
        struct oa_nim_signal signal;
        int rc = c->backend->nim->monitor(c->path, &result);
        if (!rc) rc = c->backend->nim->signal(c->path, &signal);
        if (rc) {
            c->state = OA_TUNER_FAILED;
            c->error = -rc;
            c->active = 0;
        } else {
            apply_signal(c, &signal);
            if (c->progress >= 100) {
                c->progress = 100;
                c->services_found = SCAN_SERVICES;
                c->state = OA_TUNER_DONE;
            } else {
                c->progress = (uint8_t)(c->progress + 25);
                c->services_found =
                    (uint32_t)c->progress * SCAN_SERVICES / 100u;
                feed_eit(c);
                if (c->error) {
                    c->state = OA_TUNER_FAILED;
                    c->active = 0;
                }
            }
        }
    }
    memset(out, 0, sizeof *out);
    out->network = c->network;
    out->scan_state = c->state;
    out->progress = c->progress;
    out->locked = c->locked;
    out->services_found = c->services_found;
    out->error = c->error;
    out->selected_handle = c->selected_handle;
    out->signal_strength = c->signal_strength;
    out->signal_quality = c->signal_quality;
    out->unc = c->unc;
    out->ber = c->ber;
    out->snr = c->snr;
    return 0;
}

static int tuner_scan_start(uint8_t network, int clear_existing)
{
    if (!g_ctx) return -EINVAL;
    struct ctx *c = g_ctx;
    uint8_t path = 0;
    int rc = path_for_network(network, &path);
    if (rc) return rc;
    if (c->state == OA_TUNER_SCANNING) return -EBUSY;
    uint32_t params[2] = { 0, OA_NIM_DEFAULT_FREQUENCY_KHZ };
    struct oa_nim_tune_result result;
    rc = c->backend->nim->start(path, params, 2, &result);
    if (rc) return rc;
    c->network = network;
    c->path = path;
    c->state = OA_TUNER_SCANNING;
    c->progress = 0;
    c->services_found = 0;
    c->locked = 0;
    c->error = 0;
    c->active = 1;
    if (clear_existing) c->selected_handle = 0;
    metrics_default(c);
    if (result.state == OA_NIM_TUNE_LOCKED) c->locked = 1;
    return 0;
}

static int tuner_scan_cancel(void)
{
    if (!g_ctx || g_ctx->state != OA_TUNER_SCANNING) return -EINVAL;
    struct ctx *c = g_ctx;
    if (c->active) c->backend->nim->stop(c->path);
    c->active = 0;
    c->state = OA_TUNER_CANCELED;
    c->locked = 0;
    c->unc = OA_TUNER_UNC_UNKNOWN;
    return 0;
}

static int tuner_select(uint32_t service_handle)
{
    if (!g_ctx || !service_handle || service_handle > SCAN_SERVICES)
        return -EINVAL;
    if (g_ctx->state != OA_TUNER_DONE) return -EAGAIN;
    if (service_handle > g_ctx->services_found) return -EINVAL;
    g_ctx->selected_handle = service_handle;
    return 0;
}

static uint32_t tuner_selected(void)
{
    return g_ctx ? g_ctx->selected_handle : 0;
}

static const oa_tuner_iface tuner_iface = {
    tuner_status, tuner_scan_start, tuner_scan_cancel, tuner_select,
    tuner_selected
};

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION ||
        !out)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api;
    const char *ts_path = api->config_get(api->priv, "tuner.ts_path", "");
    const char *backend = api->config_get(api->priv, "tuner.backend", "");
    if (ts_path && *ts_path) {
        if (strlen(ts_path) >= sizeof c->ts_path) { free(c); return -1; }
        memcpy(c->ts_path, ts_path, strlen(ts_path) + 1);
        c->backend = oa_nim_ts_backend();
        c->ts_source = 1;
    } else if (backend && !strcmp(backend, "ndal")) {
        const char *frontend = api->config_get(api->priv,
            "tuner.frontend", "/dev/dvb/adapter0/frontend0");
        c->backend = oa_nim_ndal_backend(frontend);
        if (!c->backend) { free(c); return -1; }
        const char *eit = api->config_get(api->priv, "tuner.eit_demux", "");
        if (eit && *eit) {
            int adapter = -1, index = -1;
            unsigned pid = 0x12;
            int fields = sscanf(eit, "%d:%d:%x", &adapter, &index, &pid);
            if (fields < 2 || adapter < 0 || index < 0 ||
                oa_nim_ndal_enable_eit(adapter, index, (uint16_t)pid,
                    NULL, NULL)) {
                free(c);
                return -1;
            }
        }
    } else {
        c->backend = oa_nim_stub_backend();
    }
    metrics_default(c);
    *out = c;
    g_ctx = c;
    return 0;
}

/* No host API calls after this returns. oa_nim_ts_close owns the global TS
 * FILE*; the host serializes the new start until this worker has joined. */
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
    oa_nim_ts_close();
    c->ts_open = 0;
    return NULL;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->published || c->cleanup_running) return -EBUSY;
    c->cleanup_running = 0;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "tuner.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    if (c->active) c->backend->nim->stop(c->path);
    c->active = 0;
    c->state = OA_TUNER_IDLE;
    c->network = 0;
    c->progress = 0;
    c->services_found = 0;
    c->selected_handle = 0;
    c->error = 0;
    metrics_default(c);
    if (c->ts_source) {
        int rc = oa_nim_ts_open(c->ts_path);
        if (rc) return rc;
        c->ts_open = 1;
    }
    if (c->api->publish(c->api->priv, OA_SVC_TUNER, 1, &tuner_iface)) {
        if (c->ts_source) oa_nim_ts_close();
        return -1;
    }
    c->published = 1;
    return 0;
}

/* ABI 3 gate: unpublish tuner synchronously and stop any scan, then close the
 * TS FILE* synchronously or on a worker after tuner.cleanup_delay_ms. */
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
    if (c->active) {
        c->backend->nim->stop(c->path);
        c->active = 0;
    }
    if (c->published) {
        c->api->unpublish(c->api->priv, OA_SVC_TUNER);
        c->published = 0;
    }
    if (!c->ts_open) return 0;
    if (!c->cleanup_delay_ms) {
        oa_nim_ts_close();
        c->ts_open = 0;
        return 0;
    }
    int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
    if (rc) {
        oa_nim_ts_close();
        c->ts_open = 0;
        return -EIO;
    }
    c->cleanup_running = 1;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->active) {
        c->backend->nim->stop(c->path);
        c->active = 0;
    }
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_TUNER);
    c->published = 0;
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->ts_open) {
        oa_nim_ts_close();
        c->ts_open = 0;
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
    if (c->ts_open) oa_nim_ts_close();
    if (g_ctx == v) g_ctx = NULL;
    free(v);
}

static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "tuner", .version = "1",
    .requires = { "", 0 }, .init = m_init, .start = m_start,
    .stop = m_stop, .destroy = m_destroy, .suspend = m_suspend,
    .resume = m_resume, .quiesce = m_quiesce
};

OA_MODULE_EXPORT(descriptor)
