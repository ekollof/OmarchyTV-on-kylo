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
#include <sys/stat.h>
#include <unistd.h>

#include "openaurora/broadcast.h"
#include "openaurora/eit_cache.h"
#include "openaurora/eit_section.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

#define EPG_CAPACITY 512
#define EPG_PATH_MAX 512

struct ctx {
    const oa_host_api *api;
    struct oa_eit_cache *cache;
    int published;
    int persist;
    char path[EPG_PATH_MAX];
};

static struct ctx *g_ctx;

static int parse_u32(const char *text, uint32_t *out)
{
    if (!text || !*text) return -1;
    char *end;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno || *end || value > UINT32_MAX) return -1;
    *out = (uint32_t)value;
    return 0;
}

static int save_cache(struct ctx *c)
{
    if (!c->persist) return 0;
    char tmp[EPG_PATH_MAX + 8];
    if ((size_t)snprintf(tmp, sizeof tmp, "%s.tmp", c->path) >= sizeof tmp)
        return -ENAMETOOLONG;
    FILE *file = fopen(tmp, "wb");
    if (!file) return -errno;
    int rc = 0;
    if (fprintf(file, "OAEPG1\n") < 0) rc = -EIO;
    size_t count = oa_eit_cache_count(c->cache);
    for (size_t i = 0; i < count && !rc; i++) {
        struct oa_eit_record record;
        if (oa_eit_cache_get_at(c->cache, i, &record)) {
            rc = -EIO;
            break;
        }
        char title[OA_EIT_TITLE_MAX];
        size_t n = 0;
        for (; record.title[n] && n + 1 < sizeof title; n++) {
            unsigned char ch = (unsigned char)record.title[n];
            title[n] = ch < 0x20 || ch == 0x7f || ch == '\t' ? '?' : (char)ch;
        }
        title[n] = 0;
        if (fprintf(file, "%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%s\n",
            record.service_id, record.event_id, record.range.begin,
            record.range.end, record.program_crid_id, record.running_status,
            record.first_showing, record.state, record.version, record.actual,
            title) < 0)
            rc = -EIO;
    }
    if (!rc && fflush(file) != 0) rc = -EIO;
    if (!rc && fsync(fileno(file)) != 0) rc = -errno;
    if (fclose(file) != 0 && !rc) rc = -errno;
    if (rc) {
        unlink(tmp);
        return rc;
    }
    if (rename(tmp, c->path) != 0) {
        rc = -errno;
        unlink(tmp);
    }
    return rc;
}

static int parse_cache_line(char *line)
{
    char *fields[11];
    int n = 0;
    char *cursor = line;
    for (int i = 0; i < 10; i++) {
        char *tab = strchr(cursor, '\t');
        if (!tab) return -1;
        *tab = 0;
        fields[n++] = cursor;
        cursor = tab + 1;
    }
    fields[n++] = cursor;
    if (strchr(cursor, '\t')) return -1;
    uint32_t values[10];
    for (int i = 0; i < 10; i++)
        if (parse_u32(fields[i], &values[i])) return -1;
    if (!values[0] || values[3] < values[2] || values[5] > 7 ||
        values[6] > 1 || values[8] > 31 || values[9] > 1)
        return -1;
    struct oa_eit_record record;
    memset(&record, 0, sizeof record);
    record.service_id = values[0];
    record.event_id = values[1];
    record.range.begin = values[2];
    record.range.end = values[3];
    record.program_crid_id = values[4];
    record.running_status = values[5];
    record.first_showing = (uint8_t)values[6];
    record.state = (uint8_t)values[7];
    record.version = (uint8_t)values[8];
    record.actual = (uint8_t)values[9];
    snprintf(record.title, sizeof record.title, "%s", fields[10]);
    return oa_eit_cache_put(g_ctx->cache, &record);
}

static int load_cache(struct ctx *c)
{
    oa_eit_cache_clear(c->cache);
    struct stat st;
    if (stat(c->path, &st) == 0 && S_ISDIR(st.st_mode)) return -EISDIR;
    FILE *file = fopen(c->path, "rb");
    if (!file) return errno == ENOENT ? 0 : -errno;
    char line[1024];
    if (!fgets(line, sizeof line, file)) {
        fclose(file);
        return -EINVAL;
    }
    size_t len = strlen(line);
    while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = 0;
    if (strcmp(line, "OAEPG1") != 0) {
        fclose(file);
        return -EINVAL;
    }
    while (fgets(line, sizeof line, file)) {
        len = strlen(line);
        if (len && line[len - 1] != '\n' && !feof(file)) {
            fclose(file);
            return -EINVAL;
        }
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = 0;
        if (!len) continue;
        int rc = parse_cache_line(line);
        if (rc) {
            fclose(file);
            oa_eit_cache_clear(c->cache);
            return rc;
        }
    }
    if (ferror(file)) {
        fclose(file);
        oa_eit_cache_clear(c->cache);
        return -EIO;
    }
    fclose(file);
    return 0;
}

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
    if (state.stored) {
        int saved = save_cache(g_ctx);
        if (saved) return saved;
    }
    return state.stored;
}

static void epg_clear(void)
{
    if (!g_ctx || !g_ctx->cache) return;
    oa_eit_cache_clear(g_ctx->cache);
    if (save_cache(g_ctx) && g_ctx->persist)
        g_ctx->api->log(g_ctx->api->priv, OA_LOG_ERROR,
                        "epg: clear could not persist %s", g_ctx->path);
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
    const char *path = c->api->config_get(c->api->priv, "epg.path", "");
    c->persist = path && *path;
    if (c->persist) {
        if (strlen(path) >= sizeof c->path) return -1;
        snprintf(c->path, sizeof c->path, "%s", path);
        int rc = load_cache(c);
        if (rc) {
            c->api->log(c->api->priv, OA_LOG_ERROR,
                        "epg: cannot load %s (%d); refusing start", c->path, rc);
            return -1;
        }
    } else {
        oa_eit_cache_clear(c->cache);
    }
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
        if (c->persist) (void)save_cache(c);
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
