/*
 * mod_service_db — read-only broadcast-service inventory from settingsDB.
 *
 * This is the first core-TV data provider. It deliberately exposes the raw
 * DVBSerTable/EASISerTable/AnalogSerTable rows; channel-list filtering, LCN
 * ordering, tuner selection, EPG and PVR consume this service later.
 */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/module.h"
#include "openaurora/service_db.h"
#include "openaurora/services.h"

#define DEFAULT_DB "/conf/sqlite_db/settingsDB.db"

struct ctx {
    const oa_host_api *api;
    struct oa_service_list list;
    int published;
};

static struct ctx *g_ctx;

static const struct oa_service_list *snapshot(void)
{
    return g_ctx && g_ctx->published ? &g_ctx->list : NULL;
}

static int format(const struct oa_service_list *list, size_t index,
    char *buf, size_t cap)
{
    if (!list || index >= list->count || !buf || !cap) return -EINVAL;
    const struct oa_service_row *row = &list->rows[index];
    return snprintf(buf, cap,
        "source=%s id=%" PRId64 " handle=%" PRId64 " lcn=%" PRId64
        " type=%" PRId64 " flags=%" PRId64 " visible=%d available=%d"
        " encrypted=%d numeric=%d missing=%d name=%s provider=%s\n",
        oa_service_source_name(row->source), row->id, row->handle, row->lcn,
        row->type, row->flags, oa_service_visible(row),
        oa_service_available(row), oa_service_encrypted(row),
        oa_service_numeric_selection(row), oa_service_not_found(row),
        row->name, row->provider);
}

static const oa_service_list_iface service = { snapshot, format };

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
    oa_service_db_free(&c->list);
    const char *path = c->api->config_get(c->api->priv, "service_db.path",
        DEFAULT_DB);
    char error[200];
    int rc = oa_service_db_read_all(path, &c->list, error, sizeof error);
    if (rc) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "service_db: %s", error);
        return -1;
    }
    size_t counts[3] = { 0, 0, 0 };
    for (size_t i = 0; i < c->list.count; i++) {
        unsigned source = c->list.rows[i].source;
        if (source < 3) counts[source]++;
    }
    if (c->api->publish(c->api->priv, OA_SVC_SERVICE_LIST, 1, &service)) {
        oa_service_db_free(&c->list);
        return -1;
    }
    c->published = 1;
    c->api->log(c->api->priv, OA_LOG_INFO,
        "service_db: %zu rows (%zu dvb, %zu easi, %zu analog) from %s",
        c->list.count, counts[0], counts[1], counts[2], path);
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    int published = c->published;
    c->published = 0;
    if (published) c->api->unpublish(c->api->priv, OA_SVC_SERVICE_LIST);
    oa_service_db_free(&c->list);
}

static int m_suspend(void *v) { (void)v; return 0; }
static int m_resume(void *v) { (void)v; return 0; }

static void m_destroy(void *v)
{
    struct ctx *c = v;
    oa_service_db_free(&c->list);
    if (g_ctx == c) g_ctx = NULL;
    free(c);
}

static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "service_db", .version = "1", .requires = { "", 0 },
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
    .suspend = m_suspend, .resume = m_resume
};

OA_MODULE_EXPORT(descriptor)
