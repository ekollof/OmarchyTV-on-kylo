/*
 * mod_ui_fb — the native home screen, drawn from the set's own content.
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

struct icon {
    char name[48];
    int w, h;
    unsigned char *rgba;
};

enum { BAND_RAIL = 0, BAND_CARDS = 1 };

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
/* Title + five glyphs + the configured card limit + the entire APNG. */
#define MAX_ICONS (1 + 5 + MAX_CARDS + OA_RAIL_HL_FRAMES)

struct ctx {
    const oa_host_api *api;
    const oa_display_iface *disp;
    const oa_display_iface *disp2;   /* v2 lookup: set_scene_transform */
    const oa_display_async_iface *async;
    uint64_t present_serial;         /* async frame serial last accepted */
    int present_pending;             /* rail playback paces to this completion */
    const oa_display_alpha_iface *alpha;
    uint64_t receipt_tick, first_serial, last_receipt;
    int paint_w, paint_h, scene_painted;
    char dir[128];
    char cards[MAX_CARDS][48];
    int nc;
    int band;
    int rail;
    int card;
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
    uint64_t tick;    /* repaint timer, when ui.fps > 0 */
    int acknowledged;
    struct icon icons[MAX_ICONS];
    int n;
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

static int load_blob(struct ctx *c, const char *name)
{
    if (c->n >= MAX_ICONS || !name || !name[0])
        return -1;

    char path[256];
    snprintf(path, sizeof path, "%s/%s.rgba", c->dir, name);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        c->api->log(c->api->priv, OA_LOG_WARN, "icon %s: not found", path);
        return -1;
    }
    unsigned char hdr[12];
    if (read(fd, hdr, sizeof hdr) != (ssize_t)sizeof hdr || memcmp(hdr, "OAI1", 4) != 0) {
        close(fd);
        c->api->log(c->api->priv, OA_LOG_WARN, "icon %s: bad header", path);
        return -1;
    }
    uint32_t w = (uint32_t)hdr[4] | (uint32_t)hdr[5] << 8 | (uint32_t)hdr[6] << 16 |
                 (uint32_t)hdr[7] << 24;
    uint32_t h = (uint32_t)hdr[8] | (uint32_t)hdr[9] << 8 | (uint32_t)hdr[10] << 16 |
                 (uint32_t)hdr[11] << 24;
    size_t len = (size_t)w * h * 4;
    unsigned char *buf = malloc(len);
    if (!buf) {
        close(fd);
        return -1;
    }
    ssize_t got = read(fd, buf, len);
    close(fd);
    if (got != (ssize_t)len) {
        free(buf);
        c->api->log(c->api->priv, OA_LOG_WARN, "icon %s: short read", path);
        return -1;
    }
    struct icon *ic = &c->icons[c->n++];
    snprintf(ic->name, sizeof ic->name, "%s", name);
    ic->w = (int)w;
    ic->h = (int)h;
    ic->rgba = buf;
    return 0;
}

static const struct icon *find_icon(struct ctx *c, const char *name)
{
    for (int i = 0; i < c->n; i++)
        if (!strcmp(c->icons[i].name, name))
            return &c->icons[i];
    return NULL;
}

/* ------------------------------------------------------------------ geometry */

enum { SCREEN_PORTAL = 0, SCREEN_OPEN = 1, ANIM_DIALOG = 1 };

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

static void acknowledge(struct ctx *c)
{
    const char *ok = c->api->config_get(c->api->priv, "ui.ok_file", "/tmp/openaurora.ready");
    if (!c->acknowledged && ok && ok[0] && c->paint_w > 0 && c->paint_h > 0) {
        char tmp[288];
        if (strlen(ok) >= sizeof tmp - 32) return;
        snprintf(tmp, sizeof tmp, "%s.%ld.tmp", ok, (long)getpid());
        FILE *f = fopen(tmp, "w");
        if (f) {
            int failed = fprintf(f, "%ld %dx%d\n", (long)getpid(), c->paint_w, c->paint_h) < 0;
            if (fclose(f) != 0) failed = 1;
            if (!failed && rename(tmp, ok) == 0) c->acknowledged = 1;
            else unlink(tmp);
        }
    }
}
static void receipt_tick(void *ud)
{
    struct ctx *c = ud;
    if (c->suspended || !c->disp || !c->async || !c->first_serial) return;
    oa_display_result result;
    if (c->async->latest(&result) != 1 || result.serial < c->first_serial ||
        (result.serial >> 32) != (c->first_serial >> 32)) return;
    if (result.serial != c->last_receipt && result.error)
        c->api->log(c->api->priv, OA_LOG_ERROR, "portal frame failed errno %d", result.error);
    c->last_receipt = result.serial;
    if (c->present_pending && result.serial >= c->present_serial &&
        (result.serial >> 32) == (c->present_serial >> 32))
        c->present_pending = 0;
    if (!result.error) acknowledge(c);
}
/* Called only from ui_fb lifecycle/deferred rebind, never another provider's
 * animation callback: timer ownership must stay with this module. */
static int bind_async(struct ctx *c)
{
    if (!c->alpha) c->alpha = c->api->lookup(c->api->priv, OA_SVC_DISPLAY_ALPHA, 1);
    if (!c->async) c->async = c->api->lookup(c->api->priv, OA_SVC_DISPLAY_ASYNC, 1);
    if (c->async && !c->receipt_tick)
        c->receipt_tick = c->api->timer_add(c->api->priv, 10, 10, receipt_tick, c);
    return c->async && !c->receipt_tick ? -1 : 0;
}
static void drop_async(struct ctx *c, int release)
{
    if (c->receipt_tick) c->api->timer_del(c->api->priv, c->receipt_tick);
    c->receipt_tick = c->first_serial = c->last_receipt = 0;
    c->present_serial = 0;
    c->present_pending = 0;
    if (release && c->async) c->api->release(c->api->priv, OA_SVC_DISPLAY_ASYNC);
    c->async = NULL;
    if (release) {
        if (c->alpha) c->api->release(c->api->priv, OA_SVC_DISPLAY_ALPHA);
        c->alpha = NULL;
    }
}
static int request_present(struct ctx *c)
{
    if (!c || !c->disp) return -1;
    if (c->disp->info(&c->paint_w, &c->paint_h, NULL, NULL)) return -1;
    if (c->async) {
        uint64_t serial;
        if (!c->receipt_tick || c->async->request(&serial)) return -1;
        if (!c->first_serial) c->first_serial = serial;
        c->present_serial = serial;
        c->present_pending = 1;
        return 0; /* queue acceptance, never readiness */
    }
    if (c->disp->present()) return -1;
    acknowledge(c); return 0;
}

static void paint(struct ctx *c)
{
    const oa_display_iface *d;
    int W = 0, H = 0;
    if (!c || !c->disp) return;   /* a suspended/stopped module has no display hold */
    d = c->disp;
    rail_hl_sync(c);              /* keeps the 30 ms highlight timer in step with the screen */
    if (d->info(&W, &H, NULL, NULL) != 0 || W <= 0 || H <= 0)
        return;

    /* clear() covers the whole OSD surface the display service reports. */
    /* DialogMainMenu.svg's unnamed full-screen #242628 rectangle: raw opacity6,
     * scaled conversion + rgbColourParser complement yield alpha153. Named
     * background/main_rectangle records describe layout, not opaque paint. */
    int cleared = c->alpha ? c->alpha->clear_rgba(36, 38, 40, 153) :
                            d->clear(14, 14, 13); /* legacy provider fallback */
    if (cleared) { c->api->log(c->api->priv, OA_LOG_ERROR, "portal clear failed"); return; }

    const struct icon *title = find_icon(c, "title");
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
            const struct icon *hl = NULL;
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

        const struct icon *g = find_icon(c, RAIL[i].glyph);
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
        const struct icon *card = find_icon(c, c->cards[i]);
        if (card)
            d->blit_scaled(x, cy, w, h, card->w, card->h, card->rgba);
        else
            d->fill(x, cy, w, h, 32, 32, 32);
    }

    c->scene_painted = 1;
    if (request_present(c) != 0) {
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
    if (g_ctx->screen == SCREEN_PORTAL && g_ctx->disp) paint(g_ctx);
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

static void tick_cb(void *ud)
{
    struct ctx *c = ud;
    if (c && c->disp)
        paint(c);
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
    if (!c->disp) return;
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

    if (!c->disp || !c->clock) {
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
    int want = c->rail_hl_n > 0 && c->disp && c->screen == SCREEN_PORTAL &&
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

    if (!c->disp || c->rail_hl_n <= 0) return;
    /* Async display: advance only after the previous frame completed, so every
     * decoded frame is presented in order instead of being coalesced away. */
    if (c->async && c->present_pending) return;
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
 * as owner, so lookups and releases attribute their holds to ui_fb. While suspended the display
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
        c->api->log(c->api->priv, OA_LOG_WARN, "ui_fb: rebind timer allocation failed");
}

static void on_module_changed(const void *evp, void *ud)
{
    struct ctx *c = ud;
    const oa_module_event *ev = evp;
    if (!c || !ev || !ev->name) return;
    if (!strcmp(ev->name, "ui_fb")) return;   /* our own start/stop cannot change our providers */
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
     * dangle; the deferred rebind reacquires whatever is published again. */
    if (!strcmp(ev->name, OA_SVC_DISPLAY)) {
        drop_async(c, 1);
        c->disp = NULL;
        c->disp2 = NULL;
        c->scene_painted = 0;
    } else if (!strcmp(ev->name, OA_SVC_DISPLAY_ASYNC)) {
        drop_async(c, 0); /* host already cleared this service's hold */
    } else if (!strcmp(ev->name, OA_SVC_DISPLAY_ALPHA)) {
        c->alpha = NULL; /* host already cleared this service's hold */
    } else if (!strcmp(ev->name, OA_SVC_UI_ANIM)) {
        c->anim = NULL;
        c->anim2 = NULL;
        if (c->disp2 && c->disp2->set_scene_transform)
            c->disp2->set_scene_transform(0, 0.0f);
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
    if (!c->disp) {
        c->disp = c->api->lookup(c->api->priv, OA_SVC_DISPLAY, 1);
        if (!c->disp) return;              /* nothing can be drawn or timed without a display */
    }
    c->disp2 = c->api->lookup(c->api->priv, OA_SVC_DISPLAY, 2);
    if (bind_async(c)) return;
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
        if (c->disp2 && c->disp2->set_scene_transform && c->fs_type > 0)
            c->disp2->set_scene_transform(done ? 0 : c->fs_type, done ? 0.0f : eased);
        if (done) {
            c->screen = SCREEN_OPEN;
        }
        if (!c->suspended && c->disp) {
            if (c->async && !done && c->scene_painted) (void)request_present(c);
            else paint(c);
        }
        return;
    }
    if (kind == OA_ANIM_LIST && rect && c->disp) {
        c->disp->clear(14, 14, 13);
        c->disp->fill(rect->x, rect->y, rect->w, rect->h, 247, 148, 30);
        c->scene_painted = 1;
        (void)request_present(c);
    }
}

static void cancel_dialog(struct ctx *c)
{
    if (c->anim) (void)c->anim->cancel(ANIM_DIALOG);
    if (c->disp2 && c->disp2->set_scene_transform)
        c->disp2->set_scene_transform(0, 0.0f);
}

static void move_key(struct ctx *c, uint8_t id)
{
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
        if (c->anim && c->anim->enabled() &&
            c->anim->start(ANIM_DIALOG, OA_ANIM_DIALOG, NULL, NULL, 1000, NULL) == 0)
            return;
        c->screen = SCREEN_OPEN;
        if (c->disp) paint(c);
        return;
    } else if (id == 10) { /* Exit */
        cancel_dialog(c);
        if (c->screen == SCREEN_OPEN) c->screen = SCREEN_PORTAL;
        emit_action(c, OA_ACTION_BACK);
        if (c->disp) paint(c);
        return;
    } else if (id == 48) { /* Menu */
        cancel_dialog(c);
        if (c->screen == SCREEN_OPEN) c->screen = SCREEN_PORTAL;
        emit_action(c, OA_ACTION_HOME);
        if (c->disp) paint(c);
        return;
    } else if (id == 56) { /* Aux: source, value from settings when that column reads */
        emit_action(c, OA_ACTION_SOURCE);
        return;
    } else {
        return;
    }
    if (c->disp)
        paint(c);
}

static void on_key(const void *evp, void *ud)
{
    struct ctx *c = ud;
    const oa_key_event *ev = evp;
    if (!c || !ev || ev->kind != OA_KEY_PRESS)
        return;
    move_key(c, ev->id);
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

static int m_start(void *v)
{
    struct ctx *c = v;
    snprintf(c->dir, sizeof c->dir, "%s",
             c->api->config_get(c->api->priv, "ui.dir", "/buffer/openaurora/ui"));
    c->disp = c->api->lookup(c->api->priv, OA_SVC_DISPLAY, 1);
    if (!c->disp) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "no display service — load mod_display first");
        return -1;
    }
    c->scene_painted = 0;
    if (bind_async(c)) return -1;

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
    }
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
    /* v2 display adds the offscreen scene + full-screen transition at present. */
    c->disp2 = c->api->lookup(c->api->priv, OA_SVC_DISPLAY, 2);
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
                    "ui_fb: lifecycle subscription failed; late providers will not be discovered");
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
        c->api->release(c->api->priv, OA_SVC_DISPLAY);
        c->disp = NULL;
        return -1;
    }
    c->portal_on = 1;

    load_blob(c, "title");
    for (int i = 0; i < 5; i++)
        load_blob(c, RAIL[i].glyph);
    for (int i = 0; i < c->nc; i++)
        load_blob(c, c->cards[i]);

    /* Vendor main-launcher highlight: 11 frames at 30 ms, played once per image/focus. The
     * decoder advances only while index < frame count (decodeNextFrame 0xb30654) and does not
     * rearm at the end, so the last frame is held; the APNG num_plays field is not consulted. */
    c->rail_hl_n = 0;
    c->rail_hl_frame = 0;
    c->rail_hl_play = 1;
    c->rail_hl_next = now_ms() + OA_RAIL_HL_MS;
    if (atoi(c->api->config_get(c->api->priv, "ui.rail_hl", "1")) == 1) {
        for (int i = 1; i <= OA_RAIL_HL_FRAMES; i++) {
            char name[32];
            snprintf(name, sizeof name, "rail_hl%02d", i);
            if (load_blob(c, name) != 0)
                break;
            c->rail_hl_n++;
        }
    }

    /* The slide needs the title width, so it starts after the blobs are loaded. The clipped draw
     * needs a scratch buffer of exactly the box width; without it the label would spill. */
    {
        const struct icon *t = find_icon(c, "title");
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

    c->sub_key = c->api->subscribe(c->api->priv, OA_TOPIC_KEY, on_key, c);
    if (!c->sub_key) {
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
        c->api->release(c->api->priv, OA_SVC_DISPLAY);
        c->disp = NULL;
        return -1;
    }
    if (c->api->config_get(c->api->priv, "ui.paint", "1")[0] == '1')
        paint(c);

    /* ui.fps > 0 keeps repainting on a timer.
     *
     * While the vendor aurora still owns this layer it redraws over us whenever it feels like
     * it (any key, any animation), so a one-shot paint flickers away. Holding the frame at a
     * low rate wins the layer back for testing; the real UI needs the tick anyway.
     */
    c->fps = atoi(c->api->config_get(c->api->priv, "ui.fps", "0"));
    if (c->fps > 60)
        c->fps = 60;
    if (c->fps > 0) {
        c->tick = c->api->timer_add(c->api->priv, (uint32_t)(1000 / c->fps),
                                    (uint32_t)(1000 / c->fps), tick_cb, c);
        c->api->log(c->api->priv, OA_LOG_INFO, "repainting at %d fps", c->fps);
    }
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    cancel_dialog(c);
    drop_async(c, 1);
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
    if (c->tick) {
        c->api->timer_del(c->api->priv, c->tick);
        c->tick = 0;
    }
    if (c->sub_key) {
        c->api->unsubscribe(c->api->priv, c->sub_key);
        c->sub_key = 0;
    }
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
    if (c->disp)
        c->api->release(c->api->priv, OA_SVC_DISPLAY);
    c->disp = NULL;
    c->disp2 = NULL;
    for (int i = 0; i < c->n; i++)
        free(c->icons[i].rgba);
    c->n = 0;
    free(c->slide_buf);
    c->slide_buf = NULL;
    c->title_w = 0;
    c->slide_box = 0;
    c->acknowledged = 0;
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
    drop_async(c, 1);
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
    if (c->tick) {
        c->api->timer_del(c->api->priv, c->tick);
        c->tick = 0;
    }
    if (c->sub_key) {
        c->api->unsubscribe(c->api->priv, c->sub_key);
        c->sub_key = 0;
    }
    /* the display hold must be dropped here, or every suspend/resume cycle leaks one */
    if (c->disp)
        c->api->release(c->api->priv, OA_SVC_DISPLAY);
    c->disp = NULL;
    c->disp2 = NULL;
    return 0;
}

static int m_resume(void *v)
{
    struct ctx *c = v;
    c->disp = c->api->lookup(c->api->priv, OA_SVC_DISPLAY, 1);
    if (!c->disp)
        return -1;
    c->disp2 = c->api->lookup(c->api->priv, OA_SVC_DISPLAY, 2);
    if (bind_async(c)) {
        drop_async(c, 1);
        c->api->release(c->api->priv, OA_SVC_DISPLAY);
        c->disp = c->disp2 = NULL;
        return -1;
    }
    c->sub_key = c->api->subscribe(c->api->priv, OA_TOPIC_KEY, on_key, c);
    if (!c->sub_key) {
        drop_async(c, 1);
        c->api->release(c->api->priv, OA_SVC_DISPLAY);
        c->disp = NULL;
        c->disp2 = NULL;
        return -1;
    }
    c->suspended = 0;
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
    if (c->fps > 0)
        c->tick = c->api->timer_add(c->api->priv, (uint32_t)(1000 / c->fps),
                                    (uint32_t)(1000 / c->fps), tick_cb, c);
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
    if (bytes[0] > BAND_CARDS || bytes[1] > 4 || bytes[2] >= MAX_CARDS || bytes[3] > SCREEN_OPEN)
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
    .name = "ui_fb",
    .version = "0.2",
    .requires = { OA_SVC_DISPLAY, "" },
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
