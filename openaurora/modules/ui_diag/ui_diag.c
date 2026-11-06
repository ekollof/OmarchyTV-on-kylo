/* mod_ui_diag — reloadable diagnostic skin over ui.engine.
 *
 * Probes a fixed set of project services, paints a green/red status table,
 * and lets Up/Down select a row and Select re-probe. It is a module/skin
 * integration harness, not a vendor screen. Optional EF41 text is loaded from
 * the configured font pack; without it the status bars still paint. */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

enum probe_kind {
    P_PRESENT, P_ENGINE, P_DISPLAY, P_SETTINGS, P_LAUNCHER, P_SERVICE,
    P_EPG, P_TUNER, P_PVR, P_CURVE, P_INPUT, P_PORTAL
};

struct row {
    const char *label;
    const char *service;
    unsigned version;
    enum probe_kind kind;
};

static const struct row ROWS[] = {
    { "ui.engine",   OA_SVC_UI_ENGINE,    2, P_ENGINE },
    { "display",     OA_SVC_DISPLAY,      1, P_DISPLAY },
    { "settings",    OA_SVC_SETTINGS,     1, P_SETTINGS },
    { "launcher.db", OA_SVC_LAUNCHER_DB,  1, P_LAUNCHER },
    { "service.list",OA_SVC_SERVICE_LIST, 1, P_SERVICE },
    { "epg.ingest",  OA_SVC_EPG_INGEST,   1, P_EPG },
    { "tuner",       OA_SVC_TUNER,        1, P_TUNER },
    { "pvr",         OA_SVC_PVR,          2, P_PVR },
    { "audio.curve", OA_SVC_AUDIO_CURVE,  1, P_CURVE },
    { "input",       OA_SVC_INPUT,        1, P_INPUT },
    { "ui.portal",   OA_SVC_PORTAL,       1, P_PORTAL },
    { "ui.lua",      OA_SVC_UI_LUA,       OA_LUA_API_VERSION, P_PRESENT },
};
#define ROW_COUNT ((int)(sizeof ROWS / sizeof ROWS[0]))

struct ctx {
    const oa_host_api *api;
    const oa_ui_engine_iface *d;
    int suspended;
    int selected;
    int font_on;
    int title_on;
    struct oa_font_pack pack;
    struct oa_bitmap_font font;
    struct oa_bitmap_font title;
    unsigned char *font_data;
    unsigned char *title_data;
    uint64_t changed;
    /* ABI 3 quiesce: font buffers are detached and freed by a sched slice. */
    uint32_t cleanup_task;
    struct oa_font_pack cleanup_pack;
    unsigned char *cleanup_font_data;
    unsigned char *cleanup_title_data;
    unsigned cleanup_delay_ms;
};

static const oa_ui_skin g_skin;

static void dispatch(void *ud)
{
    struct ctx *c = ud;
    if (c->d && !c->suspended) c->d->dispatch_skin();
}

static void fonts_free(struct ctx *c)
{
    free(c->font_data);
    free(c->title_data);
    c->font_data = c->title_data = NULL;
    c->font_on = c->title_on = 0;
    oa_font_pack_free(&c->pack);
}

static int cleanup_pending(const struct ctx *c)
{
    return c->cleanup_task || c->cleanup_font_data || c->cleanup_title_data ||
        c->cleanup_pack.lengths || c->cleanup_pack.offsets ||
        c->cleanup_pack.pack_path;
}

static void fonts_free_cleanup(struct ctx *c)
{
    free(c->cleanup_font_data);
    free(c->cleanup_title_data);
    c->cleanup_font_data = c->cleanup_title_data = NULL;
    oa_font_pack_free(&c->cleanup_pack);
}

static void fonts_detach(struct ctx *c)
{
    c->cleanup_pack = c->pack;
    memset(&c->pack, 0, sizeof c->pack);
    c->cleanup_font_data = c->font_data;
    c->cleanup_title_data = c->title_data;
    c->font_data = c->title_data = NULL;
    c->font_on = c->title_on = 0;
}

static int cleanup_step(void *v, uint32_t *delay_ms)
{
    (void)delay_ms;
    struct ctx *c = v;
    fonts_free_cleanup(c);
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
    fonts_free_cleanup(c);
}

static int font_open(struct oa_font_pack *pack, int id,
                     struct oa_bitmap_font *font, unsigned char **data)
{
    unsigned char *buf = NULL;
    size_t size = 0;
    if (oa_font_pack_read(pack, (unsigned)id, &buf, &size)) return -1;
    if (oa_bitmap_font_open(font, buf, size)) {
        free(buf);
        return -1;
    }
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
    int id = atoi(c->api->config_get(c->api->priv, "ui.diag.font", "7"));
    int title_id = atoi(c->api->config_get(c->api->priv, "ui.diag.title_font", "18"));
    if (font_open(&c->pack, id, &c->font, &c->font_data)) return;
    c->font_on = 1;
    if (!font_open(&c->pack, title_id, &c->title, &c->title_data))
        c->title_on = 1;
}

static void text(struct ctx *c, int x, int y, const struct oa_bitmap_font *font,
                 const char *line, int max_width)
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
    const uint8_t ink[4] = { 235, 238, 242, 255 };
    if (!oa_font_text_draw(&canvas, font, line, 1, 1, ink, 0, NULL))
        c->d->blit(x, y, w, h, buf);
    free(buf);
}

static int probe(struct ctx *c, const struct row *row, char *detail, size_t cap)
{
    snprintf(detail, cap, "--");
    const void *p = c->api->lookup(c->api->priv, row->service, row->version);
    if (!p) return 0;
    int present = 1;
    switch (row->kind) {
    case P_ENGINE: {
        const oa_ui_engine_iface *e = p;
        int w = 0, h = 0;
        if (!e->info(&w, &h, NULL, NULL)) snprintf(detail, cap, "%dx%d", w, h);
        break;
    }
    case P_DISPLAY: {
        const oa_display_iface *d = p;
        int w = 0, h = 0;
        if (!d->info(&w, &h, NULL, NULL)) snprintf(detail, cap, "%dx%d", w, h);
        break;
    }
    case P_SETTINGS: {
        const oa_settings_iface *s = p;
        int v = 0;
        if (!s->get_int("m_current_source", &v))
            snprintf(detail, cap, "src %d", v);
        break;
    }
    case P_LAUNCHER: {
        const oa_launcher_db_iface *db = p;
        struct oa_launcher_list *list = NULL;
        if (!db->read_all(&list) && list) {
            snprintf(detail, cap, "%zu", db->count(list));
            db->release(list);
        }
        break;
    }
    case P_SERVICE: {
        const oa_service_list_iface *s = p;
        const struct oa_service_list *list = s->snapshot();
        if (list) snprintf(detail, cap, "%zu", list->count);
        break;
    }
    case P_EPG: {
        const oa_epg_ingest_iface *e = p;
        snprintf(detail, cap, "%zu", e->count());
        break;
    }
    case P_TUNER: {
        const oa_tuner_iface *t = p;
        struct oa_tuner_status st;
        if (!t->status(&st)) snprintf(detail, cap, "state %d", st.scan_state);
        break;
    }
    case P_PVR: {
        const oa_pvr_iface *pvr = p;
        size_t count = 0;
        if (!pvr->list(NULL, 0, &count)) snprintf(detail, cap, "%zu", count);
        break;
    }
    case P_CURVE: {
        const oa_audio_curve_iface *curve = p;
        uint16_t value = 0;
        if (!curve->value(50, &value)) snprintf(detail, cap, "%u", value);
        break;
    }
    case P_INPUT: {
        const oa_input_iface *in = p;
        snprintf(detail, cap, "%s", in->device_open() ? "open" : "feed");
        break;
    }
    case P_PORTAL: {
        const oa_portal_iface *portal = p;
        snprintf(detail, cap, "screen %d", portal->screen());
        break;
    }
    default:
        break;
    }
    c->api->release(c->api->priv, row->service);
    return present;
}

static void paint(struct ctx *c)
{
    const oa_ui_engine_iface *d = c ? c->d : NULL;
    int W = 0, H = 0;
    if (!c || !d || c->suspended || d->info(&W, &H, NULL, NULL) ||
        W <= 0 || H <= 0)
        return;
    int pool = W - 96;
    if (pool < 400) pool = W;
    d->clear(10, 12, 16);
    d->fill(0, 0, W, 120, 0, 110, 190);
    if (c->title_on)
        text(c, 48, 28, &c->title, "OpenAurora diagnostics", pool);
    else if (c->font_on)
        text(c, 48, 48, &c->font, "OpenAurora diagnostics", pool);
    text(c, 48, 78, &c->font, "reloadable service graph", pool);

    for (int i = 0; i < ROW_COUNT; i++) {
        int y = 150 + i * 66;
        char detail[64];
        int ok = probe(c, &ROWS[i], detail, sizeof detail);
        if (i == c->selected)
            d->fill(40, y - 4, W - 80, 66, 247, 148, 30);
        d->fill(48, y, W - 96, 58, ok ? 24 : 88, ok ? 86 : 30,
                ok ? 58 : 44);
        text(c, 70, y + 12, &c->font, ROWS[i].label, pool - 400);
        text(c, W - 500, y + 12, &c->font, detail, 360);
        text(c, W - 120, y + 12, &c->font, ok ? "OK" : "--", 80);
    }
    d->fill(0, H - 64, W, 64, 24, 28, 36);
    text(c, 48, H - 48, &c->font,
         "Up/Down select   Select refresh   skin vendor to return", W - 96);
    (void)d->present();
}

static int enter(void *ud)
{
    paint(ud);
    return 0;
}

static void leave(void *ud) { (void)ud; }
static void skin_paint(void *ud) { paint(ud); }

static int skin_key(void *ud, int id)
{
    struct ctx *c = ud;
    if (!c) return 1;
    /* Project control key (emit-key 240): the host defers the switch until this
     * callback has returned, then loads ui_omarchy and unloads diag. */
    if (id == 0xf0) {
        char reply[64];
        (void)c->api->control(c->api->priv, "skin omarchy", reply, sizeof reply);
        return 0;
    }
    if (id == 19 || id == 20) {
        c->selected += id == 19 ? 1 : -1;
        if (c->selected < 0) c->selected = ROW_COUNT - 1;
        if (c->selected >= ROW_COUNT) c->selected = 0;
        paint(c);
        return 0;
    }
    if (id == 53 || id == 0x8a || id == 10 || id == 48) {
        paint(c);
        return 0;
    }
    return 1;
}

static const oa_ui_skin g_skin = { "diag", enter, leave, skin_paint, skin_key };

static void changed(const void *ev, void *ud)
{
    (void)ev;
    struct ctx *c = ud;
    if (c && c->d && !c->suspended) paint(c);
}

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION ||
        !out)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api;
    c->selected = 0;
    *out = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (cleanup_pending(c)) return -EBUSY;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "ui_diag.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 2);
    if (!c->d) return -1;
    if (c->d->register_skin_dispatch(&g_skin, c, dispatch)) {
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
        return -1;
    }
    c->changed = c->api->subscribe(c->api->priv, OA_TOPIC_MODULE_CHANGED,
        changed, c);
    fonts_load(c);
    if (c->api->config_get(c->api->priv, "ui.paint", "1")[0] == '1')
        paint(c);
    return 0;
}

/* ABI 3 gate: drop the skin/engine registration and detach the font buffers
 * synchronously, then free them from one scheduler slice after
 * ui_diag.cleanup_delay_ms. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
    if (cleanup_pending(c)) return -EAGAIN;
    c->suspended = 1;
    if (c->changed) {
        c->api->unsubscribe(c->api->priv, c->changed);
        c->changed = 0;
    }
    if (c->d) {
        c->d->unregister_skin(&g_skin);
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
    }
    fonts_detach(c);
    if (!cleanup_pending(c)) return 0;
    if (!c->cleanup_delay_ms) {
        fonts_free_cleanup(c);
        return 0;
    }
    const oa_sched_iface *s =
        c->api->lookup(c->api->priv, OA_SVC_SCHED, 1);
    if (!s) {
        fonts_free_cleanup(c);
        return 0;
    }
    uint32_t id = 0;
    int rc = s->submit(s->context, cleanup_step, c, c->cleanup_delay_ms, &id);
    c->api->release(c->api->priv, OA_SVC_SCHED);
    if (rc) {
        fonts_free_cleanup(c);
        return -EIO;
    }
    c->cleanup_task = id;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    c->suspended = 1;
    if (c->changed) {
        c->api->unsubscribe(c->api->priv, c->changed);
        c->changed = 0;
    }
    if (c->d) {
        c->d->unregister_skin(&g_skin);
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
    }
    cancel_cleanup(c);
    fonts_free(c);
    c->suspended = 0;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    cancel_cleanup(c);
    fonts_free(c);
    free(c);
}

static int m_suspend(void *v)
{
    struct ctx *c = v;
    c->suspended = 1;
    if (c->changed) {
        c->api->unsubscribe(c->api->priv, c->changed);
        c->changed = 0;
    }
    if (c->d) {
        c->d->unregister_skin(&g_skin);
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
    }
    fonts_free(c);
    return 0;
}

static int m_resume(void *v)
{
    struct ctx *c = v;
    c->d = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 2);
    if (!c->d) return -1;
    if (c->d->register_skin_dispatch(&g_skin, c, dispatch)) {
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->d = NULL;
        return -1;
    }
    c->changed = c->api->subscribe(c->api->priv, OA_TOPIC_MODULE_CHANGED,
        changed, c);
    c->suspended = 0;
    fonts_load(c);
    paint(c);
    return 0;
}

static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof descriptor,
    .name = "ui_diag",
    .version = "0.1",
    .requires = { OA_SVC_UI_ENGINE, "" },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .suspend = m_suspend,
    .resume = m_resume,
    .destroy = m_destroy,
    .quiesce = m_quiesce,
};

OA_MODULE_EXPORT(descriptor)
