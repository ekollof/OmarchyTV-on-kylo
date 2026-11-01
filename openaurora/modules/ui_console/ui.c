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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/events.h"
#include "openaurora/module.h"

#define TILES 4
static const char *g_tiles[TILES] = { "TV", "HDMI / sources", "apps & settings", "about" };

struct ctx {
    const oa_host_api *api;
    uint32_t sub_key;
    uint32_t sub_tick;
    uint8_t sel;
    unsigned redraws;
};

static void draw(struct ctx *c)
{
    c->redraws++;
    printf("\n  +-- openaurora --------------------------------+\n");
    for (int i = 0; i < TILES; i++)
        printf("  | %s %-42s|\n", i == c->sel ? ">" : " ", g_tiles[i]);
    printf("  +---------------------------------------------+\n");
    fflush(stdout);
}

/* key ids are the NEC command bytes published by mod_ir */
static void on_key(const void *evp, void *ud)
{
    struct ctx *c = ud;
    const oa_key_event *ev = evp;
    if (ev->kind != OA_KEY_PRESS)
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
    case 0x20: c->sel = 0; break;
    case 0xa0: c->sel = 1; break;
    case 0xfa: c->sel = 2; break;
    case 0xe0: c->sel = 3; break;
    case 0x8a: { /* OK */
        oa_action_event act;
        memset(&act, 0, sizeof act);
        act.kind = OA_ACTION_SELECT;
        act.index = c->sel;
        act.name = g_tiles[c->sel];
        c->api->emit(c->api->priv, OA_TOPIC_ACTION, &act);
        printf("  [ui] select %u (%s)\n", c->sel, g_tiles[c->sel]);
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
    if (c->redraws == 0)
        draw(c);
}

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    *out = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    c->sub_key = c->api->subscribe(c->api->priv, OA_TOPIC_KEY, on_key, c);
    c->sub_tick = c->api->subscribe(c->api->priv, OA_TOPIC_TICK, on_tick, c);
    if (!c->sub_key || !c->sub_tick)
        return -1;
    draw(c);
    c->api->log(c->api->priv, OA_LOG_INFO, "ui_console up (selection %u)", c->sel);
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->sub_key) {
        c->api->unsubscribe(c->api->priv, c->sub_key);
        c->sub_key = 0;
    }
    if (c->sub_tick) {
        c->api->unsubscribe(c->api->priv, c->sub_tick);
        c->sub_tick = 0;
    }
}

static void m_destroy(void *v)
{
    free(v);
}

static size_t m_save(void *v, void *buf, size_t cap)
{
    struct ctx *c = v;
    if (!buf || cap < 1)
        return 1;
    ((unsigned char *)buf)[0] = c->sel;
    return 1;
}

static int m_restore(void *v, const void *buf, size_t len)
{
    struct ctx *c = v;
    if (len < 1)
        return -1;
    c->sel = ((const unsigned char *)buf)[0];
    if (c->sel >= TILES)
        c->sel = 0;
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
};

OA_MODULE_EXPORT(g_module)
