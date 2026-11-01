#include <assert.h>
#include <stdlib.h>
#include "openaurora/module.h"
#include "quiesce_fixture.h"
struct ctx { const oa_host_api *api; struct quiesce_observer *o; };
static void completion(void *ud)
{
    struct ctx *c = ud;
    c->o->completion++;
    c->o->busy = 0;
}
static int init(const oa_host_api *api, void **out)
{
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    *out = c;
    c->api = api;
    c->o = (struct quiesce_observer *)api->lookup(api->priv, QUIESCE_OBSERVER, 1);
    if (!c->o) return -1;
    c->o->init++;
    c->o->owner = api->owner_id(api->priv);
    assert(api->publish(api->priv, QUIESCE_SERVICE, 1, c) == 0);
    assert(api->post(api->priv, c->o->owner, completion, c) == 0);
    return c->o->fail_init ? -1 : 0;
}
static int start(void *ud)
{
    struct ctx *c = ud;
    c->o->start++;
    if (c->o->post_start)
        assert(c->api->post(c->api->priv, c->o->owner, completion, c) == 0);
    return c->o->fail_start ? -1 : 0;
}
static int quiesce(void *ud)
{
    struct ctx *c = ud;
    if (!c || !c->o) return 0;
    assert(c->api->owner_id(c->api->priv) == c->o->owner);
    c->o->gate++;
    /* Queue completion without waiting. The host must dispatch it and retry. */
    if (c->o->defer && c->o->busy) {
        c->o->defer = 0;
        assert(c->api->post(c->api->priv, c->o->owner, completion, c) == 0);
    }
    return c->o->busy;
}
static void stop(void *ud) { ((struct ctx *)ud)->o->stop++; }
static void destroy(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    if (c->o) c->o->destroy++;
    free(c);
}
static int phase(void *ud) { (void)ud; return 0; }
static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "quiesce_fixture", .version = "1", .init = init, .start = start,
    .stop = stop, .destroy = destroy, .suspend = phase, .resume = phase,
    .quiesce = quiesce
};
OA_MODULE_EXPORT(descriptor)
