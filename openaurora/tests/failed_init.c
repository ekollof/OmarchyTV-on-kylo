/* A module that registers a service and timer, then fails init(). */
#include "openaurora/module.h"
static int iface;
static void callback(void *unused) { (void)unused; }
static int init(const oa_host_api *api, void **out)
{
    *out = NULL;
    api->publish(api->priv, "failed-init", 1, &iface);
    api->timer_add(api->priv, 100, 100, callback, NULL);
    return -1;
}
static int start(void *ctx) { (void)ctx; return 0; }
static void destroy(void *ctx) { (void)ctx; }
static const oa_module module = { .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "failed_init", .init = init, .start = start, .stop = destroy, .destroy = destroy };
const oa_module *oa_module_entry(void) { return &module; }
