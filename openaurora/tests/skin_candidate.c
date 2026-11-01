/* Failure fixtures: exercise the actual selector/engine/module lifetime. */
#include <stdlib.h>
#include "openaurora/module.h"
#include "openaurora/ui_engine.h"
#ifndef SKIN_NAME
#define SKIN_NAME "custom"
#endif
#ifndef FAIL_START
#define FAIL_START 0
#endif
#ifndef FAIL_ENTER
#define FAIL_ENTER 0
#endif
#ifndef SILENT
#define SILENT 0
#endif
#ifndef BUSY
#define BUSY 0
#endif
#ifndef FAIL_INIT
#define FAIL_INIT 0
#endif
struct ctx { const oa_host_api *api; const oa_ui_engine_iface *engine; };
static int blocked;
static void release_gate(void) { blocked = 0; }
static const struct { void (*release)(void); } gate = {release_gate};
static int enter(void *ud)
{
    struct ctx *c = ud;
    if (FAIL_ENTER) return -1;
    if (!SILENT && !FAIL_START) {
        if (c->engine->clear_rgba(20, 40, 60, 255)) return -1;
        return c->engine->present();
    }
    return 0;
}
static const oa_ui_skin skin = {.name = SKIN_NAME, .enter = enter};
static void dispatch(void *ud) { ((struct ctx *)ud)->engine->dispatch_skin(); }
static int init(const oa_host_api *api, void **out)
{
    struct ctx *c = calloc(1, sizeof *c); if (!c) return -1;
    c->api = api; *out = c;
    if (FAIL_INIT) { blocked = 1; (void)api->publish(api->priv, "test.skin_gate", 1, &gate); return -1; }
    return 0;
}
static int start(void *ud)
{
    struct ctx *c = ud;
    c->engine = c->api->lookup(c->api->priv, OA_SVC_UI_ENGINE, 2);
    if (!c->engine || c->engine->register_skin_dispatch(&skin, c, dispatch)) return -1;
    if (BUSY) {
        blocked = 1;
        if (c->api->publish(c->api->priv, "test.skin_gate", 1, &gate)) return -1;
    }
    return FAIL_START ? -1 : 0;
}
static int quiesce(void *ud) { (void)ud; return blocked ? -1 : 0; }
static void stop(void *ud)
{
    struct ctx *c = ud;
    if (c->engine) {
        c->engine->unregister_skin(&skin);
        c->api->release(c->api->priv, OA_SVC_UI_ENGINE);
        c->engine = NULL;
    }
    if (BUSY) c->api->unpublish(c->api->priv, "test.skin_gate");
}
static void destroy(void *ud) { free(ud); }
static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "ui_" SKIN_NAME, .version = "test", .requires = {OA_SVC_UI_ENGINE, ""},
    .init = init, .start = start, .stop = stop, .destroy = destroy, .quiesce = quiesce
};
OA_MODULE_EXPORT(descriptor)
