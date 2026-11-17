/* Stock PVR list mode (ui.pvr_stock=1): DialogPlayList.svg layout, PVR
 * service rows, focus/selection and recorder output. Not scanout evidence. */
#include "host.h"
#include "openaurora/broadcast.h"
#include "openaurora/events.h"
#include "openaurora/pvr_layout.h"
#include "openaurora/services.h"
#include "openaurora/ui_engine.h"
#include "ui_nav_trace.h"
#include <assert.h>
#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FONT_RECORDS 19

static char g_modules[256], g_ui_dir[512], g_tmp[256], g_pvr_path[512];
static char g_font_index[512], g_font_pack[512], g_capture[512];
static int g_synopsis;

static const char *config(void *p, const char *key, const char *def)
{
    (void)p;
    if (!strcmp(key, "ui.dir")) return g_ui_dir;
    if (!strcmp(key, "ui.paint")) return "1";
    if (!strcmp(key, "ui.fps")) return "0";
    if (!strcmp(key, "ui.screen")) return "pvr";
    if (!strcmp(key, "ui.pvr_stock")) return "1";
    if (!strcmp(key, "ui.pvr_synopsis")) return g_synopsis ? "1" : "0";
    if (!strcmp(key, "ui.text")) return "1";
    if (!strcmp(key, "ui.font_index")) return g_font_index;
    if (!strcmp(key, "ui.font_pack")) return g_font_pack;
    if (!strcmp(key, "ui.font_id")) return "7";
    if (!strcmp(key, "ui.font_num_id")) return "16";
    if (!strcmp(key, "ui.font_title_id")) return "18";
    if (!strcmp(key, "pvr.path")) return g_pvr_path;
    return def;
}

static int command(oa_host *h, const char *text, char *out, size_t cap)
{
    char line[128];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(h, line, out, cap);
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

static void make_icon(const char *dir, const char *name)
{
    char path[600];
    snprintf(path, sizeof path, "%s/%s.rgba", dir, name);
    unsigned char px[36 * 36 * 4];
    for (size_t i = 0; i < sizeof px; i += 4) {
        px[i] = 237; px[i + 1] = 28; px[i + 2] = 36; px[i + 3] = 255;
    }
    FILE *f = fopen(path, "wb");
    assert(f);
    const unsigned char hdr[12] = { 'O', 'A', 'I', '1', 36, 0, 0, 0, 36, 0, 0, 0 };
    assert(fwrite(hdr, 1, sizeof hdr, f) == sizeof hdr);
    assert(fwrite(px, 1, sizeof px, f) == sizeof px);
    assert(!fclose(f));
}

static int find_blit(const oa_nav_trace *trace, int x, int y, int w, int h)
{
    int n = trace->blit_count();
    for (int i = 0; i < n; ++i) {
        int bx, by, bw, bh;
        if (trace->blit_at(i, &bx, &by, &bw, &bh) == 0 &&
            bx == x && by == y && bw == w && bh == h)
            return 1;
    }
    return 0;
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

static int meta_read(uint32_t id, struct oa_pvr_meta *out)
{
    if (!out)
        return -EINVAL;
    memset(out, 0, sizeof *out);
    if (id == 1) {
        /* Recording row: recording + TV only. */
        out->being_recorded = 1;
        out->protection = 1;
        out->playback_protection = 1;
        out->new_flag = 1;
        out->service_type = 1;
        snprintf(out->synopsis, sizeof out->synopsis,
                 "Fixture synopsis text for Movie A.");
        return 0;
    }
    if (id == 2) {
        /* Scheduled row: new + lock + play-disallowed + radio. */
        out->being_recorded = 0;
        out->protection = 0;
        out->playback_protection = 0;
        out->new_flag = 0;
        out->service_type = 2;
        snprintf(out->synopsis, sizeof out->synopsis,
                 "Fixture synopsis text for Movie B.");
        return 0;
    }
    return -ENOENT;
}

static const oa_pvr_meta_iface g_meta = { meta_read };

static int stats_read(struct oa_pvr_stats *out)
{
    if (!out)
        return -EINVAL;
    out->total_bytes = 10ull * 1024 * 1024 * 1024;
    out->free_bytes = 4ull * 1024 * 1024 * 1024;
    out->reserved_bytes = 1ull * 1024 * 1024 * 1024;
    return 0;
}

static const oa_pvr_stats_iface g_stats = { stats_read };

/* Any blit whose top-left is (x,y), regardless of size. */
static int find_blit_at(const oa_nav_trace *trace, int x, int y)
{
    int n = trace->blit_count();
    for (int i = 0; i < n; ++i) {
        int bx, by, bw, bh;
        if (trace->blit_at(i, &bx, &by, &bw, &bh) == 0 && bx == x && by == y)
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
    snprintf(g_tmp, sizeof g_tmp, "/tmp/oa-ui-pvr-stock-XXXXXX");
    assert(mkdtemp(g_tmp));
    const char *ui_env = getenv("OA_PVR_UI_DIR");
    if (ui_env && *ui_env) {
        snprintf(g_ui_dir, sizeof g_ui_dir, "%s", ui_env);
    } else {
        snprintf(g_ui_dir, sizeof g_ui_dir, "%s/ui", g_tmp);
        assert(!mkdir(g_ui_dir, 0700));
        static const char *const ICONS[] = {
            "icon_record_small", "icon_right_arrow", "icon_lock",
            "icon_play_disallowed", "icon_tv_small", "icon_radio_small"
        };
        for (size_t i = 0; i < sizeof ICONS / sizeof ICONS[0]; i++)
            make_icon(g_ui_dir, ICONS[i]);
    }
    snprintf(g_pvr_path, sizeof g_pvr_path, "%s/pvr.db", g_tmp);
    const char *fi = getenv("OA_PVR_FONT_INDEX");
    const char *fp = getenv("OA_PVR_FONT_PACK");
    if (fi && *fi && fp && *fp) {
        snprintf(g_font_index, sizeof g_font_index, "%s", fi);
        snprintf(g_font_pack, sizeof g_font_pack, "%s", fp);
    } else {
        snprintf(g_font_index, sizeof g_font_index, "%s/font.index", g_tmp);
        snprintf(g_font_pack, sizeof g_font_pack, "%s/font.pack", g_tmp);
        make_font();
    }

    oa_host host;
    assert(!oa_host_init(&host, g_modules, NULL));
    host.log_level = -1;
    host.api.config_get = config;
    char out[512];
    assert(!command(&host, "load nav_display", out, sizeof out));
    assert(!command(&host, "start nav_display", out, sizeof out));
    assert(!command(&host, "load pvr", out, sizeof out));
    assert(!command(&host, "start pvr", out, sizeof out));
    const oa_pvr_iface *pvr = host.api.lookup(&host, OA_SVC_PVR, 1);
    assert(pvr);
    struct oa_pvr_request req;
    memset(&req, 0, sizeof req);
    req.service_handle = 7;
    req.start_ms = 1700000000000LL;
    req.end_ms = 1700003600000LL;
    snprintf(req.name, sizeof req.name, "Movie A");
    uint32_t id = 0;
    assert(pvr->add(&req, &id) == 0 && id);
    req.service_handle = 8;
    req.start_ms += 7200000;
    req.end_ms += 7200000;
    snprintf(req.name, sizeof req.name, "Movie B");
    assert(pvr->add(&req, &id) == 0 && id);
    assert(pvr->record(1) == 0);
    host.api.release(&host, OA_SVC_PVR);
    assert(host.api.publish(&host, OA_SVC_PVR_META, 1, &g_meta) == 0);
    assert(host.api.publish(&host, OA_SVC_PVR_STATS, 1, &g_stats) == 0);

    assert(!command(&host, "load ui_engine", out, sizeof out));
    assert(!command(&host, "start ui_engine", out, sizeof out));
    assert(!command(&host, "load ui_vendor", out, sizeof out));
    assert(!command(&host, "start ui_vendor", out, sizeof out));

    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    const oa_portal_iface *portal = host.api.lookup(&host, OA_SVC_PORTAL, 1);
    assert(trace && portal && portal->screen() == 8);

    oa_pvr_layout l;
    assert(oa_pvr_layout_default(&l) == 0);
    assert(find_fill(trace, 0, 0, 1920, 1080, 220, 219, 218));
    assert(find_fill(trace, 0, 0, 1921, 162, 87, 43, 65));
    assert(find_fill(trace, 0, 162, 1920, 54, 154, 153, 153));
    assert(find_fill(trace, l.row.x, l.row.y, l.row.x1 - l.row.x,
                     l.row_height, 254, 148, 25));
    assert(trace->blit_count() >= 10);
    /* Space labels: 10 GiB total, 4 GiB free, 1 GiB reserved -> used 6 GiB,
     * reserved 1 GiB, free 3 GiB in the vendor's KiB-as-MB convention. */
    assert(find_fill(trace, 0, 971, 1920, 109, 72, 23, 48));
    assert(find_blit_at(trace, 315, 1010));
    assert(find_blit_at(trace, 779, 1010));
    assert(find_blit_at(trace, 1295, 1010));
    /* recordingIcon rect 55,237..91,273 (id 54 icon_record_small.png) is
     * painted for the recording row only. */
    assert(find_blit(trace, 55, 237, 36, 36));          /* recording */
    assert(find_blit(trace, 1745, 241, 36, 36));        /* service type TV */
    assert(!find_blit(trace, 97, 237, 36, 36));         /* no new while rec */
    assert(!find_blit(trace, 55, 312, 36, 36));         /* row 2 not recording */
    /* Row 2 meta: new + lock + play-disallowed + radio. */
    assert(find_blit(trace, 97, 312, 36, 36));
    assert(find_blit(trace, 1793, 312, 36, 36));
    assert(find_blit(trace, 1833, 312, 36, 36));
    assert(find_blit(trace, 1745, 316, 36, 36));

    oa_key_event down = { .id = 19, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &down);
    oa_host_dispatch_posts(&host);
    assert(find_fill(trace, l.row.x, l.row.y + l.row_pitch,
                     l.row.x1 - l.row.x, l.row_height, 254, 148, 25));

    const char *capture = getenv("OA_PVR_STOCK_CAPTURE");
    if (capture && *capture)
        snprintf(g_capture, sizeof g_capture, "%s", capture);
    else
        snprintf(g_capture, sizeof g_capture, "%s.pvr-stock", g_tmp);
    assert(!trace->capture(g_capture));

    oa_key_event exit = { .id = 10, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &exit);
    oa_host_dispatch_posts(&host);
    assert(portal->screen() == 0);

    /* Synopsis view (BinSVG 693). */
    assert(!command(&host, "stop ui_vendor", out, sizeof out));
    g_synopsis = 1;
    assert(!command(&host, "start ui_vendor", out, sizeof out));
    assert(portal->screen() == 8);
    oa_pvr_synopsis y;
    assert(oa_pvr_synopsis_default(&y) == 0);
    assert(find_fill(trace, 0, 0, 1921, 162, 87, 43, 65));
    assert(find_fill(trace, 0, 162, 1920, 191, 72, 23, 48));
    assert(find_fill(trace, 0, 353, 1920, 54, 154, 153, 153));
    assert(find_fill(trace, 0, 982, 1920, 98, 72, 23, 48));
    assert(find_blit_at(trace, 82, 173));
    assert(find_blit_at(trace, 143, 204));
    assert(find_fill(trace, 206, 101, 300, 59, 255, 255, 255));
    const char *scap = getenv("OA_PVR_SYNOPSIS_CAPTURE");
    if (scap && *scap)
        assert(!trace->capture(scap));
    host.api.emit(&host, OA_TOPIC_KEY, &exit);
    oa_host_dispatch_posts(&host);
    assert(portal->screen() == 0);

    host.api.release(&host, OA_SVC_PORTAL);
    host.api.release(&host, OA_SVC_NAV_TRACE);
    assert(!command(&host, "stop ui_vendor", out, sizeof out));
    assert(!command(&host, "stop pvr", out, sizeof out));
    assert(!command(&host, "stop ui_engine", out, sizeof out));
    assert(!command(&host, "stop nav_display", out, sizeof out));
    puts("ok pvr stock: DialogPlayList fills/columns, PVR rows and focus");
    cleanup();
    return 0;
}
