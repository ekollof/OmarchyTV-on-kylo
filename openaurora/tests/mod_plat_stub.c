/* Stub plat service for fb1-only UI, VIN and panel tests.
 *
 * mod_display/mod_panel declare OA_SVC_PLAT in requires[] but do not need the
 * vendor HAL for these tests. The real mod_plat runs the vendor bring-up,
 * which must not run a second time under the stock aurora (and fails under
 * qemu without the audio mixer). This stub publishes a ready, error-free plat
 * interface. Test-only, never deployed. The module name is "plat" because
 * qemu-vin-real-check.sh loads the copied artifact as modules/mod_plat.so.
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
    .name = "plat", .version = "1",
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
};

OA_MODULE_EXPORT(g_module)
