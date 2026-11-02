/* Live settings labels over ui.engine: ui.text=1 loads a font record, erases
 * the captured labels and blits font-rendered labels at the recovered origins.
 * A missing pack must fall back to the captured panel. Synthetic assets and a
 * synthetic EF41 record keep vendor bytes out of the test. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "openaurora/events.h"
#include "openaurora/services.h"
#include "ui_nav_trace.h"

static char g_dir[256];
static int g_missing;

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "ui.screen")) return "settings_list";
    if (!strcmp(key, "ui.text")) return "1";
    if (!strcmp(key, "ui.dir")) return g_dir;
    if (!strcmp(key, "ui.ok_file")) return "";
    if (!strcmp(key, "ui.fps")) return "0";
    if (!strcmp(key, "ui.rail_hl")) return "0";
    if (!strcmp(key, "ui.font_id")) return "7";
    if (!strcmp(key, "ui.edit")) return "1";
    if (!strcmp(key, "ui.font_index")) {
        static char path[300];
        snprintf(path, sizeof path, "%s/%s", g_missing ? "/nonexistent" : g_dir, "font.index");
        return path;
    }
    if (!strcmp(key, "ui.font_pack")) {
        static char path[300];
        snprintf(path, sizeof path, "%s/%s", g_missing ? "/nonexistent" : g_dir, "font.pack");
        return path;
    }
    return def;
}

static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[160];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}

static void write_file(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fwrite(data, 1, size, f) == size);
    assert(fclose(f) == 0);
}

static void blob(const char *name)
{
    char path[320];
    snprintf(path, sizeof path, "%s/%s.rgba", g_dir, name);
    unsigned char file[12 + 4 * 4 * 4];
    memcpy(file, "OAI1", 4);
    unsigned w = 4, h = 4;
    memcpy(file + 4, &w, 4);
    memcpy(file + 8, &h, 4);
    memset(file + 12, 0, 4 * 4 * 4);
    write_file(path, file, sizeof file);
}

/* One valid EF41 font: 95 ASCII glyphs, 1x1 coverage, advance 4. */
static size_t build_font(unsigned char *b)
{
    const unsigned glyphs = 95;
    memcpy(b, "EF41", 4);
    b[4] = b[5] = b[6] = 0;
    b[7] = (unsigned char)glyphs;
    for (unsigned i = 0; i < glyphs; ++i) b[8 + i] = 255;
    size_t at = 8 + glyphs;
    memcpy(b + at, "UTF", 3);
    b[at + 3] = b[at + 4] = 0;
    at += 5;
    memcpy(b + at, "DIM", 3);
    b[at + 3] = 0;
    b[at + 4] = (unsigned char)glyphs;
    at += 5;
    for (unsigned i = 0; i < glyphs; ++i) {
        unsigned char *m = b + at + i * 9;
        m[0] = 0; m[1] = 0; m[2] = 1; m[3] = 1; m[4] = 4;
        m[5] = 0; m[6] = 0; m[7] = 0; m[8] = (unsigned char)i;
    }
    memset(b + at + glyphs * 9, 0, 9);
    return at + glyphs * 9 + 9;
}

static void make_font_pack(void)
{
    unsigned char font[1024], large[1024];
    size_t font_size = build_font(font);
    size_t large_size = build_font(large);
    char index[320], pack[320];
    snprintf(index, sizeof index, "%s/font.index", g_dir);
    snprintf(pack, sizeof pack, "%s/font.pack", g_dir);
    char text[256];
    snprintf(text, sizeof text,
             "17\n1\n1\n1\n1\n1\n1\n1\n%u\n1\n1\n1\n1\n1\n1\n1\n1\n%u\n",
             (unsigned)font_size, (unsigned)large_size);
    write_file(index, text, strlen(text));
    unsigned char data[32 + sizeof font + sizeof large];
    memset(data, 0, 7);                    /* records 0..6 */
    memcpy(data + 7, font, font_size);
    memset(data + 7 + font_size, 0, 8);    /* records 8..15 */
    memcpy(data + 15 + font_size, large, large_size);
    write_file(pack, data, 15 + font_size + large_size);
}

static int blit_at(const oa_nav_trace *trace, int x, int y, int w, int h)
{
    int n = trace->blit_count();
    for (int i = 0; i < n; i++) {
        int bx, by, bw, bh;
        if (trace->blit_at(i, &bx, &by, &bw, &bh) == 0 &&
            bx == x && by == y && bw == w && bh == h)
            return 1;
    }
    return 0;
}

static int has_fill(const oa_nav_trace *trace, int x, int y, int w, int h)
{
    int n = trace->fill_count();
    for (int i = 0; i < n; i++) {
        int fx, fy, fw, fh, r, g, b;
        if (trace->fill_at(i, &fx, &fy, &fw, &fh, &r, &g, &b) == 0 &&
            fx == x && fy == y && fw == w && fh == h && r == 190 && g == 30 && b == 45)
            return 1;
    }
    return 0;
}

static int has_fill_rgb(const oa_nav_trace *trace, int x, int y, int w, int h,
                        int r, int g, int b)
{
    int n = trace->fill_count();
    for (int i = 0; i < n; i++) {
        int fx, fy, fw, fh, fr, fg, fb;
        if (trace->fill_at(i, &fx, &fy, &fw, &fh, &fr, &fg, &fb) == 0 &&
            fx == x && fy == y && fw == w && fh == h &&
            fr == r && fg == g && fb == b)
            return 1;
    }
    return 0;
}

/* Last blit index matching a rectangle, -1 if absent. Used to prove the
 * opaque focus overlay is composited BEFORE live text, not after it. */
static int last_blit(const oa_nav_trace *trace, int x, int y, int w, int h)
{
    int n = trace->blit_count(), hit = -1;
    for (int i = 0; i < n; i++) {
        int bx, by, bw, bh;
        if (trace->blit_at(i, &bx, &by, &bw, &bh) == 0 &&
            bx == x && by == y && bw == w && bh == h)
            hit = i;
    }
    return hit;
}

static void key(oa_host *host, int id)
{
    oa_key_event ev = { .id = (uint8_t)id, .kind = OA_KEY_PRESS };
    host->api.emit(host, OA_TOPIC_KEY, &ev);
    oa_host_dispatch_posts(host);
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    snprintf(g_dir, sizeof g_dir, "%s", argv[2]);
    g_missing = !strcmp(argv[3], "missing");
    blob("settings_panel");
    for (int i = 0; i < 9; i++) {
        char name[32];
        snprintf(name, sizeof name, "settings_focus_%d", i);
        blob(name);
    }
    make_font_pack();

    oa_host host;
    assert(!oa_host_init(&host, argv[1], NULL));
    host.log_level = OA_LOG_ERROR;
    host.api.config_get = config;
    char out[256];

    assert(!command(&host, "load nav_display", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start nav_display", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_engine", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_engine", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_vendor", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_vendor", out, sizeof out) && !strcmp(out, "ok\n"));

    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    const oa_portal_iface *portal = host.api.lookup(&host, OA_SVC_PORTAL, 1);
    assert(trace && portal);
    assert(portal->screen() == 4);
    assert(blit_at(trace, 0, 0, 4, 4));            /* captured panel */

    if (g_missing) {
        assert(!has_fill(trace, 40, 178, 446, 64)); /* captured text remains */
        assert(!blit_at(trace, 45, 180, 28, 72));
        puts("  ok   missing font pack falls back to the captured settings panel");
        return 0;
    }

    assert(has_fill(trace, 40, 178, 446, 64));     /* captured label erased */
    assert(blit_at(trace, 45, 180, 28, 72));       /* "Modus" at (49,184), 5*4+8 */
    assert(blit_at(trace, 75, 250, 40, 72));       /* "Contrast" at (79,254), 8*4+8 */
    assert(blit_at(trace, 45, 740, 104, 72));      /* long row 8 label */
    assert(has_fill(trace, 480, 177, 200, 64));    /* captured text value erased */
    assert(blit_at(trace, 489, 179, 48, 72));      /* "Natuurlijk" at (493,183) */
    assert(has_fill(trace, 648, 244, 90, 64));     /* captured digits erased */
    assert(blit_at(trace, 655, 246, 16, 72));      /* "95" font16 at (659,250) */
    assert(blit_at(trace, 549, 529, 20, 72));      /* "Uit" */
    assert(has_fill(trace, 40, 596, 446, 64));     /* disabled label erased */
    assert(blit_at(trace, 47, 598, 56, 72));       /* "Beeld zoomen" font16 grey */
    assert(blit_at(trace, 532, 601, 24, 72));      /* disabled "Auto" */
    assert(blit_at(trace, 541, 669, 20, 72));      /* "Aan" */

    /* Lab value editing (ui.edit=1, invented range, firmware step 1): rows 1..4. */
    key(&host, 19); key(&host, 19);              /* sel 0 -> 1 (Contrast) */
    /* Live text replaces the baked focus asset with a clean bar: the asset
     * bakes captured glyphs and grey widgets into the band, which showed up as
     * artifacts and doubled numbers on the panel. */
    assert(has_fill_rgb(trace, 20, 256, 750, 66, 254, 148, 25));
    assert(last_blit(trace, 20, 256, 4, 4) == -1);
    {
        int digits = last_blit(trace, 655, 246, 16, 72);
        int label = last_blit(trace, 75, 250, 40, 72);
        assert(digits >= 0 && label >= 0);
    }
    /* Focused rows erase with the bar colour, not the panel colour. */
    assert(has_fill_rgb(trace, 648, 244, 90, 64, 254, 148, 25));
    assert(has_fill_rgb(trace, 40, 248, 446, 64, 254, 148, 25));
    for (int i = 0; i < 5; i++) key(&host, 22);  /* 95 -> 100 */
    assert(blit_at(trace, 655, 246, 20, 72));    /* "100": 3 digits */
    for (int i = 0; i < 200; i++) key(&host, 21);/* clamp at 0 */
    assert(blit_at(trace, 655, 246, 12, 72));    /* "0": 1 digit */
    key(&host, 19);                              /* sel 1 -> 2, untouched */
    assert(blit_at(trace, 655, 316, 16, 72));    /* "50" still there */
    assert(has_fill_rgb(trace, 648, 314, 90, 64, 254, 148, 25)); /* row2 focused */
    assert(has_fill_rgb(trace, 40, 248, 446, 64, 190, 30, 45));  /* row1 unfocused */

    key(&host, 19);                                /* sel 2 -> 3 */
    assert(has_fill_rgb(trace, 20, 396, 750, 66, 254, 148, 25)); /* clean bar */
    assert(last_blit(trace, 20, 396, 4, 4) == -1);               /* no baked asset */
    key(&host, 10);
    assert(portal->screen() == 3);

    puts("  ok   ui.text renders font7+16 labels/values/digits at fitted origins with captured fallback");
    return 0;
}
