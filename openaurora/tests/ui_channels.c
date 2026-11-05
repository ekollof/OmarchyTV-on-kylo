/* Provisional channel-list screen over the stored service query. The screen
 * deliberately uses stored channel_no/name order, not runtime EASI LCN. The
 * recorder is not scanout evidence. */
#include "host.h"
#include "openaurora/events.h"
#include "openaurora/services.h"
#include "openaurora/ui_engine.h"
#include "ui_nav_trace.h"
#include <assert.h>
#include <ftw.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_modules[256], g_db[512], g_ui_dir[512], g_font_index[512], g_font_pack[512];
static char g_tmp[256];
static char g_capture[512];

static const char *config(void *p, const char *key, const char *def)
{
    (void)p;
    if (!strcmp(key, "ui.dir")) return g_ui_dir;
    if (!strcmp(key, "ui.paint")) return "1";
    if (!strcmp(key, "ui.fps")) return "0";
    if (!strcmp(key, "ui.screen")) return "channels";
    if (!strcmp(key, "ui.text")) return "1";
    if (!strcmp(key, "ui.font_index")) return g_font_index;
    if (!strcmp(key, "ui.font_pack")) return g_font_pack;
    if (!strcmp(key, "ui.font_id"))
        return getenv("OA_CHANNELS_FONT_ID") ? getenv("OA_CHANNELS_FONT_ID") : "0";
    if (!strcmp(key, "ui.font_num_id"))
        return getenv("OA_CHANNELS_FONT_NUM_ID") ? getenv("OA_CHANNELS_FONT_NUM_ID") : "0";
    if (!strcmp(key, "service_db.path")) return g_db;
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

static void make_db(const char *path)
{
    sqlite3 *db = NULL;
    assert(sqlite3_open(path, &db) == SQLITE_OK);
    exec(db,
        "CREATE TABLE DVBSerTable(m_id integer,m_handle integer,"
        "m_channel_no integer,m_lcn integer,m_type integer,m_flags integer,"
        "m_nid integer,m_onid integer,m_tsid integer,m_service_locator integer,"
        "m_video_pid integer,m_audio_pid integer,m_pmt_pid integer,"
        "m_pcr_pid integer,m_network_type integer,m_country integer,"
        "m_name_serialized blob,m_provider_serialized blob);"
        "CREATE TABLE EASISerTable(m_handle integer,m_rsn integer,"
        "m_fine_tune_offset integer,m_flags integer,m_name_serialized blob);"
        "CREATE TABLE AnalogSerTable(m_handle integer,m_channel_no integer,"
        "m_lcn integer,m_flags integer,m_type integer,m_tv_system integer,"
        "m_vps_code integer,m_aps_db_index integer,m_name_serialized blob,"
        "m_provider_serialized blob);"
        "INSERT INTO DVBSerTable VALUES(1,1,3,0,1,7,0,0,0,0,0,0,0,0,0,0,'Gamma','P');"
        "INSERT INTO DVBSerTable VALUES(2,2,1,0,1,7,0,0,0,0,0,0,0,0,0,0,'Alpha','P');"
        "INSERT INTO DVBSerTable VALUES(3,3,2,0,1,7,0,0,0,0,0,0,0,0,0,0,'Beta','P');");
    assert(sqlite3_close(db) == SQLITE_OK);
}

static size_t build_font(unsigned char *b)
{
    const unsigned glyphs = 95;
    memcpy(b, "EF41", 4);
    b[4] = b[5] = b[6] = 0;
    b[7] = glyphs;
    for (unsigned i = 0; i < glyphs; ++i) b[8 + i] = (unsigned char)(10 + i);
    size_t at = 8 + glyphs;
    memcpy(b + at, "UTF", 3);
    b[at + 3] = b[at + 4] = 0;
    at += 5;
    memcpy(b + at, "DIM", 3);
    b[at + 3] = 0;
    b[at + 4] = glyphs;
    at += 5;
    for (unsigned i = 0; i < glyphs; ++i) {
        unsigned char *m = b + at + i * 9;
        m[0] = 0; m[1] = 0; m[2] = 1; m[3] = 1; m[4] = 3;
        m[5] = 0; m[6] = 0; m[7] = 0; m[8] = (unsigned char)i;
    }
    unsigned char *x = b + at + glyphs * 9;
    memset(x, 0, 9);
    return at + glyphs * 9 + 9;
}

static void write_bytes(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    assert(f && fwrite(data, 1, size, f) == size && !fclose(f));
}

static void make_font(void)
{
    unsigned char bytes[4096];
    size_t size = build_font(bytes);
    write_bytes(g_font_pack, bytes, size);
    char index[64];
    snprintf(index, sizeof index, "1\n%zu\n", size);
    write_bytes(g_font_index, index, strlen(index));
}

static int find_fill(const oa_nav_trace *trace, int x, int y, int w, int h,
    int r, int g, int b)
{
    int n = trace->fill_count();
    for (int i = 0; i < n; ++i) {
        int fx, fy, fw, fh, fr, fg, fb;
        if (trace->fill_at(i, &fx, &fy, &fw, &fh, &fr, &fg, &fb))
            continue;
        if (fx == x && fy == y && fw == w && fh == h &&
            fr == r && fg == g && fb == b)
            return 1;
    }
    return 0;
}

static int remove_entry(const char *path, const struct stat *st, int flag,
    struct FTW *ftw)
{
    (void)st;
    (void)flag;
    (void)ftw;
    return remove(path);
}

static void cleanup(void)
{
    if (g_tmp[0])
        (void)nftw(g_tmp, remove_entry, 16, FTW_DEPTH | FTW_PHYS);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    snprintf(g_modules, sizeof g_modules, "%s", argv[1]);
    snprintf(g_tmp, sizeof g_tmp, "/tmp/oa-ui-channels-XXXXXX");
    assert(mkdtemp(g_tmp));
    snprintf(g_db, sizeof g_db, "%s/settings.db", g_tmp);
    snprintf(g_ui_dir, sizeof g_ui_dir, "%s/ui", g_tmp);
    assert(!mkdir(g_ui_dir, 0700));
    const char *font_index = getenv("OA_CHANNELS_FONT_INDEX");
    const char *font_pack = getenv("OA_CHANNELS_FONT_PACK");
    if (font_index && *font_index && font_pack && *font_pack) {
        snprintf(g_font_index, sizeof g_font_index, "%s", font_index);
        snprintf(g_font_pack, sizeof g_font_pack, "%s", font_pack);
    } else {
        snprintf(g_font_index, sizeof g_font_index, "%s/font.index", g_tmp);
        snprintf(g_font_pack, sizeof g_font_pack, "%s/font.pack", g_tmp);
    }
    make_db(g_db);
    if (!font_index || !*font_index || !font_pack || !*font_pack)
        make_font();

    oa_host host;
    assert(!oa_host_init(&host, g_modules, NULL));
    host.log_level = -1;
    host.api.config_get = config;
    char out[512];
    assert(!command(&host, "load nav_display", out, sizeof out));
    assert(!command(&host, "start nav_display", out, sizeof out));
    assert(!command(&host, "load service_db", out, sizeof out));
    assert(!command(&host, "start service_db", out, sizeof out));
    assert(!command(&host, "load ui_engine", out, sizeof out));
    assert(!command(&host, "start ui_engine", out, sizeof out));
    assert(!command(&host, "load ui_vendor", out, sizeof out));
    assert(!command(&host, "start ui_vendor", out, sizeof out));

    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    const oa_portal_iface *portal = host.api.lookup(&host, OA_SVC_PORTAL, 1);
    assert(trace && portal && portal->screen() == 6);
    assert(find_fill(trace, 0, 0, 1921, 162, 72, 23, 48));
    assert(find_fill(trace, 0, 162, 1921, 60, 190, 30, 45));
    assert(find_fill(trace, 80, 236, 1760, 60, 247, 148, 30));
    assert(trace->blit_count() >= 4);

    oa_key_event down = { .id = 19, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &down);
    oa_host_dispatch_posts(&host);
    assert(find_fill(trace, 80, 300, 1760, 60, 247, 148, 30));

    const char *capture = getenv("OA_CHANNELS_CAPTURE");
    if (capture && *capture)
        snprintf(g_capture, sizeof g_capture, "%s", capture);
    else
        snprintf(g_capture, sizeof g_capture, "%s.channels", g_tmp);
    assert(!trace->capture(g_capture));

    oa_key_event exit = { .id = 10, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &exit);
    oa_host_dispatch_posts(&host);
    assert(portal->screen() == 0);

    host.api.release(&host, OA_SVC_PORTAL);
    host.api.release(&host, OA_SVC_NAV_TRACE);
    assert(!command(&host, "stop ui_vendor", out, sizeof out));
    assert(!command(&host, "stop service_db", out, sizeof out));
    assert(!command(&host, "stop ui_engine", out, sizeof out));
    assert(!command(&host, "stop nav_display", out, sizeof out));
    puts("ok provisional channel list: stored channel order, non-parity marker, selection and exit");
    cleanup();
    return 0;
}
