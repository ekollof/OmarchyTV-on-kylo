/* The checker must never execute these constructors or module callbacks. */
#include <unistd.h>
#include "openaurora/module.h"

#ifdef TEST_DEPENDENCY
int oa_check_dependency(void) { return 0; }
#else
#ifdef TEST_OLD_ABI
#undef OA_ABI_VERSION
#define OA_ABI_VERSION 1u
#endif
__attribute__((constructor)) static void must_not_execute(void) { _exit(90); }
static int init(const oa_host_api *api, void **ctx) { (void)api; (void)ctx; _exit(91); }
static int start(void *ctx)
{
    (void)ctx;
#if defined(TEST_NEEDED)
    extern int oa_check_dependency(void);
    return oa_check_dependency();
#elif defined(TEST_UNDEFINED)
    extern int oa_check_missing_function(void);
    return oa_check_missing_function();
#else
    _exit(92);
#endif
}
static void stop(void *ctx) { (void)ctx; _exit(93); }
static const oa_module module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "check_fixture", .init = init, .start = start, .stop = stop, .destroy = stop
};
#ifdef TEST_NO_NOTE
const oa_module *oa_module_entry(void) { return &module; }
#else
#ifdef TEST_NO_ENTRY
#define oa_module_entry deliberately_wrong_entry
#endif
OA_MODULE_EXPORT(module)
#endif
#endif
