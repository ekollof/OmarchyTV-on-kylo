/*
 * mod_spi — recovered DEV_SPI transport, as an opt-in module.
 *
 * Vendor DEV_SPI_Open prefers /conf/spi.bin and otherwise opens /dev/ntkspi;
 * DEV_SPI_Read uses fseek/fread or the 0x8018531e read ioctl. This module
 * publishes the read-only `spi` v1 boundary so a future NIM/CI consumer has a
 * single owner. It is disabled by default and is not autoloaded by the current
 * cold config. No write, reset or hardware-ownership claim is made.
 *
 *   spi.enable   1 = open the backend (default 0)
 *   spi.file     file backend path (default /conf/spi.bin)
 *   spi.device   device path (default /dev/ntkspi)
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/spi_dev.h"

struct ctx {
    const oa_host_api *api;
    oa_spi spi;
    int published;
};

static struct ctx *g_ctx;

static int iface_size(void)
{
    return g_ctx ? (int)g_ctx->spi.size : 0;
}

static int iface_read(uint32_t offset, uint32_t len, void *buf)
{
    return g_ctx ? oa_spi_read(&g_ctx->spi, offset, len, buf) : 0;
}

static const oa_spi_iface g_iface = { iface_size, iface_read };

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    c->spi.fd = -1;
    *out = c;
    g_ctx = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->api->config_get(c->api->priv, "spi.enable", "0")[0] != '1') {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "spi: disabled by config; no transport opened");
        return -1;
    }
    const char *file = c->api->config_get(c->api->priv, "spi.file",
                                         OA_SPI_DEFAULT_FILE);
    const char *dev = c->api->config_get(c->api->priv, "spi.device",
                                        OA_SPI_DEFAULT_DEVICE);
    int rc = oa_spi_open(&c->spi, file, dev);
    if (rc) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "spi: open failed: %s",
                    strerror(-rc));
        return -1;
    }
    if (c->api->publish(c->api->priv, OA_SVC_SPI, 1, &g_iface)) {
        oa_spi_close(&c->spi);
        return -1;
    }
    c->published = 1;
    c->api->log(c->api->priv, OA_LOG_INFO, "spi: %s backend, %u bytes",
                c->spi.file ? "file" : "device", c->spi.size);
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->published)
        c->api->unpublish(c->api->priv, OA_SVC_SPI);
    c->published = 0;
    /* fclose/close are synchronous; no worker or deferable state is owned. */
    oa_spi_close(&c->spi);
}

static void m_destroy(void *v)
{
    if (g_ctx == v)
        g_ctx = NULL;
    free(v);
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "spi",
    .version = "0.1",
    .requires = { "" },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
};

OA_MODULE_EXPORT(g_module)
