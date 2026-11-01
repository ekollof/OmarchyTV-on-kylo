/* Guard pages prove rejection happens before reading unavailable ABI fields. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "openaurora/module.h"

#ifdef TEST_ABI_EXTENDED
static int init(const oa_host_api *api, void **ctx) { (void)api; *ctx = NULL; return 0; }
static int start(void *ctx) { (void)ctx; return 0; }
static void stop(void *ctx) { (void)ctx; }
static const struct {
    oa_module base;
    uint32_t future[8];
} extended = { .base = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof extended,
    .name = "abi_extended", .init = init, .start = start, .stop = stop, .destroy = stop,
} };
const oa_module *oa_module_entry(void) { return &extended.base; }
#else
static void *pages;
static size_t page_size;
const oa_module *oa_module_entry(void)
{
    page_size = (size_t)sysconf(_SC_PAGESIZE);
    pages = mmap(NULL, page_size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pages == MAP_FAILED) { pages = NULL; return NULL; }
    if (mprotect((char *)pages + page_size, page_size, PROT_NONE)) return NULL;
#ifdef TEST_ABI_V1
    uint32_t header[] = { OA_MODULE_MAGIC, 1 };
    size_t len = sizeof header;
#elif defined(TEST_ABI_V2)
    uint32_t header[] = { OA_MODULE_MAGIC, 2 };
    size_t len = sizeof header;
#else
    size_t len = offsetof(oa_module, name);
    uint32_t header[] = { OA_MODULE_MAGIC, OA_ABI_VERSION, (uint32_t)len };
#endif
    char *p = (char *)pages + page_size - len;
    memcpy(p, header, sizeof header);
    return (const oa_module *)p;
}
__attribute__((destructor)) static void cleanup(void)
{
    if (pages) munmap(pages, page_size * 2);
}
#endif
