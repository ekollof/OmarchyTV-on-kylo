/* Stock EPG timeline mode (ui.epg_stock=1): record-101 chrome, list split,
 * event bars from a fixture epg service and explicit row pitch. Recorder, not
 * scanout evidence. */
#include "host.h"
#include "openaurora/broadcast.h"
#include "openaurora/epg_layout.h"
#include "openaurora/events.h"
#include "openaurora/services.h"
#include "openaurora/ui_engine.h"
#include "ui_nav_trace.h"
#include <assert.h>
#include <errno.h>
#include <ftw.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FONT_RECORDS 19
#define WINDOW_MS 1700000000000LL

static char g_modules[256], g_db[512], g_ui_dir[512], g_tmp[256];
static char g_font_index[512], g_font_pack[512], g_capture[512];
static int g_synopsis;

static const char *config(void *p, const char *key, const char *def)
{
    (void)p;
    if (!strcmp(key, "ui.dir")) return g_ui_dir;
    if (!strcmp(key, "ui.paint")) return "1";
    if (!strcmp(key, "ui.fps")) return "0";
    if (!strcmp(key, "ui.screen")) return "epg";
    if (!strcmp(key, "ui.epg_stock")) return "1";
    if (!strcmp(key, "ui.epg_row_pitch")) return "76";
    if (!strcmp(key, "ui.epg_synopsis")) return g_synopsis ? "1" : "0";
    if (!strcmp(key, "ui.epg_info_key")) return "60";
    if (!strcmp(key, "ui.epg_options")) return "1";
    if (!strcmp(key, "ui.epg_options_offset_y")) return "0";
    if (!strcmp(key, "ui.epg_window_start")) return "1700000000000";
    if (!strcmp(key, "ui.text")) return "1";
    if (!strcmp(key, "ui.font_index")) return g_font_index;
    if (!strcmp(key, "ui.font_pack")) return g_font_pack;
    if (!strcmp(key, "ui.font_id")) return "7";
    if (!strcmp(key, "ui.font_num_id")) return "16";
    if (!strcmp(key, "ui.font_title_id")) return "18";
    if (!strcmp(key, "service_db.path")) return g_db;
    if (!strcmp(key, "i18n.profile")) return getenv("OA_EPG_I18N_PROFILE");
    if (!strcmp(key, "i18n.language"))
        return getenv("OA_EPG_I18N_LANG") ? getenv("OA_EPG_I18N_LANG") : "dut";
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
        "m_provider_serialized blob);");
    static const char *names[3] = { "NPO 1", "NPO 2", "RTL 4" };
    for (int i = 0; i < 3; i++) {
        char sql[512];
        snprintf(sql, sizeof sql,
            "INSERT INTO AnalogSerTable VALUES(%d,%d,0,0,0,0,0,0,'%s','P');",
            i + 1, i + 1, names[i]);
        exec(db, sql);
    }
    assert(sqlite3_close(db) == SQLITE_OK);
}

/* Fixture schedule relative to WINDOW_MS: three 30-min events for handle 1,
 * one 2-hour event for handle 2, none for handle 3. */
static int epg_query(uint32_t service_handle, int64_t from_ms, int64_t to_ms,
                     struct oa_epg_event *events, size_t cap, size_t *count)
{
    if (!events || !count || from_ms >= to_ms)
        return -EINVAL;
    static const struct {
        uint32_t handle;
        int64_t start, end;
        const char *name;
    } rows[] = {
        { 1, WINDOW_MS, WINDOW_MS + 1800000, "Journaal" },
        { 1, WINDOW_MS + 1800000, WINDOW_MS + 3600000, "NOS Sport" },
        { 1, WINDOW_MS + 3600000, WINDOW_MS + 5400000, "Film" },
        { 2, WINDOW_MS + 600000, WINDOW_MS + 7800000, "Lang programma" }
    };
    size_t n = 0;
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        if (rows[i].handle != service_handle)
            continue;
        if (rows[i].end <= from_ms || rows[i].start >= to_ms)
            continue;
        if (n < cap) {
            struct oa_epg_event *e = &events[n];
            memset(e, 0, sizeof *e);
            e->service_handle = rows[i].handle;
            e->event_id = (uint32_t)i + 1;
            e->start_ms = rows[i].start;
            e->end_ms = rows[i].end;
            snprintf(e->name, sizeof e->name, "%s", rows[i].name);
            snprintf(e->description, sizeof e->description,
                     "%s: fixture synopsis text.", rows[i].name);
        }
        n++;
    }
    *count = n;
    return 0;
}

static const oa_epg_iface g_epg = { epg_query };

/* Fixture synopsis image: 16x9 RGBA, scaled into the record-102 eventImage
 * 62,299..382,479 (320x180). */
static unsigned char g_image_rgba[16 * 9 * 4];

static int image_get(uint32_t service_handle, uint32_t event_id,
                     struct oa_epg_image *out)
{
    if (!out)
        return -EINVAL;
    if (!service_handle || !event_id)
        return -ENOENT;
    for (int i = 0; i < 16 * 9; i++) {
        g_image_rgba[i * 4] = 40;
        g_image_rgba[i * 4 + 1] = 120;
        g_image_rgba[i * 4 + 2] = 200;
        g_image_rgba[i * 4 + 3] = 255;
    }
    out->rgba = g_image_rgba;
    out->w = 16;
    out->h = 9;
    return 0;
}

static const oa_epg_image_iface g_image = { image_get };

static int find_scaled(const oa_nav_trace *trace, int x, int y, int w, int h,
                       int sw, int sh)
{
    int n = trace->scaled_count();
    for (int i = 0; i < n; ++i) {
        int bx, by, bw, bh, bsw, bsh;
        if (trace->scaled_at(i, &bx, &by, &bw, &bh, &bsw, &bsh) == 0 &&
            bx == x && by == y && bw == w && bh == h && bsw == sw && bsh == sh)
            return 1;
    }
    return 0;
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
    snprintf(g_tmp, sizeof g_tmp, "/tmp/oa-ui-epg-stock-XXXXXX");
    assert(mkdtemp(g_tmp));
    snprintf(g_db, sizeof g_db, "%s/settings.db", g_tmp);
    snprintf(g_ui_dir, sizeof g_ui_dir, "%s/ui", g_tmp);
    assert(!mkdir(g_ui_dir, 0700));
    const char *fi = getenv("OA_EPG_FONT_INDEX");
    const char *fp = getenv("OA_EPG_FONT_PACK");
    if (fi && *fi && fp && *fp) {
        snprintf(g_font_index, sizeof g_font_index, "%s", fi);
        snprintf(g_font_pack, sizeof g_font_pack, "%s", fp);
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
    if (getenv("OA_EPG_I18N_PROFILE")) {
        assert(!command(&host, "load i18n", out, sizeof out));
        assert(!command(&host, "start i18n", out, sizeof out));
    }
    assert(host.api.publish(&host, OA_SVC_EPG, 1, &g_epg) == 0);
    assert(host.api.publish(&host, OA_SVC_EPG_IMAGE, 1, &g_image) == 0);
    assert(!command(&host, "load ui_engine", out, sizeof out));
    assert(!command(&host, "start ui_engine", out, sizeof out));
    assert(!command(&host, "load ui_vendor", out, sizeof out));
    assert(!command(&host, "start ui_vendor", out, sizeof out));

    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    const oa_portal_iface *portal = host.api.lookup(&host, OA_SVC_PORTAL, 1);
    assert(trace && portal && portal->screen() == 9);

    oa_epg_layout l;
    assert(oa_epg_layout_default(&l) == 0);
    oa_epg_rect services, timeline;
    assert(oa_epg_list_split(&l.list, l.service_list.x1 - l.service_list.x,
                             &services, &timeline) == 0);
    assert(find_fill(trace, 0, 0, 1920, 1080, 255, 255, 255));
    assert(find_fill(trace, 0, 0, 1920, 319, 72, 23, 48));
    assert(find_fill(trace, 0, 319, 1920, 761, 190, 30, 45));
    assert(find_fill(trace, 0, 264, 1920, 54, 36, 11, 24));
    assert(find_fill(trace, 3, 319, 1917, 724, 197, 201, 205));
    assert(find_fill(trace, 0, 319, 640, 724, 255, 255, 255));
    assert(find_fill(trace, 643, 319, 2, 724, 35, 31, 32));
    /* TabButtonFA 522: selected tab is a white pill; the other four are
     * transparent. The labels are text from the HLP_* i18n keys. */
    assert(find_fill(trace, 206, 198, 300, 59, 255, 255, 255));
    assert(!find_fill(trace, 508, 198, 300, 59, 255, 255, 255));

    /* Event bars use the recovered ratio (interval 7200 s, width 1276). */
    int x = 0, w = 0;
    int y0 = l.list.y + 4;
    int bar_h = 76 - 8;
    assert(oa_epg_event_span(&timeline, WINDOW_MS, 7200,
                             WINDOW_MS, WINDOW_MS + 1800000, &x, &w) == 0);
    assert(x == 644 && w == 319);
    assert(find_fill(trace, x, y0, w, bar_h, 126, 0, 10));
    assert(oa_epg_event_span(&timeline, WINDOW_MS, 7200,
                             WINDOW_MS + 1800000, WINDOW_MS + 3600000,
                             &x, &w) == 0);
    assert(find_fill(trace, x, y0, w, bar_h, 126, 0, 10));
    assert(oa_epg_event_span(&timeline, WINDOW_MS, 7200,
                             WINDOW_MS + 3600000, WINDOW_MS + 5400000,
                             &x, &w) == 0);
    /* Focus time is the window middle (now is outside the fixture window), so
     * setFocusedEvent picks the 60..90 min event. It is focused but not
     * currently running, so the style is FI (#EDEDED). */
    assert(find_fill(trace, x, y0, w, bar_h, 237, 237, 237));
    /* Second service row (pitch 76) has the clipped 2-hour event. */
    int y1 = l.list.y + 76 + 4;
    assert(oa_epg_event_span(&timeline, WINDOW_MS, 7200,
                             WINDOW_MS + 600000, WINDOW_MS + 7800000,
                             &x, &w) == 0);
    assert(x == 750 && w == 1170);
    assert(find_fill(trace, x, y1, w, bar_h, 126, 0, 10));
    assert(trace->blit_count() >= 10);

    const char *capture = getenv("OA_EPG_STOCK_CAPTURE");
    if (capture && *capture)
        snprintf(g_capture, sizeof g_capture, "%s", capture);
    else
        snprintf(g_capture, sizeof g_capture, "%s.epg-stock", g_tmp);
    /* Capture the initial window before scrolling. */
    assert(!trace->capture(g_capture));

    /* Select opens the record-151 event options overlay. The fixture event has
     * a description, so the visible list is VIEW_CHANNEL + MORE_INFO. */
    oa_key_event select_ev = { .id = 53, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &select_ev);
    oa_host_dispatch_posts(&host);
    assert(find_fill(trace, 610, 806, 700, 103, 237, 238, 239));
    assert(find_fill(trace, 610, 844, 700, 76, 254, 148, 25));
    assert(find_fill(trace, 610, 909, 700, 76, 1, 1, 1));
    const char *ocap = getenv("OA_EPG_OPTIONS_CAPTURE");
    if (ocap && *ocap)
        assert(!trace->capture(ocap));
    oa_key_event down_opt = { .id = 19, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &down_opt);
    oa_host_dispatch_posts(&host);
    assert(find_fill(trace, 610, 909, 700, 76, 254, 148, 25));
    assert(find_fill(trace, 610, 844, 700, 76, 1, 1, 1));
    host.api.emit(&host, OA_TOPIC_KEY, &select_ev);
    oa_host_dispatch_posts(&host);
    assert(!find_fill(trace, 610, 806, 700, 103, 237, 238, 239));

    /* Left/Right move the focused event inside the selected service. It
     * starts on the third event; Left focuses the second, Right the third. */
    oa_key_event left = { .id = 21, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &left);
    oa_host_dispatch_posts(&host);
    assert(find_fill(trace, 644 + 319, y0, 319, bar_h, 237, 237, 237));
    assert(find_fill(trace, 644 + 638, y0, 319, bar_h, 126, 0, 10));
    oa_key_event right = { .id = 22, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &right);
    oa_host_dispatch_posts(&host);
    assert(find_fill(trace, 644 + 638, y0, 319, bar_h, 237, 237, 237));

    /* Up/Down select the service row; the new row focuses the event at the
     * focus time (its only clipped event), and the previous row goes UFA. */
    oa_key_event down = { .id = 19, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &down);
    oa_host_dispatch_posts(&host);
    assert(find_fill(trace, 750, y1, 1170, bar_h, 237, 237, 237));
    assert(find_fill(trace, 644 + 638, y0, 319, bar_h, 126, 0, 10));
    oa_key_event up = { .id = 20, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &up);
    oa_host_dispatch_posts(&host);
    assert(find_fill(trace, 644 + 638, y0, 319, bar_h, 237, 237, 237));
    assert(find_fill(trace, 750, y1, 1170, bar_h, 126, 0, 10));

    oa_key_event exit = { .id = 10, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &exit);
    oa_host_dispatch_posts(&host);
    assert(portal->screen() == 0);

    /* Synopsis view (BinSVG 102): the vendor showSynopsis resource. */
    assert(!command(&host, "stop ui_vendor", out, sizeof out));
    g_synopsis = 1;
    assert(!command(&host, "start ui_vendor", out, sizeof out));
    assert(portal->screen() == 9);
    oa_epg_synopsis y;
    assert(oa_epg_synopsis_default(&y) == 0);
    assert(find_fill(trace, 0, 0, 1920, 264, 72, 23, 48));
    assert(find_fill(trace, 0, 264, 1920, 307, 36, 11, 24));
    assert(find_fill(trace, 0, 570, 1920, 507, 197, 201, 205));
    assert(find_fill(trace, 0, 572, 640, 422, 197, 201, 205));
    assert(find_fill(trace, 639, 572, 2, 422, 35, 31, 32));
    /* The synopsis keeps the timeline as a shorter bottom box: split at
     * 640+1, so 641..1920 (width 1279) with the same 7200 s ratio. */
    assert(find_fill(trace, 640, 570, 2, 507, 35, 31, 32));
    assert(find_fill(trace, 641, 574, 319, 68, 126, 0, 10));
    /* The focused event (window middle -> 60..90 min) is FI; the synopsis
     * timeline is the same 7200 s ratio over its own 1279 px width. */
    int sx = 0, sw = 0;
    oa_epg_rect syn_timeline;
    assert(oa_epg_list_split(&y.list, y.service_list.x1 - y.service_list.x,
                             &services, &syn_timeline) == 0);
    assert(oa_epg_event_span(&syn_timeline, WINDOW_MS, 7200,
                             WINDOW_MS + 3600000, WINDOW_MS + 5400000,
                             &sx, &sw) == 0);
    assert(find_fill(trace, sx, 574, sw, 68, 237, 237, 237));
    /* Record-102 eventImage 62,299 320x180 from the 16x9 provider image. */
    assert(find_scaled(trace, 62, 299, 320, 180, 16, 9));
    assert(trace->blit_count() >= 12);
    const char *scap = getenv("OA_EPG_SYNOPSIS_CAPTURE");
    if (scap && *scap)
        assert(!trace->capture(scap));
    /* The vendor INFO key toggles the synopsis (onInfoKey 0x00dcd234). Our
     * shell has no INFO id, so the fixture names one explicitly. */
    oa_key_event info = { .id = 60, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &info);
    oa_host_dispatch_posts(&host);
    assert(find_fill(trace, 0, 319, 1920, 761, 190, 30, 45));
    host.api.emit(&host, OA_TOPIC_KEY, &info);
    oa_host_dispatch_posts(&host);
    assert(find_fill(trace, 0, 570, 1920, 507, 197, 201, 205));
    host.api.emit(&host, OA_TOPIC_KEY, &exit);
    oa_host_dispatch_posts(&host);
    assert(portal->screen() == 0);

    host.api.release(&host, OA_SVC_PORTAL);
    host.api.release(&host, OA_SVC_NAV_TRACE);
    assert(host.api.unpublish(&host, OA_SVC_EPG_IMAGE) == 0);
    assert(host.api.unpublish(&host, OA_SVC_EPG) == 0);
    assert(!command(&host, "stop ui_vendor", out, sizeof out));
    assert(!command(&host, "stop service_db", out, sizeof out));
    if (getenv("OA_EPG_I18N_PROFILE"))
        assert(!command(&host, "stop i18n", out, sizeof out));
    assert(!command(&host, "stop ui_engine", out, sizeof out));
    assert(!command(&host, "stop nav_display", out, sizeof out));
    puts("ok epg stock: 101/102 chrome, split, selection, options, image and scroll");
    cleanup();
    return 0;
}
