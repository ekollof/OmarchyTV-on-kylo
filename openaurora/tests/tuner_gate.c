#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/module.h"

static const char *gate;

static const char *config_get(void *priv, const char *key, const char *def)
{
    (void)priv;
    if (!strcmp(key, "tuner.hw_init") && gate)
        return gate;
    return def;
}

int main(int argc, char **argv)
{
    assert(argc > 1);
    void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return 1;
    }
    void *sym = dlsym(h, "oa_module_entry");
    assert(sym != NULL);
    const oa_module *(*entry)(void) = NULL;
    _Static_assert(sizeof entry == sizeof sym, "pointer size mismatch");
    memcpy(&entry, &sym, sizeof entry);
    const oa_module *m = entry();
    assert(m && m->magic == OA_MODULE_MAGIC && m->abi == OA_ABI_VERSION);

    oa_host_api api;
    memset(&api, 0, sizeof api);
    api.size = sizeof api;
    api.abi = OA_ABI_VERSION;
    api.config_get = config_get;

    void *ctx = NULL;
    gate = "1";
    int rc = m->init(&api, &ctx);
    printf("tuner.hw_init=1 -> %d\n", rc);
    assert(rc == -ENOTSUP && ctx == NULL);

    gate = NULL;
    rc = m->init(&api, &ctx);
    printf("default -> %d\n", rc);
    assert(rc == 0 && ctx != NULL);
    m->destroy(ctx);
    assert(dlclose(h) == 0);
    puts("ok tuner gate: host build refuses tuner.hw_init=1 and default init "
         "still works");
    return 0;
}
