/* Stub plat service for fb1-only UI tests on the running set.
 *
 * mod_display declares OA_SVC_PLAT in requires[] but does not call it when
 * display.drm_init=0 / display.set_region=0. The real mod_plat runs the vendor
 * HAL bring-up, which must not run a second time under the stock aurora. This
 * stub publishes a ready, error-free plat interface so the display module can
 * start without touching audio/DRM/GPIO/I2C. Test-only, never deployed.
 */
#include "openaurora/module.h"
#include "openaurora/services.h"

static const oa_host_api *g_api;

static int ready(void) { return 1; }
static int errors(void) { return 0; }

static const oa_plat_iface g_plat = { ready, errors };

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof(oa_host_api))
        return -1;
    g_api = api;
    *out = NULL;
    return 0;
}

static int m_start(void *v)
{
    (void)v;
    return g_api->publish(g_api->priv, OA_SVC_PLAT, 1, &g_plat);
}

static void m_stop(void *v)
{
    (void)v;
    g_api->unpublish(g_api->priv, OA_SVC_PLAT);
}

static void m_destroy(void *v) { (void)v; }

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "plat_stub", .version = "1",
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
};

OA_MODULE_EXPORT(g_module)
