/* Host-only broadcast service fixture: tuner scan state, EPG events and PVR
 * reservations. No hardware, no vendor ABI and no TV state. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    struct oa_tuner_status tuner;
    struct oa_pvr_job jobs[16];
    size_t job_count;
    uint32_t next_job;
    int published;
};

static struct ctx *g_ctx;

/* Vendor defaults: no lock, zero strength/quality, float BER/SNR 0.0 and UNC
 * unavailable (0xffffffff). */
static void tuner_clear_metrics(struct oa_tuner_status *status)
{
    status->locked = 0;
    status->signal_strength = 0;
    status->signal_quality = 0;
    status->unc = OA_TUNER_UNC_UNKNOWN;
    status->ber = 0.0f;
    status->snr = 0.0f;
}

static int tuner_status(struct oa_tuner_status *out)
{
    if (!g_ctx || !out) return -EINVAL;
    if (g_ctx->tuner.scan_state == OA_TUNER_SCANNING) {
        if (g_ctx->tuner.progress >= 100) {
            g_ctx->tuner.progress = 100;
            g_ctx->tuner.scan_state = OA_TUNER_DONE;
            g_ctx->tuner.services_found = 12;
            g_ctx->tuner.locked = 1;
            g_ctx->tuner.signal_strength = 100;
            g_ctx->tuner.signal_quality = 80;
            g_ctx->tuner.unc = 0;
            g_ctx->tuner.ber = 0.0f;
            g_ctx->tuner.snr = 20.5f;
        } else {
            g_ctx->tuner.progress = (uint8_t)(g_ctx->tuner.progress + 25);
            g_ctx->tuner.services_found =
                (uint32_t)g_ctx->tuner.progress * 12u / 100u;
            g_ctx->tuner.locked = 0;
            g_ctx->tuner.signal_strength = g_ctx->tuner.progress;
            g_ctx->tuner.signal_quality =
                (uint32_t)g_ctx->tuner.progress / 2u;
            g_ctx->tuner.unc = OA_TUNER_UNC_UNKNOWN;
            g_ctx->tuner.ber = 0.0f;
            g_ctx->tuner.snr = 0.0f;
        }
    }
    *out = g_ctx->tuner;
    return 0;
}

static int tuner_scan_start(uint8_t network, int clear_existing)
{
    if (!g_ctx || network < OA_TUNER_NET_DVB_T ||
        network > OA_TUNER_NET_IP)
        return -EINVAL;
    if (g_ctx->tuner.scan_state == OA_TUNER_SCANNING) return -EBUSY;
    if (clear_existing) g_ctx->tuner.selected_handle = 0;
    g_ctx->tuner.network = network;
    g_ctx->tuner.scan_state = OA_TUNER_SCANNING;
    g_ctx->tuner.progress = 0;
    g_ctx->tuner.services_found = 0;
    g_ctx->tuner.error = 0;
    tuner_clear_metrics(&g_ctx->tuner);
    return 0;
}

static int tuner_scan_cancel(void)
{
    if (!g_ctx || g_ctx->tuner.scan_state != OA_TUNER_SCANNING) return -EINVAL;
    g_ctx->tuner.scan_state = OA_TUNER_CANCELED;
    g_ctx->tuner.locked = 0;
    g_ctx->tuner.unc = OA_TUNER_UNC_UNKNOWN;
    return 0;
}

static int tuner_select(uint32_t service_handle)
{
    if (!g_ctx || !service_handle || service_handle > 12) return -EINVAL;
    if (g_ctx->tuner.scan_state != OA_TUNER_DONE) return -EAGAIN;
    g_ctx->tuner.selected_handle = service_handle;
    return 0;
}

static uint32_t tuner_selected(void)
{
    return g_ctx ? g_ctx->tuner.selected_handle : 0;
}

static const oa_tuner_iface tuner_iface = {
    tuner_status, tuner_scan_start, tuner_scan_cancel, tuner_select,
    tuner_selected
};

static const struct oa_epg_event epg_events[] = {
    { .service_handle = 1, .event_id = 100, .start_ms = 1000, .end_ms = 2000,
      .program_crid_id = 5001, .running_status = 0, .first_showing = 1,
      .state = 1, .version = 0, .actual = 1,
      .name = "Alpha News", .description = "Fixture news" },
    { .service_handle = 1, .event_id = 101, .start_ms = 2000, .end_ms = 3000,
      .program_crid_id = 5001, .running_status = 0, .first_showing = 0,
      .state = 2, .version = 1, .actual = 1,
      .name = "Alpha Movie", .description = "Fixture movie" },
    { .service_handle = 2, .event_id = 200, .start_ms = 1500, .end_ms = 2500,
      .program_crid_id = 5002, .running_status = 0, .first_showing = 1,
      .state = 0, .version = 2, .actual = 0,
      .name = "Beta Show", .description = "Fixture show" },
    { .service_handle = 3, .event_id = 300, .start_ms = 5000, .end_ms = 6000,
      .program_crid_id = 5003, .running_status = 0, .first_showing = 1,
      .state = 3, .version = 3, .actual = 1,
      .name = "Gamma Sport", .description = "Fixture sport" }
};

static int epg_query(uint32_t service_handle, int64_t from_ms, int64_t to_ms,
    struct oa_epg_event *events, size_t cap, size_t *count)
{
    if (count) *count = 0;
    if (from_ms >= to_ms || (cap && !events)) return -EINVAL;
    size_t found = 0;
    for (size_t i = 0; i < sizeof epg_events / sizeof epg_events[0]; i++) {
        const struct oa_epg_event *event = &epg_events[i];
        if (service_handle && event->service_handle != service_handle)
            continue;
        if (event->end_ms <= from_ms || event->start_ms >= to_ms)
            continue;
        if (found < cap) events[found] = *event;
        found++;
    }
    if (count) *count = found;
    return 0;
}

static const oa_epg_iface epg_iface = { epg_query };

static int pvr_list(struct oa_pvr_job *jobs, size_t cap, size_t *count)
{
    if (!g_ctx || (cap && !jobs)) return -EINVAL;
    if (count) *count = g_ctx->job_count;
    size_t copy = g_ctx->job_count < cap ? g_ctx->job_count : cap;
    for (size_t i = 0; i < copy; i++) jobs[i] = g_ctx->jobs[i];
    return 0;
}

static int pvr_add(const struct oa_pvr_request *request, uint32_t *id)
{
    if (!g_ctx || !request || !request->service_handle ||
        request->start_ms >= request->end_ms || !request->name[0])
        return -EINVAL;
    if (g_ctx->job_count >= sizeof g_ctx->jobs / sizeof g_ctx->jobs[0])
        return -ENOSPC;
    if (strlen(request->name) >= sizeof g_ctx->jobs[0].name)
        return -ENAMETOOLONG;
    struct oa_pvr_job *job = &g_ctx->jobs[g_ctx->job_count++];
    memset(job, 0, sizeof *job);
    job->id = ++g_ctx->next_job;
    job->service_handle = request->service_handle;
    job->start_ms = request->start_ms;
    job->end_ms = request->end_ms;
    job->repeat_days = request->repeat_days;
    job->start_padding = request->start_padding;
    job->end_padding = request->end_padding;
    snprintf(job->name, sizeof job->name, "%s", request->name);
    job->state = OA_PVR_SCHEDULED;
    if (id) *id = job->id;
    return 0;
}

static int pvr_cancel(uint32_t id)
{
    if (!g_ctx || !id) return -EINVAL;
    for (size_t i = 0; i < g_ctx->job_count; i++) {
        if (g_ctx->jobs[i].id != id) continue;
        if (g_ctx->jobs[i].state != OA_PVR_SCHEDULED) return -EALREADY;
        g_ctx->jobs[i].state = OA_PVR_CANCELED;
        return 0;
    }
    return -ENOENT;
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
    *out = c;
    g_ctx = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    memset(&c->tuner, 0, sizeof c->tuner);
    tuner_clear_metrics(&c->tuner);
    memset(c->jobs, 0, sizeof c->jobs);
    c->job_count = 0;
    c->next_job = 0;
    if (c->api->publish(c->api->priv, OA_SVC_TUNER, 1, &tuner_iface))
        return -1;
    if (c->api->publish(c->api->priv, OA_SVC_EPG, 1, &epg_iface)) {
        c->api->unpublish(c->api->priv, OA_SVC_TUNER);
        return -1;
    }
    if (c->api->publish(c->api->priv, OA_SVC_PVR, 1, &pvr_iface)) {
        c->api->unpublish(c->api->priv, OA_SVC_EPG);
        c->api->unpublish(c->api->priv, OA_SVC_TUNER);
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
        c->api->unpublish(c->api->priv, OA_SVC_EPG);
        c->api->unpublish(c->api->priv, OA_SVC_TUNER);
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
    .name = "broadcast_fixture", .version = "test",
    .requires = { "", 0 }, .init = m_init, .start = m_start,
    .stop = m_stop, .destroy = m_destroy, .suspend = m_suspend,
    .resume = m_resume
};

OA_MODULE_EXPORT(descriptor)
