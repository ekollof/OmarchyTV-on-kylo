/*
 * mod_epg — EIT-backed EPG service.
 *
 * Parses complete DVB EIT sections, stores events in the firmware-grounded
 * schedule cache and publishes the project `epg` query contract plus an
 * `epg.ingest` feed used by the control plane/tuner source. This is an
 * in-memory module: no persistence, EIT filtering or vendor EIT hub.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/broadcast.h"
#include "openaurora/eit_cache.h"
#include "openaurora/eit_section.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

#define EPG_CAPACITY 512

struct ctx {
    const oa_host_api *api;
    struct oa_eit_cache *cache;
    int published;
};

static struct ctx *g_ctx;

static int epg_query(uint32_t service_handle, int64_t from_ms, int64_t to_ms,
    struct oa_epg_event *events, size_t cap, size_t *count)
{
    if (count) *count = 0;
    if (!g_ctx || !g_ctx->published || from_ms < 0 || to_ms <= from_ms ||
        (cap && !events))
        return -EINVAL;
    uint64_t from_sec = (uint64_t)from_ms / 1000u;
    uint64_t to_sec = ((uint64_t)to_ms + 999u) / 1000u;
    if (from_sec > UINT32_MAX || to_sec > UINT32_MAX || to_sec <= from_sec)
        return -EINVAL;
    if (cap > 4096) return -E2BIG;
    struct oa_eit_record *records =
        cap ? calloc(cap, sizeof *records) : NULL;
    if (cap && !records) return -ENOMEM;
    size_t found = 0;
    int rc = oa_eit_cache_query(g_ctx->cache, service_handle,
        (uint32_t)from_sec, (uint32_t)(to_sec - from_sec), records, cap,
        &found);
    if (rc) {
        free(records);
        return rc;
    }
    size_t copy = found < cap ? found : cap;
    for (size_t i = 0; i < copy; i++) {
        struct oa_epg_event *event = &events[i];
        memset(event, 0, sizeof *event);
        event->service_handle = records[i].service_id;
        event->event_id = records[i].event_id;
        event->start_ms = (int64_t)records[i].range.begin * 1000;
        event->end_ms = (int64_t)records[i].range.end * 1000;
        event->program_crid_id = records[i].program_crid_id;
        event->running_status = records[i].running_status;
        event->first_showing = records[i].first_showing;
        event->state = records[i].state;
        event->version = records[i].version;
        event->actual = records[i].actual;
        snprintf(event->name, sizeof event->name, "%s", records[i].title);
    }
    free(records);
    if (count) *count = found;
    return 0;
}

static const oa_epg_iface epg_iface = { epg_query };

struct ingest {
    int stored;
    int error;
};

static int ingest_event(const struct oa_eit_event_info *info, void *user)
{
    struct ingest *state = user;
    struct oa_eit_record record;
    memset(&record, 0, sizeof record);
    record.service_id = info->service_id;
    record.event_id = info->event_id;
    record.range = info->range;
    record.program_crid_id = 0; /* EIT does not carry the CRID database id */
    record.running_status = info->running_status;
    record.first_showing = 0;   /* ScheduleEvent column, not in the section */
    record.state = 0;
    record.version = info->version;
    record.actual = info->actual;
    snprintf(record.title, sizeof record.title, "%s", info->name);
    int rc = oa_eit_cache_put(g_ctx->cache, &record);
    if (rc) {
        state->error = rc;
        return rc;
    }
    state->stored++;
    return 0;
}

static int epg_feed_section(const uint8_t *data, size_t len)
{
    if (!g_ctx || !g_ctx->published) return -ENODEV;
    struct ingest state = { 0, 0 };
    int rc = oa_eit_parse_section(data, len, ingest_event, &state);
    if (rc < 0) return state.error ? state.error : rc;
    return state.stored;
}

static void epg_clear(void)
{
    if (g_ctx && g_ctx->cache) oa_eit_cache_clear(g_ctx->cache);
}

static size_t epg_count(void)
{
    return g_ctx ? oa_eit_cache_count(g_ctx->cache) : 0;
}

static const oa_epg_ingest_iface ingest_iface = {
    epg_feed_section, epg_clear, epg_count
};

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION ||
        !out)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->cache = oa_eit_cache_create(EPG_CAPACITY);
    if (!c->cache) {
        free(c);
        return -1;
    }
    c->api = api;
    *out = c;
    g_ctx = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    oa_eit_cache_clear(c->cache);
    if (c->api->publish(c->api->priv, OA_SVC_EPG, 1, &epg_iface))
        return -1;
    if (c->api->publish(c->api->priv, OA_SVC_EPG_INGEST, 1, &ingest_iface)) {
        c->api->unpublish(c->api->priv, OA_SVC_EPG);
        return -1;
    }
    c->published = 1;
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->published) {
        c->api->unpublish(c->api->priv, OA_SVC_EPG_INGEST);
        c->api->unpublish(c->api->priv, OA_SVC_EPG);
    }
    c->published = 0;
}

static int m_suspend(void *v) { (void)v; return 0; }
static int m_resume(void *v) { (void)v; return 0; }

static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (g_ctx == c) g_ctx = NULL;
    oa_eit_cache_destroy(c->cache);
    free(c);
}

static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "epg", .version = "1",
    .requires = { "", 0 }, .init = m_init, .start = m_start,
    .stop = m_stop, .destroy = m_destroy, .suspend = m_suspend,
    .resume = m_resume
};

OA_MODULE_EXPORT(descriptor)
