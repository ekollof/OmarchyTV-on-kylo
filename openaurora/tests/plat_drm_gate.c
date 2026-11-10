/* Fake-HAL regression for plat.drm_init: 0 skips only Hal_Video_DrmInit(1)
 * and keeps the vendor OSD, while the rest of the platform bring-up runs. */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/module.h"
#include "openaurora/services.h"

#include "../modules/plat/plat.c"

static int drm_enabled;
static char logs[8192];
static size_t logs_at;

static void fake_log(void *priv, int level, const char *fmt, ...)
{
    (void)priv;
    (void)level;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(logs + logs_at, sizeof logs - logs_at, fmt, ap);
    va_end(ap);
    if (n > 0) {
        logs_at += (size_t)n;
        if (logs_at >= sizeof logs) logs_at = sizeof logs - 1;
    }
}

static const char *fake_config(void *priv, const char *key, const char *def)
{
    (void)priv;
    if (!strcmp(key, "plat.driver_wait")) return "0";
    if (!strcmp(key, "plat.enable")) return "1";
    if (!strcmp(key, "plat.drm_init")) return drm_enabled ? "1" : "0";
    return def;
}

static int fake_publish(void *priv, const char *name, uint32_t version,
                        const void *iface)
{
    (void)priv;
    (void)name;
    (void)version;
    (void)iface;
    return 0;
}

static int fake_unpublish(void *priv, const char *name)
{
    (void)priv;
    (void)name;
    return 0;
}

static const oa_host_api api = {
    .size = sizeof api,
    .abi = OA_ABI_VERSION,
    .log = fake_log,
    .publish = fake_publish,
    .unpublish = fake_unpublish,
    .config_get = fake_config,
};

static void run(int drm, const char *expect, const char *absent)
{
    oa_plat_test_reset_common();
    drm_enabled = drm;
    logs_at = 0;
    logs[0] = 0;
    void *ctx = NULL;
    assert(m_init(&api, &ctx) == 0 && ctx);
    assert(m_start(ctx) == 0);
    assert(strstr(logs, expect));
    if (absent)
        assert(!strstr(logs, absent));
    m_stop(ctx);
    m_destroy(ctx);
}

int main(void)
{
    run(0, "Hal_Video_DrmInit skipped", "drm 1");
    assert(strstr(logs, "drm 0"));
    puts("ok plat drm gate: drm_init=0 skips Hal_Video_DrmInit and preserves the OSD");

    run(1, "drm 1", "Hal_Video_DrmInit skipped");
    puts("ok plat drm gate: drm_init=1 keeps the vendor DRM bring-up");
    puts("all plat drm gate tests passed");
    return 0;
}
