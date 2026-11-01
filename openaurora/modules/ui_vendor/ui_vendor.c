/*
 * mod_ui_vendor — the native home screen, drawn from the set's own content.
 *
 * It only talks to the `display` service (clear / fill / blit / blit_scaled / present), so it never
 * touches fbdev, and it never decodes PNG: every image is a flat "OAI1" blob built by
 * tools/mk-ui.py out of
 *
 *   - the rail glyphs   cut from the device's own portal framebuffer (artifacts/protocol/shot-launcher.png)
 *   - the app cards     /buffer/downloaded_images/launcher_app_thumb<N>, the PNGs the stock portal
 *                       downloaded for its LauncherItem rows
 *   - the title text    rendered with a font out of /vendor/fonts
 *
 * Geometry is the stock portal's, measured off that same capture:
 *
 *   background   rgb(14,14,13)
 *   title        "Startpagina" at 28,80
 *   icon rail    x 24..145, five 121x126 tiles from y 211, colours orange -> dark red,
 *                white line glyph centred, the focused tile has a lighter backdrop
 *   card row     nine 171x95 cards from x 209, 190 pitch, y 478
 *
 *   ui.dir     directory holding the .rgba blobs   (default /buffer/openaurora/ui)
 *   ui.paint   1 = draw on start                   (default 1)
 *   ui.focus   index of the highlighted rail item  (default 2)
 *   ui.cards   comma separated card blob names     (default the LauncherItem home row)
 *
 * Remote ids are the profile RC codes mod_ir emits when the frame's A1 byte is
 * one of those codes (IR-KEYMAP): Up 20, Down 19, Left 21, Right 22, Select 53,
 * Exit 10. NEC command 0x8a is Select as well: the UART driver symbol is 0x0d.
 * The live byte that carries the RC code is still unconfirmed, so this module
 * binds the ids the parser already publishes. Release and repeat are ignored.
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/ui_engine.h"
#include "openaurora/ui_time.h"

#define MAX_CARDS 12

/* stock portal palette, top to bottom (sampled from the portal capture, corrected for the
 * layer's BGRA byte order — the raw capture reads as blue->purple only because the capture
 * tool used to swap red and blue; the portal itself is orange -> dark red) */
static const struct {
    int r, g, b;
    const char *glyph;
} RAIL[5] = {
    { 247, 148, 30, "rail1" },
    { 241, 90, 41, "rail2" },
    { 190, 30, 45, "rail3" },
    { 129, 25, 36, "rail4" },
    { 72, 23, 48, "rail5" },
};

static const char *DEFAULT_CARDS[MAX_CARDS] = {
    "card_netflix",     "card_prime",  "card_youtube", "card_tabii",   "card_canaldigitaal",
    "card_tvvlaanderen", "card_telesat", "card_digiturk", "card_gain",   NULL,
};

/* Vendor p0/06 capture: ink origin and size of each source-row label. Row 0
 * carries the focused (larger) text; rows 1..8 the unfocused text. The module
 * scales between the two observed sizes when the focus moves. */
#define MAX_SOURCES 9
static const struct {
    int x, y, w, h;
} SRC_TEXT[MAX_SOURCES] = {
    { 89, 174, 67, 28 },
    { 85, 251, 166, 28 },
    { 85, 326, 124, 28 },
    { 85, 401, 131, 28 },
    { 86, 476, 132, 28 },
    { 86, 550, 123, 29 },
    { 85, 621, 146, 38 },
    { 86, 700, 273, 37 },
    { 84, 775, 264, 29 },
};

enum { BAND_RAIL = 0, BAND_CARDS = 1 };

/* Vendor source-selection panel (DialogSourceSelection.svg 185) and the nine
 * rows measured from the p0/06 capture. The text blobs are the vendor's own
 * ink extracted by tools/extract-source-selection.py; the panel and focus bar
 * are the capture's flat colours. Row text positions are the capture's ink
 * origins; the bar is the listBox row at 18,152 with a 75 px pitch. */
#define SRC_PANEL_W 780
#define SRC_BAR_X 18
#define SRC_BAR_Y 152
#define SRC_BAR_W 748
#define SRC_BAR_H 66
#define SRC_ROW_PITCH 75


/* Vendor mainlauncher_item_highlight_animated: 11 frames, 30 ms each, num_plays 0 (loop).
 * The image is created at the 160x160 button window from DialogMainMenu.svg; the measured focused
 * tile is 150x150 at (x,y), so the window is (x-5, y+10). The bar sits at the image's left edge. */
#define OA_RAIL_HL_FRAMES 11
#define OA_RAIL_HL_MS 30
/* PngImageDecoder::threadMain polls its decoder list every 10 ms and decodes a frame when
 * now - last >= fcTL delay (isReadyToDecode 0xb2fc84); the delay numerator is used directly as
 * milliseconds (updateFrameProperties 0xb2f918 stores it at +0x34). */
#define OA_RAIL_HL_POLL_MS 10
#define OA_RAIL_HL_W 160
#define OA_RAIL_HL_H 160
/* Title + five glyphs + the configured card limit + the entire APNG +
 * the source title and two focus variants for each of the nine source labels. */
#define MAX_ICONS (1 + 5 + MAX_CARDS + OA_RAIL_HL_FRAMES + 1 + 2 * MAX_SOURCES)

struct ctx {
    const oa_host_api *api;
    const oa_ui_engine_iface *d;     /* ui.engine: display/assets/present/ack */
    int scene_painted;
    char cards[MAX_CARDS][48];
    int nc;
    int band;
    int rail;
    int card;
    int src;                         /* source-selection row, 0..MAX_SOURCES-1 */
    int sel;                         /* picture-settings row, -1 = none */
    int screen;
    int restored;
    const oa_settings_iface *settings;
    const oa_ui_anim_iface *anim;
    const oa_ui_anim_iface *anim2;   /* v2 lookup: adds the text-slide helpers */
    int fs_type;                     /* full-screen transition type, ui.fs_type (0 = off) */
    const oa_ui_clock_iface *clock;
    oa_slide slide;                  /* title text slide, vendor AnimationHelper rules */
    int slide_box;                   /* usable width in px; 0 disables the effect */
    int title_w;
    unsigned char *slide_buf;        /* clipped-title scratch: slide_box x title_h x 4 */
    uint64_t slide_tick;             /* 10 ms sampler while the slide runs */
    uint64_t slide_repeat;           /* single 2000 ms shot, armed at progress > 0.99 */
    uint64_t rebind_tick;            /* deferred service rebind (timer owner = this module) */
    uint32_t mod_key;                /* module/changed subscription */
    uint32_t svc_key;                /* service/removed subscription */
    int suspended;
    int rebind_pending;              /* a relevant event arrived while suspended */
    /* Main-launcher focus highlight: vendor mainlauncher_item_highlight_animated, 11 frames at
     * 30 ms, drawn at the 160x160 button window (focused tile + (-5, +10)). */
    int rail_hl_n;
    int rail_hl_frame;
    int rail_hl_play;                /* one-shot per focus; the last frame is held */
    uint64_t rail_hl_next;           /* next frame due, host monotonic ms */
    uint64_t rail_hl_tick;
    int portal_on;
    int fps;
    uint32_t sub_key;
    uint32_t ready_key;              /* ui.engine bound the display: repaint */
    uint64_t tick;    /* repaint timer, when ui.fps > 0 */
};

static struct ctx *g_ctx;

static uint64_t now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

/* SVGDialog::create copies the constant mode table 57..64 and starts the first mode whose standard
 * animation widget exists in the dialog description. The widget names recovered from
 * SVGWindow::getWidgetStandardName (0xeee798, literals via 0xeeea84..0xeeeae8) are:
 *   57 animation_flip_fade_in       58 animation_slide_right_fade_in
 *   59 animation_slide_left_fade_in 60 animation_slide_up_fade_in
 *   61 animation_slide_down_fade_in 62 animation_fade_in
 *   63 animation_grow_fade_in       64 animation_bounce_fade_in
 * DialogLauncher.svg (resource 129, as DialogMainMenu3/4/Tall) declares only
 * animation_slide_up_fade_in, so the first-present rule selects mode 60 = HAL type 4
 * (y -= (1-p)/6, a slide up). */
static int dialog_fs_type(void)
{
    static const unsigned char present[8] = { 0, 0, 0, 1, 0, 0, 0, 0 };
    for (int i = 0; i < 8; i++)
        if (present[i])
            return i + 1;
    return 0;
}

/* Text-slide runner: implemented below, but paint() above it samples the margin. */
static uint32_t slide_elapsed(const struct ctx *c, uint32_t now);
static int slide_margin_now(const struct ctx *c, uint32_t now);
static int slide_is_running(const struct ctx *c, uint32_t now);
static float slide_progress_now(const struct ctx *c, uint32_t now);
static void rebind_tick_cb(void *ud);
static void on_anim(int id, int kind, const oa_anim_rect *rect, float eased, int done, void *ud);
static void rail_hl_sync(struct ctx *c);
static void rail_hl_tick_cb(void *ud);

/* ---------------------------------------------------------------- loading */

static const oa_ui_icon *find_icon(struct ctx *c, const char *name)
{
    return c && c->d ? c->d->icon(name) : NULL;
}

/* ------------------------------------------------------------------ geometry */

enum { SCREEN_PORTAL = 0, SCREEN_OPEN = 1, SCREEN_SOURCES = 2, SCREEN_SETTINGS = 3,
       SCREEN_SETTINGS_LIST = 4, ANIM_DIALOG = 1 };

/* Picture-settings rows: 66 px FA bar on the captured 70 px pitch, text
 * centered (row 4 centre 496). Row 6 ("Beeld zoomen") is disabled in the
 * capture and is skipped by navigation. */
#define SETTINGS_ROW_X 20
#define SETTINGS_ROW_W 750
#define SETTINGS_ROW_H 66
#define SETTINGS_ROW_Y0 183
#define SETTINGS_ROW_PITCH 70
#define SETTINGS_ROW_COUNT 9
#define SETTINGS_ROW_DISABLED 6

static int settings_row_y(int sel)
{
    return SETTINGS_ROW_Y0 + SETTINGS_ROW_PITCH * sel;
}

static int settings_row_next(int sel, int dir)
{
    int i = sel < 0 ? (dir > 0 ? 0 : SETTINGS_ROW_COUNT - 1) : sel;
    if (sel >= 0) {
        do {
            i += dir;
        } while (i >= 0 && i < SETTINGS_ROW_COUNT && i == SETTINGS_ROW_DISABLED);
    }
    if (i < 0) i = 0;
    if (i >= SETTINGS_ROW_COUNT) i = SETTINGS_ROW_COUNT - 1;
    if (i == SETTINGS_ROW_DISABLED) i = sel < 0 ? 0 : sel;
    return i;
}

static void rail_slot(int focus, int i, int *x, int *y, int *w, int *h)
{
    int y0 = 211;
    for (int k = 0; k <= i; k++) {
        int focused = focus == k;
        int ww = focused ? 150 : 121;
        int hh = focused ? 150 : 120;
        int xx = focused ? 11 : 24;
        if (k == i) {
            *x = xx;
            *y = y0;
            *w = ww;
            *h = hh;
            return;
        }
        y0 += hh;
    }
}
static void card_slot(int i, int *x, int *y, int *w, int *h)
{
    *x = 209 + i * 190;
    *y = 478;
    *w = 171;
    *h = 95;
}
static int focus_rail(const struct ctx *c)
{
    return c->band == BAND_RAIL ? c->rail : -1;
}
static int inside(int x, int y, int rx, int ry, int rw, int rh)
{
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}
static int hit_at(struct ctx *c, int x, int y, int *band, int *index)
{
    int focus = focus_rail(c);
    for (int i = 0; i < 5; i++) {
        int rx = 0, ry = 0, rw = 0, rh = 0;
        rail_slot(focus, i, &rx, &ry, &rw, &rh);
        if (inside(x, y, rx, ry, rw, rh)) {
            if (band) *band = BAND_RAIL;
            if (index) *index = i;
            return 1;
        }
    }
    for (int i = 0; i < c->nc; i++) {
        int rx = 0, ry = 0, rw = 0, rh = 0;
        card_slot(i, &rx, &ry, &rw, &rh);
        if (inside(x, y, rx, ry, rw, rh)) {
            if (band) *band = BAND_CARDS;
            if (index) *index = i;
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ paint */

static void paint_sources(struct ctx *c)
{
    const oa_ui_engine_iface *d = c->d;
    int W = 0, H = 0;
    if (!c || !d || d->info(&W, &H, NULL, NULL) != 0 || W <= 0 || H <= 0)
        return;
    /* DialogSourceSelection.svg 185 panel (0,0,780,1080) and the capture's
     * focused row at (18,152) with a 75 px pitch. Resource colours: the panel
     * is #BE1E2D (main_rectangle) and the row highlight #FE9419 (listboxrowFA);
     * p0/06 shows those with R/B swapped. The area outside the panel is
     * transparent so the video underneath stays visible, as on the set. */
    int cleared = d->clear_rgba(0, 0, 0, 0);
    if (cleared) return;
    d->fill(0, 0, SRC_PANEL_W, H, 190, 30, 45);
    d->fill(SRC_BAR_X, SRC_BAR_Y + c->src * SRC_ROW_PITCH, SRC_BAR_W, SRC_BAR_H,
            254, 148, 25);
    const oa_ui_icon *t = find_icon(c, "src_title");
    if (t)
        d->blit(62, 64, t->w, t->h, t->rgba);
    for (int i = 0; i < MAX_SOURCES; i++) {
        char name[16], focused_name[18];
        snprintf(name, sizeof name, "src_%d", i);
        snprintf(focused_name, sizeof focused_name, "src_%d_f", i);
        const oa_ui_icon *u = find_icon(c, name);
        const oa_ui_icon *f = find_icon(c, focused_name);
        const oa_ui_icon *g = i == c->src ? f : u;
        if (!g)
            continue;
        int x = SRC_TEXT[i].x, y = SRC_TEXT[i].y;
        /* The focused capture is 34/28 larger and the vendor keeps the text
         * bottom aligned, so the extra height grows upward. */
        if (i == c->src && u && f)
            y = SRC_TEXT[i].y + u->h - f->h;
        d->blit(x, y, g->w, g->h, g->rgba);
    }
    c->scene_painted = 1;
    if (d->present() != 0) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "source-selection present failed");
        return;
    }
    c->api->log(c->api->priv, OA_LOG_INFO, "painted source selection: row %d", c->src);
}

static void paint_settings(struct ctx *c)
{
    const oa_ui_engine_iface *d = c->d;
    if (!c || !d)
        return;
    /* Captured Beeld state from p0/03, R/B-corrected: rail 0,180,176,660; top
     * setting tiles 0,240,1920,180; category row and the selected "Beeld"
     * caption 0,440,1920,260. That capture stores the background as (14,14,13),
     * so the corrected background is (13,14,14). Static state; no navigation. */
    d->clear(13, 14, 14);
    const oa_ui_icon *top = find_icon(c, "settings_top");
    if (top)
        d->blit(0, 240, top->w, top->h, top->rgba);
    const oa_ui_icon *cat = find_icon(c, "settings_cat");
    if (cat)
        d->blit(0, 440, cat->w, cat->h, cat->rgba);
    const oa_ui_icon *rail = find_icon(c, "settings_rail");
    if (rail)
        d->blit(0, 180, rail->w, rail->h, rail->rgba);
    c->scene_painted = 1;
    if (d->present() != 0) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "settings present failed");
        return;
    }
    c->api->log(c->api->priv, OA_LOG_INFO, "painted captured picture-settings state");
}

static void paint_settings_list(struct ctx *c)
{
    const oa_ui_engine_iface *d = c->d;
    if (!c || !d)
        return;
    /* Vertical picture-settings panel from shot-settings.png, R/B-corrected:
     * 780x1080 at 0,0. The dialog's full-screen "background" record is a named
     * description, not a painted rectangle (UI-TRANSPARENCY-2026-11-01); the
     * unnamed 780x1080 rect is the panel. Keep the rest of the OSD transparent
     * so the video plane stays visible, exactly like the source panel. The
     * capture's black right side is an empty canvas, not opaque black. */
    int cleared = d->clear_rgba(0, 0, 0, 0);
    if (cleared) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "settings panel clear failed");
        return;
    }
    const oa_ui_icon *panel = find_icon(c, "settings_panel");
    if (panel)
        d->blit(0, 0, panel->w, panel->h, panel->rgba);
    if (c->sel >= 0 && c->sel < SETTINGS_ROW_COUNT && c->sel != SETTINGS_ROW_DISABLED) {
        char name[32];
        snprintf(name, sizeof name, "settings_focus_%d", c->sel);
        const oa_ui_icon *row = find_icon(c, name);
        if (row)
            d->blit(SETTINGS_ROW_X, settings_row_y(c->sel), row->w, row->h, row->rgba);
    }
    c->scene_painted = 1;
    if (d->present() != 0) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "settings panel present failed");
        return;
    }
    c->api->log(c->api->priv, OA_LOG_INFO, "painted captured picture-settings panel");
}

static void paint(struct ctx *c)
{
    const oa_ui_engine_iface *d;
    int W = 0, H = 0;
    if (!c || !c->d) return;   /* a suspended/stopped skin has no engine hold */
    d = c->d;
    if (c->screen == SCREEN_SETTINGS_LIST) {
        paint_settings_list(c);
        return;
    }
    if (c->screen == SCREEN_SETTINGS) {
        paint_settings(c);
        return;
    }
    if (c->screen == SCREEN_SOURCES) {
        paint_sources(c);
        return;
    }
    rail_hl_sync(c);              /* keeps the 30 ms highlight timer in step with the screen */
    if (d->info(&W, &H, NULL, NULL) != 0 || W <= 0 || H <= 0)
        return;

    /* clear() covers the whole OSD surface the display service reports. */
    /* DialogMainMenu.svg's unnamed full-screen #242628 rectangle: raw opacity6,
     * scaled conversion + rgbColourParser complement yield alpha153. Named
     * background/main_rectangle records describe layout, not opaque paint. */
    int cleared = d->clear_rgba(36, 38, 40, 153);
    if (cleared) { c->api->log(c->api->priv, OA_LOG_ERROR, "portal clear failed"); return; }

    const oa_ui_icon *title = find_icon(c, "title");
    if (title) {
        /* SVGTextBox draws the label clipped to its text box; the AnimationHelper margin moves the
         * text inside that box. Draw the visible source slice at the fixed box origin instead of
         * blitting the whole label (which would show text outside the box). */
        uint32_t now = c->clock ? c->clock->now_ms() : 0;
        int shift = slide_margin_now(c, now);
        int box = c->slide_box > 0 ? c->slide_box : title->w;
        int sx = shift > 0 ? shift : 0;
        int cw = title->w - sx;

        if (cw > box) cw = box;
        if (cw > 0 && (cw == title->w || !c->slide_buf)) {
            d->blit(29, 80, title->w, title->h, title->rgba);
        } else if (cw > 0) {
            for (int row = 0; row < title->h; row++)
                memcpy(c->slide_buf + (size_t)row * (size_t)cw * 4,
                       title->rgba + ((size_t)row * (size_t)title->w + (size_t)sx) * 4,
                       (size_t)cw * 4);
            d->blit(29, 80, cw, title->h, c->slide_buf);
        }
    }

    /*
     * Rail: five 121x120 slots from y 211; the focused one grows to 150x150 (the slots after it
     * move down by the difference) and gets a 5 px accent frame in the portal's orange.
     */
    int focus = c->screen == SCREEN_OPEN && c->band == BAND_RAIL ? c->rail : focus_rail(c);
    int y = 211;
    for (int i = 0; i < 5; i++) {
        if (c->screen == SCREEN_OPEN && (c->band != BAND_RAIL || i != c->rail))
            continue;
        int x = 0, w = 0, h = 0;
        rail_slot(focus, i, &x, &y, &w, &h);
        if (y + h > H)
            break;
        int focused = focus == i;
        if (focused) {
            const oa_ui_icon *hl = NULL;
            if (c->rail_hl_n > 0) {
                char name[32];
                snprintf(name, sizeof name, "rail_hl%02d", c->rail_hl_frame + 1);
                hl = find_icon(c, name);
            }
            if (hl)
                /* vendor renderForeground: the highlight image is created at the button window */
                d->blit_scaled(x - 5, y + 10, OA_RAIL_HL_W, OA_RAIL_HL_H, hl->w, hl->h, hl->rgba);
            else
                d->fill(x - 5, y - 5, w + 10, h + 10, 247, 148, 30);
        }
        d->fill(x, y, w, h, RAIL[i].r, RAIL[i].g, RAIL[i].b);

        const oa_ui_icon *g = find_icon(c, RAIL[i].glyph);
        if (g)
            d->blit(x + (w - g->w) / 2, y + (h - g->h) / 2, g->w, g->h, g->rgba);
        y += h;
    }

    for (int i = 0; i < c->nc; i++) {
        if (c->screen == SCREEN_OPEN && (c->band != BAND_CARDS || i != c->card))
            continue;
        int x = 0, cy = 0, w = 0, h = 0;
        card_slot(i, &x, &cy, &w, &h);
        if (x + w > W)
            break;
        /* The card row has no separately sampled focus chrome. A focused card
         * uses the same 5 px accent measured around the focused rail tile. */
        if (c->band == BAND_CARDS && i == c->card)
            d->fill(x - 5, cy - 5, w + 10, h + 10, 247, 148, 30);
        /* aspects match (both 16:9), so this is an exact scale, no cropping */
        const oa_ui_icon *card = find_icon(c, c->cards[i]);
        if (card)
            d->blit_scaled(x, cy, w, h, card->w, card->h, card->rgba);
        else
            d->fill(x, cy, w, h, 32, 32, 32);
    }

    c->scene_painted = 1;
    if (d->present() != 0) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "portal present failed");
        return;
    }
    c->api->log(c->api->priv, OA_LOG_INFO,
                "painted portal: 5 rail items, %d cards, band %d rail %d card %d",
                c->nc, c->band, c->rail, c->card);

    /* CPU commit acknowledges above; async completion acknowledges in the owned
     * sampler. Neither is liveness or independent physical scanout proof. */
}

/* ----------------------------------------------------------------- module */

static int portal_hit(int x, int y, int *band, int *index)
{
    return g_ctx && hit_at(g_ctx, x, y, band, index);
}
static int portal_activate(int x, int y)
{
    int band = 0, index = 0;
    if (!g_ctx || !hit_at(g_ctx, x, y, &band, &index)) return 0;
    if (band == BAND_RAIL) {
        g_ctx->band = BAND_RAIL;
        g_ctx->rail = index;
    } else {
        g_ctx->band = BAND_CARDS;
        g_ctx->card = index;
    }
    if (g_ctx->screen == SCREEN_PORTAL && g_ctx->d) paint(g_ctx);
    return 1;
}
static int portal_screen(void)
{
    return g_ctx ? g_ctx->screen : 0;
}
static const oa_portal_iface g_portal = { portal_hit, portal_activate, portal_screen };

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

static void emit_action(struct ctx *c, int kind)
{
    oa_action_event act;
    memset(&act, 0, sizeof act);
    act.kind = (uint8_t)kind;
    if (kind == OA_ACTION_SOURCE && c->settings) {
        int source = 0;
        if (c->settings->get_int("m_current_source", &source) == 0)
            act.index = (uint8_t)source;
    } else if (c->band == BAND_CARDS && c->nc > 0) {
        act.index = (uint8_t)c->card;
        act.name = c->cards[c->card];
    } else {
        act.index = (uint8_t)c->rail;
        act.name = RAIL[c->rail].glyph;
    }
    c->api->emit(c->api->priv, OA_TOPIC_ACTION, &act);
}

/* ------------------------------------------------------- text slide (vendor rules) */

/*
 * SVGTextBox::performTextSlideAnimation @0x10c7768 starts the animation only when the label
 * overflows its box (isTextSlidingNeeded @0x10c76b0 sets distance = text_width - box_width) and
 * animation is enabled. The framework samples at 10 ms; onAnimationProgress @0x10c8e48 arms a single
 * 2000 ms timer once progress exceeds 0.99 (0x10c8ee8) in sliding state 3 — after that the label
 * starts sliding again. Nothing here changes geometry unless ui.slide_box makes a label overflow.
 */
static void on_slide_tick(void *ud);
static void on_slide_repeat(void *ud);

/* The vendor starts the effect only when animation is enabled and the label overflows its box. */
static int slide_allowed(const struct ctx *c)
{
    return c->anim && c->anim->enabled() && c->anim2 && c->clock &&
           c->slide_box > 0 && c->title_w > c->slide_box;
}

static void slide_stop_sampler(struct ctx *c)
{
    if (c->slide_tick) {
        c->api->timer_del(c->api->priv, c->slide_tick);
        c->slide_tick = 0;
    }
}

/* Use the provider's v2 math so a reloaded ui.anim changes the rule here too; only the runner
 * state (start time, distance, duration) is local. */
static uint32_t slide_elapsed(const struct ctx *c, uint32_t now)
{
    return oa_ui_time_elapsed(now, c->slide.started);
}

static int slide_margin_now(const struct ctx *c, uint32_t now)
{
    if (!c->slide.active || !c->anim2) return 0;
    return c->anim2->slide_margin(slide_elapsed(c, now), c->slide.duration,
                                  c->slide.distance, c->slide.reverse);
}

static int slide_is_running(const struct ctx *c, uint32_t now)
{
    return c->slide.active && (float)slide_elapsed(c, now) < c->slide.duration;
}

static float slide_progress_now(const struct ctx *c, uint32_t now)
{
    float p;
    if (!c->slide.active || c->slide.duration <= 0.0f) return 1.0f;
    p = (float)slide_elapsed(c, now) / c->slide.duration;
    return p > 1.0f ? 1.0f : p;
}

static void slide_arm_repeat(struct ctx *c)
{
    if (c->slide_repeat) return;
    c->slide_repeat = c->api->timer_add(c->api->priv, OA_ANIM_SLIDE_REPEAT_MS, 0,
                                        on_slide_repeat, c);
    if (!c->slide_repeat)
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "text slide: repeat timer allocation failed; the label holds its end margin");
}

/* (Re)start one pass. Called on module start/resume and when the vendor repeat fires. */
static void slide_begin(struct ctx *c, const char *why)
{
    if (!slide_allowed(c)) return;   /* keep the current label position; nothing to start */
    int distance = c->title_w - c->slide_box;
    oa_slide_start(&c->slide, distance, 0, c->clock->now_ms());
    c->slide.duration = c->anim2->slide_duration(distance); /* the v2 provider owns the rule */
    slide_stop_sampler(c);
    c->slide_tick = c->api->timer_add(c->api->priv, 10, 10, on_slide_tick, c);
    if (!c->slide_tick) {
        c->slide.active = 0;
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "text slide: 10 ms sampler allocation failed (%s)", why);
        return;
    }
    c->api->log(c->api->priv, OA_LOG_INFO,
                "text slide: title %d px in %d px box (%u ms per pass, %d ms repeat, %s)",
                c->title_w, c->slide_box, (unsigned)c->slide.duration,
                OA_ANIM_SLIDE_REPEAT_MS, why);
}

static void on_slide_repeat(void *ud)
{
    struct ctx *c = ud;
    c->slide_repeat = 0;
    if (!c->d) return;
    if (!slide_allowed(c)) {     /* animation was disabled or the title changed: hold the end state */
        paint(c);
        return;
    }
    slide_begin(c, "repeat");
    paint(c);
}

static void on_slide_tick(void *ud)
{
    struct ctx *c = ud;
    uint32_t now;

    if (!c->d || !c->clock) {
        slide_stop_sampler(c);
        return;
    }
    now = c->clock->now_ms();
    if (!slide_is_running(c, now)) {
        slide_stop_sampler(c);
        paint(c);
        /* onAnimationProgress arms the single 2000 ms shot once progress passes 0.99; if a slow
         * tick jumps straight past that window, the completion path arms it instead. */
        if (slide_allowed(c)) slide_arm_repeat(c);
        return;
    }
    if (!c->slide_repeat && slide_progress_now(c, now) > 0.99f)
        slide_arm_repeat(c);
    paint(c);
}

/* Main-launcher highlight: the vendor image player loops the 11 frames at their APNG delay while
 * the portal rail is shown. The playhead is reset when the focused item changes. */
static void rail_hl_sync(struct ctx *c)
{
    int want = c->rail_hl_n > 0 && c->d && c->screen == SCREEN_PORTAL &&
               c->rail_hl_play && c->rail_hl_frame + 1 < c->rail_hl_n;
    if (want && !c->rail_hl_tick) {
        c->rail_hl_tick = c->api->timer_add(c->api->priv, OA_RAIL_HL_POLL_MS,
                                            OA_RAIL_HL_POLL_MS, rail_hl_tick_cb, c);
        if (!c->rail_hl_tick)
            c->api->log(c->api->priv, OA_LOG_WARN,
                        "rail highlight: %d ms poll timer allocation failed", OA_RAIL_HL_POLL_MS);
    } else if (!want && c->rail_hl_tick) {
        c->api->timer_del(c->api->priv, c->rail_hl_tick);
        c->rail_hl_tick = 0;
    }
}

static void rail_hl_tick_cb(void *ud)
{
    struct ctx *c = ud;
    uint64_t now;

    if (!c->d || c->rail_hl_n <= 0) return;
    /* Async display: advance only after the previous frame completed, so every
     * decoded frame is presented in order instead of being coalesced away. */
    if (c->d && c->d->present_pending()) return;
    now = now_ms();
    if (now < c->rail_hl_next) return;   /* 10 ms poll; the 30 ms frame is not due yet */
    if (c->rail_hl_frame + 1 < c->rail_hl_n)
        c->rail_hl_frame++;
    c->rail_hl_next = now + OA_RAIL_HL_MS;   /* vendor stamps last = now at decode */
    if (c->rail_hl_frame + 1 >= c->rail_hl_n)
        c->rail_hl_play = 0;                 /* finite sequence: hold the last frame */
    paint(c);
}

/* A provider can be started after this module, or force-unloaded while we still hold a pointer to
 * it. Lifecycle events schedule a rebind in a timer callback: timer dispatch runs with this module
 * as owner, so lookups and releases attribute their holds to ui_vendor. While suspended the display
 * hold must stay dropped, so events only mark a pending rebind and resume performs it. */
static void rebind_now(struct ctx *c);

static void schedule_rebind(struct ctx *c)
{
    if (c->suspended) {
        c->rebind_pending = 1;
        return;
    }
    if (c->rebind_tick) return;
    c->rebind_tick = c->api->timer_add(c->api->priv, 0, 0, rebind_tick_cb, c);
    if (!c->rebind_tick)
        c->api->log(c->api->priv, OA_LOG_WARN, "ui_vendor: rebind timer allocation failed");
}

static void on_module_changed(const void *evp, void *ud)
{
    struct ctx *c = ud;
    const oa_module_event *ev = evp;
    if (!c || !ev || !ev->name) return;
    if (!strcmp(ev->name, "ui_vendor")) return;   /* our own start/stop cannot change our providers */
    if (ev->what == OA_MOD_STARTED_EV || ev->what == OA_MOD_STOPPED_EV ||
        ev->what == OA_MOD_UNLOADED_EV)
        schedule_rebind(c);
}

static void on_service_removed(const void *evp, void *ud)
{
    struct ctx *c = ud;
    const oa_service_removed_event *ev = evp;
    if (!c || !ev || !ev->name) return;
    /* The host already cleared the holds with the service. Drop the cached pointer before it can
     * dangle; the deferred rebind reacquires whatever is published again. The engine owns the
     * display/async/alpha holds; this skin only watches its own providers. */
    if (!strcmp(ev->name, OA_SVC_UI_ENGINE)) {
        c->d = NULL;
        c->scene_painted = 0;
    } else if (!strcmp(ev->name, OA_SVC_UI_ANIM)) {
        c->anim = NULL;
        c->anim2 = NULL;
        if (c->d)
            c->d->set_scene_transform(0, 0.0f);
    } else if (!strcmp(ev->name, OA_SVC_SETTINGS)) {
        c->settings = NULL;
    } else if (!strcmp(ev->name, OA_SVC_UI_CLOCK)) {
        c->clock = NULL;
    } else {
        return;
    }
    schedule_rebind(c);
}

static void rebind_now(struct ctx *c)
{
    c->rebind_tick = 0;
    c->rebind_pending = 0;
    if (!c->d) {
        c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 1);
        if (!c->d)
            return;
    }
    int w = 0, h = 0;
    if (c->d->info(&w, &h, NULL, NULL) != 0)
        return;                        /* engine up, display not bound yet */
    if (!c->settings) c->settings = c->api->lookup(c->api->priv, OA_SVC_SETTINGS, 1);
    if (!c->anim) {
        c->anim = c->api->lookup(c->api->priv, OA_SVC_UI_ANIM, 1);
        if (c->anim) c->anim->bind(on_anim, c);
    }
    c->anim2 = c->api->lookup(c->api->priv, OA_SVC_UI_ANIM, 2);
    if (!c->clock) c->clock = c->api->lookup(c->api->priv, OA_SVC_UI_CLOCK, 1);
    if (!slide_allowed(c)) {
        /* a provider went away or animation is off: stop timing and hold the drawn state */
        slide_stop_sampler(c);
        if (c->slide_repeat) {
            c->api->timer_del(c->api->priv, c->slide_repeat);
            c->slide_repeat = 0;
        }
        memset(&c->slide, 0, sizeof c->slide);
        paint(c);
        return;
    }
    if (!c->slide.active)
        slide_begin(c, "service rebind");
    paint(c);
}

static void rebind_tick_cb(void *ud)
{
    struct ctx *c = ud;
    if (c->suspended) {
        c->rebind_tick = 0;
        c->rebind_pending = 1;
        return;
    }
    rebind_now(c);
}

static void on_anim(int id, int kind, const oa_anim_rect *rect, float eased, int done, void *ud)
{
    struct ctx *c = ud;
    (void)id;
    (void)rect;
    if (!c) return;
    if (kind == OA_ANIM_DIALOG) {
        /* Vendor SVGDialog::onAnimationProgress pushes min(1,1+(p-1)^3) to the full-screen
         * transform; the callback already receives that eased value. The display v2 scene warp
         * applies it; on completion the surface is back at identity. */
        if (c->d && c->fs_type > 0)
            c->d->set_scene_transform(done ? 0 : c->fs_type, done ? 0.0f : eased);
        /* The vendor animates the dialog's named rect over the launcher; our single-surface UI
         * therefore shows the dialog content from the first progress callback, so the transform
         * slides the dialog up into place instead of moving the portal underneath it. */
        c->screen = SCREEN_OPEN;
        if (!c->suspended && c->d) {
            if (!done && c->scene_painted) (void)c->d->present();
            else paint(c);
        }
        return;
    }
    if (kind == OA_ANIM_LIST && rect && c->d) {
        c->d->clear(14, 14, 13);
        c->d->fill(rect->x, rect->y, rect->w, rect->h, 247, 148, 30);
        c->scene_painted = 1;
        (void)c->d->present();
    }
}

static void cancel_dialog(struct ctx *c)
{
    if (c->anim) (void)c->anim->cancel(ANIM_DIALOG);
    if (c->d)
        c->d->set_scene_transform(0, 0.0f);
}

static void move_key(struct ctx *c, uint8_t id)
{
    if (c->screen == SCREEN_SETTINGS_LIST) {
        if (id == 19 || id == 20) {        /* Down / Up: row selection */
            c->sel = settings_row_next(c->sel, id == 19 ? 1 : -1);
            if (c->d) paint(c);
        } else if (id == 53 || id == 0x8a) {
            /* Value editing is not decoded yet; keep the selection only. */
            c->api->log(c->api->priv, OA_LOG_INFO,
                        "settings row %d selected (editing not implemented)", c->sel);
        } else if (id == 10 || id == 48) {
            c->screen = SCREEN_SETTINGS;   /* back to the quick picture tiles */
            if (c->d) paint(c);
        }
        return;
    }
    if (c->screen == SCREEN_SETTINGS) {
        if (id == 53 || id == 0x8a) {      /* Select: open the full picture list */
            c->screen = SCREEN_SETTINGS_LIST;
            if (c->d) paint(c);
        } else if (id == 10 || id == 48) {
            c->screen = SCREEN_PORTAL;
            if (c->d) paint(c);
        }
        return;
    }
    if (c->screen == SCREEN_SOURCES) {
        if (id == 20 && c->src > 0) {
            c->src--;
        } else if (id == 19 && c->src + 1 < MAX_SOURCES) {
            c->src++;
        } else if (id == 10 || id == 48) {
            c->screen = SCREEN_PORTAL;
        } else if (id == 53 || id == 0x8a) {
            emit_action(c, OA_ACTION_SELECT);
            return;
        } else {
            return;
        }
        if (c->d) paint(c);
        return;
    }
    if (id == 20) { /* Up */
        if (c->band == BAND_CARDS) {
            c->band = BAND_RAIL;
            c->rail_hl_frame = 0;
            c->rail_hl_play = 1;
            c->rail_hl_next = now_ms() + OA_RAIL_HL_MS;
        } else if (c->rail > 0) {
            c->rail--;
            c->rail_hl_frame = 0;
            c->rail_hl_play = 1;
            c->rail_hl_next = now_ms() + OA_RAIL_HL_MS;
        }
    } else if (id == 19) { /* Down */
        if (c->band == BAND_RAIL && c->rail < 4) {
            c->rail++;
            c->rail_hl_frame = 0;
            c->rail_hl_play = 1;
            c->rail_hl_next = now_ms() + OA_RAIL_HL_MS;
        }
    } else if (id == 21) { /* Left */
        if (c->band == BAND_CARDS && c->card > 0)
            c->card--;
        else {
            c->band = BAND_RAIL;
            c->rail_hl_frame = 0;
            c->rail_hl_play = 1;
            c->rail_hl_next = now_ms() + OA_RAIL_HL_MS;
        }
    } else if (id == 22) { /* Right */
        if (c->band == BAND_RAIL && c->nc > 0) {
            c->band = BAND_CARDS;
        } else if (c->band == BAND_CARDS && c->card + 1 < c->nc) {
            c->card++;
        }
    } else if (id == 53 || id == 0x8a) { /* Select: profile RC, or NEC OK */
        emit_action(c, OA_ACTION_SELECT);
        /* Rail 1 is the vendor source-list icon; Select on it opens the
         * DialogSourceSelection panel. */
        if (c->band == BAND_RAIL && c->rail == 1) {
            c->screen = SCREEN_SOURCES;
            c->src = 0;
            if (c->d) paint(c);
            return;
        }
        /* Rail 4 is the vendor wrench; Select shows the captured picture-
         * settings state. Static state until the shortcut list is decoded. */
        if (c->band == BAND_RAIL && c->rail == 4) {
            c->screen = SCREEN_SETTINGS;
            if (c->d) paint(c);
            return;
        }
        /* Firmware MainLauncherMenuDialog::onSelectPress only calls focusLauncher(false), and
         * focusLauncher does index bookkeeping: the launcher has no full-screen transition.
         * The type-4 path is a lab-only option (ui.launch_anim=1) pending vendor dialogue parity. */
        if (atoi(c->api->config_get(c->api->priv, "ui.launch_anim", "0")) == 1 &&
            c->anim && c->anim->enabled() &&
            c->anim->start(ANIM_DIALOG, OA_ANIM_DIALOG, NULL, NULL, 1000, NULL) == 0)
            return;
        c->screen = SCREEN_OPEN;
        if (c->d) paint(c);
        return;
    } else if (id == 10) { /* Exit */
        cancel_dialog(c);
        if (c->screen == SCREEN_OPEN) c->screen = SCREEN_PORTAL;
        emit_action(c, OA_ACTION_BACK);
        if (c->d) paint(c);
        return;
    } else if (id == 48) { /* Menu */
        cancel_dialog(c);
        if (c->screen == SCREEN_OPEN) c->screen = SCREEN_PORTAL;
        emit_action(c, OA_ACTION_HOME);
        if (c->d) paint(c);
        return;
    } else if (id == 56) { /* Aux: source, value from settings when that column reads */
        emit_action(c, OA_ACTION_SOURCE);
        return;
    } else {
        return;
    }
    if (c->d)
        paint(c);
}

static void parse_cards(struct ctx *c, const char *spec)
{
    c->nc = 0;
    while (*spec && c->nc < MAX_CARDS) {
        while (*spec == ' ' || *spec == ',')
            spec++;
        if (!*spec)
            break;
        int n = 0;
        while (*spec && *spec != ',' && n < (int)sizeof c->cards[0] - 1)
            c->cards[c->nc][n++] = *spec++;
        c->cards[c->nc][n] = '\0';
        c->nc++;
    }
}

static int skin_enter(void *ud)
{
    (void)ud;
    return 0;   /* m_start already painted the initial screen */
}
static void skin_paint(void *ud)
{
    struct ctx *c = ud;
    if (c && !c->suspended && c->d)
        paint(c);
}
static int skin_key(void *ud, int id)
{
    struct ctx *c = ud;
    if (!c)
        return 1;
    move_key(c, (uint8_t)id);
    return 0;
}
static const oa_ui_skin g_skin = { "vendor", skin_enter, NULL, skin_paint, skin_key };

static int m_start(void *v)
{
    struct ctx *c = v;
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 1);
    if (!c->d) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "no ui.engine service — load mod_ui_engine first");
        return -1;
    }
    c->d->reset_ready();
    c->scene_painted = 0;

    const char *spec = c->api->config_get(c->api->priv, "ui.cards", "");
    if (spec && spec[0]) {
        parse_cards(c, spec);
    } else {
        for (c->nc = 0; DEFAULT_CARDS[c->nc]; c->nc++)
            snprintf(c->cards[c->nc], sizeof c->cards[0], "%s", DEFAULT_CARDS[c->nc]);
    }

    if (!c->restored) {
        c->band = BAND_RAIL;
        c->rail = atoi(c->api->config_get(c->api->priv, "ui.focus", "2"));
        c->card = 0;
        c->screen = SCREEN_PORTAL;
        c->src = 0;
    }
    /* Lab/screen-test entry point: ui.screen=sources starts on the vendor
     * source-selection panel without needing an input device. */
    if (!strcmp(c->api->config_get(c->api->priv, "ui.screen", ""), "sources")) {
        c->screen = SCREEN_SOURCES;
        c->src = atoi(c->api->config_get(c->api->priv, "ui.src", "0"));
        if (c->src < 0 || c->src >= MAX_SOURCES)
            c->src = 0;
    }
    if (!strcmp(c->api->config_get(c->api->priv, "ui.screen", ""), "settings"))
        c->screen = SCREEN_SETTINGS;
    if (!strcmp(c->api->config_get(c->api->priv, "ui.screen", ""), "settings_list"))
        c->screen = SCREEN_SETTINGS_LIST;
    /* Lab/test preselection for a focused-row capture; -1 starts unfocused
     * like the vendor capture. The disabled row is never preselected. */
    c->sel = atoi(c->api->config_get(c->api->priv, "ui.sel", "-1"));
    if (c->sel < 0 || c->sel >= SETTINGS_ROW_COUNT || c->sel == SETTINGS_ROW_DISABLED)
        c->sel = -1;
    if (c->rail < 0 || c->rail > 4)
        c->rail = 2;
    if (c->nc > 0 && c->card >= c->nc)
        c->card = c->nc - 1;
    if (c->nc == 0)
        c->band = BAND_RAIL;

    c->settings = c->api->lookup(c->api->priv, OA_SVC_SETTINGS, 1);
    c->anim = c->api->lookup(c->api->priv, OA_SVC_UI_ANIM, 1);
    if (c->anim) c->anim->bind(on_anim, c);
    /* v2 adds the recovered text-slide helpers; v1-only hosts simply get no slide. */
    c->anim2 = c->api->lookup(c->api->priv, OA_SVC_UI_ANIM, 2);
    c->clock = c->api->lookup(c->api->priv, OA_SVC_UI_CLOCK, 1);
    c->fs_type = dialog_fs_type();
    {
        const char *cfg = c->api->config_get(c->api->priv, "ui.fs_type", "");
        if (cfg[0])
            c->fs_type = atoi(cfg);   /* explicit override for tests/lab runs */
    }
    if (c->fs_type < 0 || c->fs_type > 8)
        c->fs_type = 0;
    c->slide_box = atoi(c->api->config_get(c->api->priv, "ui.slide_box", "0"));
    /* Vendor UIAnimationController starts disabled and its bootstrap caller is not recovered yet.
     * This default-off switch is an explicit bootstrap/test lever, not a chosen native policy. */
    if (c->anim && atoi(c->api->config_get(c->api->priv, "ui.anim_enable", "0")) == 1)
        c->anim->set_enabled(1);
    /* React to providers that appear later, and drop cached pointers when one is removed. */
    c->mod_key = c->api->subscribe(c->api->priv, OA_TOPIC_MODULE_CHANGED, on_module_changed, c);
    c->svc_key = c->api->subscribe(c->api->priv, OA_TOPIC_SERVICE_REMOVED, on_service_removed, c);
    if (!c->mod_key || !c->svc_key)
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "ui_vendor: lifecycle subscription failed; late providers will not be discovered");
    if (c->api->publish(c->api->priv, OA_SVC_PORTAL, 1, &g_portal) != 0) {
        if (c->anim) {
            c->anim->bind(NULL, NULL);
            c->api->release(c->api->priv, OA_SVC_UI_ANIM);
            c->anim = NULL;
        }
        if (c->settings) {
            c->api->release(c->api->priv, OA_SVC_SETTINGS);
            c->settings = NULL;
        }
        if (c->d) {
            c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
            c->d = NULL;
        }
        return -1;
    }
    c->portal_on = 1;

    /* Vendor main-launcher highlight: 11 frames at 30 ms, played once per image/focus. The
     * decoder advances only while index < frame count (decodeNextFrame 0xb30654) and does not
     * rearm at the end, so the last frame is held; the APNG num_plays field is not consulted.
     * The engine loads rail_hlNN lazily; a missing frame falls back to the flat accent. */
    c->rail_hl_n = 0;
    c->rail_hl_frame = 0;
    c->rail_hl_play = 1;
    c->rail_hl_next = now_ms() + OA_RAIL_HL_MS;
    if (atoi(c->api->config_get(c->api->priv, "ui.rail_hl", "1")) == 1) {
        for (int i = 1; i <= OA_RAIL_HL_FRAMES; i++) {
            char name[32];
            snprintf(name, sizeof name, "rail_hl%02d", i);
            if (!find_icon(c, name))
                break;
            c->rail_hl_n++;
        }
    }

    /* The slide needs the title width, so it starts after the blobs are loaded. The clipped draw
     * needs a scratch buffer of exactly the box width; without it the label would spill. */
    {
        const oa_ui_icon *t = find_icon(c, "title");
        c->title_w = t ? t->w : 0;
        if (t && c->slide_box > 0 && c->slide_box < t->w) {
            size_t need = (size_t)c->slide_box * (size_t)t->h * 4u;
            c->slide_buf = malloc(need);
            if (!c->slide_buf) {
                c->api->log(c->api->priv, OA_LOG_WARN,
                            "text slide: no %zu-byte scratch buffer; slide disabled", need);
                c->slide_box = 0;
            }
        }
    }
    slide_begin(c, "start");

    if (c->d->register_skin(&g_skin, c) != 0) {
        c->api->unpublish(c->api->priv, OA_SVC_PORTAL);
        c->portal_on = 0;
        if (c->anim) {
            c->anim->bind(NULL, NULL);
            c->api->release(c->api->priv, OA_SVC_UI_ANIM);
            c->anim = NULL;
        }
        if (c->settings) {
            c->api->release(c->api->priv, OA_SVC_SETTINGS);
            c->settings = NULL;
        }
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
        return -1;
    }
    if (c->api->config_get(c->api->priv, "ui.paint", "1")[0] == '1')
        paint(c);
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    cancel_dialog(c);
    c->scene_painted = 0;
    if (c->mod_key) {
        c->api->unsubscribe(c->api->priv, c->mod_key);
        c->mod_key = 0;
    }
    if (c->svc_key) {
        c->api->unsubscribe(c->api->priv, c->svc_key);
        c->svc_key = 0;
    }
    if (c->rebind_tick) {
        c->api->timer_del(c->api->priv, c->rebind_tick);
        c->rebind_tick = 0;
    }
    if (c->rail_hl_tick) {
        c->api->timer_del(c->api->priv, c->rail_hl_tick);
        c->rail_hl_tick = 0;
    }
    c->rail_hl_n = 0;
    c->rail_hl_frame = 0;
    c->rail_hl_play = 0;
    c->suspended = 0;
    c->rebind_pending = 0;
    slide_stop_sampler(c);
    if (c->slide_repeat) {
        c->api->timer_del(c->api->priv, c->slide_repeat);
        c->slide_repeat = 0;
    }
    memset(&c->slide, 0, sizeof c->slide);
    if (c->anim)
        c->anim->bind(NULL, NULL);
    if (c->anim || c->anim2) {
        c->api->release(c->api->priv, OA_SVC_UI_ANIM);
        c->anim = NULL;
        c->anim2 = NULL;
    }
    if (c->clock) {
        c->api->release(c->api->priv, OA_SVC_UI_CLOCK);
        c->clock = NULL;
    }
    if (c->settings) {
        c->api->release(c->api->priv, OA_SVC_SETTINGS);
        c->settings = NULL;
    }
    if (c->portal_on) {
        c->api->unpublish(c->api->priv, OA_SVC_PORTAL);
        c->portal_on = 0;
    }
    if (c->d) {
        c->d->unregister_skin(&g_skin);
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
    }
    free(c->slide_buf);
    c->slide_buf = NULL;
    c->title_w = 0;
    c->slide_box = 0;
}

static void m_destroy(void *v)
{
    if (g_ctx == v)
        g_ctx = NULL;
    free(v);
}

static int m_suspend(void *v)
{
    struct ctx *c = v;
    c->suspended = 1;
    cancel_dialog(c);
    c->scene_painted = 0;
    slide_stop_sampler(c);
    if (c->slide_repeat) {
        c->api->timer_del(c->api->priv, c->slide_repeat);
        c->slide_repeat = 0;
    }
    memset(&c->slide, 0, sizeof c->slide);
    if (c->rail_hl_tick) {
        c->api->timer_del(c->api->priv, c->rail_hl_tick);
        c->rail_hl_tick = 0;
    }
    /* the engine hold must be dropped here, or every suspend/resume cycle leaks one */
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
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 1);
    if (!c->d)
        return -1;
    c->suspended = 0;
    if (c->d->register_skin(&g_skin, c) != 0) {
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
        return -1;
    }
    /* a resumed portal replays the focus highlight from its first frame */
    c->rail_hl_frame = 0;
    c->rail_hl_play = 1;
    c->rail_hl_next = now_ms() + OA_RAIL_HL_MS;
    /* providers may have appeared or disappeared while suspended; rebind_now reacquires them and
     * restarts or holds the slide as appropriate (it also paints). */
    if (c->rebind_pending || !c->anim || !c->anim2 || !c->clock)
        rebind_now(c);
    else {
        slide_begin(c, "resume");
        paint(c);
    }
    return 0;
}

static size_t m_save(void *v, void *buf, size_t cap)
{
    struct ctx *c = v;
    if (!c)
        return 0;
    if (!buf || cap < 4)
        return 4;
    unsigned char *bytes = buf;
    bytes[0] = (unsigned char)c->band;
    bytes[1] = (unsigned char)c->rail;
    bytes[2] = (unsigned char)c->card;
    bytes[3] = (unsigned char)c->screen;
    return 4;
}

static int m_restore(void *v, const void *buf, size_t len)
{
    struct ctx *c = v;
    if (!c || !buf || len != 4)
        return -1;
    const unsigned char *bytes = buf;
    if (bytes[0] > BAND_CARDS || bytes[1] > 4 || bytes[2] >= MAX_CARDS || bytes[3] > SCREEN_SETTINGS_LIST)
        return -1;
    c->band = bytes[0];
    c->rail = bytes[1];
    c->card = bytes[2];
    c->screen = bytes[3];
    c->restored = 1;
    return 0;
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "ui_vendor",
    .version = "0.2",
    .requires = { OA_SVC_UI_ENGINE, "" },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
    .suspend = m_suspend,
    .resume = m_resume,
    .save = m_save,
    .restore = m_restore,
};

OA_MODULE_EXPORT(g_module)
