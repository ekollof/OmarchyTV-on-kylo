/* mod_ui_omarchy — OmarchyTV skin skeleton over ui.engine.
 *
 * This is the second skin for the engine seam: it owns its own screens and key
 * semantics, draws only through ui.engine, and does not use the vendor assets.
 * The real OmarchyTV UI replaces paint()/key() here; the vendor skin remains the
 * fallback. It deliberately publishes no service yet (no portal contract).
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

enum { SCREEN_HOME, SCREEN_SOURCES, SCREEN_APPS, SCREEN_SETTINGS, SCREEN_ABOUT };
#define HOME_TILES 4
#define APP_ROWS 8
#define SOURCE_ROWS 9

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
    int screen;
    int focus;
    int source_focus;
    int app_focus;
    int app_total;
    int app_n;
    char apps[APP_ROWS][64];
    int settings_focus;
    int font_on, title_on;
    struct oa_font_pack pack;
    struct oa_bitmap_font font, title_font;
    unsigned char *font_data, *title_data;
    /* ABI 3 quiesce: cached skin state is detached and freed by a sched slice. */
    uint32_t cleanup_task;
    oa_omarchy_state *cleanup_state;
    unsigned cleanup_delay_ms;
    oa_omarchy_state saved;
    int have_saved;
};
static const oa_ui_skin g_skin;
static void paint(struct ctx *c);

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
    c->font_data = c->title_data = NULL;
    c->font_on = c->title_on = 0;
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
    int id = atoi(c->api->config_get(c->api->priv, "ui.omarchy.font", "9"));
    int title_id = atoi(c->api->config_get(c->api->priv, "ui.omarchy.title_font", "2"));
    if (font_open(&c->pack, id, &c->font, &c->font_data)) return;
    c->font_on = 1;
    if (!font_open(&c->pack, title_id, &c->title_font, &c->title_data))
        c->title_on = 1;
}

static void text_at(struct ctx *c, int x, int y, const struct oa_bitmap_font *font,
                    const char *line, int r, int g, int b, int max_width)
{
    if (!c || !c->d || !c->font_on || !font || !font->pixels || !line || !*line)
        return;
    int advance = 0, height = 0;
    if (oa_font_text_measure(font, line, 0, &advance) || advance < 1 ||
        advance > max_width || oa_bitmap_font_height(font, &height) ||
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

static void frame(struct ctx *c, int x, int y, int w, int h, int t,
                  int r, int g, int b)
{
    c->d->fill(x, y, w, t, r, g, b);
    c->d->fill(x, y + h - t, w, t, r, g, b);
    c->d->fill(x, y, t, h, r, g, b);
    c->d->fill(x + w - t, y, t, h, r, g, b);
}

static void header(struct ctx *c, int W, int H, const char *title,
                   int ar, int ag, int ab)
{
    (void)H;
    const struct oa_bitmap_font *tf = c->title_on ? &c->title_font : &c->font;
    text_at(c, 40, 24, tf, title, 235, 238, 242, W - 300);
    char clock[16] = "--:--";
    time_t now = time(NULL);
    struct tm tm;
    if (now != (time_t)-1 && localtime_r(&now, &tm))
        (void)strftime(clock, sizeof clock, "%H:%M", &tm);
    text_at(c, W - 180, 24, c->font_on ? &c->font : tf, clock,
            180, 190, 205, 140);
    c->d->fill(40, 78, W - 80, 2, 40, 46, 58);
    c->d->fill(40, 80, 80, 3, ar, ag, ab);
}

static void footer(struct ctx *c, int W, int H, const char *line)
{
    c->d->fill(40, H - 54, W - 80, 1, 40, 46, 58);
    text_at(c, 40, H - 44, c->font_on ? &c->font : NULL, line,
            150, 160, 175, W - 80);
}

static void tile(struct ctx *c, int x, int y, int w, int h, int selected,
                 const char *label, const char *sub, int ar, int ag, int ab)
{
    c->d->fill(x, y, w, h, 24, 28, 36);
    c->d->fill(x + 20, y + 20, 44, 44, ar, ag, ab);
    c->d->fill(x + 20, y + 84, 60, 3, ar, ag, ab);
    text_at(c, x + 20, y + 96, c->font_on ? &c->font : NULL, label,
            235, 238, 242, w - 40);
    text_at(c, x + 20, y + 132, c->font_on ? &c->font : NULL, sub,
            150, 160, 175, w - 40);
    if (selected) frame(c, x, y, w, h, 4, ar, ag, ab);
}

static void apps_load(struct ctx *c)
{
    c->app_n = 0;
    c->app_total = 0;
    const oa_launcher_db_iface *db =
        c->api->lookup(c->api->priv, OA_SVC_LAUNCHER_DB, 1);
    if (!db) return;
    struct oa_launcher_list *list = NULL;
    if (db->read_all(&list) == 0 && list) {
        size_t n = db->count(list);
        c->app_total = n > 999 ? 999 : (int)n;
        for (int i = 0; i < APP_ROWS && (size_t)i < n; i++) {
            struct oa_launcher_row_view row;
            if (db->get(list, (size_t)i, &row) != 0) continue;
            const char *name = row.application_name ? row.application_name : "";
            snprintf(c->apps[c->app_n], sizeof c->apps[c->app_n], "%s", name);
            if (c->apps[c->app_n][0]) c->app_n++;
        }
        db->release(list);
    }
    c->api->release(c->api->priv, OA_SVC_LAUNCHER_DB);
}

static const char *SOURCE_NAMES[SOURCE_ROWS] = {
    "TV", "BACK AV", "HDMI 1", "HDMI 2", "HDMI 3",
    "YPbPr", "VGA / PC", "Wireless Display", "AV Share"
};
static const int SOURCE_TYPES[SOURCE_ROWS] = { 0, 15, 17, 18, 19, 20, 16, -1, -1 };

static void paint_home(struct ctx *c, int W, int H, int ar, int ag, int ab)
{
    const char *labels[HOME_TILES] = { "Sources", "Apps", "Settings", "About" };
    const char *subs[HOME_TILES] = {
        "Input and VIN", "Launcher catalog", "CEC and source", "Native shell"
    };
    for (int i = 0; i < HOME_TILES; i++) {
        int x = 40 + (i % 2) * 320;
        int y = 88 + (i / 2) * 200;
        tile(c, x, y, 300, 180, c->focus == i, labels[i], subs[i], ar, ag, ab);
    }
    int px = 700, py = 88, pw = W - 740, ph = H - 176;
    if (pw > 240) {
        c->d->fill(px, py, pw, ph, 18, 21, 28);
        text_at(c, px + 30, py + 20, c->title_on ? &c->title_font : &c->font,
                "SYSTEM", 235, 238, 242, pw - 60);
        int source = -1, cec = -1;
        const oa_settings_iface *set =
            c->api->lookup(c->api->priv, OA_SVC_SETTINGS, 1);
        if (set) {
            (void)set->get_int("m_current_source", &source);
            (void)set->get_int("m_cec_enabled", &cec);
            c->api->release(c->api->priv, OA_SVC_SETTINGS);
        } else {
            const oa_source_iface *src =
                c->api->lookup(c->api->priv, OA_SVC_SOURCE, 1);
            if (src) {
                source = src->current_type();
                c->api->release(c->api->priv, OA_SVC_SOURCE);
            }
        }
        char line[96];
        const char *lines[4] = { NULL, NULL, NULL, NULL };
        snprintf(line, sizeof line, "Source  %d", source);
        lines[0] = line;
        char cec_line[32];
        snprintf(cec_line, sizeof cec_line, "CEC  %s",
                 cec < 0 ? "unknown" : cec ? "on" : "off");
        lines[1] = cec_line;
        lines[2] = "No vendor skin assets";
        lines[3] = "Native shell ready";
        for (int i = 0; i < 4; i++)
            text_at(c, px + 30, py + 84 + i * 44,
                    c->font_on ? &c->font : NULL, lines[i],
                    180, 190, 205, pw - 60);
    }
    footer(c, W, H, "Arrows move   Select opens   Exit returns");
}

static void paint_sources(struct ctx *c, int W, int H, int ar, int ag, int ab)
{
    c->d->fill(40, 88, W - 80, H - 176, 18, 21, 28);
    text_at(c, 70, 108, c->title_on ? &c->title_font : &c->font,
            "Sources", 235, 238, 242, W - 140);
    const oa_source_iface *src = c->api->lookup(c->api->priv, OA_SVC_SOURCE, 1);
    int current = src ? src->current_type() : -1;
    for (int i = 0; i < SOURCE_ROWS; i++) {
        int y = 160 + i * 72;
        int selected = c->source_focus == i;
        c->d->fill(70, y, W - 140, 56, selected ? ar : 24, selected ? ag : 28,
                   selected ? ab : 36);
        char line[96];
        snprintf(line, sizeof line, "%s%s", SOURCE_NAMES[i],
                 SOURCE_TYPES[i] < 0 ? "  (unavailable)" :
                 (current == SOURCE_TYPES[i] ? "  (current)" : ""));
        text_at(c, 90, y + 14, c->font_on ? &c->font : NULL, line,
                selected ? 255 : 235, selected ? 255 : 238,
                selected ? 255 : 242, W - 180);
    }
    if (src) c->api->release(c->api->priv, OA_SVC_SOURCE);
    footer(c, W, H, "Up/Down select   Select switches source   Exit returns");
}

static void paint_apps(struct ctx *c, int W, int H, int ar, int ag, int ab)
{
    c->d->fill(40, 88, W - 80, H - 176, 20, 24, 32);
    char title[64];
    snprintf(title, sizeof title, "Apps  %d", c->app_total);
    text_at(c, 70, 108, c->title_on ? &c->title_font : &c->font,
            title, 235, 238, 242, W - 140);
    if (!c->app_n) {
        text_at(c, 70, 170, c->font_on ? &c->font : NULL,
                "launcher.db unavailable or empty", 180, 190, 205, W - 140);
    }
    for (int i = 0; i < c->app_n; i++) {
        int y = 160 + i * 64;
        int selected = c->app_focus == i;
        c->d->fill(70, y, W - 140, 52, selected ? ar : 24, selected ? ag : 28,
                   selected ? ab : 36);
        text_at(c, 90, y + 12, c->font_on ? &c->font : NULL, c->apps[i],
                selected ? 255 : 235, selected ? 255 : 238,
                selected ? 255 : 242, W - 180);
    }
    footer(c, W, H, "Up/Down scroll   Exit returns");
}

static void paint_settings(struct ctx *c, int W, int H, int ar, int ag, int ab)
{
    c->d->fill(40, 88, W - 80, H - 176, 21, 25, 33);
    text_at(c, 70, 108, c->title_on ? &c->title_font : &c->font,
            "Settings", 235, 238, 242, W - 140);
    const oa_settings_iface *set = c->api->lookup(c->api->priv, OA_SVC_SETTINGS, 1);
    int source = -1, cec = -1;
    if (set) {
        (void)set->get_int("m_current_source", &source);
        (void)set->get_int("m_cec_enabled", &cec);
        c->api->release(c->api->priv, OA_SVC_SETTINGS);
    }
    char line[96];
    for (int i = 0; i < 2; i++) {
        int y = 160 + i * 72;
        int selected = c->settings_focus == i;
        c->d->fill(70, y, W - 140, 56, selected ? ar : 24, selected ? ag : 28,
                   selected ? ab : 36);
        if (i == 0)
            snprintf(line, sizeof line, "Current source  %d", source);
        else
            snprintf(line, sizeof line, "CEC  %s", cec < 0 ? "unavailable" :
                     cec ? "on" : "off");
        text_at(c, 90, y + 14, c->font_on ? &c->font : NULL, line,
                selected ? 255 : 235, selected ? 255 : 238,
                selected ? 255 : 242, W - 180);
    }
    footer(c, W, H, "Up/Down select   Select toggles CEC   Exit returns");
}

static void paint_about(struct ctx *c, int W, int H)
{
    c->d->fill(40, 88, W - 80, H - 176, 22, 26, 35);
    text_at(c, 70, 108, c->title_on ? &c->title_font : &c->font,
            "OmarchyTV", 235, 238, 242, W - 140);
    text_at(c, 70, 170, c->font_on ? &c->font : NULL,
            "Native replacement shell over ui.engine", 180, 190, 205, W - 140);
    text_at(c, 70, 210, c->font_on ? &c->font : NULL,
            "No vendor skin assets, portal, telemetry or advertising.", 180, 190, 205, W - 140);
    footer(c, W, H, "Exit returns");
}

static void paint(struct ctx *c)
{
    const oa_ui_engine_iface *d = c ? c->d : NULL;
    int W = 0, H = 0;
    int br, bg, bb, ba, ar, ag, ab, x, y, w, h;
    if (!c || !d || !c->active || !c->state || c->suspended)
        return;
    if (d->info(&W, &H, NULL, NULL) != 0 || W <= 0 || H <= 0)
        return;
    oa_omarchy_state *st = c->state;
    st->screen[0] = W;
    st->screen[1] = H;
    st->have_screen = 1;
    br = st->have_colors ? st->bg[0] : 12;
    bg = st->have_colors ? st->bg[1] : 14;
    bb = st->have_colors ? st->bg[2] : 18;
    ba = st->have_colors ? st->bg[3] : 255;
    ar = st->have_colors ? st->accent[0] : 0;
    ag = st->have_colors ? st->accent[1] : 122;
    ab = st->have_colors ? st->accent[2] : 204;
    x = st->have_layout ? st->layout[0] : 0;
    y = st->have_layout ? st->layout[1] : 0;
    w = st->have_layout ? st->layout[2] : 14;
    h = st->have_layout ? st->layout[3] : H;
    take_lua_paint(c, &br, &bg, &bb, &ba, &ar, &ag, &ab, &x, &y, &w, &h);
    d->clear_rgba(br, bg, bb, ba);
    if (st->have_layout)
        d->fill(x, y, w, h, ar, ag, ab);
    else
        d->fill(0, 0, 14, H, ar, ag, ab);
    header(c, W, H, "OMARCHY TV", ar, ag, ab);
    switch (c->screen) {
    case SCREEN_SOURCES: paint_sources(c, W, H, ar, ag, ab); break;
    case SCREEN_APPS: paint_apps(c, W, H, ar, ag, ab); break;
    case SCREEN_SETTINGS: paint_settings(c, W, H, ar, ag, ab); break;
    case SCREEN_ABOUT: paint_about(c, W, H); break;
    default: paint_home(c, W, H, ar, ag, ab); break;
    }
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

static void back_home(struct ctx *c)
{
    c->screen = SCREEN_HOME;
    c->focus = 0;
    paint(c);
}

static int key_home(struct ctx *c, int id)
{
    int f = c->focus;
    if (id == 19) f = f >= 2 ? f - 2 : f;          /* Up */
    else if (id == 20) f = f < 2 ? f + 2 : f;      /* Down */
    else if (id == 21) f = (f % 2) ? f - 1 : f;    /* Left */
    else if (id == 22) f = (f % 2) ? f : f + 1;    /* Right */
    else if (id == 53 || id == 0x8a) {             /* Select */
        c->screen = SCREEN_SOURCES + f;
        if (c->screen == SCREEN_APPS) apps_load(c);
        paint(c);
        return 0;
    } else {
        return 0;
    }
    if (f < 0) f = 0;
    if (f >= HOME_TILES) f = HOME_TILES - 1;
    if (f != c->focus) {
        c->focus = f;
        paint(c);
    }
    return 0;
}

static int key_sources(struct ctx *c, int id)
{
    if (id == 10 || id == 48) { back_home(c); return 0; }
    if (id == 19 || id == 20) {
        int n = c->source_focus + (id == 19 ? -1 : 1);
        if (n < 0) n = 0;
        if (n >= SOURCE_ROWS) n = SOURCE_ROWS - 1;
        if (n != c->source_focus) { c->source_focus = n; paint(c); }
        return 0;
    }
    if (id == 53 || id == 0x8a) {
        int type = SOURCE_TYPES[c->source_focus];
        if (type >= 0) {
            const oa_source_iface *src =
                c->api->lookup(c->api->priv, OA_SVC_SOURCE, 1);
            if (src) {
                int rc = src->select_type(type);
                c->api->release(c->api->priv, OA_SVC_SOURCE);
                if (rc == 0) paint(c);
            }
        }
        return 0;
    }
    return 0;
}

static int key_apps(struct ctx *c, int id)
{
    if (id == 10 || id == 48) { back_home(c); return 0; }
    if (id == 19 || id == 20) {
        int n = c->app_focus + (id == 19 ? -1 : 1);
        if (n < 0) n = 0;
        if (n >= c->app_n) n = c->app_n ? c->app_n - 1 : 0;
        if (n != c->app_focus) { c->app_focus = n; paint(c); }
    }
    return 0;
}

static int key_settings(struct ctx *c, int id)
{
    if (id == 10 || id == 48) { back_home(c); return 0; }
    if (id == 19 || id == 20) {
        int n = c->settings_focus + (id == 19 ? -1 : 1);
        if (n < 0) n = 0;
        if (n > 1) n = 1;
        if (n != c->settings_focus) { c->settings_focus = n; paint(c); }
        return 0;
    }
    if ((id == 53 || id == 0x8a) && c->settings_focus == 1) {
        const oa_settings_iface *set =
            c->api->lookup(c->api->priv, OA_SVC_SETTINGS, 1);
        if (set) {
            int cec = 0;
            if (set->get_int("m_cec_enabled", &cec) == 0)
                (void)set->set_int("m_cec_enabled", cec ? 0 : 1);
            c->api->release(c->api->priv, OA_SVC_SETTINGS);
            paint(c);
        }
        return 0;
    }
    return 0;
}

static int native_key(struct ctx *c, int id)
{
    switch (c->screen) {
    case SCREEN_SOURCES: return key_sources(c, id);
    case SCREEN_APPS: return key_apps(c, id);
    case SCREEN_SETTINGS: return key_settings(c, id);
    case SCREEN_ABOUT:
        if (id == 10 || id == 48) { back_home(c); return 0; }
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
    .version = "0.2",
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
