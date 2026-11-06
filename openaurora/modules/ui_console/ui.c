/*
 * mod_ui_console — a UI as a module, deliberately trivial.
 *
 * It subscribes to input/key and draws a text "home screen" on stdout, so the
 * whole UI lifecycle (load, start, stop, hot reload) can be exercised without a
 * display. The selection index is saved/restored, so `reload ui_console` keeps
 * the cursor where it was — that is the point of the state-handoff API.
 *
 * On the TV this module is replaced by a real renderer (DRM/Wayland/OMI); the
 * event contract stays identical.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

#define TILES 4
static const char *g_tiles[TILES] = { "TV", "HDMI / sources", "apps & settings", "about" };

struct console_state {
    uint8_t sel;
    unsigned redraws;
};
struct ctx {
    const oa_host_api *api;
    uint32_t sub_key;
    uint32_t sub_tick;
    struct console_state *state;
    int active;
    /* ABI 3 quiesce: selection state is detached and freed by a sched slice. */
    uint32_t cleanup_task;
    struct console_state *cleanup_state;
    unsigned cleanup_delay_ms;
    uint8_t saved_sel;
    int have_saved;
};

static void draw(struct ctx *c)
{
    if (!c->active || !c->state) return;
    c->state->redraws++;
    printf("\n  +-- openaurora --------------------------------+\n");
    for (int i = 0; i < TILES; i++)
        printf("  | %s %-42s|\n", i == c->state->sel ? ">" : " ", g_tiles[i]);
    printf("  +---------------------------------------------+\n");
    fflush(stdout);
}

/* key ids are the NEC command bytes published by mod_ir */
static void on_key(const void *evp, void *ud)
{
    struct ctx *c = ud;
    const oa_key_event *ev = evp;
    if (!c->active || !c->state || ev->kind != OA_KEY_PRESS)
        return;

    switch (ev->id) {
    case 0x30: /* '9' */
    case 0x50: /* '7' */
        break;
    default:
        break;
    }

    /* digits select a tile by number: 1..4 are cmd 0x20,0xa0,0xfa,0xe0 */
    switch (ev->id) {
    case 0x20: c->state->sel = 0; break;
    case 0xa0: c->state->sel = 1; break;
    case 0xfa: c->state->sel = 2; break;
    case 0xe0: c->state->sel = 3; break;
    case 0x8a: { /* OK */
        oa_action_event act;
        memset(&act, 0, sizeof act);
        act.kind = OA_ACTION_SELECT;
        act.index = c->state->sel;
        act.name = g_tiles[c->state->sel];
        c->api->emit(c->api->priv, OA_TOPIC_ACTION, &act);
        printf("  [ui] select %u (%s)\n", c->state->sel,
               g_tiles[c->state->sel]);
        fflush(stdout);
        return;
    }
    default:
        c->api->log(c->api->priv, OA_LOG_DEBUG, "ui: key 0x%02x not bound", ev->id);
        return;
    }
    draw(c);
}

static void on_tick(const void *evp, void *ud)
{
    (void)evp;
    struct ctx *c = ud;
    if (c->active && c->state && c->state->redraws == 0)
        draw(c);
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

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->state = calloc(1, sizeof *c->state);
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
                                         "ui_console.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    if (!c->state && !(c->state = calloc(1, sizeof *c->state))) return -ENOMEM;
    if (c->have_saved) c->state->sel = c->saved_sel;
    c->sub_key = c->api->subscribe(c->api->priv, OA_TOPIC_KEY, on_key, c);
    c->sub_tick = c->api->subscribe(c->api->priv, OA_TOPIC_TICK, on_tick, c);
    if (!c->sub_key || !c->sub_tick) {
        if (c->sub_key) c->api->unsubscribe(c->api->priv, c->sub_key);
        if (c->sub_tick) c->api->unsubscribe(c->api->priv, c->sub_tick);
        c->sub_key = c->sub_tick = 0;
        return -1;
    }
    c->active = 1;
    draw(c);
    c->api->log(c->api->priv, OA_LOG_INFO, "ui_console up (selection %u)",
                c->state->sel);
    return 0;
}

/* ABI 3 gate: stop callbacks and unsubscribe synchronously, snapshot/detach
 * the selection state, then free it from a scheduler slice after
 * ui_console.cleanup_delay_ms. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
    if (c->cleanup_task || c->cleanup_state) return -EAGAIN;
    c->active = 0;
    if (c->sub_key) {
        c->api->unsubscribe(c->api->priv, c->sub_key);
        c->sub_key = 0;
    }
    if (c->sub_tick) {
        c->api->unsubscribe(c->api->priv, c->sub_tick);
        c->sub_tick = 0;
    }
    struct console_state *state = c->state;
    c->state = NULL;
    if (!state) return 0;
    c->saved_sel = state->sel;
    c->have_saved = 1;
    if (!c->cleanup_delay_ms) {
        free(state);
        return 0;
    }
    c->cleanup_state = state;
    const oa_sched_iface *s =
        c->api->lookup(c->api->priv, OA_SVC_SCHED, 1);
    if (!s) {
        free(state);
        c->cleanup_state = NULL;
        return 0;
    }
    uint32_t id = 0;
    int rc = s->submit(s->context, cleanup_step, c, c->cleanup_delay_ms, &id);
    c->api->release(c->api->priv, OA_SVC_SCHED);
    if (rc) {
        free(state);
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
    if (c->sub_key) {
        c->api->unsubscribe(c->api->priv, c->sub_key);
        c->sub_key = 0;
    }
    if (c->sub_tick) {
        c->api->unsubscribe(c->api->priv, c->sub_tick);
        c->sub_tick = 0;
    }
    cancel_cleanup(c);
    free(c->state);
    c->state = NULL;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    cancel_cleanup(c);
    free(c->state);
    free(c);
}

static size_t m_save(void *v, void *buf, size_t cap)
{
    struct ctx *c = v;
    uint8_t sel = c->state ? c->state->sel : c->saved_sel;
    if (!c->state && !c->have_saved) return 0;
    if (buf && cap >= 1) ((unsigned char *)buf)[0] = sel;
    return 1;
}

static int m_restore(void *v, const void *buf, size_t len)
{
    struct ctx *c = v;
    if (len < 1) return -1;
    uint8_t sel = ((const unsigned char *)buf)[0];
    if (sel >= TILES) sel = 0;
    c->saved_sel = sel;
    c->have_saved = 1;
    if (!c->state && !(c->state = calloc(1, sizeof *c->state))) return -1;
    c->state->sel = sel;
    return 0;
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "ui_console",
    .version = "0.1",
    .requires = { "", 0 },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
    .save = m_save,
    .restore = m_restore,
    .quiesce = m_quiesce,
};

OA_MODULE_EXPORT(g_module)
