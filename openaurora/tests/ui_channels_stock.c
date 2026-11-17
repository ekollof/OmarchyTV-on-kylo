/* Stock channel-list mode (ui.channels_stock=1): layout fills, tab/row text
 * and selection over a synthetic analog service DB. The recorder is not
 * scanout evidence. */
#include "host.h"
#include "openaurora/channels.h"
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

#define FONT_RECORDS 19

static char g_modules[256], g_db[512], g_ui_dir[512], g_tmp[256];
static char g_font_index[512], g_font_pack[512], g_capture[512];

static const char *config(void *p, const char *key, const char *def)
{
    (void)p;
    if (!strcmp(key, "ui.dir")) return g_ui_dir;
    if (!strcmp(key, "ui.paint")) return "1";
    if (!strcmp(key, "ui.fps")) return "0";
    if (!strcmp(key, "ui.screen")) return "channels";
    if (!strcmp(key, "ui.text")) return "1";
    if (!strcmp(key, "ui.channels_stock")) return "1";
    if (!strcmp(key, "ui.font_index")) return g_font_index;
    if (!strcmp(key, "ui.font_pack")) return g_font_pack;
    if (!strcmp(key, "ui.font_id")) return "7";
    if (!strcmp(key, "ui.font_num_id")) return "16";
    if (!strcmp(key, "ui.font_title_id")) return "18";
    if (!strcmp(key, "ui.channel_tab_font_id")) return "12";
    if (!strcmp(key, "ui.channel_marker_font_id")) return "1";
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

/* The p0/04 channel list: 11 analog entries, numbers 1..11. */
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
        "m_provider_serialized blob);");
    static const char *names[11] = {
        "C12", "C07", "BBC2", "C21", "CHANNEL 4", "C04", "C39", "C43",
        "BBC1", "S15", "ZDF"
    };
    for (int i = 0; i < 11; i++) {
        char sql[512];
        snprintf(sql, sizeof sql,
            "INSERT INTO AnalogSerTable VALUES(%d,%d,0,0,0,0,0,0,'%s','P');",
            i + 1, i + 1, names[i]);
        exec(db, sql);
    }
    assert(sqlite3_close(db) == SQLITE_OK);
}

static size_t build_record(unsigned char *b)
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
    unsigned char rec[4096];
    size_t rec_size = build_record(rec);
    unsigned char *pack = malloc(rec_size * FONT_RECORDS);
    assert(pack);
    for (unsigned i = 0; i < FONT_RECORDS; i++)
        memcpy(pack + i * rec_size, rec, rec_size);
    write_bytes(g_font_pack, pack, rec_size * FONT_RECORDS);
    free(pack);
    char *index = malloc(32 + FONT_RECORDS * 16);
    assert(index);
    size_t at = 0;
    at += (size_t)snprintf(index + at, 32, "%d\n", FONT_RECORDS);
    for (unsigned i = 0; i < FONT_RECORDS; i++)
        at += (size_t)snprintf(index + at, 16, "%zu\n", rec_size);
    write_bytes(g_font_index, index, at);
    free(index);
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
    (void)st; (void)flag; (void)ftw;
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
    snprintf(g_tmp, sizeof g_tmp, "/tmp/oa-ui-channels-stock-XXXXXX");
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
        make_font();
    }
    make_db(g_db);

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

    /* DialogAdvancedChannelList record-80 rects and the p0/04 colours. */
    assert(find_fill(trace, 0, 0, 780, 1080, 190, 30, 45));
    assert(find_fill(trace, 0, 0, 780, 162, 178, 29, 43));
    assert(find_fill(trace, 12, 100, 250, 59, 255, 255, 255));
    assert(find_fill(trace, 264, 100, 250, 59, 193, 71, 83));
    assert(find_fill(trace, 516, 100, 250, 59, 193, 71, 83));
    assert(find_fill(trace, 19, 195, 748, 66, 254, 148, 25));
    assert(trace->blit_count() >= 20);

    const char *capture = getenv("OA_CHANNELS_STOCK_CAPTURE");
    if (capture && *capture)
        snprintf(g_capture, sizeof g_capture, "%s", capture);
    else
        snprintf(g_capture, sizeof g_capture, "%s.channels-stock", g_tmp);
    /* Capture the initial state (row 1 focused), matching the p0/04 oracle. */
    assert(!trace->capture(g_capture));

    oa_key_event down = { .id = 19, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &down);
    oa_host_dispatch_posts(&host);
    assert(find_fill(trace, 19, 270, 748, 66, 254, 148, 25));

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
    puts("ok channel list stock: record-80 fills/tabs, 75 px focus rows and text");
    cleanup();
    return 0;
}
