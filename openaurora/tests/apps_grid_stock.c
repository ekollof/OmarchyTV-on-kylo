/* Offline stock-layout apps-grid capture. The asset directory supplies OAI1
 * background/title/tile blobs; this only renders and captures, with no
 * assertions about icon content. Not display/scanout evidence. */
#include "host.h"
#include "openaurora/services.h"
#include "openaurora/ui_engine.h"
#include "ui_nav_trace.h"

#include <assert.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char g_ui_dir[512], g_db[512], g_modules[512];

static const char *config(void *p, const char *key, const char *def)
{
    (void)p;
    if (!strcmp(key, "ui.dir")) return g_ui_dir;
    if (!strcmp(key, "settings.db")) return g_db;
    if (!strcmp(key, "settings.readonly")) return "1";
    if (!strcmp(key, "ui.paint")) return "1";
    if (!strcmp(key, "ui.fps")) return "0";
    if (!strcmp(key, "ui.live_cards")) return "1";
    if (!strcmp(key, "ui.screen")) return "apps";
    if (!strcmp(key, "ui.cards_parent")) return "1";
    if (!strcmp(key, "ui.grid_sel")) {
        const char *sel = getenv("OA_APPS_GRID_SEL");
        return sel && *sel ? sel : "0";
    }
    if (!strcmp(key, "ui.font_index"))
        return getenv("OA_FONT_INDEX");
    if (!strcmp(key, "ui.font_pack"))
        return getenv("OA_FONT_PACK");
    return def;
}

static int command(oa_host *h, const char *text, char *out, size_t cap)
{
    char line[128];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(h, line, out, cap);
}

static void exec(sqlite3 *db, const char *sql)
{
    char *error = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &error) != SQLITE_OK) {
        fprintf(stderr, "sql: %s\n", error ? error : "?");
        exit(1);
    }
}

static void copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    FILE *out = fopen(dst, "wb");
    assert(in && out);
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0)
        assert(fwrite(buf, 1, n, out) == n);
    assert(fclose(in) == 0 && fclose(out) == 0);
}

static void use_live_db(const char *source)
{
    copy_file(source, g_db);
    sqlite3 *db = NULL;
    assert(sqlite3_open(g_db, &db) == SQLITE_OK);
    exec(db,
        "INSERT INTO LauncherItem VALUES(0,0,0,-1,9900,0,0,1,0,0,2,-1,0,0,1,0,"
        "'','','Markt','markt','','','','','','');");
    assert(sqlite3_close(db) == SQLITE_OK);
}

static void make_db(void)
{
    sqlite3 *db = NULL;
    assert(sqlite3_open(g_db, &db) == SQLITE_OK);
    exec(db,
        "CREATE TABLE LauncherItem(m_application_image_crc integer,"
        "m_application_thumb_update_time integer,m_application_update_time integer,"
        "m_channel_handle integer,m_id integer,m_is_delete_disabled integer,"
        "m_is_deleted integer,m_is_featured integer,m_is_moved_by_user integer,"
        "m_is_out_of_sync integer,m_item_type integer,m_menu_return_id integer,"
        "m_order_index integer,m_original_id integer,m_parent_id integer,"
        "m_server_id integer,m_application_ad_image_url blob,"
        "m_application_ad_url blob,m_application_name blob,"
        "m_application_thumb_file_name blob,m_application_thumb_url blob,"
        "m_application_url blob,m_fvp_application_id blob,"
        "m_menu_string_id blob,m_recommendation_url blob,"
        "m_application_ad_tracking_url blob)");
    for (int i = 0; i < 25; i++) {
        char sql[512];
        snprintf(sql, sizeof sql,
            "INSERT INTO LauncherItem VALUES(0,0,0,-1,%d,0,0,1,0,0,2,-1,%d,0,0,0,"
            "'','','App %d','live_%02d','','','','','','')",
            200 + i, i + 1, i, i);
        exec(db, sql);
    }
    assert(sqlite3_close(db) == SQLITE_OK);
}

int main(int argc, char **argv)
{
    assert(argc == 4 || argc == 5);
    snprintf(g_modules, sizeof g_modules, "%s", argv[1]);
    snprintf(g_ui_dir, sizeof g_ui_dir, "%s", argv[2]);
    char tmp[] = "/tmp/oa-apps-stock.XXXXXX";
    assert(mkdtemp(tmp));
    snprintf(g_db, sizeof g_db, "%s/settings.db", tmp);
    if (argc == 5) use_live_db(argv[4]);
    else make_db();

    oa_host host;
    assert(!oa_host_init(&host, g_modules, NULL));
    host.log_level = -1;
    host.api.config_get = config;
    char out[512];
    assert(!command(&host, "load nav_display", out, sizeof out));
    assert(!command(&host, "start nav_display", out, sizeof out));
    assert(!command(&host, "load settings", out, sizeof out));
    assert(!command(&host, "start settings", out, sizeof out));
    assert(!command(&host, "load ui_engine", out, sizeof out));
    assert(!command(&host, "start ui_engine", out, sizeof out));
    assert(!command(&host, "load ui_vendor", out, sizeof out));
    assert(!command(&host, "start ui_vendor", out, sizeof out));
    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    const oa_portal_iface *portal = host.api.lookup(&host, OA_SVC_PORTAL, 1);
    assert(trace && portal && portal->screen() == 5);
    assert(!trace->capture(argv[3]));
    host.api.release(&host, OA_SVC_PORTAL);
    host.api.release(&host, OA_SVC_NAV_TRACE);
    assert(!command(&host, "stop ui_vendor", out, sizeof out));
    assert(!command(&host, "stop settings", out, sizeof out));
    assert(!command(&host, "stop ui_engine", out, sizeof out));
    assert(!command(&host, "stop nav_display", out, sizeof out));
    unlink(g_db);
    rmdir(tmp);
    puts("ok apps grid stock capture: rendered offline candidate");
    return 0;
}
