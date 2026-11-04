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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/events.h"
#include "openaurora/launcher_db.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

#define DEFAULT_DB "/conf/sqlite_db/settingsDB.db"

struct ctx {
    const oa_host_api *api;
    sqlite3 *db;
    int readonly;
    int published,hotel_published,launcher_published;
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
    return g_ctx && g_ctx->db ? (g_ctx->readonly ? 0 : 1) : 0;
}

static int sql_error(int rc)
{
    rc&=255;
    return rc==SQLITE_BUSY || rc==SQLITE_LOCKED?EBUSY:rc==SQLITE_NOMEM?ENOMEM:EIO;
}
static int hotel_read(struct oa_tab_hotel *out)
{
    if(!out)return EINVAL;
    struct ctx *c=g_ctx;
    if(!c || !c->db || !c->hotel_published)return ENODEV;
    sqlite3_stmt *st=NULL;
    int rc=sqlite3_prepare_v2(c->db,"SELECT m_hotel_mode_availability,m_osd_menu_mode,"
        "m_pic_mode_availability FROM HMCSettings LIMIT 1",-1,&st,NULL);
    if(rc!=SQLITE_OK){if(st)sqlite3_finalize(st);return sql_error(rc);}
    rc=sqlite3_step(st);int error=0;struct oa_tab_hotel next={0};
    if(rc==SQLITE_ROW) {
        next.mode_available=(uint8_t)sqlite3_column_int(st,0);
        next.osd_menu_mode=(uint32_t)sqlite3_column_int(st,1);
        next.picture_flag=(uint8_t)sqlite3_column_int(st,2);
    } else error=rc==SQLITE_DONE?ENODATA:sql_error(rc);
    rc=sqlite3_finalize(st);if(!error && rc!=SQLITE_OK)error=sql_error(rc);
    if(!error)*out=next;
    return error;
}

static const oa_settings_iface g_iface = { get_int, set_int, is_writable };
static const oa_settings_hotel_iface hotel_iface={hotel_read};

static int launcher_read_all(struct oa_launcher_list **out)
{
    struct ctx *c=g_ctx;
    if(!c)return -ENODEV;
    if(!out)return -EINVAL;
    struct oa_launcher_list *list=calloc(1,sizeof *list);
    if(!list)return -ENOMEM;
    int rc=oa_launcher_db_read_all(c->path,list,c->err,sizeof c->err);
    if(rc){free(list);return rc;}
    *out=list;return 0;
}
static int launcher_read_parent(int32_t parent_id,int include_deleted,
    int include_out_of_sync,struct oa_launcher_list **out)
{
    struct ctx *c=g_ctx;
    if(!c)return -ENODEV;
    if(!out)return -EINVAL;
    struct oa_launcher_list *list=calloc(1,sizeof *list);
    if(!list)return -ENOMEM;
    int rc=oa_launcher_db_read_parent(c->path,parent_id,include_deleted,
        include_out_of_sync,list,c->err,sizeof c->err);
    if(rc){free(list);return rc;}
    *out=list;return 0;
}
static size_t launcher_count(const struct oa_launcher_list *list)
{ return list?list->count:0; }
static int launcher_get(const struct oa_launcher_list *list,size_t index,
    struct oa_launcher_row_view *out)
{
    if(!list || index>=list->count || !out)return -EINVAL;
    const struct oa_launcher_info *row=&list->rows[index];
    *out=(struct oa_launcher_row_view){
        .id=row->id,.parent_id=row->parent_id,.order_index=row->order_index,
        .item_type=row->item_type,.is_deleted=row->is_deleted,
        .is_featured=row->is_featured,.is_out_of_sync=row->is_out_of_sync,
        .is_delete_disabled=row->is_delete_disabled,
        .application_name=row->application_name,
        .thumb_file_name=row->thumb_file_name,
        .menu_string_id=row->menu_string_id,
        .application_url=row->application_url,.thumb_url=row->thumb_url};
    return 0;
}
static int launcher_format(const struct oa_launcher_list *list,size_t index,
    char *buf,size_t cap)
{
    if(!list || index>=list->count || !buf || !cap)return -EINVAL;
    const struct oa_launcher_info *row=&list->rows[index];
    return snprintf(buf,cap,
        "id=%d parent=%d order=%d type=%d name=%s thumb=%s menu=%s\n",
        row->id,row->parent_id,row->order_index,row->item_type,
        row->application_name,row->thumb_file_name,row->menu_string_id);
}
static void launcher_release(struct oa_launcher_list *list)
{
    if(!list)return;
    oa_launcher_db_free(list);
    free(list);
}
static const oa_launcher_db_iface launcher_iface={launcher_read_all,
    launcher_read_parent,launcher_count,launcher_get,launcher_format,
    launcher_release};
static void m_stop(void *v);

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

    if(c->api->publish(c->api->priv,OA_SVC_SETTINGS,1,&g_iface)) {
        m_stop(c);return -1;
    }
    c->published=1;
    if(c->api->publish(c->api->priv,OA_SVC_SETTINGS_HOTEL,1,&hotel_iface)) {
        m_stop(c);return -1;
    }
    c->hotel_published=1;
    if(c->api->publish(c->api->priv,OA_SVC_LAUNCHER_DB,1,&launcher_iface)) {
        m_stop(c);return -1;
    }
    c->launcher_published=1;
    int cec = -1;
    if (get_int("m_cec_enabled", &cec) == 0)
        c->api->log(c->api->priv, OA_LOG_INFO, "probe: m_cec_enabled=%d", cec);

    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    int published=c->published,hotel_published=c->hotel_published;
    int launcher_published=c->launcher_published;
    sqlite3 *db=c->db;c->db=NULL;
    c->published=c->hotel_published=c->launcher_published=0;
    if(launcher_published)c->api->unpublish(c->api->priv,OA_SVC_LAUNCHER_DB);
    if(hotel_published)c->api->unpublish(c->api->priv,OA_SVC_SETTINGS_HOTEL);
    if(published)c->api->unpublish(c->api->priv,OA_SVC_SETTINGS);
    if(db)sqlite3_close(db);
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
    .version = "0.2",
    .requires = { "", 0 },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
};

OA_MODULE_EXPORT(g_module)
