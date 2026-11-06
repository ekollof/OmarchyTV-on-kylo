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
 *   background   rgba(36,38,40,153), from the shipped unnamed rectangle
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
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/events.h"
#include "openaurora/lua_skin.h"
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/channels.h"
#include "openaurora/langprofile.h"
#include "openaurora/menu_model.h"
#include "openaurora/ui_engine.h"
#include "openaurora/ui_time.h"
#include "openaurora/canvas.h"
#include "openaurora/svg_scrollbar.h"
#include "openaurora/font_bitmap.h"
#include "openaurora/font_draw.h"
#include "openaurora/font_pack.h"
#include "openaurora/font_text.h"
#include "openaurora/font_box.h"

#define MAX_CARDS 12
#define MAX_GRID 64
/* Full-screen apps grid: DialogFullScreenPortal.svg appsGrid 183,180,1554x720,
 * 5x3 cells of 310x230, scrollbar 1750,180,8x720 (APPS-GRID). */
#define APPS_X 183
#define APPS_Y 180
#define APPS_COLS 5
#define APPS_ROWS 3
#define APPS_CELL_W 310
#define APPS_CELL_H 230
#define APPS_TRACK_X 1750
#define APPS_TRACK_Y 180
#define APPS_TRACK_W 8
#define APPS_TRACK_H 720
#define APPS_CAP_TOP 20
#define APPS_CAP_BOTTOM 16
#define APPS_VISIBLE (APPS_COLS * APPS_ROWS)
/* Provisional channel list over the stored service inventory. The order is
 * stored channel_no/name only; runtime EASI LCN/visibility is not available
 * offline. The red strip and its label make that non-parity state visible. */
#define CH_ROW_Y 236
#define CH_ROW_H 64
#define CH_ROWS 10
#define CH_X 80
#define CH_W 1760
#define CH_MARKER_H 60

/* Nine picture-settings rows; row 6 is disabled in the capture. Defined here
 * because ctx stores per-row live-text canvases. */
#define SETTINGS_ROW_COUNT 9
#define SETTINGS_TEXT_MAX 17

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
    char grid_cards[MAX_GRID][48];
    int grid_count;
    int grid_sel;
    int band;
    int rail;
    int card;
    int src;                         /* source-selection row, 0..MAX_SOURCES-1 */
    int sel;                         /* picture-settings row, -1 = none */
    int sub_sel;                     /* JSON sub-launcher card, -1 = none */
    float sub_progress;              /* lab capture progress 0..1 */
    int sub_anim_live;               /* runtime key animation is running */
    uint32_t sub_anim_start;         /* ui.clock timestamp */
    uint64_t sub_tick;               /* 10 ms repaint timer */
    int chan_sel;                    /* provisional channel-list selection */
    int text_on;                     /* live labels/values loaded (ui.text=1) */
    struct oa_font_pack font_pack;
    unsigned char *font_data, *font_num_data;
    struct oa_bitmap_font font;      /* font7: row labels and text values */
    struct oa_bitmap_font font_num;  /* font16: slider digits and disabled label */
    int font_num_on;
    struct oa_bitmap_font app_font;  /* font4: focused apps-grid label */
    unsigned char *app_font_data;
    int app_font_on;
    char grid_labels[MAX_GRID][64];
    struct oa_bitmap_font font_title; /* font18: tab title */
    unsigned char *font_title_data;
    struct settings_text_canvas {
        unsigned char *rgba;
        unsigned char *rgba_focus;   /* same glyphs in focused-row ink (#7E000A) */
        int w, h, x, y;
    } text[SETTINGS_TEXT_MAX];
    struct settings_text_canvas title; /* live tab title blob (no focus variant) */
    /* Sub-launcher live text: font3 labels + font12 values, localized through
     * ui.i18n. Each blob is one card-sized transparent text surface. */
    const oa_i18n_iface *i18n;
    struct oa_bitmap_font sub_font, sub_value_font;
    unsigned char *sub_font_data, *sub_value_data;
    int sub_fonts_on;
    struct { unsigned char *rgba; int w, h; } sub_text[3];
    int sub_text_ready;
    char sub_text_locale[8];
    /* Selected value index per SUB_CARDS item, in branch item order. -1 means
     * the item has no decoded value list yet (MODE -> picture_modes). The
     * initial values reproduce the p0/03 capture (MAXIMUM_STR, ZOOM_MODE_16_9).
     * ui.state_file persists KEY=index across module reload/process restart;
     * default /tmp is boot-scoped, a /conf path survives reboot. */
    int sub_value[3];
    char sub_state_file[128];
    /* Lab value editing (ui.edit=1): per-process shadow values for the four
     * slider rows 1..4. Range 0..100 is an INVENTED placeholder (per-setting
     * bounds come from runtime provider virtuals, not decodable statically);
     * step 1 matches the slidebar ctor default and setSliderStep has no
     * caller. Slider graphics follow these same shadow values. Never
     * touches the settings database; no set_int call exists on this path. */
    int edit_val[4];
    int screen;
    int restored;
    const oa_settings_iface *settings;
    const oa_ui_theme_iface *theme; /* held; resolve/copy policy, never retain borrowed views */
    const oa_lua_iface *lua;        /* optional ui.lua; hooks only, pixels stay vendor */
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
    int rail_hl_late;                /* previous paint overran its 30 ms slot */
    uint64_t rail_hl_next;           /* next frame due, host monotonic ms */
    uint64_t rail_hl_tick;
    int portal_on;
    int fps;
    uint32_t sub_key;
    uint32_t ready_key;              /* ui.engine bound the display: repaint */
    uint64_t tick;    /* repaint timer, when ui.fps > 0 */
};

static struct ctx *g_ctx;
static void paint(struct ctx *c);
static const oa_ui_skin g_skin;
static void skin_dispatch(void *ud)
{
    struct ctx *c = ud;
    if (c && c->d && !c->suspended) c->d->dispatch_skin();
}

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
/* DemoApp::checkUIDesign constructs SVGDesignChecker and calls
 * UIFramework::setAnimationEnable(checkAnimation()). checkAnimation returns the
 * flag set by checkUsabilityStyle when DesignStyle.svg (resource 79) contains a
 * rectangle named "animation"; this TV's resource 79 does. The explicit
 * ui.anim_enable lever remains for tests/other themes. */
static int theme_has_animation(struct ctx *c)
{
    struct oa_binsvg resource;
    struct oa_binsvg_rect rect;
    return c->theme && !c->theme->named("DesignStyle.svg", &resource) &&
           !oa_binsvg_find_rect(&resource, NULL, "animation", &rect);
}

static void apply_anim_policy(struct ctx *c)
{
    if (!c || !c->anim) return;
    const char *override = c->api->config_get(c->api->priv, "ui.anim_enable", "");
    if (override[0]) {
        int enabled = atoi(override) == 1;
        (void)c->anim->set_enabled(enabled);
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "animations %s (ui.anim_enable override)",
                    enabled ? "enabled" : "disabled");
        return;
    }
    /* No theme is not a policy source: leave the controller state untouched
     * for tests/consumers that enabled it directly. */
    if (!c->theme) return;
    int enabled = theme_has_animation(c);
    (void)c->anim->set_enabled(enabled);
    c->api->log(c->api->priv, OA_LOG_INFO, "animations %s (%s)",
                enabled ? "enabled" : "disabled",
                enabled ? "DesignStyle.svg animation marker" :
                "no DesignStyle animation marker");
}

static int dialog_fs_type(struct ctx *c)
{
    const char *override = c->api->config_get(c->api->priv, "ui.fs_type", "");
    if (override[0]) {
        int type = atoi(override); /* existing explicit lab override */
        return type >= 0 && type <= 8 ? type : 0;
    }
    /* Compatibility fallback for setups without the optional provider: the
     * previously verified DialogLauncher resource has only mode60/type4.
     * An available provider with no resource/marker instead means no effect. */
    if (!c->theme) return 4;
    struct oa_binsvg resource;
    if (c->theme->named("DialogLauncher.svg", &resource)) return 0;
    static const char *const markers[] = {
        "animation_flip_fade_in", "animation_slide_right_fade_in",
        "animation_slide_left_fade_in", "animation_slide_up_fade_in",
        "animation_slide_down_fade_in", "animation_fade_in",
        "animation_grow_fade_in", "animation_bounce_fade_in"
    };
    struct oa_binsvg_rect rect;
    for (unsigned i = 0; i < sizeof markers / sizeof *markers; ++i)
        if (!oa_binsvg_find_rect(&resource, NULL, markers[i], &rect)) return (int)i + 1;
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
static void paint_rail_hl(struct ctx *c);

/* ---------------------------------------------------------------- loading */

static const oa_ui_icon *find_icon(struct ctx *c, const char *name)
{
    return c && c->d ? c->d->icon(name) : NULL;
}

/* ------------------------------------------------------------------ geometry */

enum { SCREEN_PORTAL = 0, SCREEN_OPEN = 1, SCREEN_SOURCES = 2, SCREEN_SETTINGS = 3,
       SCREEN_SETTINGS_LIST = 4, SCREEN_APPS = 5, SCREEN_CHANNELS = 6,
       SCREEN_SUBLAUNCHER = 7, ANIM_DIALOG = 1 };

/* Picture-settings rows: 66 px FA bar content on the 70 px pitch, text
 * centered (row 4 centre 496). Row 6 ("Beeld zoomen") is disabled in the
 * capture and is skipped by navigation. The bar Y0=186 is firmware-derived:
 * resource 179 listBox (20,170,770,996) + top inset 16 + 70-step rows
 * (SVGListBoxBase::getRowPosition), rows created with those rects
 * (createListItems → setWindowRect) and the row surface resized to the row
 * rect. The 66 px bar content sits top-aligned (HorizontalMaskScaleTransform
 * translates y only); the bottom 4 px strip keeps surface content. The old
 * fitted Y0=183 is superseded. */
#define SETTINGS_ROW_X 20
#define SETTINGS_ROW_W 750
#define SETTINGS_ROW_H 66
#define SETTINGS_ROW_Y0 186
#define SETTINGS_ROW_PITCH 70
#define SETTINGS_ROW_DISABLED 6

/* Live settings text: font7 for row labels/text values, font16 for the larger
 * slider digits and the disabled "Beeld zoomen" label. Origins come from
 * fitting each string against the corrected panel capture (labels IoU ~0.98,
 * values 0.85..0.95). Text values are centered around x=577, digits around
 * x=682; the origins below are the fitted text-box top-lefts. */
#define SETTINGS_TEXT_MARGIN 4
#define SETTINGS_TEXT_HEIGHT 72
#define FONT_ROW 7
#define FONT_LARGE 16
enum { TEXT_LABEL = 0, TEXT_VALUE = 1 };
static const struct settings_text_entry {
    unsigned char row, kind, font;
    unsigned char r, g, b;
    const char *text;
    int x, y;
} SETTINGS_TEXT[] = {
    { 0, TEXT_LABEL, FONT_ROW, 255, 255, 255, "Modus", 49, 184 },
    { 1, TEXT_LABEL, FONT_ROW, 255, 255, 255, "Contrast", 79, 254 },
    { 2, TEXT_LABEL, FONT_ROW, 255, 255, 255, "Helderheid", 79, 324 },
    { 3, TEXT_LABEL, FONT_ROW, 255, 255, 255, "Scherpte", 79, 394 },
    { 4, TEXT_LABEL, FONT_ROW, 255, 255, 255, "Kleur", 79, 464 },
    { 5, TEXT_LABEL, FONT_ROW, 255, 255, 255, "Energiebesparing", 49, 534 },
    { 6, TEXT_LABEL, FONT_LARGE, 170, 170, 170, "Beeld zoomen", 51, 602 },
    { 7, TEXT_LABEL, FONT_ROW, 255, 255, 255, "Virtual HDR", 49, 674 },
    { 8, TEXT_LABEL, FONT_ROW, 255, 255, 255, "Geavanceerde instellinge", 49, 744 },
    { 0, TEXT_VALUE, FONT_ROW, 255, 255, 255, "Natuurlijk", 493, 183 },
    { 1, TEXT_VALUE, FONT_LARGE, 255, 255, 255, "95", 659, 250 },
    { 2, TEXT_VALUE, FONT_LARGE, 255, 255, 255, "50", 659, 320 },
    { 3, TEXT_VALUE, FONT_LARGE, 255, 255, 255, "50", 659, 390 },
    { 4, TEXT_VALUE, FONT_LARGE, 255, 255, 255, "60", 659, 460 },
    { 5, TEXT_VALUE, FONT_ROW, 255, 255, 255, "Uit", 553, 533 },
    { 6, TEXT_VALUE, FONT_ROW, 170, 170, 170, "Auto", 536, 605 },
    { 7, TEXT_VALUE, FONT_ROW, 255, 255, 255, "Aan", 545, 673 },
};
#define SETTINGS_TEXT_COUNT (sizeof SETTINGS_TEXT / sizeof SETTINGS_TEXT[0])
_Static_assert(SETTINGS_TEXT_COUNT == SETTINGS_TEXT_MAX, "settings text table size");

/* Live tab title. The vendor picture-settings screen shows "Instellingen"
 * (SETTINGS default), not "Beeld": per-tab keys from updateGlyphAndTitle
 * apply on tab switches, which are not implemented — the freshly opened
 * dialog carries the SETTINGS default (verified against the corrected
 * capture: title ink x120..388, same per-glyph advance as our font-18
 * render, confirming the font choice). The string is the verified Dutch
 * profile entry for SETTINGS. Box (108,5,691,95) h90, font 18, white ink,
 * left aligned, as composed (tests/textbox_layout.c corpus). The dialog is
 * assumed at screen origin (panel records start at 0,0); the runtime
 * dynamic-center-align override is not modeled. Glyph ink inside the box
 * comes from the real font-18 bearings. */
#define SETTINGS_TITLE_TEXT "Instellingen"
#define SETTINGS_TITLE_X 108
#define SETTINGS_TITLE_Y 5
#define SETTINGS_TITLE_W 583
#define SETTINGS_TITLE_H 90

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

/* p0/03 picture-shortcut row, driven by the JSON ui.menu picture branch.
 * The card cutouts are the vendor's own pixels; the row origin/pitch and the
 * 300 ms scale come from the checked JSON layout and launcher_anim. The lab
 * ui.sub_sel/ui.sub_progress knobs select a card and sample the animation. */
static const struct {
    const char *key, *asset, *clean;
} SUB_CARDS[] = {
    { "MODE", "sublauncher_card_0", "sublauncher_card_0_bg" },
    { "ENERGY_SAVING", "sublauncher_card_1", "sublauncher_card_1_bg" },
    { "PICTURE_ZOOM", "sublauncher_card_2", "sublauncher_card_2_bg" },
};

/* The p0/03 picture-shortcut state is also the fallback when the persisted
 * state has no valid index. Vendor values visible in the oracle capture. */
static int sub_card_for_item(const char *key)
{
    for (unsigned i = 0; i < sizeof SUB_CARDS / sizeof SUB_CARDS[0]; i++)
        if (!strcmp(SUB_CARDS[i].key, key)) return (int)i;
    return -1;
}

/* A string-valued "values" field is a firmware source marker, not a value
 * list: MODE -> picture_modes, system rows -> sleep_timer_values etc. Those
 * lists are decoded elsewhere; do not render or cycle the marker itself. */
static int sub_values_dynamic(const struct oa_menu_item *item)
{
    if (item->dynamic) return 1;
    if (item->value_count != 1) return 0;
    const char *v = item->values[0].key;
    return !strcmp(v, "picture_modes") || !strcmp(v, "sleep_timer_values") ||
           !strcmp(v, "auto_tv_off_values") || !strcmp(v, "languages") ||
           !strcmp(v, "favourite_lists");
}

static int sub_default_value_index(const struct oa_menu_item *item)
{
    const char *want = NULL;
    if (!strcmp(item->key, "ENERGY_SAVING")) want = "MAXIMUM_STR";
    else if (!strcmp(item->key, "PICTURE_ZOOM")) want = "ZOOM_MODE_16_9";
    if (!want) return -1;
    for (unsigned i = 0; i < item->value_count; i++)
        if (!strcmp(item->values[i].key, want)) return (int)i;
    return item->value_count ? 0 : -1;
}

static int sub_value_index(struct ctx *c, const struct oa_menu_item *item,
                           int card)
{
    if (!item || !item->value_count || card < 0 ||
        sub_values_dynamic(item))
        return -1;
    int index = c->sub_value[card];
    if (index < 0 || (unsigned)index >= item->value_count) {
        index = sub_default_value_index(item);
        if (index < 0) index = 0;
        c->sub_value[card] = index;
    }
    return index;
}

static const char *sub_value_key(struct ctx *c,
                                 const struct oa_menu_item *item, int card)
{
    int index = sub_value_index(c, item, card);
    if (index >= 0) return item->values[index].key;
    /* MODE's picture_modes list is a profile list, not a JSON array; the
     * p0/03 capture value remains the fallback until it is decoded. */
    if (!strcmp(item->key, "MODE")) return "NATURAL";
    return NULL;
}

static void sub_state_defaults(struct ctx *c)
{
    c->sub_value[0] = -1; /* MODE: values come from dynamic picture_modes */
    c->sub_value[1] = 3;  /* MAXIMUM_STR */
    c->sub_value[2] = 1;  /* ZOOM_MODE_16_9 */
}

static void sub_state_load(struct ctx *c)
{
    const char *path = c->api->config_get(c->api->priv, "ui.state_file",
        "/tmp/openaurora-sub-launcher.state");
    snprintf(c->sub_state_file, sizeof c->sub_state_file, "%s", path);
    if (!c->sub_state_file[0]) return;
    FILE *f = fopen(c->sub_state_file, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *end = NULL;
        long index = strtol(eq + 1, &end, 10);
        if (!end || (*end != '\0' && *end != '\n')) continue;
        int card = sub_card_for_item(line);
        if (card >= 0 && index >= -1 && index <= 64)
            c->sub_value[card] = (int)index;
    }
    fclose(f);
}

static void sub_state_save(struct ctx *c)
{
    if (!c->sub_state_file[0]) return;
    char tmp[160];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", c->sub_state_file) >=
        (int)sizeof tmp)
        return;
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    for (unsigned i = 0; i < sizeof SUB_CARDS / sizeof SUB_CARDS[0]; i++)
        if (c->sub_value[i] >= 0)
            fprintf(f, "%s=%d\n", SUB_CARDS[i].key, c->sub_value[i]);
    if (fclose(f) != 0 || rename(tmp, c->sub_state_file) != 0)
        remove(tmp);
}

static const uint8_t sub_white[4] = { 255, 255, 255, 255 };
static void sub_text_free(struct ctx *c);

/* Ink bounds relative to the pen, with a margin so negative bearings do not
 * clip. Font3/font12 are the vendor card fonts (ink boxes match p0/03). */
static int sub_text_ink(const struct oa_bitmap_font *font, const char *text,
                        int *x0, int *y0, int *w, int *h)
{
    unsigned char *scratch = calloc(1024u * 200u, 4);
    if (!scratch) return ENOMEM;
    oa_canvas cv = { scratch, 1024, 200 };
    int rc = oa_font_text_draw(&cv, font, text, 60, 60, sub_white, 0, NULL);
    if (rc) {
        free(scratch);
        return rc;
    }
    int minx = 1024, miny = 200, maxx = -1, maxy = -1;
    for (int y = 0; y < 200; y++)
        for (int x = 0; x < 1024; x++)
            if (scratch[((size_t)y * 1024 + x) * 4 + 3]) {
                if (x < minx) minx = x;
                if (y < miny) miny = y;
                if (x > maxx) maxx = x;
                if (y > maxy) maxy = y;
            }
    free(scratch);
    if (maxx < 0) return ENOENT;
    *x0 = minx - 60;
    *y0 = miny - 60;
    *w = maxx - minx + 1;
    *h = maxy - miny + 1;
    return 0;
}

static int sub_text_draw(oa_canvas *cv, const struct oa_bitmap_font *font,
                         const char *text, int pen_y)
{
    /* Center the rendered ink in the card. This is language-neutral: a longer
     * translation grows symmetrically and clips at both card edges instead of
     * using a Dutch-fitted offset. */
    int x0 = 0, y0 = 0, w = 0, h = 0;
    int rc = sub_text_ink(font, text, &x0, &y0, &w, &h);
    if (rc) return rc;
    int pen_x = (cv->width - w) / 2 - x0;
    return oa_font_text_draw(cv, font, text, pen_x, pen_y, sub_white, 0, NULL);
}

static int sub_text_localized(oa_canvas *cv, const struct oa_bitmap_font *font,
                              const oa_i18n_iface *i18n, const char *key,
                              const char *locale, int pen_y)
{
    char clean[256], visual[256];
    const char *loc = locale ? locale : i18n->current_locale();
    const char *text = i18n->translate(key, loc);
    if (oa_langprofile_sanitize(text, clean, sizeof clean) != 0) return EINVAL;
    /* The Hebrew profile values are already stored in visual order (e.g.
     * MODE is 'בצמ', the display form of 'מצב'). Dictionary::ArabicShaping
     * converts them to logical order for the vendor's RTL text engine, whose
     * layout restores the stored visual order. This renderer draws LTR, so it
     * must use the stored order directly and must not reverse it again.
     * Arabic/Persian values are stored logical and pre-shaped, so those need
     * the run reorder before LTR drawing. */
    text = clean;
    if (loc && (!strcmp(loc, "ara") || !strcmp(loc, "per")) &&
        oa_langprofile_visual_order(clean, visual, sizeof visual) == 0)
        text = visual;
    return sub_text_draw(cv, font, text, pen_y);
}

/* Render the three localized label/value pairs into card-sized transparent
 * blobs. The label/value pen lines are fixed from the captured reference
 * strings; localized ink heights then vary with the font exactly like the
 * vendor. A missing glyph falls back to English, then the ASCII key. */
static int sub_text_build(struct ctx *c, const struct oa_menu_branch *b)
{
    if (!c->i18n || !c->sub_fonts_on || !b || b->item_count < 3)
        return EINVAL;
    const char *locale = c->i18n->current_locale();
    if (!locale) return EINVAL;
    int x0, y0, w, h;
    if (sub_text_ink(&c->sub_font, "Modus", &x0, &y0, &w, &h))
        return EINVAL;
    int label_pen = 19 - y0;
    if (sub_text_ink(&c->sub_value_font, "Natuurlijk", &x0, &y0, &w, &h))
        return EINVAL;
    int value_pen = 76 - y0;
    unsigned char *blobs[3] = { NULL, NULL, NULL };
    for (unsigned i = 0; i < 3; i++) {
        const char *key = b->items[i].key;
        int card = sub_card_for_item(key);
        if (card < 0) goto fail;
        const char *value_key = sub_value_key(c, &b->items[i], card);
        if (!value_key) goto fail;
        unsigned char *rgba = calloc(240u * 130u, 4);
        if (!rgba) goto fail;
        oa_canvas cv = { rgba, 240, 130 };
        int rc = sub_text_localized(&cv, &c->sub_font, c->i18n, key, locale,
                                    label_pen);
        if (rc) rc = sub_text_localized(&cv, &c->sub_font, c->i18n, key, "eng",
                                        label_pen);
        if (rc) rc = sub_text_draw(&cv, &c->sub_font, key, label_pen);
        if (rc) { free(rgba); goto fail; }
        rc = sub_text_localized(&cv, &c->sub_value_font, c->i18n, value_key,
                                locale, value_pen);
        if (rc) rc = sub_text_localized(&cv, &c->sub_value_font, c->i18n,
                                        value_key, "eng", value_pen);
        if (rc) rc = sub_text_draw(&cv, &c->sub_value_font, value_key,
                                   value_pen);
        if (rc) { free(rgba); goto fail; }
        blobs[i] = rgba;
    }
    sub_text_free(c);
    for (unsigned i = 0; i < 3; i++) {
        c->sub_text[i].rgba = blobs[i];
        c->sub_text[i].w = 240;
        c->sub_text[i].h = 130;
    }
    snprintf(c->sub_text_locale, sizeof c->sub_text_locale, "%s", locale);
    c->sub_text_ready = 1;
    return 0;
fail:
    for (unsigned i = 0; i < 3; i++) free(blobs[i]);
    return EINVAL;
}

static uint32_t sub_now(struct ctx *c)
{
    return c->clock ? c->clock->now_ms() : (uint32_t)now_ms();
}

static uint32_t sub_elapsed(struct ctx *c, uint32_t now, uint32_t before)
{
    return c->clock ? c->clock->elapsed(now, before) : now - before;
}

static void sub_tick_cb(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    if (c->screen != SCREEN_SUBLAUNCHER || !c->d) {
        c->sub_anim_live = 0;
        if (c->sub_tick) {
            c->api->timer_del(c->api->priv, c->sub_tick);
            c->sub_tick = 0;
        }
        return;
    }
    uint32_t elapsed = sub_elapsed(c, sub_now(c), c->sub_anim_start);
    if (elapsed >= 300) {
        c->sub_anim_live = 0;
        c->sub_progress = 1.0f;
        if (c->sub_tick) {
            c->api->timer_del(c->api->priv, c->sub_tick);
            c->sub_tick = 0;
        }
        paint(c);
        return;
    }
    paint(c);
}

static void sub_tick_start(struct ctx *c)
{
    if (c->sub_tick) return;
    c->sub_tick = c->api->timer_add(c->api->priv, 10, 10, sub_tick_cb, c);
}

static void paint_sub_launcher(struct ctx *c)
{
    if (!c || !c->d) return;
    const oa_ui_engine_iface *d = c->d;
    d->clear(13, 14, 14);
    const oa_ui_icon *top = find_icon(c, "settings_top");
    if (top) d->blit(0, 240, top->w, top->h, top->rgba);
    const oa_ui_icon *cat = find_icon(c, "settings_cat");
    if (cat) d->blit(0, 440, cat->w, cat->h, cat->rgba);
    const oa_ui_icon *rail = find_icon(c, "settings_rail");
    if (rail) d->blit(0, 180, rail->w, rail->h, rail->rgba);

    const oa_ui_menu_iface *menu =
        c->api->lookup(c->api->priv, OA_SVC_UI_MENU, 1);
    const struct oa_menu_dialog *md = menu ? menu->dialog("sub_launcher") : NULL;
    const struct oa_menu_branch *mb = md ? menu->branch(md, 2) : NULL;
    if (!mb || mb->item_count == 0) {
        if (menu) c->api->release(c->api->priv, OA_SVC_UI_MENU);
        c->scene_painted = 1;
        (void)d->present();
        return;
    }
    unsigned count = mb->item_count > 3 ? 3 : mb->item_count;
    int sel = c->sub_sel;
    if (sel >= (int)count) sel = (int)count - 1;
    float progress = sel >= 0 ? c->sub_progress : 0.0f;
    if (c->sub_anim_live && sel >= 0) {
        uint32_t elapsed = sub_elapsed(c, sub_now(c), c->sub_anim_start);
        progress = elapsed >= 300 ? 1.0f : (float)elapsed / 300.0f;
    }
    if (progress < 0) progress = 0;
    if (progress > 1) progress = 1;

    c->api->log(c->api->priv, OA_LOG_DEBUG,
                "sub-row md=%p items=%u card=%d,%d %dx%d pitch=%d sel=%d p=%.2f",
                (void *)md, count, md->card_x, md->card_y, md->card_w,
                md->card_h, md->card_pitch, sel, progress);
    if (sel >= 0) {
        /* The unfocused row is the captured vendor row itself; do not repaint
         * its gaps. Only the focused card's original+scaled union needs
         * clearing before the scaled card is drawn over it. */
        oa_launcher_rect base, scaled;
        oa_menu_card_rect(md, (unsigned)sel, 0.0f, &base, NULL);
        oa_menu_card_rect(md, (unsigned)sel, progress, &scaled, NULL);
        int x1 = base.x1 < scaled.x1 ? base.x1 : scaled.x1;
        int y1 = base.y1 < scaled.y1 ? base.y1 : scaled.y1;
        int x2 = base.x2 > scaled.x2 ? base.x2 : scaled.x2;
        int y2 = base.y2 > scaled.y2 ? base.y2 : scaled.y2;
        d->fill(x1, y1, x2 - x1, y2 - y1, 13, 14, 14);
    }
    if (!c->sub_text_ready && c->i18n && c->sub_fonts_on)
        (void)sub_text_build(c, mb);
    int live = c->sub_text_ready;
    for (int pass = 0; pass < 2; pass++) {
        for (unsigned i = 0; i < count; i++) {
            if ((pass == 0 && (int)i == sel) || (pass == 1 && (int)i != sel))
                continue;
            const char *asset = NULL, *clean = NULL;
            for (unsigned j = 0; j < sizeof SUB_CARDS / sizeof SUB_CARDS[0]; j++)
                if (!strcmp(mb->items[i].key, SUB_CARDS[j].key)) {
                    asset = SUB_CARDS[j].asset;
                    clean = SUB_CARDS[j].clean;
                }
            int use_text = live && clean && c->sub_text[i].rgba;
            const oa_ui_icon *card =
                find_icon(c, use_text ? clean : asset);
            if (!card && use_text) {
                use_text = 0;
                card = find_icon(c, asset);
            }
            c->api->log(c->api->priv, OA_LOG_DEBUG,
                        "sub-card i=%u key=%s asset=%s text=%d icon=%p", i,
                        mb->items[i].key, asset ? asset : "(none)", use_text,
                        (void *)card);
            if (!card) continue;
            oa_launcher_rect r;
            oa_menu_card_rect(md, i, (int)i == sel ? progress : 0.0f, &r,
                              NULL);
            int w = r.x2 - r.x1, h = r.y2 - r.y1;
            if (w == card->w && h == card->h)
                d->blit(r.x1, r.y1, w, h, card->rgba);
            else
                d->blit_scaled(r.x1, r.y1, w, h, card->w, card->h,
                               card->rgba);
            if (use_text) {
                /* Text is independent of the scalerect: keep it at the
                 * unscaled card origin so only the card surface grows. */
                int tx = md->runtime_item_x + (int)i * md->card_pitch;
                int ty = md->card_y;
                d->blit(tx, ty, c->sub_text[i].w, c->sub_text[i].h,
                        c->sub_text[i].rgba);
            }
        }
    }
    c->api->release(c->api->priv, OA_SVC_UI_MENU);
    c->scene_painted = 1;
    if (d->present() != 0)
        c->api->log(c->api->priv, OA_LOG_ERROR, "sub-launcher present failed");
}

static void sub_text_free(struct ctx *c)
{
    for (int i = 0; i < 3; i++) {
        free(c->sub_text[i].rgba);
        memset(&c->sub_text[i], 0, sizeof c->sub_text[i]);
    }
    c->sub_text_ready = 0;
    c->sub_text_locale[0] = '\0';
}

static void settings_text_free(struct ctx *c)
{
    for (int i = 0; i < SETTINGS_TEXT_MAX; i++) {
        free(c->text[i].rgba);
        free(c->text[i].rgba_focus);
        memset(&c->text[i], 0, sizeof c->text[i]);
    }
    free(c->font_data);
    free(c->font_num_data);
    c->font_data = c->font_num_data = NULL;
    free(c->app_font_data);
    c->app_font_data = NULL;
    c->app_font_on = 0;
    free(c->title.rgba);
    free(c->font_title_data);
    c->title.rgba = NULL;
    c->font_title_data = NULL;
    oa_font_pack_free(&c->font_pack);
    c->text_on = 0;
    c->font_num_on = 0;
    free(c->sub_font_data);
    free(c->sub_value_data);
    c->sub_font_data = c->sub_value_data = NULL;
    c->sub_fonts_on = 0;
    sub_text_free(c);
}

static int settings_font_open(struct oa_font_pack *pack, int id,
                              struct oa_bitmap_font *font, unsigned char **data)
{
    unsigned char *buf = NULL;
    size_t size = 0;
    int rc = oa_font_pack_read(pack, (unsigned)id, &buf, &size);
    if (rc) return rc;
    rc = oa_bitmap_font_open(font, buf, size);
    if (rc) {
        free(buf);
        return rc;
    }
    *data = buf;
    return 0;
}

/* The stock apps grid draws the focused application name in the orange strip
 * with the same EF41 font as the Markt label (record 4 in this theme pack). */
static int app_label_open(struct ctx *c)
{
    if (!c || c->app_font_on) return 0;
    if (!c->font_pack.count) {
        const char *index_path = c->api->config_get(c->api->priv, "ui.font_index",
            "/vendor2/dynamic_ui/telefunken18_font_byte_data.txt");
        const char *pack_path = c->api->config_get(c->api->priv, "ui.font_pack",
            "/vendor2/dynamic_ui/telefunken18_font_byte_data.bin");
        int rc = oa_font_pack_init(&c->font_pack, index_path, pack_path);
        if (rc) return rc;
    }
    int id = atoi(c->api->config_get(c->api->priv, "ui.app_font_id", "4"));
    int rc = settings_font_open(&c->font_pack, id, &c->app_font,
        &c->app_font_data);
    if (rc) return rc;
    c->app_font_on = 1;
    return 0;
}

/* Optional live text (ui.text=1): load font7 and font16 and render every
 * settings label/value once. On any failure keep the captured panel working. */
static int render_text_blob(struct ctx *c, size_t i, const char *text)
{
    const struct settings_text_entry *entry = &SETTINGS_TEXT[i];
    const struct oa_bitmap_font *font =
        entry->font == FONT_LARGE ? &c->font_num : &c->font;
    int advance = 0;
    int rc = oa_font_text_measure(font, text, 0, &advance);
    if (rc) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "settings text: '%s' unavailable (%d); using capture", text, rc);
        return rc;
    }
    int w = advance + 2 * SETTINGS_TEXT_MARGIN;
    int h = SETTINGS_TEXT_HEIGHT;
    if (w < 1) w = 1;
    unsigned char *rgba = calloc((size_t)w * (size_t)h * 4u, 1);
    unsigned char *focus = calloc((size_t)w * (size_t)h * 4u, 1);
    if (!rgba || !focus) {
        free(rgba);
        free(focus);
        return ENOMEM;
    }
    oa_canvas canvas = { rgba, w, h };
    const uint8_t ink[4] = { entry->r, entry->g, entry->b, 255 };
    rc = oa_font_text_draw(&canvas, font, text, SETTINGS_TEXT_MARGIN,
                           SETTINGS_TEXT_MARGIN, ink, 0, NULL);
    if (!rc) {
        /* Focused rows paint on the #FE9419 bar, which carries #7E000A ink
         * (row textboxes 488/496/504 and the reconstructed overlay agree). */
        oa_canvas fcanvas = { focus, w, h };
        const uint8_t fink[4] = { 126, 0, 10, 255 };
        rc = oa_font_text_draw(&fcanvas, font, text, SETTINGS_TEXT_MARGIN,
                               SETTINGS_TEXT_MARGIN, fink, 0, NULL);
    }
    if (rc) {
        free(rgba);
        free(focus);
        return rc;
    }
    free(c->text[i].rgba);
    free(c->text[i].rgba_focus);
    c->text[i].rgba = rgba;
    c->text[i].rgba_focus = focus;
    c->text[i].w = w;
    c->text[i].h = h;
    c->text[i].x = entry->x - SETTINGS_TEXT_MARGIN;
    c->text[i].y = entry->y - SETTINGS_TEXT_MARGIN;
    return 0;
}

/* Live tab title blob: white SETTINGS_TITLE_TEXT in font18 at the composed
 * box origin. No focus variant (the title never sits on the orange bar) and
 * no margins: the box (108,5,691,95) is already the surface-space text
 * surface; alignment4 applies start/center layout before glyph bearings. A
 * wider-than-box advance fails closed (fallback to the captured title). */
static int render_title_blob(struct ctx *c)
{
    int advance = 0;
    int rc = oa_font_text_measure(&c->font_title, SETTINGS_TITLE_TEXT, 0, &advance);
    if (rc) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "settings title unavailable (%d); using capture", rc);
        return rc;
    }
    if (advance < 1)
        advance = 1;
    if (advance > SETTINGS_TITLE_W)
        return EOVERFLOW;
    unsigned char *rgba = calloc((size_t)SETTINGS_TITLE_W * (size_t)SETTINGS_TITLE_H * 4u, 1);
    if (!rgba)
        return ENOMEM;
    oa_canvas canvas = { rgba, SETTINGS_TITLE_W, SETTINGS_TITLE_H };
    static const uint8_t ink[4] = { 255, 255, 255, 255 };
    rc = oa_font_text_draw_box(&canvas, &c->font_title, SETTINGS_TITLE_TEXT, 4, 0, ink, NULL);
    if (rc) {
        free(rgba);
        return rc;
    }
    free(c->title.rgba);
    c->title.rgba = rgba;
    c->title.rgba_focus = NULL;
    c->title.w = SETTINGS_TITLE_W;
    c->title.h = SETTINGS_TITLE_H;
    c->title.x = SETTINGS_TITLE_X;
    c->title.y = SETTINGS_TITLE_Y;
    return 0;
}

/* Strict bounded decimal parse: digits only, at most `max`, advancing *pp.
 * Returns 0 on success. Rejects empty, non-digit and out-of-range tokens, so a
 * malformed lab preset cannot silently become a valid value. */
static int parse_bounded_int(const char **pp, int max, int *out)
{
    const char *p = *pp;
    int v = 0, n = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        if (v > max)
            return -1;
        p++;
        n++;
    }
    if (n == 0)
        return -1;
    *pp = p;
    *out = v;
    return 0;
}

/* Lab preset for captures without key injection: "r=v,r=v" with rows 1..4
 * and values 0..100. Malformed or out-of-range tokens are ignored. */
static void apply_edit_init(struct ctx *c)
{
    if (atoi(c->api->config_get(c->api->priv, "ui.edit", "0")) != 1)
        return;
    const char *spec = c->api->config_get(c->api->priv, "ui.edit_init", "");
    while (*spec) {
        while (*spec == ' ' || *spec == ',')
            spec++;
        if (!*spec)
            break;
        int row = 0, val = 0;
        if (parse_bounded_int(&spec, 4, &row) != 0 || *spec != '=' ||
            (spec++, parse_bounded_int(&spec, 100, &val) != 0) || row < 1) {
            while (*spec && *spec != ',')
                spec++;
            continue;
        }
        if (*spec && *spec != ',') {   /* trailing junk, e.g. "49junk" */
            while (*spec && *spec != ',')
                spec++;
            continue;
        }
        c->edit_val[row - 1] = val;
        if (c->text_on) {
            char text[16];
            snprintf(text, sizeof text, "%d", val);
            size_t idx = 9u + (unsigned)row;
            if (idx < SETTINGS_TEXT_COUNT &&
                SETTINGS_TEXT[idx].kind == TEXT_VALUE &&
                render_text_blob(c, idx, text) != 0)
                settings_text_free(c);
        }
        while (*spec && *spec != ',')
            spec++;
    }
}

/* Sub-launcher card text uses the vendor item fonts: font3 for the label and
 * font12 for the value (ink boxes match the p0/03 capture). The font pack is
 * already open from settings_text_load. */
static int sub_text_fonts_load(struct ctx *c)
{
    if (!c || !c->text_on || c->sub_fonts_on) return EINVAL;
    int label_id = atoi(c->api->config_get(c->api->priv, "ui.sub_font_id", "3"));
    int value_id = atoi(c->api->config_get(c->api->priv, "ui.sub_value_font_id", "12"));
    if (label_id < 0 || value_id < 0) return EINVAL;
    int rc = settings_font_open(&c->font_pack, label_id, &c->sub_font,
                                &c->sub_font_data);
    if (rc) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "sub text: font %d unavailable (%d)", label_id, rc);
        return rc;
    }
    rc = settings_font_open(&c->font_pack, value_id, &c->sub_value_font,
                            &c->sub_value_data);
    if (rc) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "sub text: font %d unavailable (%d)", value_id, rc);
        free(c->sub_font_data);
        c->sub_font_data = NULL;
        return rc;
    }
    c->sub_fonts_on = 1;
    return 0;
}

static int settings_text_load(struct ctx *c)
{
    const char *index_path = c->api->config_get(c->api->priv, "ui.font_index",
                                                "/vendor2/dynamic_ui/telefunken18_font_byte_data.txt");
    const char *pack_path = c->api->config_get(c->api->priv, "ui.font_pack",
                                               "/vendor2/dynamic_ui/telefunken18_font_byte_data.bin");
    int font_id = atoi(c->api->config_get(c->api->priv, "ui.font_id", "7"));
    int num_id = atoi(c->api->config_get(c->api->priv, "ui.font_num_id", "16"));
    int rc = oa_font_pack_init(&c->font_pack, index_path, pack_path);
    if (rc) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "settings text: font pack unavailable (%d); using capture", rc);
        return rc;
    }
    if (font_id < 0 || num_id < 0) {
        settings_text_free(c);
        return EINVAL;
    }
    rc = settings_font_open(&c->font_pack, font_id, &c->font, &c->font_data);
    if (rc) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "settings text: font %d unavailable (%d); using capture", font_id, rc);
        settings_text_free(c);
        return rc;
    }
    rc = settings_font_open(&c->font_pack, num_id, &c->font_num, &c->font_num_data);
    if (rc) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "settings text: font %d unavailable (%d); using capture", num_id, rc);
        settings_text_free(c);
        return rc;
    }
    c->font_num_on = 1;
    for (size_t i = 0; i < SETTINGS_TEXT_COUNT; i++) {
        rc = render_text_blob(c, i, SETTINGS_TEXT[i].text);
        if (rc) {
            settings_text_free(c);
            return rc;
        }
    }
    c->text_on = 1;
    /* The title is an independent enhancement: a missing/unusable title font
     * keeps the verified row text live and falls back to the captured title. */
    /* The title is lab-gated (ui.title=1, default 0). Box/font/string are
     * firmware-derived and the draw origin follows the recovered global
     * bearing/centering rule (FONT-X-OFFSET): no fitted offset. Full raster
     * parity is still open (core pixels differ), and this path uses the
     * boxed renderer, not the complete vendor drawText/GPU policy. Keep
     * default-off; fitting an offset to the capture is forbidden. */
    int title_on = atoi(c->api->config_get(c->api->priv, "ui.title", "0"));
    int title_id = atoi(c->api->config_get(c->api->priv, "ui.font_title_id", "18"));
    if (title_on && title_id >= 0 &&
        !settings_font_open(&c->font_pack, title_id, &c->font_title, &c->font_title_data) &&
        render_title_blob(c) == 0)
        c->api->log(c->api->priv, OA_LOG_INFO, "live settings title: font %d '%s'",
                    title_id, SETTINGS_TITLE_TEXT);
    else {
        free(c->title.rgba);
        free(c->font_title_data);
        c->title.rgba = NULL;
        c->font_title_data = NULL;
    }
    for (int r = 1; r <= 4; r++) {
        size_t idx = 9u + (unsigned)r;
        c->edit_val[r - 1] = atoi(SETTINGS_TEXT[idx].text);
    }
    apply_edit_init(c);
    c->api->log(c->api->priv, OA_LOG_INFO, "live settings text: fonts %d+%d, %u items",
                font_id, num_id, (unsigned)SETTINGS_TEXT_COUNT);
    return 0;
}

/* Captured picture-panel reconstruction, NOT the general SVGSlideBar renderer.
 * Firmware: rowItems460 slidebar(510,1,180,49), mask(0,-17,813,66),
 * supplied row client749px -> window(489,rowY+18,165,49). SVGSlideBar ctor
 * 10c4530 selects528FA/530UFA. renderBackground10c41ec leaves11px end
 * masks and stretches the210px middle: track(500,rowY+30,143,6).
 * renderSlider10c4af0 uses24px mSlider, hence141px travel. The PNG matrix
 * (46,54) relative to mask(47,52) contributes(-1,+2). Position10c4a80
 * divides travel by range before multiplying the offset (double, truncation).
 * The explicit0..100 shadow range remains a lab/capture input, not a native
 * settings-provider result. Both normal/focused states redraw, so editing
 * cannot leave the captured knob at an obsolete position. */
static void paint_settings_sliders(struct ctx *c, int focus_row,
                                    const oa_ui_icon *fa, const oa_ui_icon *ufa)
{
    for (int row = 1; row <= 4; ++row) {
        int y = settings_row_y(row);
        int focused = row == focus_row;
        const oa_ui_icon *knob = focused ? fa : ufa;
        /* The child window extends one pixel below the66px focus bar.
         * Keep that pixel in panel red: the focused background was already
         * drawn by the parent, and must not be extended by this child. */
        if (!focused) c->d->fill(489, y + 18, 165, 49, 190, 30, 45);
        c->d->fill(500, y + 30, 143, 6, focused ? 255 : 155,
                    focused ? 0 : 155, focused ? 20 : 155);
        int position = (int)((141.0 / 100.0) * c->edit_val[row - 1]);
        c->d->blit(488 + position, y + 20, knob->w, knob->h, knob->rgba);
    }
}

static void paint_settings_list(struct ctx *c)
{
    if (!c || !c->d)
        return;
    const oa_ui_engine_iface *d = c->d;
    const oa_ui_icon *slider_fa = NULL, *slider_ufa = NULL;
    const oa_ui_icon *select_left = NULL, *select_right = NULL;
    if (c->text_on) {
        slider_fa = find_icon(c, "slidebarfa");
        slider_ufa = find_icon(c, "slidebarufa");
        select_left = find_icon(c, "select_left");
        select_right = find_icon(c, "select_right");
        if (!slider_fa || !slider_ufa || slider_fa->w != 24 || slider_fa->h != 24 ||
            slider_ufa->w != 24 || slider_ufa->h != 24 ||
            !select_left || !select_right || select_left->w != 18 || select_left->h != 24 ||
            select_right->w != 18 || select_right->h != 24) {
            c->api->log(c->api->priv, OA_LOG_ERROR,
                        "live settings requires exported slider and selection assets");
            return; /* Never acknowledge a panel with its focused widget missing. */
        }
    }
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
    /* Focused row. Record 479's three fills are all solid #FE9419, so a flat
     * bar is equivalent to the composed 3-slice background. The reconstructed
     * settings_focus_N assets are NOT used with live text: they bake the
     * captured glyphs plus grey widget shapes into the band, which owner
     * observation (2026-11-02) reported as "red widgets on a bar" and doubled,
     * misaligned numbers. SETTINGS_ROW_Y0 now follows firmware listBox top170
     * + inset16 and the verified70px pitch. Live geometry confirms that row
     * frame; complete focused-widget/font parity is still open. */
    int focus_row = -1;
    if (c->sel >= 0 && c->sel < SETTINGS_ROW_COUNT && c->sel != SETTINGS_ROW_DISABLED) {
        focus_row = c->sel;
        if (c->text_on) {
            d->fill(SETTINGS_ROW_X, settings_row_y(focus_row), 750, 66, 254, 148, 25);
        } else {
            char name[32];
            snprintf(name, sizeof name, "settings_focus_%d", focus_row);
            const oa_ui_icon *row = find_icon(c, name);
            if (row)
                d->blit(SETTINGS_ROW_X, settings_row_y(focus_row), row->w, row->h, row->rgba);
        }
    }
    if (c->text_on) {
        /* No firmware erase step exists: the vendor draws opaque bar content
         * then text on top, and row surfaces clip to their own frames. Our
         * screen-space erases are reconstruction scaffolding for the captured
         * panel's baked glyphs, so they must not stomp another row's bar:
         * unfocused erases are clipped against the focused bar rect (row
         * surfaces cannot paint outside their frame). The focused row only
         * erases the above-bar blob strip with panel red; the bar interior
         * needs no erase since the flat fill just laid it. */
        int bar_x = SETTINGS_ROW_X, bar_w = 750, bar_y = -1, bar_h = 66;
        if (focus_row >= 0)
            bar_y = settings_row_y(focus_row);
        for (size_t i = 0; i < SETTINGS_TEXT_COUNT; i++) {
            const struct settings_text_entry *entry = &SETTINGS_TEXT[i];
            int x, w, y = entry->y - 6, h = 64;
            if (entry->kind == TEXT_LABEL) {
                x = 40;
                w = 446;
            } else if (entry->font == FONT_LARGE) {
                x = 648;
                w = 90;
            } else {
                x = 480;
                w = 200;
            }
            if (entry->row == focus_row) {
                if (bar_y > y)
                    d->fill(x, y, w, bar_y - y, 190, 30, 45);
                continue;
            }
            if (focus_row >= 0 && x < bar_x + bar_w && x + w > bar_x &&
                y < bar_y + bar_h && y + h > bar_y) {
                if (y < bar_y) {
                    /* Overlap at the bottom (row below the bar): keep top. */
                    h = bar_y - y;
                } else {
                    /* Overlap at the top or fully inside: keep bottom part. */
                    h = (y + h) - (bar_y + bar_h);
                    y = bar_y + bar_h;
                }
                if (h <= 0)
                    continue;
            }
            d->fill(x, y, w, h, 190, 30, 45);
        }
        paint_settings_sliders(c, focus_row, slider_fa, slider_ufa);
        for (size_t i = 0; i < SETTINGS_TEXT_COUNT; i++) {
            const struct settings_text_entry *entry = &SETTINGS_TEXT[i];
            const unsigned char *blob = entry->row == focus_row && c->text[i].rgba_focus
                                            ? c->text[i].rgba_focus
                                            : c->text[i].rgba;
            d->blit(c->text[i].x, c->text[i].y, c->text[i].w, c->text[i].h, blob);
        }
        /* SVGSelectionBox::renderBackground10b3798 creates images12/13
         * (img index: select_left/right.png) from transformed iconL/iconR.
         * rowItems460 selectionbox with the same explicit749px row gives
         * (399,rowY+8,362,60). FA504's18px caps and icon Y121-mask107
         * yield x399/743 and rowY+22. updateArrows10b4020 shows both for
         * focused, enabled, wrapping choices. These captured choice rows are
         * still reconstruction data; provider-specific arrow gating is open. */
        if (focus_row == 0 || focus_row == 5 || focus_row == 7) {
            int y = settings_row_y(focus_row) + 22;
            d->blit(399, y, select_left->w, select_left->h, select_left->rgba);
            d->blit(743, y, select_right->w, select_right->h, select_right->rgba);
        }
        if (c->title.rgba) {
            /* Live tab title over the captured title glyphs. The erase covers
             * the composed box only: it starts at x=108, so the captured tab
             * glyph icon (ink x41..99) is untouched, and it ends at y=95, so
             * the tab buttons (y104+) and row 0 (y180+) are untouched. */
            d->fill(SETTINGS_TITLE_X, SETTINGS_TITLE_Y, SETTINGS_TITLE_W,
                    SETTINGS_TITLE_H, 190, 30, 45);
            d->blit(c->title.x, c->title.y, c->title.w, c->title.h, c->title.rgba);
        }
    }
    c->scene_painted = 1;
    if (d->present() != 0) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "settings panel present failed");
        return;
    }
    c->api->log(c->api->priv, OA_LOG_INFO, "painted captured picture-settings panel");
}

/* Apps-grid scrollbar rasterizer. The recovered render ops drive this:
 * surface 0 is the #811924 track, surface 1 the #FFFFFF bar; the top/bottom
 * caps are 20/16 px and the row fills the remainder. */
struct apps_sbar_ctx {
    struct ctx *c;
    int surface[2][4];
    int row[4];
};
static struct apps_sbar_ctx *g_apps_sbar;

static int32_t apps_supplier(struct oa_svg_scrollbar_render *r,int32_t id)
{ (void)r;(void)id;return 1; }
static void apps_window(struct oa_svg_scrollbar_render *r,int32_t rect[4])
{
    (void)r;
    rect[0]=APPS_TRACK_X;rect[1]=APPS_TRACK_Y;
    rect[2]=APPS_TRACK_X+APPS_TRACK_W;
    rect[3]=APPS_TRACK_Y+APPS_TRACK_H;
}
static void apps_resize(struct oa_svg_scrollbar_render *r,int surface,
    const int32_t rect[4])
{ (void)r;memcpy(g_apps_sbar->surface[surface],rect,16); }
static void apps_clear(struct oa_svg_scrollbar_render *r,int surface)
{ (void)r;(void)surface; }
static void apps_rect(struct oa_svg_scrollbar_render *r,int32_t rect[4],
    int32_t left,int32_t top,int32_t right,int32_t bottom)
{
    (void)r;
    rect[0]=left;rect[1]=top;rect[2]=right;rect[3]=bottom;
}
static void apps_reset(struct oa_svg_scrollbar_render *r) { (void)r; }
static void apps_align(struct oa_svg_scrollbar_render *r,int align)
{ (void)r;(void)align; }
static void apps_mask_surface(struct oa_svg_scrollbar_render *r,
    const int32_t rect[4])
{ (void)r;(void)rect; }
static void apps_mask_name(struct oa_svg_scrollbar_render *r,int32_t id)
{ (void)r;(void)id; }
static void apps_mask_container(struct oa_svg_scrollbar_render *r) { (void)r; }
static void apps_draw(struct oa_svg_scrollbar_render *r,int surface,
    int32_t image,int32_t id,int transform)
{
    (void)r;(void)image;
    int *s=g_apps_sbar->surface[surface];
    int x=s[0],y=s[1],w=s[2]-s[0],h=s[3]-s[1];
    int cr=surface==0?129:255,cg=surface==0?25:255,cb=surface==0?36:255;
    if (transform==0) {
        if (id==3 || id==32)
            g_apps_sbar->c->d->fill(x,y,w,APPS_CAP_TOP,cr,cg,cb);
        else if (id==12 || id==33)
            g_apps_sbar->c->d->fill(x,y+h-APPS_CAP_BOTTOM,w,APPS_CAP_BOTTOM,cr,cg,cb);
    } else {
        int *rr=g_apps_sbar->row;
        g_apps_sbar->c->d->fill(x+rr[0],y+rr[1],rr[2]-rr[0],rr[3]-rr[1],cr,cg,cb);
    }
}
static int32_t apps_rect_height(struct oa_svg_scrollbar_render *r,int32_t id)
{
    (void)r;
    if(id==15||id==34)return APPS_CAP_TOP;
    if(id==24||id==35)return APPS_CAP_BOTTOM;
    return 0;
}
static void apps_vertical_rect(struct oa_svg_scrollbar_render *r,
    const int32_t rect[4])
{ (void)r;memcpy(g_apps_sbar->row,rect,16); }
static void apps_vertical_name(struct oa_svg_scrollbar_render *r,int32_t id)
{ (void)r;(void)id; }
static void apps_vertical_container(struct oa_svg_scrollbar_render *r) { (void)r; }
static const struct oa_svg_scrollbar_render_ops apps_sbar_ops={
    apps_supplier,apps_window,apps_resize,apps_clear,apps_rect,apps_reset,
    apps_align,apps_mask_surface,apps_mask_name,apps_mask_container,apps_draw,
    apps_rect_height,apps_vertical_rect,apps_vertical_name,
    apps_vertical_container};

static void paint_app_label(struct ctx *c, int x, int y, int idx)
{
    if (!c || !c->d || !c->app_font_on || idx < 0 || idx >= MAX_GRID)
        return;
    const char *text = c->grid_labels[idx];
    if (!text[0])
        return;
    int advance = 0, font_height = 0;
    if (oa_font_text_measure(&c->app_font, text, 0, &advance) != 0 ||
        advance < 1 || advance > APPS_CELL_W - 16 ||
        oa_bitmap_font_height(&c->app_font, &font_height) != 0 ||
        font_height < 1 || font_height > 128)
        return;
    const int margin = 8;
    int width = advance + 2 * margin, height = font_height + 2 * margin;
    unsigned char *buf = calloc((size_t)width * (size_t)height * 4u, 1);
    if (!buf) return;
    oa_canvas canvas = { buf, width, height };
    static const uint8_t ink[4] = { 38, 23, 25, 255 };
    if (oa_font_text_draw(&canvas, &c->app_font, text, margin, margin, ink,
            0, NULL) != 0) {
        free(buf);
        return;
    }
    int minx = width, miny = height, maxx = -1, maxy = -1;
    for (int py = 0; py < height; py++)
        for (int px = 0; px < width; px++)
            if (buf[((size_t)py * (size_t)width + (size_t)px) * 4u + 3u]) {
                if (px < minx) minx = px;
                if (px > maxx) maxx = px;
                if (py < miny) miny = py;
                if (py > maxy) maxy = py;
            }
    if (maxx < minx || maxy < miny) {
        free(buf);
        return;
    }
    int cw = maxx - minx + 1, ch = maxy - miny + 1;
    unsigned char *crop = calloc((size_t)cw * (size_t)ch * 4u, 1);
    if (!crop) {
        free(buf);
        return;
    }
    for (int py = 0; py < ch; py++)
        memcpy(crop + (size_t)py * (size_t)cw * 4u,
            buf + ((size_t)(miny + py) * (size_t)width + (size_t)minx) * 4u,
            (size_t)cw * 4u);
    c->d->blit(x + (APPS_CELL_W - cw) / 2, y + 189, cw, ch, crop);
    free(crop);
    free(buf);
}

/* SVGGrid::onDownKey moves the window by one row (columns) when the focused
 * row reaches the last visible row; FullScreenGrid::onNextKey does the same
 * when wrapping past the last column. The window is therefore row-aligned and
 * first is at most (total_rows - APPS_ROWS) * APPS_COLS. */
static int apps_first_row(const struct ctx *c)
{
    int total_rows=(c->grid_count+APPS_COLS-1)/APPS_COLS;
    int row=c->grid_sel/APPS_COLS;
    int first=row>=APPS_ROWS?row-APPS_ROWS+1:0;
    int max_first=total_rows>APPS_ROWS?total_rows-APPS_ROWS:0;
    return first>max_first?max_first:first;
}

static int apps_first_visible(const struct ctx *c)
{
    return apps_first_row(c)*APPS_COLS;
}

static void paint_apps(struct ctx *c)
{
    const oa_ui_engine_iface *d=c->d;
    int W=0,H=0;
    if (!d) return;
    if (d->info(&W,&H,NULL,NULL) != 0 || W<=0 || H<=0) return;
    const oa_ui_icon *background=find_icon(c,"background_fullscreen");
    int stock=background && background->w>0 && background->h>0;
    if (stock) {
        d->blit(0,0,background->w,background->h,background->rgba);
        /* DialogFullScreenPortal.svg top strip 0,0,1921,162 #481730 is
         * opaque over the theme background. */
        d->fill(0,0,1921,162,0x48,0x17,0x30);
    } else {
        if (d->clear_rgba(36,38,40,153)) return;
        /* DialogFullScreenPortal.svg top strip 0,0,1921,162 #481730. */
        d->fill(0,0,1921,162,0x48,0x17,0x30);
    }
    const oa_ui_icon *title=find_icon(c,"apps_title");
    if (stock && title && title->w>0 && title->h>0)
        d->blit(81,68,title->w,title->h,title->rgba);
    int first=apps_first_visible(c);
    for (int row=0;row<APPS_ROWS;row++) for (int col=0;col<APPS_COLS;col++) {
        int idx=first+row*APPS_COLS+col;
        if (idx>=c->grid_count) continue;
        int x=APPS_X+col*APPS_CELL_W,y=APPS_Y+row*APPS_CELL_H;
        if (idx==c->grid_sel) {
            if (stock) d->fill(x,y,APPS_CELL_W,APPS_CELL_H,247,148,30);
            else d->fill(x-4,y-4,APPS_CELL_W+8,APPS_CELL_H+8,247,148,30);
        }
        if (!stock) d->fill(x,y,APPS_CELL_W,APPS_CELL_H,20,20,20);
        const oa_ui_icon *icon=find_icon(c,c->grid_cards[idx]);
        if (icon && icon->w>0 && icon->h>0) {
            if (stock) {
                /* Every stock tile, focused or not, is 294x165 at cell+7,+7;
                 * the focus state only paints the orange cell backdrop. The
                 * earlier full-width focused tile was a measurement error. */
                int w=294,h=w*icon->h/icon->w;
                if (h>166) { h=166;w=h*icon->w/icon->h; }
                d->blit_scaled(x+7,y+7,w,h,
                    icon->w,icon->h,icon->rgba);
            } else {
                int w=APPS_CELL_W-40,h=w*icon->h/icon->w;
                if (h>APPS_CELL_H-20) { h=APPS_CELL_H-20;w=h*icon->w/icon->h; }
                d->blit_scaled(x+(APPS_CELL_W-w)/2,y+(APPS_CELL_H-h)/2,w,h,
                    icon->w,icon->h,icon->rgba);
            }
        }
        if (stock && idx==c->grid_sel) {
            if (idx==0) {
                const oa_ui_icon *label=find_icon(c,"markt_label");
                if (label && label->w>0 && label->h>0)
                    d->blit(x+125,y+189,label->w,label->h,label->rgba);
            } else {
                paint_app_label(c,x,y,idx);
            }
        }
    }
    if (stock) {
        /* The stock apps page draws a thin white scrollbar over the
         * gradient rather than the dark themed track. */
        d->fill(1750,180,4,688,129,25,36);
        if (c->grid_count > APPS_VISIBLE) {
            int bar=688*APPS_VISIBLE/(int)c->grid_count-APPS_CAP_BOTTOM;
            if (bar<36)bar=36;
            if (bar>688)bar=688;
            int total_rows=(c->grid_count+APPS_COLS-1)/APPS_COLS;
            int max_first=total_rows>APPS_ROWS
                ? (total_rows-APPS_ROWS)*APPS_COLS : 0;
            int pos=max_first>0 ? (688-bar)*first/max_first : 688-bar;
            if (pos<0) pos=0;
            if (pos>688-bar) pos=688-bar;
            if (bar<688) d->fill(1750,180+pos,4,bar,255,255,255);
        }
    } else {
        struct oa_svg_scrollbar_render sr;
        memset(&sr,0,sizeof sr);
        sr.skin.cached_image[0]=1;
        sr.state.min_bar=APPS_CAP_TOP+APPS_CAP_BOTTOM;
        sr.state.total=c->grid_count;
        sr.state.visible=APPS_VISIBLE;
        sr.state.first=first;
        struct apps_sbar_ctx sb;
        memset(&sb,0,sizeof sb);
        sb.c=c;
        g_apps_sbar=&sb;
        /* This immediate raster adapter has no retained surface to clear.
         * Suppress the whole scrollbar when the firmware geometry hides its
         * bar, rather than leaving the track painted after a no-op clear. */
        if (c->grid_count > APPS_VISIBLE) {
            oa_svg_scrollbar_render_background(&sr,&apps_sbar_ops);
            oa_svg_scrollbar_render_bar(&sr,&apps_sbar_ops);
        }
        g_apps_sbar=NULL;
    }
    if (d->present()!=0) {
        c->api->log(c->api->priv,OA_LOG_ERROR,"apps grid present failed");
        return;
    }
    c->api->log(c->api->priv,OA_LOG_INFO,
        "painted %s apps grid: %d items, first %d",
        stock?"stock":"synthetic",c->grid_count,first);
}

static void channel_text_blit(struct ctx *c, const char *text, int x, int y,
    int width, int height, int focused)
{
    if (!c || !c->d || !c->text_on || !text || !*text || width <= 0 || height <= 0)
        return;
    int advance = 0;
    if (oa_font_text_measure(&c->font, text, 0, &advance) || advance <= 0)
        return;
    int w = advance < width ? advance : width;
    unsigned char *rgba = calloc((size_t)w * (size_t)height * 4u, 1);
    if (!rgba) return;
    oa_canvas canvas = { rgba, w, height };
    const uint8_t ink[4] = {
        (uint8_t)(focused ? 126 : 255),
        (uint8_t)(focused ? 0 : 255),
        (uint8_t)(focused ? 10 : 255), 255
    };
    struct oa_font_clip clip = { 0, 0, w, height };
    if (!oa_font_text_draw(&canvas, &c->font, text, 0, 0, ink, 0, &clip))
        c->d->blit(x, y, w, height, rgba);
    free(rgba);
}

static void paint_channels(struct ctx *c)
{
    const oa_ui_engine_iface *d = c->d;
    int W = 0, H = 0;
    if (!c || !d || d->info(&W, &H, NULL, NULL) || W <= 0 || H <= 0)
        return;
    if (d->clear_rgba(36, 38, 40, 153)) return;

    const oa_channel_iface *channels =
        c->api->lookup(c->api->priv, OA_SVC_CHANNELS, 1);
    const struct oa_channel_snapshot *runtime =
        channels ? channels->snapshot() : NULL;
    int have_runtime = runtime && runtime->count > 0;

    d->fill(0, 0, 1921, 162, 72, 23, 48);
    d->fill(0, 162, 1921, CH_MARKER_H,
        have_runtime ? 190 : 190, have_runtime ? 120 : 30,
        have_runtime ? 30 : 45);
    channel_text_blit(c,
        have_runtime ? "RUNTIME ACTIVE LIST - LCN/VISIBILITY UNAVAILABLE"
                     : "PROVISIONAL STORED ORDER - NOT RUNTIME EASI",
        20, 166, 1800, CH_MARKER_H, 0);

    const oa_service_list_iface *svc = NULL;
    const struct oa_service_list *list = NULL;
    size_t *order = NULL;
    size_t total = 0;
    if (have_runtime) {
        total = runtime->count;
    } else {
        svc = c->api->lookup(c->api->priv, OA_SVC_SERVICE_LIST, 1);
        list = svc ? svc->snapshot() : NULL;
        if (list) {
            struct oa_service_query q;
            memset(&q, 0, sizeof q);
            q.source_mask = 0x7;
            q.sort = OA_SERVICE_SORT_CHANNEL_NO;
            order = malloc(list->count * sizeof *order);
            if (!order || oa_service_list_query(list, &q, order, list->count,
                    &total))
                total = 0;
        }
    }

    if (total == 0)
        c->chan_sel = 0;
    else if (c->chan_sel >= (int)total)
        c->chan_sel = (int)total - 1;
    int first = c->chan_sel >= CH_ROWS ? c->chan_sel - CH_ROWS + 1 : 0;
    for (int row = 0; row < CH_ROWS && first + row < (int)total; row++) {
        int index = first + row;
        int focused = index == c->chan_sel;
        int y = CH_ROW_Y + row * CH_ROW_H;
        d->fill(CH_X, y, CH_W, CH_ROW_H - 4,
                focused ? 247 : 20, focused ? 148 : 20,
                focused ? 30 : 20);
        char name[OA_SERVICE_NAME_MAX + 1];
        const char *source = "";
        int64_t number = 0;
        if (have_runtime) {
            const struct oa_channel_entry *entry = &runtime->entries[index];
            source = entry->name;
            number = entry->rsn;
        } else {
            const struct oa_service_row *service =
                &list->rows[order[index]];
            source = service->name;
            number = service->channel_no;
        }
        size_t n = 0;
        for (; source[n] && n < OA_SERVICE_NAME_MAX; n++)
            name[n] = (unsigned char)source[n] < 32 ? '?' : source[n];
        name[n] = '\0';
        char line[OA_SERVICE_NAME_MAX + 64];
        snprintf(line, sizeof line, "%" PRId64 "  %s", number, name);
        channel_text_blit(c, line, CH_X + 20, y + 2, CH_W - 40,
                          CH_ROW_H - 6, focused);
    }
    if (svc) c->api->release(c->api->priv, OA_SVC_SERVICE_LIST);
    if (channels) c->api->release(c->api->priv, OA_SVC_CHANNELS);
    free(order);
    c->scene_painted = 1;
    if (d->present() != 0) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "channel list present failed");
        return;
    }
    c->api->log(c->api->priv, OA_LOG_INFO,
        "painted %s channel list: %zu rows",
        have_runtime ? "runtime" : "stored", total);
}

/* The five rail slots are a separate surface: the portal background behind
 * them is uniform, so the 30 ms highlight only needs this strip repainted. */
#define PORTAL_BG_R 36
#define PORTAL_BG_G 38
#define PORTAL_BG_B 40
#define PORTAL_BG_A 153
#define RAIL_STRIP_X 0
#define RAIL_STRIP_W 209
#define RAIL_STRIP_Y 211
#define RAIL_STRIP_H 670

static void paint_rail(struct ctx *c, int focus, int H)
{
    const oa_ui_engine_iface *d = c->d;
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
}

static void paint(struct ctx *c)
{
    const oa_ui_engine_iface *d;
    int W = 0, H = 0;
    if (!c || !c->d) return;   /* a suspended/stopped skin has no engine hold */
    /* A script may record state. It does not choose vendor pixels. */
    if (c->lua)
        (void)c->lua->call("paint", "vendor", 0, NULL);
    d = c->d;
    if (c->screen == SCREEN_SETTINGS_LIST) {
        paint_settings_list(c);
        return;
    }
    if (c->screen == SCREEN_SUBLAUNCHER) {
        paint_sub_launcher(c);
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
    if (c->screen == SCREEN_APPS) {
        paint_apps(c);
        return;
    }
    if (c->screen == SCREEN_CHANNELS) {
        paint_channels(c);
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
    paint_rail(c, focus, H);

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

/* A firmware-shaped partial update for the one-shot rail highlight. The
 * vendor APNG only changes the 160x160 button window; repainting the whole
 * 1920x1080 portal for each 30 ms frame is a software-only cost. The patch
 * restores the uniform dialog background in the rail strip and the display
 * provider presents only that rectangle. */
static void paint_rail_hl(struct ctx *c)
{
    if (!c || !c->d || c->screen != SCREEN_PORTAL) {
        if (c) paint(c);
        return;
    }
    rail_hl_sync(c);
    const oa_display_patch_iface *p =
        c->api->lookup(c->api->priv, OA_SVC_DISPLAY_PATCH, 1);
    if (!p) {
        paint(c);
        return;
    }
    int W = 0, H = 0;
    if (c->d->info(&W, &H, NULL, NULL) || W <= 0 || H <= 0 ||
        p->fill_rgba(RAIL_STRIP_X, RAIL_STRIP_Y, RAIL_STRIP_W, RAIL_STRIP_H,
                     PORTAL_BG_R, PORTAL_BG_G, PORTAL_BG_B, PORTAL_BG_A)) {
        c->api->release(c->api->priv, OA_SVC_DISPLAY_PATCH);
        paint(c);
        return;
    }
    c->api->release(c->api->priv, OA_SVC_DISPLAY_PATCH);
    int focus = c->band == BAND_RAIL ? c->rail : -1;
    paint_rail(c, focus, H);
    c->scene_painted = 1;
    if (c->d->present() != 0)
        c->api->log(c->api->priv, OA_LOG_ERROR, "rail highlight present failed");
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
static int portal_menu_action(const char *action)
{
    if (!g_ctx || !action) return EINVAL;
    if (!strncmp(action, "open:", 5)) {
        const char *name = action + 5;
        int screen;
        if (!strcmp(name, "portal")) screen = SCREEN_PORTAL;
        else if (!strcmp(name, "sources")) screen = SCREEN_SOURCES;
        else if (!strcmp(name, "settings")) screen = SCREEN_SETTINGS;
        else if (!strcmp(name, "settings_list")) screen = SCREEN_SETTINGS_LIST;
        else if (!strcmp(name, "sub_launcher")) screen = SCREEN_SUBLAUNCHER;
        else if (!strcmp(name, "apps")) screen = SCREEN_APPS;
        else if (!strcmp(name, "channels")) screen = SCREEN_CHANNELS;
        else return ENOENT;
        g_ctx->screen = screen;
        if (g_ctx->d) paint(g_ctx);
        return 0;
    }
    if (strncmp(action, "setting:", 8) && strncmp(action, "set:", 4))
        return ENOTSUP;
    const oa_ui_menu_iface *menu =
        g_ctx->api->lookup(g_ctx->api->priv, OA_SVC_UI_MENU, 1);
    if (!menu) return ENODEV;
    const struct oa_menu_dialog *md = menu->dialog("sub_launcher");
    const struct oa_menu_branch *b = md ? menu->branch(md, 2) : NULL;
    int rc = ENOENT;
    if (b && !strncmp(action, "setting:", 8)) {
        const char *key = action + 8;
        for (unsigned i = 0; i < b->item_count; i++) {
            if (strcmp(b->items[i].key, key)) continue;
            /* Focus the card and play the vendor 300 ms selection animation. */
            g_ctx->screen = SCREEN_SUBLAUNCHER;
            g_ctx->sub_sel = (int)i;
            g_ctx->sub_progress = 0;
            g_ctx->sub_anim_live = 1;
            g_ctx->sub_anim_start = sub_now(g_ctx);
            sub_tick_start(g_ctx);
            if (g_ctx->d) paint(g_ctx);
            rc = 0;
            break;
        }
    } else if (b) {
        const char *spec = action + 4;
        const char *eq = strchr(spec, '=');
        if (!eq || eq == spec || !eq[1]) {
            rc = EINVAL;
        } else {
            for (unsigned i = 0; i < b->item_count && rc == ENOENT; i++) {
                const struct oa_menu_item *item = &b->items[i];
                size_t klen = (size_t)(eq - spec);
                if (strlen(item->key) != klen ||
                    strncmp(item->key, spec, klen))
                    continue;
                int card = sub_card_for_item(item->key);
                if (card < 0 || !item->value_count ||
                    sub_values_dynamic(item)) {
                    rc = ENOTSUP; /* dynamic list (MODE) not decoded yet */
                    break;
                }
                for (unsigned v = 0; v < item->value_count; v++) {
                    if (strcmp(item->values[v].key, eq + 1)) continue;
                    g_ctx->sub_value[card] = (int)v;
                    sub_text_free(g_ctx);
                    sub_state_save(g_ctx);
                    if (g_ctx->screen == SCREEN_SUBLAUNCHER && g_ctx->d)
                        paint(g_ctx);
                    rc = 0;
                    break;
                }
            }
        }
    }
    g_ctx->api->release(g_ctx->api->priv, OA_SVC_UI_MENU);
    return rc;
}
static const oa_portal_iface g_portal = {
    portal_hit, portal_activate, portal_screen, portal_menu_action
};

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
    sub_state_defaults(c);
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

/* Project rail prototype plays11 frames once and holds the last. The lower
 * vendor decoder does not rearm; complete higher widget/image lifetime is
 * still unverified. Reset the project playhead when focused item changes. */
static void rail_hl_sync(struct ctx *c)
{
    int want = c->rail_hl_n > 0 && c->d && c->screen == SCREEN_PORTAL &&
               c->rail_hl_play && c->rail_hl_frame + 1 < c->rail_hl_n;
    if (want && !c->rail_hl_tick) {
        c->rail_hl_late = 0;
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
    /* A software-rendered portal can take much longer than the vendor's 30 ms
     * frame. After a slow paint, skip the frames that are already overdue so
     * the 11-frame highlight still finishes on schedule and holds its last
     * frame. Normal 30 ms ticks (including a slightly late host test tick)
     * still advance exactly one frame. */
    if (c->rail_hl_late) {
        c->rail_hl_late = 0;
        unsigned step = 1 + (unsigned)((now - c->rail_hl_next) / OA_RAIL_HL_MS);
        unsigned left = (unsigned)(c->rail_hl_n - 1 - c->rail_hl_frame);
        if (step > left) step = left;
        c->rail_hl_frame += (int)step;
        c->rail_hl_next += (uint64_t)step * OA_RAIL_HL_MS;
    } else {
        if (c->rail_hl_frame + 1 < c->rail_hl_n)
            c->rail_hl_frame++;
        c->rail_hl_next = now + OA_RAIL_HL_MS;
    }
    if (c->rail_hl_frame + 1 >= c->rail_hl_n)
        c->rail_hl_play = 0;                 /* finite sequence: hold the last frame */
    uint64_t started = now_ms();
    paint_rail_hl(c);
    if (now_ms() - started > OA_RAIL_HL_MS * 2u)
        c->rail_hl_late = 1;                 /* next tick catches up */
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
    } else if (!strcmp(ev->name, OA_SVC_UI_THEME)) {
        c->theme = NULL; /* active dialog uses only its copied numeric mode */
        apply_anim_policy(c);
    } else if (!strcmp(ev->name, OA_SVC_I18N)) {
        c->i18n = NULL;
        sub_text_free(c);
    } else if (!strcmp(ev->name, OA_SVC_UI_LUA)) {
        c->lua = NULL; /* host already dropped the hold; do not repaint for this */
        return;
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
        c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 2);
        if (!c->d)
            return;
        if (c->d->register_skin_dispatch(&g_skin, c, skin_dispatch)) {
            c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
            c->d = NULL;
            return;
        }
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
    if (!c->theme) c->theme = c->api->lookup(c->api->priv, OA_SVC_UI_THEME, 1);
    if (!c->i18n) c->i18n = c->api->lookup(c->api->priv, OA_SVC_I18N, 1);
    apply_anim_policy(c);
    if (!c->lua) c->lua = c->api->lookup(c->api->priv, OA_SVC_UI_LUA, OA_LUA_API_VERSION);
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

/* Vendor SVGSelectionBox::onKeyLeft/onKeyRight default to cyclic navigation
 * (constructor writes 0x1 at +0x2f4), so the value list wraps. The setting
 * backend is NovoPictureSettings through UserPreferences; this UI state store
 * is the replacement's persisted shadow until the PQ/video backend is wired. */
static int sub_value_step(struct ctx *c, int dir)
{
    if (c->sub_sel < 0 || !c->d) return 0;
    const oa_ui_menu_iface *menu =
        c->api->lookup(c->api->priv, OA_SVC_UI_MENU, 1);
    if (!menu) return 0;
    const struct oa_menu_dialog *md = menu->dialog("sub_launcher");
    const struct oa_menu_branch *b = md ? menu->branch(md, 2) : NULL;
    int changed = 0;
    if (b && (unsigned)c->sub_sel < b->item_count) {
        const struct oa_menu_item *item = &b->items[c->sub_sel];
        int card = sub_card_for_item(item->key);
        if (card >= 0 && item->value_count && !sub_values_dynamic(item)) {
            int index = sub_value_index(c, item, card);
            int next = (index + dir + (int)item->value_count) %
                       (int)item->value_count;
            if (next != index) {
                c->sub_value[card] = next;
                sub_text_free(c);
                sub_state_save(c);
                paint(c);
                changed = 1;
            }
        }
    }
    c->api->release(c->api->priv, OA_SVC_UI_MENU);
    return changed;
}

static void move_key(struct ctx *c, uint8_t id)
{
    if (c->screen == SCREEN_SUBLAUNCHER) {        /* SVGListBoxBase::onKeyPress maps Up/Down (ICE 0x20a/0x20b) to
         * onUpKey/onDownKey; both move the selected row and call
         * startSelectAnimation (the 300 ms scale). Left/Right (0x20c/0x20d)
         * are forwarded to the row's selection box, i.e. value changes. */
        if (id == 19 || id == 20) {
            int next = c->sub_sel;
            if (next < 0) next = 0;
            else if (id == 19 && next > 0) next--;
            else if (id == 20 && next < 2) next++;
            if (next != c->sub_sel) {
                c->sub_sel = next;
                c->sub_progress = 0;
                c->sub_anim_live = 1;
                c->sub_anim_start = sub_now(c);
                sub_tick_start(c);
                if (c->d) paint(c);
            }
        } else if (id == 21 || id == 22) {
            (void)sub_value_step(c, id == 22 ? 1 : -1);
        } else if (id == 10 || id == 48) {
            c->sub_anim_live = 0;
            if (c->sub_tick) {
                c->api->timer_del(c->api->priv, c->sub_tick);
                c->sub_tick = 0;
            }
            c->screen = SCREEN_SETTINGS;
            if (c->d) paint(c);
        }
        return;
    }
    if (c->screen == SCREEN_CHANNELS) {
        if (id == 10 || id == 48) {
            c->screen = SCREEN_PORTAL;
            if (c->d) paint(c);
            return;
        }
        const oa_service_list_iface *svc =
            c->api->lookup(c->api->priv, OA_SVC_SERVICE_LIST, 1);
        const struct oa_service_list *list = svc ? svc->snapshot() : NULL;
        size_t *order = NULL, matched = 0;
        if (list) {
            struct oa_service_query q;
            memset(&q, 0, sizeof q);
            q.source_mask = 0x7;
            q.sort = OA_SERVICE_SORT_CHANNEL_NO;
            order = malloc(list->count * sizeof *order);
            if (!order || oa_service_list_query(list, &q, order,
                    list->count, &matched))
                matched = 0;
        }
        if (id == 20 || id == 19) {
            if (matched) {
                c->chan_sel += id == 19 ? 1 : -1;
                if (c->chan_sel < 0) c->chan_sel = 0;
                if (c->chan_sel >= (int)matched) c->chan_sel = (int)matched - 1;
            }
            if (c->d) paint(c);
        } else if ((id == 53 || id == 0x8a) && matched) {
            oa_action_event act;
            memset(&act, 0, sizeof act);
            act.kind = OA_ACTION_SELECT;
            act.index = (uint8_t)c->chan_sel;
            act.name = list->rows[order[c->chan_sel]].name;
            c->api->emit(c->api->priv, OA_TOPIC_ACTION, &act);
        }
        free(order);
        if (svc) c->api->release(c->api->priv, OA_SVC_SERVICE_LIST);
        return;
    }
    if (c->screen == SCREEN_APPS) {
        int sel = c->grid_sel;
        if (id == 21 || id == 22)
            sel += id == 22 ? 1 : -1;
        else if (id == 19 || id == 20)
            sel += id == 19 ? APPS_COLS : -APPS_COLS;
        else if (id == 53 || id == 0x8a) {
            if (c->grid_count > 0) {
                oa_action_event act;
                memset(&act, 0, sizeof act);
                act.kind = OA_ACTION_SELECT;
                act.index = (uint8_t)sel;
                act.name = c->grid_cards[sel];
                c->api->emit(c->api->priv, OA_TOPIC_ACTION, &act);
            }
            return;
        } else if (id == 10 || id == 48) {
            c->screen = SCREEN_PORTAL;
            if (c->d) paint(c);
            return;
        } else {
            return;
        }
        if (c->grid_count <= 0)
            return;
        if (sel < 0) sel = 0;
        if (sel >= c->grid_count) sel = c->grid_count - 1;
        c->grid_sel = sel;
        if (c->d) paint(c);
        return;
    }
    if (c->screen == SCREEN_SETTINGS_LIST) {
        if (id == 19 || id == 20) {        /* Down / Up: row selection */
            c->sel = settings_row_next(c->sel, id == 19 ? 1 : -1);
            if (c->d) paint(c);
        } else if (id == 21 || id == 22) { /* Left / Right: slider value */
            /* Lab-only (ui.edit=1): firmware dispatches Left/Right to the
             * row adjust path, but ranges/steps are not decoded, so 0..100
             * step 1 are invented placeholders and only rows 1..4 (slider
             * digits) are adjustable. Selection rows need option lists that
             * are not decoded; Select sub-dialogs are not implemented. */
            if (atoi(c->api->config_get(c->api->priv, "ui.edit", "0")) == 1 &&
                c->sel >= 1 && c->sel <= 4) {
                int v = c->edit_val[c->sel - 1] + (id == 22 ? 1 : -1);
                if (v < 0) v = 0;
                if (v > 100) v = 100;
                c->edit_val[c->sel - 1] = v;
                if (c->text_on) {
                    char text[16];
                    snprintf(text, sizeof text, "%d", v);
                    size_t idx = 9u + (unsigned)c->sel;
                    if (idx < SETTINGS_TEXT_COUNT &&
                        SETTINGS_TEXT[idx].kind == TEXT_VALUE &&
                        render_text_blob(c, idx, text) != 0)
                        settings_text_free(c);
                }
                c->api->log(c->api->priv, OA_LOG_INFO,
                            "settings row %d value %d (lab placeholder range)", c->sel, v);
                if (c->d) paint(c);
            }
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
        c->fs_type = dialog_fs_type(c);
        if (c->fs_type > 0 && atoi(c->api->config_get(c->api->priv, "ui.launch_anim", "0")) == 1 &&
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

/* Live LauncherItem home row: parent 0, non-deleted, type 2 with a thumbnail
 * name, ordered by m_order_index (the stock portal's card order). Falls back to
 * DEFAULT_CARDS when the launcher.db service or its rows are unavailable. */
struct live_card { int32_t order,id; char name[48]; char label[64]; };
static int live_card_cmp(const void *a,const void *b)
{
    const struct live_card *x=a,*y=b;
    if(x->order!=y->order)return x->order<y->order?-1:1;
    return x->id<y->id?-1:x->id>y->id?1:0;
}
static int load_live_cards(struct ctx *c)
{
    const oa_launcher_db_iface *db=c->api->lookup(c->api->priv,
        OA_SVC_LAUNCHER_DB,1);
    if(!db)return 0;
    struct oa_launcher_list *list=NULL;
    int parent=0;
    const char *override=c->api->config_get(c->api->priv,"ui.cards_parent","");
    if(override && *override) parent=atoi(override);
    else if(!strcmp(c->api->config_get(c->api->priv,"ui.screen",""),"apps")) parent=1;
    int rc=db->read_parent(parent,0,0,&list);
    if(rc){db->release(list);c->api->release(c->api->priv,OA_SVC_LAUNCHER_DB);return 0;}
    size_t count=db->count(list);
    struct live_card cards[64];
    unsigned n=0;
    for(size_t i=0;i<count;++i) {
        struct oa_launcher_row_view row;
        if(db->get(list,i,&row) || row.item_type!=2 || !row.thumb_file_name[0] ||
           strchr(row.thumb_file_name,'/') || strlen(row.thumb_file_name)>=sizeof cards[0].name)
            continue;
        struct live_card candidate={.order=row.order_index,.id=row.id};
        snprintf(candidate.name,sizeof candidate.name,"%s",row.thumb_file_name);
        snprintf(candidate.label,sizeof candidate.label,"%s",
            row.application_name ? row.application_name : "");
        unsigned at=0;
        while(at<n && live_card_cmp(&cards[at],&candidate)<=0)++at;
        if(at>=MAX_GRID)continue;
        if(n<MAX_GRID)++n;
        memmove(&cards[at+1],&cards[at],(n-at-1)*sizeof cards[0]);
        cards[at]=candidate;
    }
    db->release(list);
    c->api->release(c->api->priv,OA_SVC_LAUNCHER_DB);
    if(!n)return 0;
    c->grid_count=(int)n;
    for(unsigned i=0;i<n;++i) {
        snprintf(c->grid_cards[i],sizeof c->grid_cards[0],"%s",cards[i].name);
        snprintf(c->grid_labels[i],sizeof c->grid_labels[0],"%s",
            cards[i].label);
    }
    /* The stock full-screen Apps catalog starts with the special Markt tile,
     * then the 23 parent-1 LauncherItem rows. The Markt row is not a
     * LauncherItem, so insert it explicitly for the apps grid unless the
     * supplied database already carries one. */
    int has_markt = 0;
    for (unsigned i = 0; i < n; i++)
        if (!strcmp(cards[i].name, "markt")) has_markt = 1;
    if(parent==1 && !has_markt && n<MAX_GRID) {
        memmove(&c->grid_cards[1],&c->grid_cards[0],
            n*sizeof c->grid_cards[0]);
        memmove(&c->grid_labels[1],&c->grid_labels[0],
            n*sizeof c->grid_labels[0]);
        snprintf(c->grid_cards[0],sizeof c->grid_cards[0],"%s","markt");
        snprintf(c->grid_labels[0],sizeof c->grid_labels[0],"%s","Markt");
        c->grid_count=(int)n+1;
    } else if (has_markt) {
        for (unsigned i = 0; i < n; i++)
            if (!strcmp(cards[i].name, "markt"))
                snprintf(c->grid_labels[i],sizeof c->grid_labels[0],"%s",
                    "Markt");
    }
    if(n>MAX_CARDS)n=MAX_CARDS;
    c->nc=(int)n;
    for(unsigned i=0;i<n;++i)
        snprintf(c->cards[i],sizeof c->cards[0],"%s",cards[i].name);
    return 1;
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
    int handled = 0;
    if (!c)
        return 1;
    if (c->lua && c->lua->call("key", "vendor", id, &handled) == 0 && handled)
        return 0;
    move_key(c, (uint8_t)id);
    return 0;
}
static const oa_ui_skin g_skin = { "vendor", skin_enter, NULL, skin_paint, skin_key };

static int m_start(void *v)
{
    struct ctx *c = v;
    sub_state_defaults(c);
    sub_state_load(c);
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 2);
    if (!c->d) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "no ui.engine service — load mod_ui_engine first");
        return -1;
    }
    c->scene_painted = 0;

    const char *spec = c->api->config_get(c->api->priv, "ui.cards", "");
    c->grid_count = 0; /* stop/start may reuse this context with new DB/config */
    const char *live = c->api->config_get(c->api->priv, "ui.live_cards", "1");
    if (spec && spec[0]) {
        parse_cards(c, spec);
    } else if (atoi(live) && load_live_cards(c)) {
        /* live parent-0 rows, sorted by m_order_index */
    } else {
        for (c->nc = 0; DEFAULT_CARDS[c->nc]; c->nc++)
            snprintf(c->cards[c->nc], sizeof c->cards[0], "%s", DEFAULT_CARDS[c->nc]);
    }
    if (c->grid_count == 0 && c->nc > 0) {
        c->grid_count = c->nc;
        for (int i = 0; i < c->nc; i++)
            memcpy(c->grid_cards[i],c->cards[i],sizeof c->cards[i]);
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
    if (!strcmp(c->api->config_get(c->api->priv, "ui.screen", ""), "sub_launcher")) {
        c->screen = SCREEN_SUBLAUNCHER;
        c->sub_sel = atoi(c->api->config_get(c->api->priv, "ui.sub_sel", "-1"));
        if (c->sub_sel < -1 || c->sub_sel > 2) c->sub_sel = -1;
        c->sub_progress = (float)atof(
            c->api->config_get(c->api->priv, "ui.sub_progress", "0"));
        if (c->sub_progress < 0) c->sub_progress = 0;
        if (c->sub_progress > 1) c->sub_progress = 1;
    }
    if (!strcmp(c->api->config_get(c->api->priv, "ui.screen", ""), "apps")) {
        c->screen = SCREEN_APPS;
        if (!c->restored)
            c->grid_sel = atoi(c->api->config_get(c->api->priv, "ui.grid_sel", "0"));
        if (c->grid_sel < 0) c->grid_sel = 0;
        if (c->grid_count == 0) c->grid_sel = 0;
        if (c->grid_count > 0 && c->grid_sel >= c->grid_count)
            c->grid_sel = c->grid_count - 1;
    }
    if (!strcmp(c->api->config_get(c->api->priv, "ui.screen", ""), "channels")) {
        c->screen = SCREEN_CHANNELS;
        if (!c->restored)
            c->chan_sel = atoi(c->api->config_get(c->api->priv,
                "ui.channel_sel", "0"));
        if (c->chan_sel < 0) c->chan_sel = 0;
    }
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
    c->theme = c->api->lookup(c->api->priv, OA_SVC_UI_THEME, 1);
    c->i18n = c->api->lookup(c->api->priv, OA_SVC_I18N, 1);
    c->fs_type = dialog_fs_type(c);
    c->slide_box = atoi(c->api->config_get(c->api->priv, "ui.slide_box", "0"));
    apply_anim_policy(c);
    /* React to providers that appear later, and drop cached pointers when one is removed. */
    c->mod_key = c->api->subscribe(c->api->priv, OA_TOPIC_MODULE_CHANGED, on_module_changed, c);
    c->svc_key = c->api->subscribe(c->api->priv, OA_TOPIC_SERVICE_REMOVED, on_service_removed, c);
    if (!c->mod_key || !c->svc_key) {
        c->api->log(c->api->priv, OA_LOG_ERROR,
                    "ui_vendor: lifecycle subscription failed; refusing start");
        return -1;
    }
    if (c->api->publish(c->api->priv, OA_SVC_PORTAL, 2, &g_portal) != 0) {
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

    if (c->d->register_skin_dispatch(&g_skin, c, skin_dispatch) != 0) {
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
    if (atoi(c->api->config_get(c->api->priv, "ui.text", "0")) == 1 &&
        settings_text_load(c) == 0)
        (void)sub_text_fonts_load(c);
    if (c->screen == SCREEN_APPS)
        (void)app_label_open(c);
    c->lua = c->api->lookup(c->api->priv, OA_SVC_UI_LUA, OA_LUA_API_VERSION);
    if (c->api->config_get(c->api->priv, "ui.paint", "1")[0] == '1')
        paint(c);
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    cancel_dialog(c);
    c->sub_anim_live = 0;
    if (c->sub_tick) {
        c->api->timer_del(c->api->priv, c->sub_tick);
        c->sub_tick = 0;
    }
    settings_text_free(c);
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
    if (c->theme) {
        c->api->release(c->api->priv, OA_SVC_UI_THEME);
        c->theme = NULL;
    }
    if (c->i18n) {
        c->api->release(c->api->priv, OA_SVC_I18N);
        c->i18n = NULL;
    }
    if (c->lua) {
        c->api->release(c->api->priv, OA_SVC_UI_LUA);
        c->lua = NULL;
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
    settings_text_free(v);
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
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 2);
    if (!c->d)
        return -1;
    if (c->d->register_skin_dispatch(&g_skin, c, skin_dispatch) != 0) {
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
        return -1;
    }
    c->suspended = 0;
    /* a resumed portal replays the focus highlight from its first frame */
    c->rail_hl_frame = 0;
    c->rail_hl_play = 1;
    c->rail_hl_next = now_ms() + OA_RAIL_HL_MS;
    /* providers may have appeared or disappeared while suspended; rebind_now reacquires them and
     * restarts or holds the slide as appropriate (it also paints). */
    if (c->rebind_pending || !c->anim || !c->anim2 || !c->clock || !c->theme)
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
    if (!buf || cap < 5)
        return 5;
    unsigned char *bytes = buf;
    bytes[0] = (unsigned char)c->band;
    bytes[1] = (unsigned char)c->rail;
    bytes[2] = (unsigned char)c->card;
    bytes[3] = (unsigned char)c->screen;
    bytes[4] = (unsigned char)c->grid_sel;
    return 5;
}

static int m_restore(void *v, const void *buf, size_t len)
{
    struct ctx *c = v;
    if (!c || !buf || (len != 4 && len != 5))
        return -1;
    const unsigned char *bytes = buf;
    if (bytes[0] > BAND_CARDS || bytes[1] > 4 || bytes[2] >= MAX_CARDS || bytes[3] > SCREEN_CHANNELS ||
        (len == 5 && bytes[4] >= MAX_GRID))
        return -1;
    c->band = bytes[0];
    c->rail = bytes[1];
    c->card = bytes[2];
    c->screen = bytes[3];
    c->grid_sel = len == 5 ? bytes[4] : 0;
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
