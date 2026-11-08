/* mod_ui_omarchy — native OmarchyTV workspace shell over ui.engine.
 *
 * Visual/behavioural reference: the browser OmarchyTV app in
 * launcher/omarchy (top bar with five workspaces and a clock, workspace 1
 * with a status terminal and dock, workspaces 2-5 with input/app/system
 * panels). This module reproduces that structure with native canvas draws
 * only: no browser, webview, HTML/CSS/JS runtime or web app is started.
 *
 * The skin owns its screens and key semantics and draws only through
 * ui.engine. The vendor skin remains the fallback.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "openaurora/canvas.h"
#include "openaurora/events.h"
#include "openaurora/font_bitmap.h"
#include "openaurora/font_draw.h"
#include "openaurora/font_pack.h"
#include "openaurora/font_text.h"
#include "openaurora/lua_skin.h"
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/ui_engine.h"
#include "openaurora/ui_omarchy.h"

enum {
    SCREEN_HOME = 0,
    SCREEN_SOURCES = 1,
    SCREEN_APPS = 2,
    SCREEN_SETTINGS = 3,
    SCREEN_ABOUT = 4
};
#define HOME_TILES 6
#define APP_ROWS 21
#define APP_COLS 7
#define SOURCE_ROWS 9
#define SOURCE_COLS 4
#define SETTINGS_ROWS 4
#define BAR_H 52
#define ZONE_CONTENT 0
#define ZONE_BAR 1

/* Theme colours are the browser OmarchyTV palettes (data.js). */
struct om_theme {
    const char *name;
    int bg[3], panel[3], panel2[3];
    int fg[3], bright[3], dim[3];
    int accent[3], accent2[3];
    int green[3], red[3], yellow[3];
};

static const struct om_theme THEMES[] = {
    { "last-horizon",
      {12,11,12}, {9,8,9}, {24,22,24}, {250,252,251}, {226,221,220}, {88,78,81},
      {181,151,144}, {196,216,226}, {135,169,176}, {195,139,123}, {107,94,115} },
    { "tokyo-night",
      {26,27,38}, {19,20,28}, {36,40,59}, {169,177,214}, {192,202,245}, {86,95,137},
      {122,162,247}, {173,142,230}, {158,206,106}, {247,118,142}, {224,175,104} },
    { "catppuccin",
      {30,30,46}, {22,22,34}, {49,50,68}, {205,214,244}, {205,214,244}, {108,112,134},
      {137,180,250}, {245,194,231}, {166,227,161}, {243,139,168}, {249,226,175} },
    { "gruvbox",
      {40,40,40}, {30,30,30}, {60,56,54}, {212,190,152}, {212,190,152}, {124,111,100},
      {125,174,163}, {211,134,155}, {169,182,101}, {234,105,98}, {216,166,87} },
    { "nord",
      {46,52,64}, {34,39,48}, {59,66,82}, {216,222,233}, {216,222,233}, {102,112,128},
      {129,161,193}, {180,142,173}, {163,190,140}, {191,97,106}, {235,203,139} },
    { "everforest",
      {45,53,59}, {33,39,44}, {52,63,68}, {211,198,170}, {211,198,170}, {79,88,94},
      {127,187,179}, {214,153,182}, {167,192,128}, {230,126,128}, {219,188,127} },
};
#define THEME_COUNT ((int)(sizeof THEMES / sizeof THEMES[0]))

enum { DOCK_TV = 0, DOCK_APP, DOCK_SOURCES, DOCK_APPS, DOCK_SETTINGS, DOCK_ABOUT };
struct dock_entry {
    int kind;
    int app;
    char name[64];
    char sub[32];
};

struct ctx {
    const oa_host_api *api;
    const oa_ui_engine_iface *d;
    const oa_lua_iface *lua;
    oa_omarchy_state *state;
    int active;
    int suspended;
    uint32_t removed, changed;
    uint64_t rebind;
    uint32_t clock_timer;
    uint64_t volume_timer;
    int volume, mute;
    int volume_show, volume_value, overlay_mute;
    int screen;
    int zone;       /* ZONE_CONTENT or ZONE_BAR */
    int bar_focus;  /* 0 logo, 1..5 workspace */
    int focus;      /* dock focus 0..HOME_TILES-1 */
    int theme;
    int source_focus;
    int app_focus;
    int app_total;
    int app_n;
    char apps[APP_ROWS][64];
    struct dock_entry dock[HOME_TILES];
    int dock_n;
    int settings_focus;
    int font_on, title_on, small_on;
    struct oa_font_pack pack;
    struct oa_bitmap_font font, title_font, small_font;
    unsigned char *font_data, *title_data, *small_data;
    /* ABI 3 quiesce: cached skin state is detached and freed by a sched slice. */
    uint32_t cleanup_task;
    oa_omarchy_state *cleanup_state;
    unsigned cleanup_delay_ms;
    oa_omarchy_state saved;
    int have_saved;
};
static const oa_ui_skin g_skin;
static void paint(struct ctx *c);

static int mix(int a, int b, int pct)
{
    return a + (b - a) * pct / 100;
}

static const struct om_theme *theme_of(const struct ctx *c)
{
    int i = c ? c->theme : 0;
    if (i < 0 || i >= THEME_COUNT) i = 0;
    return &THEMES[i];
}

static void clock_tick(void *ud)
{
    struct ctx *c = ud;
    if (c && c->active && !c->suspended) paint(c);
}

static void dispatch(void *ud)
{
    struct ctx *c = ud;
    if (c && c->active && c->d && !c->suspended) c->d->dispatch_skin();
}

static void take_lua_paint(struct ctx *c, int *br, int *bg, int *bb, int *ba,
                           int *ar, int *ag, int *ab, int *x, int *y, int *w, int *h)
{
    int bgc[4], acc[4];
    oa_lua_layout lay;
    oa_omarchy_state *st = c->state;
    if (!st || !c->lua || c->lua->call("paint", "omarchy", 0, NULL) != 0)
        return;
    if (c->lua->theme(bgc, acc) == 0) {
        *br = bgc[0];
        *bg = bgc[1];
        *bb = bgc[2];
        *ba = bgc[3];
        *ar = acc[0];
        *ag = acc[1];
        *ab = acc[2];
        st->bg[0] = *br;
        st->bg[1] = *bg;
        st->bg[2] = *bb;
        st->bg[3] = *ba;
        st->accent[0] = *ar;
        st->accent[1] = *ag;
        st->accent[2] = *ab;
        st->have_colors = 1;
    }
    if (c->lua->layout_at(0, &lay) == 0 && lay.w > 0 && lay.h > 0) {
        *x = lay.x;
        *y = lay.y;
        *w = lay.w;
        *h = lay.h;
        st->layout[0] = lay.x;
        st->layout[1] = lay.y;
        st->layout[2] = lay.w;
        st->layout[3] = lay.h;
        st->have_layout = 1;
    }
}

static void fonts_free(struct ctx *c)
{
    free(c->font_data);
    free(c->title_data);
    free(c->small_data);
    c->font_data = c->title_data = c->small_data = NULL;
    c->font_on = c->title_on = c->small_on = 0;
    oa_font_pack_free(&c->pack);
}

static int font_open(struct oa_font_pack *pack, int id,
                     struct oa_bitmap_font *font, unsigned char **data)
{
    unsigned char *buf = NULL;
    size_t size = 0;
    if (oa_font_pack_read(pack, (unsigned)id, &buf, &size)) return -1;
    if (oa_bitmap_font_open(font, buf, size)) { free(buf); return -1; }
    *data = buf;
    return 0;
}

static void fonts_load(struct ctx *c)
{
    fonts_free(c);
    const char *index = c->api->config_get(c->api->priv, "ui.font_index",
        "/vendor2/dynamic_ui/telefunken18_font_byte_data.txt");
    const char *pack = c->api->config_get(c->api->priv, "ui.font_pack",
        "/vendor2/dynamic_ui/telefunken18_font_byte_data.bin");
    if (oa_font_pack_init(&c->pack, index, pack)) return;
    int id = atoi(c->api->config_get(c->api->priv, "ui.omarchy.font", "10"));
    int title_id = atoi(c->api->config_get(c->api->priv, "ui.omarchy.title_font", "12"));
    int small_id = atoi(c->api->config_get(c->api->priv, "ui.omarchy.small_font", "9"));
    if (font_open(&c->pack, id, &c->font, &c->font_data)) return;
    c->font_on = 1;
    if (!font_open(&c->pack, title_id, &c->title_font, &c->title_data))
        c->title_on = 1;
    if (!font_open(&c->pack, small_id, &c->small_font, &c->small_data))
        c->small_on = 1;
}

static int text_width(const struct oa_bitmap_font *font, const char *line)
{
    int advance = 0;
    if (!font || !line || !*line || oa_font_text_measure(font, line, 0, &advance))
        return 0;
    return advance;
}

static void text_at(struct ctx *c, int x, int y, const struct oa_bitmap_font *font,
                    const char *line, int r, int g, int b, int max_width)
{
    if (!c || !c->d || !c->font_on || !font || !font->pixels || !line || !*line)
        return;
    int advance = text_width(font, line), height = 0;
    if (advance < 1 || advance > max_width || oa_bitmap_font_height(font, &height) ||
        height < 1 || height > 96)
        return;
    int w = advance + 2, h = height + 2;
    unsigned char *buf = calloc((size_t)w * (size_t)h * 4u, 1);
    if (!buf) return;
    oa_canvas canvas = { buf, w, h };
    const uint8_t ink[4] = { (uint8_t)r, (uint8_t)g, (uint8_t)b, 255 };
    if (!oa_font_text_draw(&canvas, font, line, 1, 1, ink, 0, NULL))
        c->d->blit(x, y, w, h, buf);
    free(buf);
}

static const struct oa_bitmap_font *body_font(struct ctx *c)
{
    return c->font_on ? &c->font : NULL;
}

static const struct oa_bitmap_font *head_font(struct ctx *c)
{
    return c->title_on ? &c->title_font : body_font(c);
}

static const struct oa_bitmap_font *small_font(struct ctx *c)
{
    return c->small_on ? &c->small_font : body_font(c);
}

static void text_center(struct ctx *c, int cx, int y,
                        const struct oa_bitmap_font *font, const char *line,
                        int r, int g, int b, int max_width)
{
    int w = text_width(font, line);
    if (w < 1 || w > max_width) return;
    text_at(c, cx - w / 2, y, font, line, r, g, b, max_width);
}

static void frame(struct ctx *c, int x, int y, int w, int h, int t,
                  int r, int g, int b)
{
    c->d->fill(x, y, w, t, r, g, b);
    c->d->fill(x, y + h - t, w, t, r, g, b);
    c->d->fill(x, y, t, h, r, g, b);
    c->d->fill(x + w - t, y, t, h, r, g, b);
}

static void apps_load(struct ctx *c)
{
    c->app_n = 0;
    c->app_total = 0;
    const oa_launcher_db_iface *db =
        c->api->lookup(c->api->priv, OA_SVC_LAUNCHER_DB, 1);
    if (!db) return;
    struct oa_launcher_list *list = NULL;
    int rc = db->read_parent ? db->read_parent(0, 0, 0, &list) : -1;
    if (rc != 0) list = NULL;
    if (!list) (void)db->read_all(&list);
    if (list) {
        size_t n = db->count(list);
        for (size_t i = 0; i < n; i++) {
            struct oa_launcher_row_view row;
            if (db->get(list, i, &row) != 0) continue;
            if (row.is_deleted || row.is_out_of_sync) continue;
            const char *name = row.application_name ? row.application_name : "";
            if (!name[0]) continue;
            int dup = 0;
            for (int k = 0; k < c->app_n; k++)
                if (!strcmp(c->apps[k], name)) { dup = 1; break; }
            if (dup) continue;
            c->app_total++;
            if (c->app_n < APP_ROWS)
                snprintf(c->apps[c->app_n++], sizeof c->apps[0], "%s", name);
        }
        if (c->app_total > 999) c->app_total = 999;
        db->release(list);
    }
    c->api->release(c->api->priv, OA_SVC_LAUNCHER_DB);
}

static const char *SOURCE_NAMES[SOURCE_ROWS] = {
    "TV", "BACK AV", "HDMI 1", "HDMI 2", "HDMI 3",
    "YPbPr", "VGA / PC", "Wireless Display", "AV Share"
};
static const int SOURCE_TYPES[SOURCE_ROWS] = { 0, 15, 17, 18, 19, 20, 16, -1, -1 };

static void dock_add(struct ctx *c, int kind, int app, const char *name,
                     const char *sub)
{
    if (c->dock_n >= HOME_TILES) return;
    struct dock_entry *e = &c->dock[c->dock_n++];
    e->kind = kind;
    e->app = app;
    snprintf(e->name, sizeof e->name, "%s", name ? name : "");
    snprintf(e->sub, sizeof e->sub, "%s", sub ? sub : "");
}

static void dock_build(struct ctx *c)
{
    c->dock_n = 0;
    dock_add(c, DOCK_TV, -1, "TV", "watch TV");
    for (int i = 0; i < c->app_n && c->dock_n < HOME_TILES; i++)
        dock_add(c, DOCK_APP, i, c->apps[i], "application");
    static const struct { int kind; const char *name, *sub; } rest[] = {
        { DOCK_SOURCES, "Sources", "inputs" },
        { DOCK_APPS, "Apps", "catalog" },
        { DOCK_SETTINGS, "Settings", "system" },
        { DOCK_ABOUT, "About", "native shell" },
    };
    for (unsigned i = 0; i < sizeof rest / sizeof rest[0] &&
                       c->dock_n < HOME_TILES; i++)
        dock_add(c, rest[i].kind, -1, rest[i].name, rest[i].sub);
}

static const char *current_source_name(struct ctx *c, char *buf, size_t cap)
{
    int source = -1;
    const oa_settings_iface *set =
        c->api->lookup(c->api->priv, OA_SVC_SETTINGS, 1);
    if (set) {
        (void)set->get_int("m_current_source", &source);
        c->api->release(c->api->priv, OA_SVC_SETTINGS);
    }
    const char *name = NULL;
    switch (source) {
    case 0: name = "TV"; break;
    case 3: name = "BACK AV"; break;
    case 5: name = "HDMI 1"; break;
    case 6: name = "HDMI 2"; break;
    case 7: name = "HDMI 3"; break;
    case 9: name = "YPbPr"; break;
    case 10: name = "VGA / PC"; break;
    case 12: name = "Wireless Display"; break;
    case 13: name = "AV Share"; break;
    default: break;
    }
    if (!name) {
        const oa_source_iface *src =
            c->api->lookup(c->api->priv, OA_SVC_SOURCE, 1);
        if (src) {
            int type = src->current_type();
            for (int i = 0; i < SOURCE_ROWS; i++)
                if (SOURCE_TYPES[i] == type) { name = SOURCE_NAMES[i]; break; }
            c->api->release(c->api->priv, OA_SVC_SOURCE);
        }
    }
    if (!name) name = "unknown";
    snprintf(buf, cap, "%s", name);
    return buf;
}

static int current_volume(struct ctx *c)
{
    int v = c->volume;
    if (v < 0) {
        const oa_audio_iface *a = c->api->lookup(c->api->priv, OA_SVC_AUDIO, 1);
        if (a) {
            if (a->get_volume) v = a->get_volume();
            c->api->release(c->api->priv, OA_SVC_AUDIO);
        }
    }
    return v;
}

/* ---------------------------------------------------------------- shell */

static void background(struct ctx *c, int W, int H, const struct om_theme *t)
{
    const char *wp = c->api->config_get(c->api->priv,
                                        "ui.omarchy.wallpaper", "");
    if (wp[0] && c->d->icon) {
        const oa_ui_icon *ic = c->d->icon(wp);
        if (ic && ic->rgba && c->d->blit_scaled(0, 0, W, H, ic->w, ic->h,
                                                ic->rgba) == 0)
            return;
    }
    /* Theme gradient. Cheap horizontal bands avoid a large asset when no
     * wallpaper is configured. */
    for (int i = 0; i < 64; i++) {
        int r = mix(t->bg[0], t->panel[0], i * 100 / 63);
        int g = mix(t->bg[1], t->panel[1], i * 100 / 63);
        int b = mix(t->bg[2], t->panel[2], i * 100 / 63);
        c->d->fill(0, i * H / 64, W, H / 64 + 1, r, g, b);
    }
    /* A muted accent wash in the top-right, matching the browser's
     * wallpaper glow without alpha fills. */
    for (int i = 0; i < 5; i++) {
        int inset = i * 24;
        int w = W / 3 - inset;
        if (w <= 0) break;
        int pct = 5 + i * 2;
        c->d->fill(W - W / 3 + inset / 2, 60 + inset / 2, w, 4,
                   mix(t->bg[0], t->accent[0], pct),
                   mix(t->bg[1], t->accent[1], pct),
                   mix(t->bg[2], t->accent[2], pct));
    }
}

static void bar(struct ctx *c, int W, const struct om_theme *t)
{
    c->d->fill(0, 0, W, BAR_H, t->panel[0], t->panel[1], t->panel[2]);
    c->d->fill(0, BAR_H - 1, W, 1, mix(t->panel[0], t->fg[0], 18),
               mix(t->panel[1], t->fg[1], 18), mix(t->panel[2], t->fg[2], 18));

    int logo_sel = c->zone == ZONE_BAR && c->bar_focus == 0;
    int lx = 32, ly = 14;
    if (logo_sel) c->d->fill(lx - 6, ly - 6, 36, 36,
                             t->accent[0], t->accent[1], t->accent[2]);
    frame(c, lx, ly, 24, 24, 2, t->bright[0], t->bright[1], t->bright[2]);
    c->d->fill(lx + 6, ly + 6, 12, 12, t->accent[0], t->accent[1], t->accent[2]);

    for (int i = 0; i < 5; i++) {
        int x = 96 + i * 46;
        int active = c->screen == i;
        int focused = c->zone == ZONE_BAR && c->bar_focus == i + 1;
        if (focused)
            c->d->fill(x - 6, 9, 46, 34, t->panel2[0], t->panel2[1], t->panel2[2]);
        if (active)
            c->d->fill(x, BAR_H - 4, 34, 3,
                       t->accent[0], t->accent[1], t->accent[2]);
        char num[4];
        snprintf(num, sizeof num, "%d", i + 1);
        text_center(c, x + 17, 17, body_font(c), num,
                    active ? t->bright[0] : t->fg[0],
                    active ? t->bright[1] : t->fg[1],
                    active ? t->bright[2] : t->fg[2], 40);
    }

    char clock[64] = "--:--";
    time_t now = time(NULL);
    struct tm tm;
    if (now != (time_t)-1 && localtime_r(&now, &tm)) {
        if (strftime(clock, sizeof clock, "%a %d %b  %I:%M %p", &tm) == 0)
            snprintf(clock, sizeof clock, "--:--");
        for (char *p = clock; *p && p < clock + 10; p++)
            if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    }
    text_center(c, W / 2, 17, body_font(c), clock,
                t->fg[0], t->fg[1], t->fg[2], W / 3);

    text_at(c, W - 280, 17, body_font(c), "NET", t->dim[0], t->dim[1], t->dim[2], 60);
    char vol[32];
    int shown_volume = current_volume(c);
    if (shown_volume >= 0) snprintf(vol, sizeof vol, "VOL %d", shown_volume);
    else snprintf(vol, sizeof vol, "VOL --");
    text_at(c, W - 195, 17, body_font(c), vol, t->dim[0], t->dim[1], t->dim[2], 90);
    text_at(c, W - 78, 17, body_font(c), "IN", t->dim[0], t->dim[1], t->dim[2], 40);
}

static void status_box(struct ctx *c, int x, int y, int w, int h,
                       const struct om_theme *t, const char *legend,
                       const char *l1, const char *l2, const char *l3)
{
    c->d->fill(x, y, w, h, t->panel[0], t->panel[1], t->panel[2]);
    frame(c, x, y, w, h, 1, mix(t->panel[0], t->fg[0], 14),
          mix(t->panel[1], t->fg[1], 14), mix(t->panel[2], t->fg[2], 14));
    c->d->fill(x, y, 4, h, t->accent[0], t->accent[1], t->accent[2]);
    text_at(c, x + 22, y + 16, body_font(c), legend,
            t->accent[0], t->accent[1], t->accent[2], w - 40);
    c->d->fill(x + 22, y + 52, w - 44, 1,
               mix(t->panel[0], t->fg[0], 12),
               mix(t->panel[1], t->fg[1], 12),
               mix(t->panel[2], t->fg[2], 12));
    const char *lines[3] = { l1, l2, l3 };
    for (int i = 0; i < 3; i++)
        if (lines[i] && lines[i][0])
            text_at(c, x + 22, y + 72 + i * 44, body_font(c), lines[i],
                    i ? t->dim[0] : t->fg[0], i ? t->dim[1] : t->fg[1],
                    i ? t->dim[2] : t->fg[2], w - 44);
}

static void pane_box(struct ctx *c, int x, int y, int w, int h,
                     const struct om_theme *t, const oa_lua_pane *p)
{
    c->d->fill(x, y, w, h, t->panel[0], t->panel[1], t->panel[2]);
    frame(c, x, y, w, h, 1, mix(t->panel[0], t->fg[0], 14),
          mix(t->panel[1], t->fg[1], 14), mix(t->panel[2], t->fg[2], 14));
    c->d->fill(x, y, 4, h, t->accent[0], t->accent[1], t->accent[2]);
    text_at(c, x + 22, y + 16, body_font(c), p->title,
            t->accent[0], t->accent[1], t->accent[2], w - 40);
    c->d->fill(x + 22, y + 50, w - 44, 1,
               mix(t->panel[0], t->fg[0], 12),
               mix(t->panel[1], t->fg[1], 12),
               mix(t->panel[2], t->fg[2], 12));
    int ly = y + 60;
    for (int i = 0; i < p->line_count; i++) {
        const oa_lua_pane_line *ln = &p->lines[i];
        int cr = t->fg[0], cg = t->fg[1], cb = t->fg[2];
        if (ln->tone == 1) {
            cr = t->green[0]; cg = t->green[1]; cb = t->green[2];
        } else if (ln->tone == 2) {
            cr = t->red[0]; cg = t->red[1]; cb = t->red[2];
        }
        if (ly + 26 > y + h - 8) break;
        if (ln->label[0]) {
            text_at(c, x + 22, ly, small_font(c), ln->label,
                    t->dim[0], t->dim[1], t->dim[2], 130);
            text_at(c, x + 160, ly, small_font(c), ln->value,
                    cr, cg, cb, w - 182);
        } else {
            text_at(c, x + 22, ly, small_font(c), ln->value,
                    cr, cg, cb, w - 44);
        }
        ly += 30;
    }
}

static void dock_tile(struct ctx *c, int x, int y, int w, int h,
                      const struct om_theme *t, int i)
{
    int selected = c->focus == i;
    const struct dock_entry *e = &c->dock[i];
    c->d->fill(x, y, w, h, t->panel[0], t->panel[1], t->panel[2]);
    frame(c, x, y, w, h, 1, mix(t->panel[0], t->fg[0], 14),
          mix(t->panel[1], t->fg[1], 14), mix(t->panel[2], t->fg[2], 14));
    c->d->fill(x, y, w, 4, t->accent[0], t->accent[1], t->accent[2]);
    c->d->fill(x + 24, y + 34, 54, 54, t->panel2[0], t->panel2[1], t->panel2[2]);
    c->d->fill(x + 40, y + 50, 22, 22, t->accent[0], t->accent[1], t->accent[2]);
    text_at(c, x + 24, y + 110, body_font(c), e->name,
            t->bright[0], t->bright[1], t->bright[2], w - 48);
    text_at(c, x + 24, y + 158, body_font(c), e->sub,
            t->dim[0], t->dim[1], t->dim[2], w - 48);
    if (selected)
        frame(c, x - 4, y - 4, w + 8, h + 8, 4,
              t->accent[0], t->accent[1], t->accent[2]);
}

static void paint_home(struct ctx *c, int W, int H, const struct om_theme *t)
{
    dock_build(c);
    char source[64];
    current_source_name(c, source, sizeof source);
    char vol[32];
    int shown_volume = current_volume(c);
    if (shown_volume >= 0) snprintf(vol, sizeof vol, "%d%%", shown_volume);
    else snprintf(vol, sizeof vol, "--");

    int top = BAR_H + 34;
    int term_w = 520;
    c->d->fill(40, top, term_w, 336, t->panel[0], t->panel[1], t->panel[2]);
    frame(c, 40, top, term_w, 336, 1, mix(t->panel[0], t->fg[0], 14),
          mix(t->panel[1], t->fg[1], 14), mix(t->panel[2], t->fg[2], 14));
    c->d->fill(40, top, 4, 336, t->accent[0], t->accent[1], t->accent[2]);
    text_at(c, 70, top + 20, head_font(c), "OMARCHY TV",
            t->accent[0], t->accent[1], t->accent[2], term_w - 60);
    text_at(c, 70, top + 74, body_font(c), "user@omarchy",
            t->fg[0], t->fg[1], t->fg[2], term_w - 60);
    c->d->fill(70, top + 112, term_w - 60, 1,
               mix(t->panel[0], t->fg[0], 14),
               mix(t->panel[1], t->fg[1], 14),
               mix(t->panel[2], t->fg[2], 14));
    char line[96];
    snprintf(line, sizeof line, "host     TELEFUNKEN-TV");
    text_at(c, 70, top + 132, body_font(c), line, t->fg[0], t->fg[1], t->fg[2],
            term_w - 60);
    snprintf(line, sizeof line, "shell    openaurora");
    text_at(c, 70, top + 176, body_font(c), line, t->fg[0], t->fg[1], t->fg[2],
            term_w - 60);
    snprintf(line, sizeof line, "source   %s", source);
    text_at(c, 70, top + 220, body_font(c), line, t->fg[0], t->fg[1], t->fg[2],
            term_w - 60);
    snprintf(line, sizeof line, "volume   %s", vol);
    text_at(c, 70, top + 264, body_font(c), line, t->fg[0], t->fg[1], t->fg[2],
            term_w - 60);

    int wx = 600, ww = W - wx - 40;
    int bw = (ww - 40) / 3;
    int pane_n = c->lua && c->lua->pane_count ? c->lua->pane_count() : 0;
    if (pane_n > 3) pane_n = 3;
    for (int i = 0; i < 3; i++) {
        int bx = wx + i * (bw + 20);
        oa_lua_pane p;
        if (i < pane_n && c->lua->pane_at && c->lua->pane_at(i, &p) == 0) {
            pane_box(c, bx, top, bw, 336, t, &p);
        } else if (i == 0) {
            status_box(c, bx, top, bw, 336, t, "system",
                       "native ui.engine", "no telemetry", "no advertising");
        } else if (i == 1) {
            status_box(c, bx, top, bw, 336, t, "audio",
                       vol, c->mute == 1 ? "mute on" : "mute off",
                       "optical / SPDIF");
        } else {
            status_box(c, bx, top, bw, 336, t, "inputs",
                       source, "VIN path", "native switch pending");
        }
    }

    int wy = 700;
    int wh = H - wy - 40;
    c->d->fill(40, wy, W - 80, wh, t->panel[0], t->panel[1], t->panel[2]);
    frame(c, 40, wy, W - 80, wh, 1, mix(t->panel[0], t->fg[0], 14),
          mix(t->panel[1], t->fg[1], 14), mix(t->panel[2], t->fg[2], 14));
    c->d->fill(40, wy, W - 80, 3, t->accent[0], t->accent[1], t->accent[2]);
    const struct dock_entry *cur = &c->dock[c->focus < c->dock_n ? c->focus : 0];
    text_at(c, 70, wy + 18, head_font(c), cur->name,
            t->bright[0], t->bright[1], t->bright[2], 600);
    text_at(c, 70, wy + 62, body_font(c),
            cur->kind == DOCK_TV ? "OK watch TV  ·  UP bar  ·  1-5 workspaces"
                                 : "OK open  ·  UP bar  ·  1-5 workspaces",
            t->dim[0], t->dim[1], t->dim[2], W - 140);

    int tw = (W - 80 - 100 - 5 * 20) / 6;
    for (int i = 0; i < c->dock_n; i++)
        dock_tile(c, 60 + i * (tw + 20), wy + 104, tw, wh - 144, t, i);
}

static void paint_sources(struct ctx *c, int W, int H, const struct om_theme *t)
{
    c->d->fill(40, BAR_H + 34, W - 80, H - BAR_H - 74,
               t->panel[0], t->panel[1], t->panel[2]);
    frame(c, 40, BAR_H + 34, W - 80, H - BAR_H - 74, 1,
          mix(t->panel[0], t->fg[0], 14), mix(t->panel[1], t->fg[1], 14),
          mix(t->panel[2], t->fg[2], 14));
    c->d->fill(40, BAR_H + 34, W - 80, 3,
               t->accent[0], t->accent[1], t->accent[2]);
    text_at(c, 70, BAR_H + 54, head_font(c), "Inputs",
            t->bright[0], t->bright[1], t->bright[2], 400);
    text_at(c, 70, BAR_H + 104, body_font(c),
            "OK switch input  ·  UP bar  ·  1-5 workspaces",
            t->dim[0], t->dim[1], t->dim[2], W - 140);

    const oa_source_iface *src = c->api->lookup(c->api->priv, OA_SVC_SOURCE, 1);
    int current = src ? src->current_type() : -1;
    int tw = (W - 140 - 3 * 20) / 4;
    int th = 260;
    for (int i = 0; i < SOURCE_ROWS; i++) {
        int col = i % SOURCE_COLS;
        int row = i / SOURCE_COLS;
        int x = 70 + col * (tw + 20);
        int y = BAR_H + 150 + row * (th + 20);
        int selected = c->source_focus == i;
        c->d->fill(x, y, tw, th, t->panel[0], t->panel[1], t->panel[2]);
        frame(c, x, y, tw, th, 1, mix(t->panel[0], t->fg[0], 14),
              mix(t->panel[1], t->fg[1], 14), mix(t->panel[2], t->fg[2], 14));
        if (SOURCE_TYPES[i] == current)
            c->d->fill(x, y, tw, 3, t->green[0], t->green[1], t->green[2]);
        c->d->fill(x + 24, y + 28, 48, 48, t->panel2[0], t->panel2[1],
                   t->panel2[2]);
        c->d->fill(x + 38, y + 44, 20, 16, t->accent[0], t->accent[1],
                   t->accent[2]);
        text_at(c, x + 24, y + 100, body_font(c), SOURCE_NAMES[i],
                t->bright[0], t->bright[1], t->bright[2], tw - 48);
        text_at(c, x + 24, y + 152, body_font(c),
                SOURCE_TYPES[i] < 0 ? "no VIN slot" : "VIN input",
                t->dim[0], t->dim[1], t->dim[2], tw - 48);
        if (selected)
            frame(c, x - 4, y - 4, tw + 8, th + 8, 4,
                  t->accent[0], t->accent[1], t->accent[2]);
    }
    if (src) c->api->release(c->api->priv, OA_SVC_SOURCE);
}

static void paint_apps(struct ctx *c, int W, int H, const struct om_theme *t)
{
    c->d->fill(40, BAR_H + 34, W - 80, H - BAR_H - 74,
               t->panel[0], t->panel[1], t->panel[2]);
    frame(c, 40, BAR_H + 34, W - 80, H - BAR_H - 74, 1,
          mix(t->panel[0], t->fg[0], 14), mix(t->panel[1], t->fg[1], 14),
          mix(t->panel[2], t->fg[2], 14));
    c->d->fill(40, BAR_H + 34, W - 80, 3,
               t->accent[0], t->accent[1], t->accent[2]);
    char title[64];
    snprintf(title, sizeof title, "Apps  %d", c->app_total);
    text_at(c, 70, BAR_H + 54, head_font(c), title,
            t->bright[0], t->bright[1], t->bright[2], 400);
    text_at(c, 70, BAR_H + 104, body_font(c),
            "OK open  ·  UP bar  ·  1-5 workspaces",
            t->dim[0], t->dim[1], t->dim[2], W - 140);
    if (!c->app_n) {
        text_at(c, 70, BAR_H + 170, body_font(c),
                "launcher.db unavailable or empty",
                t->dim[0], t->dim[1], t->dim[2], W - 140);
        return;
    }
    int tw = (W - 140 - 6 * 20) / 7;
    int th = 200;
    for (int i = 0; i < c->app_n && i < APP_ROWS; i++) {
        int col = i % APP_COLS;
        int row = i / APP_COLS;
        int x = 70 + col * (tw + 20);
        int y = BAR_H + 150 + row * (th + 20);
        int selected = c->app_focus == i;
        c->d->fill(x, y, tw, th, t->panel[0], t->panel[1], t->panel[2]);
        frame(c, x, y, tw, th, 1, mix(t->panel[0], t->fg[0], 14),
              mix(t->panel[1], t->fg[1], 14), mix(t->panel[2], t->fg[2], 14));
        c->d->fill(x, y, tw, 3, t->accent2[0], t->accent2[1], t->accent2[2]);
        c->d->fill(x + 24, y + 28, 64, 64, t->panel2[0], t->panel2[1],
                   t->panel2[2]);
        c->d->fill(x + 40, y + 44, 32, 32, t->accent[0], t->accent[1],
                   t->accent[2]);
        text_at(c, x + 24, y + 108, body_font(c), c->apps[i],
                t->bright[0], t->bright[1], t->bright[2], tw - 48);
        if (selected)
            frame(c, x - 4, y - 4, tw + 8, th + 8, 4,
                  t->accent[0], t->accent[1], t->accent[2]);
    }
}

static void settings_row(struct ctx *c, int W, int y, int i, const
                         struct om_theme *t, const char *label,
                         const char *value)
{
    int selected = c->settings_focus == i;
    c->d->fill(70, y, W - 140, 76, t->panel[0], t->panel[1], t->panel[2]);
    frame(c, 70, y, W - 140, 76, 1, mix(t->panel[0], t->fg[0], 14),
          mix(t->panel[1], t->fg[1], 14), mix(t->panel[2], t->fg[2], 14));
    if (selected)
        c->d->fill(70, y, 4, 76, t->accent[0], t->accent[1], t->accent[2]);
    text_at(c, 96, y + 22, body_font(c), label,
            selected ? t->bright[0] : t->fg[0],
            selected ? t->bright[1] : t->fg[1],
            selected ? t->bright[2] : t->fg[2], 500);
    text_at(c, 700, y + 22, body_font(c), value,
            t->dim[0], t->dim[1], t->dim[2], W - 820);
    if (selected)
        frame(c, 66, y - 3, W - 132, 82, 3,
              t->accent[0], t->accent[1], t->accent[2]);
}

static void paint_settings(struct ctx *c, int W, int H, const struct om_theme *t)
{
    c->d->fill(40, BAR_H + 34, W - 80, H - BAR_H - 74,
               t->panel[0], t->panel[1], t->panel[2]);
    frame(c, 40, BAR_H + 34, W - 80, H - BAR_H - 74, 1,
          mix(t->panel[0], t->fg[0], 14), mix(t->panel[1], t->fg[1], 14),
          mix(t->panel[2], t->fg[2], 14));
    c->d->fill(40, BAR_H + 34, W - 80, 3,
               t->accent[0], t->accent[1], t->accent[2]);
    text_at(c, 70, BAR_H + 54, head_font(c), "System",
            t->bright[0], t->bright[1], t->bright[2], 400);
    text_at(c, 70, BAR_H + 104, body_font(c),
            "OK change  ·  UP bar  ·  1-5 workspaces",
            t->dim[0], t->dim[1], t->dim[2], W - 140);

    const oa_settings_iface *set =
        c->api->lookup(c->api->priv, OA_SVC_SETTINGS, 1);
    int cec = -1;
    if (set) {
        (void)set->get_int("m_cec_enabled", &cec);
        c->api->release(c->api->priv, OA_SVC_SETTINGS);
    }
    const oa_source_iface *src =
        c->api->lookup(c->api->priv, OA_SVC_SOURCE, 1);
    int vin = src ? src->is_available() : 0;
    if (src) c->api->release(c->api->priv, OA_SVC_SOURCE);

    char theme_line[64];
    snprintf(theme_line, sizeof theme_line, "%s", THEMES[c->theme].name);
    char cec_line[32];
    snprintf(cec_line, sizeof cec_line, "%s",
             cec < 0 ? "unknown" : cec ? "on" : "off");
    int y = BAR_H + 160;
    settings_row(c, W, y, 0, t, "Theme", theme_line);
    settings_row(c, W, y + 96, 1, t, "CEC", cec_line);
    settings_row(c, W, y + 192, 2, t, "Source switching",
                 vin ? "VIN open; firmware path pending" : "VIN unavailable");
    settings_row(c, W, y + 288, 3, t, "About", "native ui.engine shell");
}

static void paint_about(struct ctx *c, int W, int H, const struct om_theme *t)
{
    c->d->fill(40, BAR_H + 34, W - 80, H - BAR_H - 74,
               t->panel[0], t->panel[1], t->panel[2]);
    frame(c, 40, BAR_H + 34, W - 80, H - BAR_H - 74, 1,
          mix(t->panel[0], t->fg[0], 14), mix(t->panel[1], t->fg[1], 14),
          mix(t->panel[2], t->fg[2], 14));
    c->d->fill(40, BAR_H + 34, W - 80, 3,
               t->accent[0], t->accent[1], t->accent[2]);
    text_at(c, 70, BAR_H + 54, head_font(c), "OmarchyTV",
            t->bright[0], t->bright[1], t->bright[2], 400);
    const char *lines[] = {
        "Native replacement shell over ui.engine.",
        "Drawn with the native canvas; no browser or webview.",
        "No vendor skin assets, telemetry or advertising.",
        "Workspace shell modelled on the browser OmarchyTV app.",
    };
    for (int i = 0; i < 4; i++)
        text_at(c, 70, BAR_H + 140 + i * 52, body_font(c), lines[i],
                t->fg[0], t->fg[1], t->fg[2], W - 140);
}

static void volume_hide(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    c->volume_timer = 0;
    c->volume_show = 0;
    if (c->active && !c->suspended) paint(c);
}

static void volume_show(struct ctx *c, int volume, int mute)
{
    c->volume_show = 1;
    c->volume_value = volume < 0 ? 0 : volume > 100 ? 100 : volume;
    c->overlay_mute = mute;
    if (c->volume_timer) {
        c->api->timer_del(c->api->priv, c->volume_timer);
        c->volume_timer = 0;
    }
    c->volume_timer = c->api->timer_add(c->api->priv, 1500, 0, volume_hide, c);
    paint(c);
}

static void paint_volume(struct ctx *c, int W, int H, int ar, int ag, int ab)
{
    int w = 360, h = 96;
    if (w > W - 80) w = W - 80;
    if (h > H - 80) h = H - 80;
    int x = W - w - 40;
    int y = H - h - 40;
    c->d->fill(x, y, w, h, 18, 21, 28);
    c->d->fill(x + 16, y + h - 24, w - 32, 8, 40, 45, 55);
    int fillw = (w - 32) * c->volume_value / 100;
    if (fillw > 0)
        c->d->fill(x + 16, y + h - 24, fillw, 8, ar, ag, ab);
    char line[64];
    if (c->overlay_mute)
        snprintf(line, sizeof line, "Mute");
    else
        snprintf(line, sizeof line, "Volume %d%%", c->volume_value);
    text_at(c, x + 16, y + 16, c->font_on ? &c->font : NULL, line,
            235, 238, 242, w - 32);
}

/* Volume keys are global to the shell: code 16 up, 17 down, 13 mute. The
 * audio service can refuse (disabled/CEC); consume the key either way so it
 * does not move the current screen selection. */
static int volume_key(struct ctx *c, int id)
{
    const oa_audio_iface *a = c->api->lookup(c->api->priv, OA_SVC_AUDIO, 1);
    if (!a) return 0;
    int rc = 0;
    if (id == 13) {
        int on = c->mute == 1 ? 0 : 1;
        rc = a->set_mute(on);
        if (rc == 0) {
            c->mute = on;
            volume_show(c, c->volume, on);
        }
    } else {
        int v = c->volume;
        if (v < 0 && a->get_volume) v = a->get_volume();
        if (v < 0) v = 50;
        v += id == 16 ? 2 : -2;
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        rc = a->set_volume(v);
        if (rc == 0) {
            c->volume = v;
            c->mute = 0;
            volume_show(c, v, 0);
        }
    }
    c->api->release(c->api->priv, OA_SVC_AUDIO);
    if (rc != 0)
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "volume/mute key %d failed (%d)", id, rc);
    return 0;
}

static void paint(struct ctx *c)
{
    const oa_ui_engine_iface *d = c ? c->d : NULL;
    int W = 0, H = 0;
    int br, bg, bb, ba, ar, ag, ab, lx, ly, lw, lh;
    if (!c || !d || !c->active || !c->state || c->suspended)
        return;
    if (d->info(&W, &H, NULL, NULL) != 0 || W <= 0 || H <= 0)
        return;
    oa_omarchy_state *st = c->state;
    st->screen[0] = W;
    st->screen[1] = H;
    st->have_screen = 1;

    const struct om_theme *base = theme_of(c);
    struct om_theme t = *base;
    br = t.bg[0];
    bg = t.bg[1];
    bb = t.bg[2];
    ba = 255;
    ar = t.accent[0];
    ag = t.accent[1];
    ab = t.accent[2];
    lx = ly = 0;
    lw = lh = 0;
    int have_lua = 0;
    if (c->lua) {
        take_lua_paint(c, &br, &bg, &bb, &ba, &ar, &ag, &ab,
                       &lx, &ly, &lw, &lh);
        have_lua = st->have_colors || st->have_layout;
    }
    if (st->have_colors) {
        t.bg[0] = br; t.bg[1] = bg; t.bg[2] = bb;
        t.accent[0] = ar; t.accent[1] = ag; t.accent[2] = ab;
        t.panel[0] = mix(br, 255, 6);
        t.panel[1] = mix(bg, 255, 6);
        t.panel[2] = mix(bb, 255, 6);
        t.panel2[0] = mix(br, 255, 12);
        t.panel2[1] = mix(bg, 255, 12);
        t.panel2[2] = mix(bb, 255, 12);
    }

    d->clear_rgba(br, bg, bb, ba);
    if (have_lua && st->have_layout)
        d->fill(lx, ly, lw, lh, ar, ag, ab);
    background(c, W, H, &t);
    bar(c, W, &t);
    switch (c->screen) {
    case SCREEN_SOURCES: paint_sources(c, W, H, &t); break;
    case SCREEN_APPS: paint_apps(c, W, H, &t); break;
    case SCREEN_SETTINGS: paint_settings(c, W, H, &t); break;
    case SCREEN_ABOUT: paint_about(c, W, H, &t); break;
    default: paint_home(c, W, H, &t); break;
    }
    if (c->volume_show)
        paint_volume(c, W, H, t.accent[0], t.accent[1], t.accent[2]);
    (void)d->present();
}

static int enter(void *ud)
{
    struct ctx *c = ud;
    if (!c || !c->active) return 0;
    if (c->lua)
        (void)c->lua->call("enter", "omarchy", 0, NULL);
    paint(ud);
    return 0;
}

static void leave(void *ud)
{
    struct ctx *c = ud;
    if (!c || !c->active) return;
    if (c->lua)
        (void)c->lua->call("leave", "omarchy", 0, NULL);
}

static void skin_paint(void *ud)
{
    paint(ud);
}

static void emit_action(struct ctx *c, uint8_t kind, int index, const char *name)
{
    oa_action_event act;
    memset(&act, 0, sizeof act);
    act.kind = kind;
    act.index = (uint8_t)index;
    act.name = name;
    c->api->log(c->api->priv, OA_LOG_INFO, "action kind=%u index=%d name=%s",
                (unsigned)kind, index, name ? name : "-");
    c->api->emit(c->api->priv, OA_TOPIC_ACTION, &act);
}

static void go_content(struct ctx *c)
{
    c->zone = ZONE_CONTENT;
    paint(c);
}

static int activate_dock(struct ctx *c)
{
    if (c->focus < 0 || c->focus >= c->dock_n) return 0;
    const struct dock_entry *e = &c->dock[c->focus];
    switch (e->kind) {
    case DOCK_TV:
        emit_action(c, OA_ACTION_BACK, c->screen, NULL);
        return 0;
    case DOCK_APP:
        emit_action(c, OA_ACTION_SELECT, e->app, e->name);
        return 0;
    case DOCK_SOURCES: c->screen = SCREEN_SOURCES; break;
    case DOCK_APPS: c->screen = SCREEN_APPS; apps_load(c); break;
    case DOCK_SETTINGS: c->screen = SCREEN_SETTINGS; break;
    default: c->screen = SCREEN_ABOUT; break;
    }
    paint(c);
    return 0;
}

static int key_bar(struct ctx *c, int id)
{
    if (id == 21) {
        if (c->bar_focus > 0) { c->bar_focus--; paint(c); }
        return 0;
    }
    if (id == 22) {
        if (c->bar_focus < 5) { c->bar_focus++; paint(c); }
        return 0;
    }
    if (id == 10 || id == 37 || id == 48) {
        go_content(c);
        return 0;
    }
    if (id == 19 || id == 53 || id == 0x8a) {
        c->screen = c->bar_focus == 0 ? SCREEN_HOME : c->bar_focus - 1;
        if (c->screen == SCREEN_APPS) apps_load(c);
        c->zone = ZONE_CONTENT;
        paint(c);
        return 0;
    }
    return 0;
}

static int key_home(struct ctx *c, int id)
{
    if (id == 20) {                 /* Up (ICE 0x20a) */
        c->zone = ZONE_BAR;
        c->bar_focus = c->screen + 1;
        paint(c);
        return 0;
    }
    if (id == 21) {                 /* Left */
        if (c->focus > 0) { c->focus--; paint(c); }
        return 0;
    }
    if (id == 22) {                 /* Right */
        if (c->focus + 1 < c->dock_n) { c->focus++; paint(c); }
        return 0;
    }
    if (id == 53 || id == 0x8a) {   /* Select */
        return activate_dock(c);
    }
    if (id == 10) {                 /* Exit: leave the launcher */
        emit_action(c, OA_ACTION_BACK, c->screen, NULL);
        return 0;
    }
    return 0;
}

static int key_sources(struct ctx *c, int id)
{
    if (id == 10 || id == 37 || id == 48) {
        c->screen = SCREEN_HOME;
        c->zone = ZONE_CONTENT;
        paint(c);
        return 0;
    }
    if (id == 20 || id == 19 || id == 21 || id == 22) {
        int col = c->source_focus % SOURCE_COLS;
        int row = c->source_focus / SOURCE_COLS;
        if (id == 20 && row > 0) row--;
        else if (id == 19 && row < (SOURCE_ROWS - 1) / SOURCE_COLS) row++;
        else if (id == 21 && col > 0) col--;
        else if (id == 22 && col < SOURCE_COLS - 1) col++;
        else if (id == 20 && c->zone == ZONE_CONTENT) {
            c->zone = ZONE_BAR;
            c->bar_focus = c->screen + 1;
            paint(c);
            return 0;
        }
        int n = row * SOURCE_COLS + col;
        if (n >= 0 && n < SOURCE_ROWS && n != c->source_focus) {
            c->source_focus = n;
            paint(c);
        }
        return 0;
    }
    if ((id == 53 || id == 0x8a) && c->source_focus < SOURCE_ROWS) {
        int type = SOURCE_TYPES[c->source_focus];
        if (type >= 0) {
            const oa_source_iface *src =
                c->api->lookup(c->api->priv, OA_SVC_SOURCE, 1);
            if (!src) {
                c->api->log(c->api->priv, OA_LOG_WARN,
                            "source: no VIN service");
                return 0;
            }
            int rc = src->select_type(type);
            c->api->release(c->api->priv, OA_SVC_SOURCE);
            if (rc == 0) {
                emit_action(c, OA_ACTION_SOURCE, type,
                            SOURCE_NAMES[c->source_focus]);
                paint(c);
            } else {
                c->api->log(c->api->priv, OA_LOG_INFO,
                            "source: type %d refused (%d)", type, rc);
            }
        }
        return 0;
    }
    return 0;
}

static int key_apps(struct ctx *c, int id)
{
    if (id == 10 || id == 37 || id == 48) {
        c->screen = SCREEN_HOME;
        c->zone = ZONE_CONTENT;
        paint(c);
        return 0;
    }
    if (id == 20 || id == 19 || id == 21 || id == 22) {
        int n = c->app_focus;
        int col = n % APP_COLS;
        int row = n / APP_COLS;
        if (id == 21 && col > 0) n--;
        else if (id == 22 && col < APP_COLS - 1 && n + 1 < c->app_n) n++;
        else if (id == 20 && row > 0) n -= APP_COLS;
        else if (id == 19 && n + APP_COLS < c->app_n) n += APP_COLS;
        else if (id == 20) {
            c->zone = ZONE_BAR;
            c->bar_focus = c->screen + 1;
            paint(c);
            return 0;
        }
        if (n < 0) n = 0;
        if (n >= c->app_n) n = c->app_n ? c->app_n - 1 : 0;
        if (n != c->app_focus) {
            c->app_focus = n;
            paint(c);
        }
        return 0;
    }
    if ((id == 53 || id == 0x8a) && c->app_focus >= 0 &&
        c->app_focus < c->app_n) {
        emit_action(c, OA_ACTION_SELECT, c->app_focus, c->apps[c->app_focus]);
        return 0;
    }
    return 0;
}

static int key_settings(struct ctx *c, int id)
{
    if (id == 10 || id == 37 || id == 48) {
        c->screen = SCREEN_HOME;
        c->zone = ZONE_CONTENT;
        paint(c);
        return 0;
    }
    if (id == 20 || id == 19) {
        if (id == 20 && c->settings_focus == 0) {
            c->zone = ZONE_BAR;
            c->bar_focus = c->screen + 1;
            paint(c);
            return 0;
        }
        int n = c->settings_focus + (id == 20 ? -1 : 1);
        if (n < 0) n = 0;
        if (n >= SETTINGS_ROWS) n = SETTINGS_ROWS - 1;
        if (n != c->settings_focus) {
            c->settings_focus = n;
            paint(c);
        }
        return 0;
    }
    if (id == 21 || id == 22 || id == 53 || id == 0x8a) {
        if (c->settings_focus == 0) {
            if (id == 21) c->theme = (c->theme + THEME_COUNT - 1) % THEME_COUNT;
            else c->theme = (c->theme + 1) % THEME_COUNT;
            paint(c);
        } else if (c->settings_focus == 1 && (id == 53 || id == 0x8a)) {
            const oa_settings_iface *set =
                c->api->lookup(c->api->priv, OA_SVC_SETTINGS, 1);
            if (set) {
                int cec = 0;
                if (set->get_int("m_cec_enabled", &cec) == 0)
                    (void)set->set_int("m_cec_enabled", cec ? 0 : 1);
                c->api->release(c->api->priv, OA_SVC_SETTINGS);
                paint(c);
            }
        } else if (c->settings_focus == 3 && (id == 53 || id == 0x8a)) {
            c->screen = SCREEN_ABOUT;
            paint(c);
        }
        return 0;
    }
    return 0;
}

static int native_key(struct ctx *c, int id)
{
    /* Digits are the browser workspace shortcuts (RC codes 1..5). */
    if (id >= 1 && id <= 5) {
        c->screen = id - 1;
        c->zone = ZONE_CONTENT;
        if (c->screen == SCREEN_APPS) apps_load(c);
        paint(c);
        return 0;
    }
    if (id == 32 || id == 33) {     /* Channel up/down: previous/next workspace */
        int n = c->screen + (id == 32 ? 1 : -1);
        if (n < 0) n = SCREEN_ABOUT;
        if (n > SCREEN_ABOUT) n = SCREEN_HOME;
        c->screen = n;
        c->zone = ZONE_CONTENT;
        if (n == SCREEN_APPS) apps_load(c);
        paint(c);
        return 0;
    }
    if (c->zone == ZONE_BAR)
        return key_bar(c, id);
    switch (c->screen) {
    case SCREEN_SOURCES: return key_sources(c, id);
    case SCREEN_APPS: return key_apps(c, id);
    case SCREEN_SETTINGS: return key_settings(c, id);
    case SCREEN_ABOUT:
        if (id == 10 || id == 37 || id == 48) {
            c->screen = SCREEN_HOME;
            paint(c);
        }
        return 0;
    default:
        return key_home(c, id);
    }
}

static int skin_key(void *ud, int id)
{
    struct ctx *c = ud;
    int handled = 0;
    if (!c || !c->active)
        return 1;
    /* Project control key (emit-key 240): delegate the switch to the host so
     * the current skin is removed only after this callback returns. */
    if (id == 0xf0) {
        char reply[64];
        (void)c->api->control(c->api->priv, "skin diag", reply, sizeof reply);
        return 0;
    }
    if (id == 13 || id == 16 || id == 17)
        return volume_key(c, id);
    /* Scripts may change theme/key semantics first, then native navigation
     * applies the resulting colours. */
    if (c->lua && c->lua->call("key", "omarchy", id, &handled) == 0 && handled) {
        (void)native_key(c, id);
        paint(c);
        return 0;
    }
    return native_key(c, id);
}

static const oa_ui_skin g_skin = { "omarchy", enter, leave, skin_paint, skin_key };

static void rebind(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    c->rebind = 0;
    if (!c->active || c->suspended || c->d) return;
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 2);
    if (!c->d) return;
    if (c->d->register_skin_dispatch(&g_skin, c, dispatch)) {
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
        return;
    }
    if (!c->lua)
        c->lua = c->api->lookup(c->api->priv, OA_SVC_UI_LUA, OA_LUA_API_VERSION);
}
static void changed(const void *ev, void *ud)
{
    (void)ev;
    struct ctx *c = ud;
    if (!c || !c->active) return;
    if (!c->lua)
        c->lua = c->api->lookup(c->api->priv, OA_SVC_UI_LUA,
                                OA_LUA_API_VERSION);
    if (c->screen == SCREEN_APPS || c->screen == SCREEN_HOME) {
        apps_load(c);
        if (!c->suspended) paint(c);
    }
    if (!c->suspended && !c->d && !c->rebind)
        c->rebind = c->api->timer_add(c->api->priv, 0, 0, rebind, c);
}
static void removed(const void *ev, void *ud)
{
    const oa_service_removed_event *e = ev;
    struct ctx *c = ud;
    if (!c || !c->active || !e || !e->name)
        return;
    if (!strcmp(e->name, OA_SVC_UI_LUA)) {
        c->lua = NULL;
        return;
    }
    if (!strcmp(e->name, OA_SVC_LAUNCHER_DB) && c->screen == SCREEN_APPS) {
        c->app_n = 0;
        c->app_total = 0;
        if (!c->suspended) paint(c);
        return;
    }
    if (!strcmp(e->name, OA_SVC_UI_ENGINE)) {
        c->d = NULL;
        changed(NULL, c);
    }
}

static oa_omarchy_state *state_new(void)
{
    oa_omarchy_state *st = calloc(1, sizeof *st);
    if (st) {
        st->magic = OA_OMARCHY_STATE_MAGIC;
        st->version = OA_OMARCHY_STATE_VERSION;
    }
    return st;
}

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->state = state_new();
    if (!c->state) { free(c); return -1; }
    c->api = api;
    c->volume = -1;
    c->mute = -1;
    *out = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->active || c->cleanup_task || c->cleanup_state) return -EBUSY;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "ui_omarchy.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    if (!c->state && !(c->state = state_new())) return -ENOMEM;
    if (c->have_saved) *c->state = c->saved;
    const char *tn = c->api->config_get(c->api->priv,
                                        "ui.omarchy.theme", "last-horizon");
    c->theme = 0;
    for (int i = 0; i < THEME_COUNT; i++)
        if (!strcmp(THEMES[i].name, tn)) { c->theme = i; break; }
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 2);
    if (!c->d)
        return -1;
    c->active = 1;
    if (c->d->register_skin_dispatch(&g_skin, c, dispatch) != 0) {
        c->active = 0;
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
        return -1;
    }
    c->removed = c->api->subscribe(c->api->priv, OA_TOPIC_SERVICE_REMOVED, removed, c);
    c->changed = c->api->subscribe(c->api->priv, OA_TOPIC_MODULE_CHANGED, changed, c);
    if (!c->removed || !c->changed) return -1;
    c->lua = c->api->lookup(c->api->priv, OA_SVC_UI_LUA, OA_LUA_API_VERSION);
    if (atoi(c->api->config_get(c->api->priv, "ui.omarchy.lua", "0")) == 1) {
        oa_lua_status st;
        if (!c->lua || c->lua->status(&st) != 0 || !st.loaded)
            return -1;
    }
    fonts_load(c);
    apps_load(c);
    dock_build(c);
    c->clock_timer = c->api->timer_add(c->api->priv, 30000, 30000, clock_tick, c);
    if (c->api->config_get(c->api->priv, "ui.paint", "1")[0] == '1')
        paint(c);
    return 0;
}

static int cleanup_step(void *v, uint32_t *delay_ms)
{
    (void)delay_ms;
    struct ctx *c = v;
    free(c->cleanup_state);
    c->cleanup_state = NULL;
    c->cleanup_task = 0;
    return OA_SCHED_DONE;
}

static void cancel_cleanup(struct ctx *c)
{
    if (c->cleanup_task) {
        const oa_sched_iface *s =
            c->api->lookup(c->api->priv, OA_SVC_SCHED, 1);
        if (s) {
            (void)s->cancel(s->context, c->cleanup_task);
            c->api->release(c->api->priv, OA_SVC_SCHED);
        }
        c->cleanup_task = 0;
    }
    free(c->cleanup_state);
    c->cleanup_state = NULL;
}

/* ABI 3 gate: invalidate callbacks and tear down the engine/lua registrations
 * synchronously, snapshot/detach the cached skin state, then free it from a
 * scheduler slice after ui_omarchy.cleanup_delay_ms. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
    if (c->cleanup_task || c->cleanup_state) return -EAGAIN;
    c->active = 0;
    if (c->rebind) {
        c->api->timer_del(c->api->priv, c->rebind);
        c->rebind = 0;
    }
    if (c->removed) {
        c->api->unsubscribe(c->api->priv, c->removed);
        c->removed = 0;
    }
    if (c->changed) {
        c->api->unsubscribe(c->api->priv, c->changed);
        c->changed = 0;
    }
    if (c->d) {
        c->d->unregister_skin(&g_skin);
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
    }
    if (c->lua) {
        c->api->release(c->api->priv, OA_SVC_UI_LUA);
        c->lua = NULL;
    }
    if (c->clock_timer) {
        c->api->timer_del(c->api->priv, c->clock_timer);
        c->clock_timer = 0;
    }
    if (c->volume_timer) {
        c->api->timer_del(c->api->priv, c->volume_timer);
        c->volume_timer = 0;
    }
    c->volume_show = 0;
    fonts_free(c);
    c->suspended = 0;
    oa_omarchy_state *st = c->state;
    c->state = NULL;
    if (!st) return 0;
    c->saved = *st;
    c->have_saved = 1;
    if (!c->cleanup_delay_ms) {
        free(st);
        return 0;
    }
    c->cleanup_state = st;
    const oa_sched_iface *s =
        c->api->lookup(c->api->priv, OA_SVC_SCHED, 1);
    if (!s) {
        free(st);
        c->cleanup_state = NULL;
        return 0;
    }
    uint32_t id = 0;
    int rc = s->submit(s->context, cleanup_step, c, c->cleanup_delay_ms, &id);
    c->api->release(c->api->priv, OA_SVC_SCHED);
    if (rc) {
        free(st);
        c->cleanup_state = NULL;
        return -EIO;
    }
    c->cleanup_task = id;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    c->active = 0;
    if (c->removed) c->api->unsubscribe(c->api->priv, c->removed);
    if (c->changed) c->api->unsubscribe(c->api->priv, c->changed);
    c->removed = c->changed = 0;
    if (c->rebind) c->api->timer_del(c->api->priv, c->rebind);
    c->rebind = 0;
    if (c->d) {
        c->d->unregister_skin(&g_skin);
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
    }
    if (c->lua) {
        c->api->release(c->api->priv, OA_SVC_UI_LUA);
        c->lua = NULL;
    }
    if (c->clock_timer) {
        c->api->timer_del(c->api->priv, c->clock_timer);
        c->clock_timer = 0;
    }
    if (c->volume_timer) {
        c->api->timer_del(c->api->priv, c->volume_timer);
        c->volume_timer = 0;
    }
    c->volume_show = 0;
    fonts_free(c);
    c->suspended = 0;
    cancel_cleanup(c);
    free(c->state);
    c->state = NULL;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    cancel_cleanup(c);
    fonts_free(c);
    free(c->state);
    free(c);
}

static size_t m_save(void *v, void *buf, size_t cap)
{
    struct ctx *c = v;
    oa_omarchy_state *st = c->state;
    if (!st) {
        if (!c->have_saved) return 0;
        st = &c->saved;
    }
    if (buf && cap >= sizeof *st) memcpy(buf, st, sizeof *st);
    return sizeof *st;
}

static int m_restore(void *v, const void *buf, size_t len)
{
    struct ctx *c = v;
    if (!buf || len != sizeof(oa_omarchy_state)) return -1;
    oa_omarchy_state in;
    memcpy(&in, buf, sizeof in);
    if (in.magic != OA_OMARCHY_STATE_MAGIC ||
        in.version != OA_OMARCHY_STATE_VERSION)
        return -1;
    c->saved = in;
    c->have_saved = 1;
    if (!c->state && !(c->state = state_new())) return -1;
    *c->state = in;
    return 0;
}

static int m_suspend(void *v)
{
    struct ctx *c = v;
    if (!c->active) return 0;
    c->suspended = 1;
    if (c->rebind) c->api->timer_del(c->api->priv, c->rebind);
    c->rebind = 0;
    if (c->volume_timer) {
        c->api->timer_del(c->api->priv, c->volume_timer);
        c->volume_timer = 0;
    }
    c->volume_show = 0;
    if (c->d) {
        c->d->unregister_skin(&g_skin);
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
    }
    return 0;
}

static int m_resume(void *v)
{
    struct ctx *c = v;
    if (!c->active || !c->state || c->d) return -1;
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 2);
    if (!c->d)
        return -1;
    if (c->d->register_skin_dispatch(&g_skin, c, dispatch) != 0) {
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
        return -1;
    }
    c->suspended = 0;
    paint(c);
    return 0;
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "ui_omarchy",
    .version = "0.3",
    .requires = { OA_SVC_UI_ENGINE, "" },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .suspend = m_suspend,
    .resume = m_resume,
    .destroy = m_destroy,
    .save = m_save,
    .restore = m_restore,
    .quiesce = m_quiesce,
};

OA_MODULE_EXPORT(g_module)
