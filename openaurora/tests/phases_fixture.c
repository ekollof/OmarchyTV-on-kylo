/* Three real DSOs: A requires B, B requires C. No device calls. */
#include <stdlib.h>
#include "openaurora/module.h"
#include "phases.h"

#if PHASE_ID == 0
#define MODULE_NAME "phase_a"
#define SERVICE_NAME "phase_a_service"
#define REQUIRED_SERVICE "phase_b_service"
#elif PHASE_ID == 1
#define MODULE_NAME "phase_b"
#define SERVICE_NAME "phase_b_service"
#define REQUIRED_SERVICE "phase_c_service"
#else
#ifdef PHASE_REPLACEMENT
#define MODULE_NAME "phase_c_new"
#else
#define MODULE_NAME "phase_c"
#endif
#define SERVICE_NAME "phase_c_service"
#define REQUIRED_SERVICE ""
#endif

struct ctx { const oa_host_api *api; struct phase_observer *o; };
static void record(struct ctx *c, char event)
{
    if (c->o->length + 1 < sizeof c->o->sequence) {
        c->o->sequence[c->o->length++] = event;
        c->o->sequence[c->o->length] = 0;
    }
}
static int provider_ready(struct ctx *c)
{
#if PHASE_ID < 2
    return c->o->running[PHASE_ID + 1];
#else
    (void)c;
    return 1;
#endif
}
static int init(const oa_host_api *api, void **out)
{
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    *out = c;
    c->api = api;
    c->o = (struct phase_observer *)api->lookup(api->priv, PHASE_OBSERVER, 1);
    return c->o ? 0 : -1;
}
static int start(void *ud)
{
    struct ctx *c = ud;
    record(c, 'A' + PHASE_ID);
    c->o->starts[PHASE_ID]++;
    if (!provider_ready(c)) return -2;
    c->o->running[PHASE_ID] = 1;
    return c->api->publish(c->api->priv, SERVICE_NAME, 1, c);
}
static void stop(void *ud)
{
    struct ctx *c = ud;
    c->o->running[PHASE_ID] = 0;
    c->api->unpublish(c->api->priv, SERVICE_NAME);
}
static void destroy(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    c->api->release(c->api->priv, PHASE_OBSERVER);
    free(c);
}
#ifndef NO_PHASE_HOOKS
static int suspend(void *ud)
{
    struct ctx *c = ud;
    record(c, 'a' + PHASE_ID);
    c->o->suspends[PHASE_ID]++;
    if (c->o->fail_suspend == PHASE_ID + 1) return -7;
    /* No provider may shut down while its direct consumer remains active. */
#if PHASE_ID > 0
    if (c->o->running[PHASE_ID - 1]) return -2;
#endif
    c->o->running[PHASE_ID] = 0;
    if (c->o->after_suspend) c->o->after_suspend(c->o->after_suspend_ud);
    return c->api->unpublish(c->api->priv, SERVICE_NAME);
}
static int resume(void *ud)
{
    struct ctx *c = ud;
    record(c, 'A' + PHASE_ID);
    c->o->resumes[PHASE_ID]++;
    if (c->o->fail_resume == PHASE_ID + 1) return -7;
    if (!provider_ready(c)) return -2;
    c->o->running[PHASE_ID] = 1;
    return c->api->publish(c->api->priv, SERVICE_NAME, 1, c);
}
#endif
static const oa_module module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = MODULE_NAME, .version = "test", .requires = { REQUIRED_SERVICE, "" },
    .init = init, .start = start, .stop = stop, .destroy = destroy,
#ifndef NO_PHASE_HOOKS
    .suspend = suspend, .resume = resume,
#endif
};
OA_MODULE_EXPORT(module)
