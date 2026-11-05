/* Host-only plat service fixture. It does not initialize HAL, GPIO, I2C,
 * audio, video or any device; it only satisfies consumers' service dependency
 * so their host-build lifecycle can be exercised over the socket. */
#include <stdlib.h>
#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    int published;
};

static int ready(void) { return 1; }
static int no_errors(void) { return 0; }
static const oa_plat_iface plat = { ready, no_errors };

static int init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof *api || !out)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api;
    *out = c;
    return 0;
}

static int start(void *ud)
{
    struct ctx *c = ud;
    if (c->api->publish(c->api->priv, OA_SVC_PLAT, 1, &plat)) return -1;
    c->published = 1;
    return 0;
}

static void stop(void *ud)
{
    struct ctx *c = ud;
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_PLAT);
    c->published = 0;
}

static void destroy(void *ud) { free(ud); }

static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "plat_fixture", .version = "test", .init = init,
    .start = start, .stop = stop, .destroy = destroy
};
OA_MODULE_EXPORT(descriptor)
