/* Independent no-device DSOs used to prove the control batch queue runs one
 * module lifecycle operation per host-loop turn. */
#include <stdlib.h>
#include "openaurora/module.h"

#ifndef BATCH_MODULE_NAME
#define BATCH_MODULE_NAME "batch_a"
#endif

static int init(const oa_host_api *api, void **out)
{
    (void)api;
    *out = NULL;
    return 0;
}

static int start(void *ctx)
{
    (void)ctx;
    return 0;
}

static void stop(void *ctx)
{
    (void)ctx;
}

static void destroy(void *ctx)
{
    (void)ctx;
}

static const oa_module desc = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof desc,
    .name = BATCH_MODULE_NAME,
    .version = "1",
    .requires = { "" },
    .init = init,
    .start = start,
    .stop = stop,
    .destroy = destroy,
};

const oa_module *oa_module_entry(void)
{
    return &desc;
}
