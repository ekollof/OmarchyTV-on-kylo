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
static int g_title;
static int g_bad_title;
static int g_edit = 1;
static const char *g_preset = "";

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "ui.screen")) return "settings_list";
    if (!strcmp(key, "ui.text")) return "1";
    if (!strcmp(key, "ui.title")) return g_title ? "1" : "0";
    if (!strcmp(key, "ui.dir")) return g_dir;
    if (!strcmp(key, "ui.ok_file")) return "";
    if (!strcmp(key, "ui.fps")) return "0";
    if (!strcmp(key, "ui.rail_hl")) return "0";
    if (!strcmp(key, "ui.font_id")) return "7";
    if (!strcmp(key, "ui.font_title_id")) return "18";
    if (!strcmp(key, "ui.edit")) return g_edit ? "1" : "0";
    if (!strcmp(key, "ui.edit_init")) return g_preset;
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
    b[at + glyphs * 9 + 2] = 1; /* valid logical height for boxed text */
    return at + glyphs * 9 + 9;
}

static void make_font_pack(void)
{
    unsigned char font[1024], large[1024], title[1024];
    size_t font_size = build_font(font);
    size_t large_size = build_font(large);
    size_t title_size = build_font(title);
    if (g_bad_title) title[title_size - 9 + 2] = 0;
    char index[320], pack[320];
    snprintf(index, sizeof index, "%s/font.index", g_dir);
    snprintf(pack, sizeof pack, "%s/font.pack", g_dir);
    char text[256];
    snprintf(text, sizeof text,
             "19\n1\n1\n1\n1\n1\n1\n1\n%u\n1\n1\n1\n1\n1\n1\n1\n1\n%u\n1\n%u\n",
             (unsigned)font_size, (unsigned)large_size, (unsigned)title_size);
    write_file(index, text, strlen(text));
    unsigned char data[48 + sizeof font + sizeof large + sizeof title];
    memset(data, 0, 7);                    /* records 0..6 */
    memcpy(data + 7, font, font_size);
    memset(data + 7 + font_size, 0, 8);    /* records 8..15 */
    memcpy(data + 15 + font_size, large, large_size);
    memset(data + 15 + font_size + large_size, 0, 1); /* record 17 */
    memcpy(data + 16 + font_size + large_size, title, title_size);
    write_file(pack, data, 16 + font_size + large_size + title_size);
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

/* True when no fill with exactly this rectangle was recorded (any colour).
 * Guards against erase rects stomping another row's bar. */
static int no_fill(const oa_nav_trace *trace, int x, int y, int w, int h)
{
    int n = trace->fill_count();
    for (int i = 0; i < n; i++) {
        int fx, fy, fw, fh, fr, fg, fb;
        if (trace->fill_at(i, &fx, &fy, &fw, &fh, &fr, &fg, &fb) == 0 &&
            fx == x && fy == y && fw == w && fh == h)
            return 0;
    }
    return 1;
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
    g_title = !g_missing && strcmp(argv[3], "notitle");
    g_bad_title = !strcmp(argv[3], "badtitle");
    int preset = !strcmp(argv[3], "preset") || !strcmp(argv[3], "editoff");
    if (preset) {
        g_title = 0;
        g_edit = strcmp(argv[3], "editoff") != 0;
        /* Only row1=7 is valid: malformed/overflow tokens must not overwrite
         * it or change another row. Long input exercises bounded parsing. */
        g_preset = "1=7,1=49junk,2=101,3=-1,4=9999999999999999999999";
    }
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
        assert(!has_fill(trace, 108, 5, 583, 90)); /* captured title remains */
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
    assert(blit_at(trace, 655, 246, preset && g_edit ? 12 : 16, 72));
    if (preset) {
        assert(blit_at(trace, 655, 316, 16, 72)); /* invalid row2 preset ignored */
        key(&host, 19); /* row0 */
        key(&host, 19); /* slider row1 */
        key(&host, 22); /* 7->8 if enabled, otherwise captured95 retained */
        assert(blit_at(trace, 655, 246, g_edit ? 12 : 16, 72));
        assert(!blit_at(trace, 655, 246, g_edit ? 16 : 12, 72));
        puts(g_edit ? "  ok   lab preset validates tokens and preserves enabled editing" :
                      "  ok   lab edit disabled ignores presets and key edits");
        return 0;
    }
    assert(blit_at(trace, 549, 529, 20, 72));      /* "Uit" */
    assert(has_fill(trace, 40, 596, 446, 64));     /* disabled label erased */
    assert(blit_at(trace, 47, 598, 56, 72));       /* "Beeld zoomen" font16 grey */
    assert(blit_at(trace, 532, 601, 24, 72));      /* disabled "Auto" */
    assert(blit_at(trace, 541, 669, 20, 72));      /* "Aan" */

    if (!g_title || g_bad_title) {
        /* Lab-gated title defaults off: rows stay live, captured title stays. */
        assert(has_fill(trace, 40, 178, 446, 64));
        assert(blit_at(trace, 45, 180, 28, 72));
        assert(!has_fill(trace, 108, 5, 583, 90));
        puts(g_bad_title ? "  ok   unusable title font retains live rows and captured title" :
                           "  ok   title lab flag defaults off: rows live, captured title kept");
        return 0;
    }

    /* Live tab title: composed box (108,5,691,95), font18, white. The vendor
     * screen shows the SETTINGS default "Instellingen", not "Beeld". */
    assert(has_fill(trace, 108, 5, 583, 90));      /* captured title erased */
    assert(blit_at(trace, 108, 5, 583, 90));       /* complete text viewport */
    /* Synthetic glyph height1 is centered within90: the first source pixel
     * must be transparent. The old direct-glyph path painted it at Y0. */
    {
        int index = last_blit(trace, 108, 5, 583, 90), r, g, b, a;
        assert(index >= 0 && !trace->blit_first(index, &r, &g, &b, &a));
        assert(a == 0);
    }
    /* Title-band erases must not touch the tab glyph icon (ink x41..99):
     * any fill overlapping y[5,95) starts at x>=108. */
    {
        int n = trace->fill_count();
        for (int i = 0; i < n; i++) {
            int fx, fy, fw, fh, fr, fg, fb;
            assert(trace->fill_at(i, &fx, &fy, &fw, &fh, &fr, &fg, &fb) == 0);
            if (fy < 95 && fy + fh > 5)
                assert(fx >= 108);
        }
    }

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
    /* Focused rows erase only the above-bar blob strip, in panel red; the
     * bar interior needs no erase since the flat fill just laid it. */
    assert(has_fill_rgb(trace, 648, 244, 90, 12, 190, 30, 45));
    assert(has_fill_rgb(trace, 40, 248, 446, 8, 190, 30, 45));
    for (int i = 0; i < 5; i++) key(&host, 22);  /* 95 -> 100 */
    assert(blit_at(trace, 655, 246, 20, 72));    /* "100": 3 digits */
    for (int i = 0; i < 200; i++) key(&host, 21);/* clamp at 0 */
    assert(blit_at(trace, 655, 246, 12, 72));    /* "0": 1 digit */
    /* Unfocused erases must not stomp the focused bar (row surfaces clip
     * to their own frames): row 2's erases are clipped below the bar. */
    assert(no_fill(trace, 648, 314, 90, 64));
    assert(no_fill(trace, 40, 318, 446, 64));
    assert(has_fill_rgb(trace, 648, 322, 90, 56, 190, 30, 45));
    assert(has_fill_rgb(trace, 40, 322, 446, 60, 190, 30, 45));
    key(&host, 19);                              /* sel 1 -> 2, untouched */
    assert(blit_at(trace, 655, 316, 16, 72));    /* "50" still there */
    assert(has_fill_rgb(trace, 648, 314, 90, 12, 190, 30, 45)); /* row2 strip */
    assert(has_fill_rgb(trace, 40, 248, 446, 64, 190, 30, 45));  /* row1 unfocused */

    key(&host, 19);                                /* sel 2 -> 3 */
    assert(has_fill_rgb(trace, 20, 396, 750, 66, 254, 148, 25)); /* clean bar */
    assert(last_blit(trace, 20, 396, 4, 4) == -1);               /* no baked asset */
    key(&host, 10);
    assert(portal->screen() == 3);

    puts("  ok   ui.text renders font7+16 labels/values/digits and the font18 title at composed origins with captured fallback");
    return 0;
}
