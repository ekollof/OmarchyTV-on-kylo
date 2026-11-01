/*
 * mod_settings — the set's settings database.
 *
 * /conf/sqlite_db/settingsDB.db is a single-row `Settings` table with one INTEGER column per
 * setting (m_cec_enabled, m_privacy_*, m_current_source, …). That makes a small, safe API possible:
 * read or write one named column of that row.
 *
 * Read-only by default — aurora caches many of these values, so an unexpected write while it runs can
 * confuse the stock UI. Enable writes with settings.readonly=0.
 *
 * Column names are validated against [A-Za-z0-9_] before being interpolated, since SQLite has no way
 * to parameterise an identifier.
 */
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

#define DEFAULT_DB "/conf/sqlite_db/settingsDB.db"

struct ctx {
    const oa_host_api *api;
    sqlite3 *db;
    int readonly;
    char path[128];
    char err[160];
};

static struct ctx *g_ctx;

static int valid_col(const char *c)
{
    if (!c || !*c)
        return 0;
    for (const char *p = c; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
              *p == '_'))
            return 0;
    return 1;
}

static int get_int(const char *col, int *out)
{
    struct ctx *c = g_ctx;
    if (!c || !c->db || !valid_col(col))
        return -1;

    char sql[256];
    snprintf(sql, sizeof sql, "SELECT %s FROM Settings LIMIT 1;", col);

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(c->db, sql, -1, &st, NULL) != SQLITE_OK) {
        snprintf(c->err, sizeof c->err, "%s", sqlite3_errmsg(c->db));
        return -1;
    }
    int rc = -1;
    if (sqlite3_step(st) == SQLITE_ROW) {
        if (out)
            *out = sqlite3_column_int(st, 0);
        rc = 0;
    } else {
        snprintf(c->err, sizeof c->err, "no row");
    }
    sqlite3_finalize(st);
    return rc;
}

static int set_int(const char *col, int v)
{
    struct ctx *c = g_ctx;
    if (!c || !c->db || !valid_col(col))
        return -1;
    if (c->readonly)
        return -2;

    char sql[256];
    snprintf(sql, sizeof sql, "UPDATE Settings SET %s=%d;", col, v);
    if (sqlite3_exec(c->db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        snprintf(c->err, sizeof c->err, "%s", sqlite3_errmsg(c->db));
        return -1;
    }
    return 0;
}

static int is_writable(void)
{
    return g_ctx ? (g_ctx->readonly ? 0 : 1) : 0;
}

static const oa_settings_iface g_iface = { get_int, set_int, is_writable };

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    *out = c;
    g_ctx = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    snprintf(c->path, sizeof c->path, "%s",
             c->api->config_get(c->api->priv, "settings.db", DEFAULT_DB));
    const char *ro = c->api->config_get(c->api->priv, "settings.readonly", "1");
    c->readonly = atoi(ro) != 0;

    int flags = SQLITE_OPEN_READONLY;
    if (!c->readonly)
        flags = SQLITE_OPEN_READWRITE;

    if (sqlite3_open_v2(c->path, &c->db, flags, NULL) != SQLITE_OK) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "%s: %s", c->path,
                    c->db ? sqlite3_errmsg(c->db) : "open failed");
        if (c->db) {
            sqlite3_close(c->db);
            c->db = NULL;
        }
        return -1;
    }
    c->api->log(c->api->priv, OA_LOG_INFO, "%s open (%s)", c->path,
                c->readonly ? "read-only" : "read-write");

    int cec = -1;
    if (get_int("m_cec_enabled", &cec) == 0)
        c->api->log(c->api->priv, OA_LOG_INFO, "probe: m_cec_enabled=%d", cec);

    c->api->publish(c->api->priv, OA_SVC_SETTINGS, 1, &g_iface);
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    c->api->release(c->api->priv, OA_SVC_SETTINGS);
    c->api->unpublish(c->api->priv, OA_SVC_SETTINGS);
    if (c->db) {
        sqlite3_close(c->db);
        c->db = NULL;
    }
}

static void m_destroy(void *v)
{
    if (g_ctx == v)
        g_ctx = NULL;
    free(v);
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "settings",
    .version = "0.1",
    .requires = { "", 0 },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
};

OA_MODULE_EXPORT(g_module)
