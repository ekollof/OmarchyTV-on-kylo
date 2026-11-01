/* mod_ui_engine — generic UI engine (display/assets/present/ack).
 *
 * Owns the display service and the OAI1 asset cache. The vendor skin
 * (mod_ui_vendor) and a later OmarchyTV skin draw only through this service.
 * See include/openaurora/ui_engine.h and docs/ARCHITECTURE.md.
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

#define ENGINE_MAX_ICONS 64

struct engine {
    const oa_host_api *api;
    const oa_display_iface *disp;
    const oa_display_iface *disp2;
    const oa_display_async_iface *async;
    const oa_display_alpha_iface *alpha;
    uint64_t present_serial, first_serial, last_receipt;
    int present_pending;
    uint64_t receipt_tick;
    int paint_w, paint_h, scene_painted;
    int acknowledged;
    char dir[128];
    oa_ui_icon icons[ENGINE_MAX_ICONS];
    int n;
    uint32_t mod_key, svc_key;
    uint64_t rebind_tick;
    int suspended, rebind_pending;
    const oa_ui_skin *skin;
    void *skin_ud;
    uint32_t skin_owner;
    int skin_entered;
    uint32_t key_sub, fps_tick;
    int fps;
};

static struct engine *g_engine;

static uint32_t engine_now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

static void engine_log(struct engine *c, int level, const char *msg)
{
    c->api->log(c->api->priv, level, "%s", msg);
}

/* ---------------------------------------------------------------- assets */

static const oa_ui_icon *find_icon(const char *name)
{
    struct engine *c = g_engine;
    if (!c || !name)
        return NULL;
    for (int i = 0; i < c->n; i++)
        if (!strcmp(c->icons[i].name, name))
            return &c->icons[i];
    if (c->n >= ENGINE_MAX_ICONS)
        return NULL;

    char path[256];
    snprintf(path, sizeof path, "%s/%s.rgba", c->dir, name);
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return NULL;
    unsigned char hdr[12];
    if (read(fd, hdr, sizeof hdr) != (ssize_t)sizeof hdr || memcmp(hdr, "OAI1", 4) != 0) {
        close(fd);
        return NULL;
    }
    uint32_t w = (uint32_t)hdr[4] | (uint32_t)hdr[5] << 8 | (uint32_t)hdr[6] << 16 |
                 (uint32_t)hdr[7] << 24;
    uint32_t h = (uint32_t)hdr[8] | (uint32_t)hdr[9] << 8 | (uint32_t)hdr[10] << 16 |
                 (uint32_t)hdr[11] << 24;
    if (w == 0 || h == 0 || w > 8192 || h > 8192) {
        close(fd);
        return NULL;
    }
    size_t len = (size_t)w * h * 4;
    unsigned char *buf = malloc(len);
    if (!buf) {
        close(fd);
        return NULL;
    }
    ssize_t got = read(fd, buf, len);
    close(fd);
    if (got != (ssize_t)len) {
        free(buf);
        return NULL;
    }
    oa_ui_icon *ic = &c->icons[c->n++];
    snprintf(ic->name, sizeof ic->name, "%s", name);
    ic->w = (int)w;
    ic->h = (int)h;
    ic->rgba = buf;
    return ic;
}

/* --------------------------------------------------------- present/ack */

static void acknowledge(struct engine *c)
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
    struct engine *c = ud;
    if (c->suspended || !c->disp || !c->async || !c->first_serial ||
        c->first_serial == UINT64_MAX) return;
    oa_display_result result;
    if (c->async->latest(&result) != 1 || result.serial < c->first_serial ||
        (result.serial >> 32) != (c->first_serial >> 32)) return;
    if (result.serial != c->last_receipt && result.error)
        c->api->log(c->api->priv, OA_LOG_ERROR, "ui frame failed errno %d", result.error);
    c->last_receipt = result.serial;
    if (c->present_pending && result.serial >= c->present_serial &&
        (result.serial >> 32) == (c->present_serial >> 32)) {
        c->present_pending = 0;
    }
    if (!result.error) acknowledge(c);
}

static int bind_async(struct engine *c)
{
    if (!c->alpha) c->alpha = c->api->lookup(c->api->priv, OA_SVC_DISPLAY_ALPHA, 1);
    if (!c->async) c->async = c->api->lookup(c->api->priv, OA_SVC_DISPLAY_ASYNC, 1);
    if (c->async && !c->receipt_tick)
        c->receipt_tick = c->api->timer_add(c->api->priv, 10, 10, receipt_tick, c);
    return c->async && !c->receipt_tick ? -1 : 0;
}

static void drop_async(struct engine *c, int release)
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

static int present(void)
{
    struct engine *c = g_engine;
    if (!c || !c->disp) return -1;
    if (c->disp->info(&c->paint_w, &c->paint_h, NULL, NULL)) return -1;
    if (c->async) {
        uint64_t serial;
        if (!c->receipt_tick || c->async->request(&serial)) return -1;
        if (!c->first_serial || c->first_serial == UINT64_MAX) c->first_serial = serial;
        c->present_serial = serial;
        c->present_pending = 1;
        return 0;
    }
    if (c->disp->present()) return -1;
    acknowledge(c);
    return 0;
}

/* ------------------------------------------------------------- display */

static void drop_display(struct engine *c, int release)
{
    drop_async(c, release);
    if (release && c->disp) c->api->release(c->api->priv, OA_SVC_DISPLAY);
    c->disp = NULL;
    c->disp2 = NULL;
    c->scene_painted = 0;
}

static int bind_display(struct engine *c)
{
    if (!c->disp) {
        c->disp = c->api->lookup(c->api->priv, OA_SVC_DISPLAY, 1);
        if (!c->disp) return -1;
    }
    c->disp2 = c->api->lookup(c->api->priv, OA_SVC_DISPLAY, 2);
    return bind_async(c);
}

struct skin_post {
    const oa_ui_skin *skin;
    void *ud;
    int kind;   /* 0 enter, 1 paint, 2 key */
    int id;
};

static void skin_post_cb(void *ud)
{
    struct engine *c = g_engine;
    struct skin_post *p = ud;
    if (!p) return;
    if (c && c->skin == p->skin) {
        if (p->kind == 0) {
            if (p->skin->enter) p->skin->enter(p->ud);
            c->skin_entered = 1;
        } else if (p->kind == 1) {
            if (p->skin->paint) p->skin->paint(p->ud);
        } else if (p->skin->key) {
            p->skin->key(p->ud, p->id);
        }
    }
    free(p);
}

static void post_skin(struct engine *c, int kind, int id)
{
    if (!c || !c->skin || !c->skin_owner) return;
    struct skin_post *p = malloc(sizeof *p);
    if (!p) return;
    p->skin = c->skin;
    p->ud = c->skin_ud;
    p->kind = kind;
    p->id = id;
    if (c->api->post(c->api->priv, c->skin_owner, skin_post_cb, p) != 0)
        free(p);
}

static void on_key_event(const void *evp, void *ud)
{
    struct engine *c = ud;
    const oa_key_event *ev = evp;
    if (!c || !ev || ev->kind != OA_KEY_PRESS) return;
    post_skin(c, 2, ev->id);
}

static void fps_cb(void *ud)
{
    post_skin(ud, 1, 0);
}

static void ready_now(struct engine *c)
{
    c->rebind_tick = 0;
    c->rebind_pending = 0;
    int had = c->disp != NULL;
    if (!c->disp && bind_display(c) != 0)
        return;
    if (!had && c->skin)
        post_skin(c, c->skin_entered ? 1 : 0, 0);
}

static void rebind_tick_cb(void *ud)
{
    struct engine *c = ud;
    if (c->suspended) {
        c->rebind_tick = 0;
        c->rebind_pending = 1;
        return;
    }
    ready_now(c);
}

static void schedule_rebind(struct engine *c)
{
    if (c->suspended) {
        c->rebind_pending = 1;
        return;
    }
    if (c->rebind_tick) return;
    c->rebind_tick = c->api->timer_add(c->api->priv, 0, 0, rebind_tick_cb, c);
}

static void on_module_changed(const void *evp, void *ud)
{
    struct engine *c = ud;
    const oa_module_event *ev = evp;
    if (!c || !ev || !ev->name) return;
    if (!strcmp(ev->name, "ui_engine")) return;
    if (ev->what == OA_MOD_STARTED_EV || ev->what == OA_MOD_STOPPED_EV ||
        ev->what == OA_MOD_UNLOADED_EV)
        schedule_rebind(c);
}

static void on_service_removed(const void *evp, void *ud)
{
    struct engine *c = ud;
    const oa_service_removed_event *ev = evp;
    if (!c || !ev || !ev->name) return;
    if (!strcmp(ev->name, OA_SVC_DISPLAY)) {
        drop_display(c, 1);
    } else if (!strcmp(ev->name, OA_SVC_DISPLAY_ASYNC)) {
        drop_async(c, 0);
    } else if (!strcmp(ev->name, OA_SVC_DISPLAY_ALPHA)) {
        c->alpha = NULL;
    } else {
        return;
    }
    schedule_rebind(c);
}

/* ---------------------------------------------------------------- iface */

static int e_info(int *w, int *h, int *bpp, int *frames)
{
    struct engine *c = g_engine;
    if (!c || !c->disp) return -1;
    return c->disp->info(w, h, bpp, frames);
}

static int e_clear(int r, int g, int b)
{
    struct engine *c = g_engine;
    return c && c->disp ? c->disp->clear(r, g, b) : -1;
}

static int e_clear_rgba(int r, int g, int b, int a)
{
    struct engine *c = g_engine;
    if (!c || !c->disp) return -1;
    if (c->alpha) return c->alpha->clear_rgba(r, g, b, a);
    return c->disp->clear(r, g, b);   /* legacy provider: opaque fallback */
}

static int e_fill(int x, int y, int w, int h, int r, int g, int b)
{
    struct engine *c = g_engine;
    return c && c->disp ? c->disp->fill(x, y, w, h, r, g, b) : -1;
}

static int e_blit(int x, int y, int w, int h, const unsigned char *rgba)
{
    struct engine *c = g_engine;
    return c && c->disp ? c->disp->blit(x, y, w, h, rgba) : -1;
}

static int e_blit_scaled(int x, int y, int w, int h, int sw, int sh, const unsigned char *rgba)
{
    struct engine *c = g_engine;
    return c && c->disp ? c->disp->blit_scaled(x, y, w, h, sw, sh, rgba) : -1;
}

static int e_set_scene_transform(int type, float progress)
{
    struct engine *c = g_engine;
    if (!c || !c->disp2 || !c->disp2->set_scene_transform) return -1;
    return c->disp2->set_scene_transform(type, progress);
}

static int e_present_pending(void)
{
    struct engine *c = g_engine;
    return c ? c->present_pending : 0;
}

static int e_register_skin(const oa_ui_skin *skin, void *ud)
{
    struct engine *c = g_engine;
    if (!c || !skin || !skin->name || c->skin)
        return -1;
    const char *want = c->api->config_get(c->api->priv, "ui.skin", "");
    if (want[0] && strcmp(want, skin->name)) {
        c->api->log(c->api->priv, OA_LOG_WARN, "ui_engine: skin '%s' not selected (ui.skin=%s)",
                    skin->name, want);
        return -1;
    }
    c->skin = skin;
    c->skin_ud = ud;
    c->skin_owner = c->api->owner_id(c->api->priv);
    c->skin_entered = 0;
    if (c->disp && !c->suspended)
        post_skin(c, 0, 0);
    return 0;
}

static void e_unregister_skin(const oa_ui_skin *skin)
{
    struct engine *c = g_engine;
    if (c && c->skin == skin) {
        c->skin = NULL;
        c->skin_ud = NULL;
        c->skin_owner = 0;
        c->skin_entered = 0;
    }
}

static void e_reset_ready(void)
{
    struct engine *c = g_engine;
    if (!c) return;
    c->acknowledged = 0;
    c->first_serial = UINT64_MAX;   /* ignore completions that predate this skin generation */
    c->last_receipt = 0;
    c->present_serial = 0;
    c->present_pending = 0;
}

static const oa_ui_engine_iface g_iface = {
    .info = e_info,
    .clear = e_clear,
    .clear_rgba = e_clear_rgba,
    .fill = e_fill,
    .blit = e_blit,
    .blit_scaled = e_blit_scaled,
    .present = present,
    .present_pending = e_present_pending,
    .icon = find_icon,
    .set_scene_transform = e_set_scene_transform,
    .reset_ready = e_reset_ready,
    .now_ms = engine_now_ms,
    .register_skin = e_register_skin,
    .unregister_skin = e_unregister_skin,
};

/* --------------------------------------------------------------- module */

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct engine *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api;
    g_engine = c;
    *out = c;
    return 0;
}

static int m_start(void *v)
{
    struct engine *c = v;
    snprintf(c->dir, sizeof c->dir, "%s",
             c->api->config_get(c->api->priv, "ui.dir", "/buffer/openaurora/ui"));
    if (bind_display(c) != 0) {
        engine_log(c, OA_LOG_ERROR, "ui_engine: no display service");
        drop_display(c, 1);
        return -1;
    }
    c->mod_key = c->api->subscribe(c->api->priv, OA_TOPIC_MODULE_CHANGED, on_module_changed, c);
    c->svc_key = c->api->subscribe(c->api->priv, OA_TOPIC_SERVICE_REMOVED, on_service_removed, c);
    c->key_sub = c->api->subscribe(c->api->priv, OA_TOPIC_KEY, on_key_event, c);
    c->fps = atoi(c->api->config_get(c->api->priv, "ui.fps", "0"));
    if (c->fps > 60) c->fps = 60;
    if (c->fps > 0)
        c->fps_tick = c->api->timer_add(c->api->priv, (uint32_t)(1000 / c->fps),
                                        (uint32_t)(1000 / c->fps), fps_cb, c);
    if (!c->mod_key || !c->svc_key || !c->key_sub) {
        if (c->mod_key) c->api->unsubscribe(c->api->priv, c->mod_key);
        if (c->svc_key) c->api->unsubscribe(c->api->priv, c->svc_key);
        if (c->key_sub) c->api->unsubscribe(c->api->priv, c->key_sub);
        if (c->fps_tick) c->api->timer_del(c->api->priv, c->fps_tick);
        c->mod_key = c->svc_key = c->key_sub = 0;
        c->fps_tick = 0;
        drop_display(c, 1);
        return -1;
    }
    if (c->api->publish(c->api->priv, OA_SVC_UI_ENGINE, 1, &g_iface) != 0) {
        c->api->unsubscribe(c->api->priv, c->mod_key);
        c->api->unsubscribe(c->api->priv, c->svc_key);
        c->api->unsubscribe(c->api->priv, c->key_sub);
        c->mod_key = c->svc_key = c->key_sub = 0;
        if (c->fps_tick) c->api->timer_del(c->api->priv, c->fps_tick);
        c->fps_tick = 0;
        drop_display(c, 1);
        return -1;
    }
    engine_log(c, OA_LOG_INFO, "ui_engine started");
    return 0;
}

static void m_stop(void *v)
{
    struct engine *c = v;
    drop_display(c, 1);
    if (c->mod_key) {
        c->api->unsubscribe(c->api->priv, c->mod_key);
        c->mod_key = 0;
    }
    if (c->svc_key) {
        c->api->unsubscribe(c->api->priv, c->svc_key);
        c->svc_key = 0;
    }
    if (c->key_sub) {
        c->api->unsubscribe(c->api->priv, c->key_sub);
        c->key_sub = 0;
    }
    if (c->fps_tick) {
        c->api->timer_del(c->api->priv, c->fps_tick);
        c->fps_tick = 0;
    }
    c->skin = NULL;
    c->skin_ud = NULL;
    c->skin_owner = 0;
    c->skin_entered = 0;
    if (c->rebind_tick) {
        c->api->timer_del(c->api->priv, c->rebind_tick);
        c->rebind_tick = 0;
    }
    c->api->unpublish(c->api->priv, OA_SVC_UI_ENGINE);
    for (int i = 0; i < c->n; i++)
        free(c->icons[i].rgba);
    c->n = 0;
    c->acknowledged = 0;
    c->suspended = 0;
    c->rebind_pending = 0;
}

static void m_destroy(void *v)
{
    if (g_engine == v)
        g_engine = NULL;
    free(v);
}

static int m_suspend(void *v)
{
    struct engine *c = v;
    c->suspended = 1;
    drop_display(c, 1);
    if (c->fps_tick) {
        c->api->timer_del(c->api->priv, c->fps_tick);
        c->fps_tick = 0;
    }
    c->skin = NULL;
    c->skin_ud = NULL;
    c->skin_owner = 0;
    c->skin_entered = 0;
    if (c->rebind_tick) {
        c->api->timer_del(c->api->priv, c->rebind_tick);
        c->rebind_tick = 0;
    }
    return 0;
}

static int m_resume(void *v)
{
    struct engine *c = v;
    c->suspended = 0;
    if (bind_display(c) != 0) {
        drop_display(c, 1);
        return -1;
    }
    if (c->fps > 0 && !c->fps_tick)
        c->fps_tick = c->api->timer_add(c->api->priv, (uint32_t)(1000 / c->fps),
                                        (uint32_t)(1000 / c->fps), fps_cb, c);
    if (c->skin)
        post_skin(c, c->skin_entered ? 1 : 0, 0);
    return 0;
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "ui_engine",
    .version = "1",
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .suspend = m_suspend,
    .resume = m_resume,
    .destroy = m_destroy,
    .requires = { OA_SVC_DISPLAY, "" },
};

OA_MODULE_EXPORT(g_module)
