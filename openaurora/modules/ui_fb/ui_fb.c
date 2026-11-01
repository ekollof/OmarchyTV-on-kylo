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
#include <unistd.h>

#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

#define MAX_ICONS 24
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

struct ctx {
    const oa_host_api *api;
    const oa_display_iface *disp;
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
    const oa_ui_clock_iface *clock;
    oa_slide slide;                  /* title text slide, vendor AnimationHelper rules */
    int slide_box;                   /* usable width in px; 0 disables the effect */
    int title_w;
    uint64_t slide_tick;             /* 10 ms sampler while the slide runs */
    uint64_t slide_repeat;           /* 2000 ms one-shot, armed after completion */
    int portal_on;
    int fps;
    uint32_t sub_key;
    uint64_t tick;    /* repaint timer, when ui.fps > 0 */
    int acknowledged;
    struct icon icons[MAX_ICONS];
    int n;
};

static struct ctx *g_ctx;

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

static void paint(struct ctx *c)
{
    const oa_display_iface *d = c->disp;
    int W = 0, H = 0;
    if (d->info(&W, &H, NULL, NULL) != 0 || W <= 0 || H <= 0)
        return;

    /* clear() covers the whole OSD surface the display service reports. */
    d->clear(14, 14, 13);

    const struct icon *title = find_icon(c, "title");
    if (title) {
        /* SVGTextBox draw: the label is shifted by the AnimationHelper margin (positive = scrolled
         * left by that many pixels), so the visible window is the 29 px + slide_box area. */
        uint32_t now = c->clock ? c->clock->now_ms() : 0;
        int shift = oa_slide_offset(&c->slide, now);
        d->blit(29 - shift, 80, title->w, title->h, title->rgba);
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
        if (focused)
            d->fill(x - 5, y - 5, w + 10, h + 10, 247, 148, 30);
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

    if (d->present() != 0) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "portal present failed");
        return;
    }
    c->api->log(c->api->priv, OA_LOG_INFO,
                "painted portal: 5 rail items, %d cards, band %d rail %d card %d",
                c->nc, c->band, c->rail, c->card);

    /* One-shot first-present receipt, not liveness or proof of physical scanout.
     * The host loop has a separate heartbeat on tmpfs. */
    const char *ok = c->api->config_get(c->api->priv, "ui.ok_file", "/tmp/openaurora.ready");
    if (!c->acknowledged && ok && ok[0]) {
        char tmp[288];
        if (strlen(ok) >= sizeof tmp - 32)
            return;
        snprintf(tmp, sizeof tmp, "%s.%ld.tmp", ok, (long)getpid());
        FILE *f = fopen(tmp, "w");
        if (f) {
            int failed = fprintf(f, "%ld %dx%d\n", (long)getpid(), W, H) < 0;
            if (fclose(f) != 0)
                failed = 1;
            if (!failed && rename(tmp, ok) == 0)
                c->acknowledged = 1;
            else
                unlink(tmp);
        }
    }
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

static void on_slide_repeat(void *ud)
{
    struct ctx *c = ud;
    c->slide_repeat = 0;
    if (!c->disp || c->title_w <= c->slide_box) return;
    oa_slide_start(&c->slide, c->title_w - c->slide_box, 0,
                   c->clock ? c->clock->now_ms() : 0);
    if (!c->slide_tick)
        c->slide_tick = c->api->timer_add(c->api->priv, 10, 10, on_slide_tick, c);
}

static void on_slide_tick(void *ud)
{
    struct ctx *c = ud;
    uint32_t now = c->clock ? c->clock->now_ms() : 0;

    if (oa_slide_running(&c->slide, now)) {
        paint(c);
        return;
    }
    /* completed: stop sampling, draw the final offset, then arm the vendor's 2000 ms repeat */
    if (c->slide_tick) {
        c->api->timer_del(c->api->priv, c->slide_tick);
        c->slide_tick = 0;
    }
    paint(c);
    if (!c->slide_repeat && c->title_w > c->slide_box)
        c->slide_repeat = c->api->timer_add(c->api->priv, OA_ANIM_SLIDE_REPEAT_MS, 0, on_slide_repeat, c);
}

static void on_anim(int id, int kind, const oa_anim_rect *rect, float eased, int done, void *ud)
{
    struct ctx *c = ud;
    (void)id;
    (void)eased;
    if (!c) return;
    if (kind == OA_ANIM_DIALOG && done) {
        c->screen = SCREEN_OPEN;
        if (c->disp) paint(c);
        return;
    }
    if (kind == OA_ANIM_LIST && rect && c->disp) {
        c->disp->clear(14, 14, 13);
        c->disp->fill(rect->x, rect->y, rect->w, rect->h, 247, 148, 30);
        (void)c->disp->present();
    }
}

static void move_key(struct ctx *c, uint8_t id)
{
    if (id == 20) { /* Up */
        if (c->band == BAND_CARDS)
            c->band = BAND_RAIL;
        else if (c->rail > 0)
            c->rail--;
    } else if (id == 19) { /* Down */
        if (c->band == BAND_RAIL && c->rail < 4)
            c->rail++;
    } else if (id == 21) { /* Left */
        if (c->band == BAND_CARDS && c->card > 0)
            c->card--;
        else
            c->band = BAND_RAIL;
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
        if (c->screen == SCREEN_OPEN) c->screen = SCREEN_PORTAL;
        emit_action(c, OA_ACTION_BACK);
        if (c->disp) paint(c);
        return;
    } else if (id == 48) { /* Menu */
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
    c->slide_box = atoi(c->api->config_get(c->api->priv, "ui.slide_box", "0"));
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

    /* The slide needs the title width, so it starts after the blobs are loaded. */
    {
        const struct icon *t = find_icon(c, "title");
        c->title_w = t ? t->w : 0;
    }
    if (c->anim2 && c->clock && c->slide_box > 0 && c->title_w > c->slide_box) {
        oa_slide_start(&c->slide, c->title_w - c->slide_box, 0, c->clock->now_ms());
        c->slide_tick = c->api->timer_add(c->api->priv, 10, 10, on_slide_tick, c);
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "text slide: title %d px in %d px box (%u ms per pass, %d ms repeat)",
                    c->title_w, c->slide_box, (unsigned)c->slide.duration,
                    OA_ANIM_SLIDE_REPEAT_MS);
    }

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
    if (c->tick) {
        c->api->timer_del(c->api->priv, c->tick);
        c->tick = 0;
    }
    if (c->sub_key) {
        c->api->unsubscribe(c->api->priv, c->sub_key);
        c->sub_key = 0;
    }
    if (c->anim) {
        c->anim->bind(NULL, NULL);
        c->api->release(c->api->priv, OA_SVC_UI_ANIM);
        c->anim = NULL;
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
    for (int i = 0; i < c->n; i++)
        free(c->icons[i].rgba);
    c->n = 0;
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
    return 0;
}

static int m_resume(void *v)
{
    struct ctx *c = v;
    c->disp = c->api->lookup(c->api->priv, OA_SVC_DISPLAY, 1);
    if (!c->disp)
        return -1;
    c->sub_key = c->api->subscribe(c->api->priv, OA_TOPIC_KEY, on_key, c);
    if (!c->sub_key) {
        c->api->release(c->api->priv, OA_SVC_DISPLAY);
        c->disp = NULL;
        return -1;
    }
    paint(c);
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
