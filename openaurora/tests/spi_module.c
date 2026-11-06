/* mod_spi lifecycle over a temp spi.bin: opt-in gate, publish, bounded read,
 * stop/unpublish/close. No device is opened. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "openaurora/module.h"
#include "openaurora/services.h"

const oa_module *oa_module_entry(void);
static const oa_spi_iface *spi_iface;
static char g_file[64];
static int g_enable;

static void log_message(void *priv, int level, const char *fmt, ...)
{
    (void)priv; (void)level; (void)fmt;
}
static const char *config(void *priv, const char *key, const char *def)
{
    (void)priv;
    if (!strcmp(key, "spi.enable")) return g_enable ? "1" : "0";
    if (!strcmp(key, "spi.file")) return g_file;
    if (!strcmp(key, "spi.device")) return "/dev/oa-spi-absent";
    return def;
}
static int publish(void *priv, const char *name, uint32_t version, const void *iface)
{
    (void)priv;
    assert(!strcmp(name, OA_SVC_SPI) && version == 1);
    spi_iface = iface;
    return 0;
}
static int unpublish(void *priv, const char *name)
{
    (void)priv;
    assert(!strcmp(name, OA_SVC_SPI));
    spi_iface = NULL;
    return 0;
}
static void release(void *priv, const char *name) { (void)priv; (void)name; }
static const oa_host_api api = { .size = sizeof(oa_host_api), .abi = OA_ABI_VERSION,
    .log = log_message, .config_get = config, .publish = publish,
    .unpublish = unpublish, .release = release };

int main(void)
{
    snprintf(g_file, sizeof g_file, "/tmp/oa-spi-module-%ld", (long)getpid());
    int fd = open(g_file, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    assert(fd >= 0);
    const unsigned char content[8] = { 10, 20, 30, 40, 50, 60, 70, 80 };
    assert(write(fd, content, sizeof content) == (ssize_t)sizeof content);
    close(fd);

    /* Disabled is the default and must not publish or open anything. */
    g_enable = 0;
    void *ctx = NULL;
    assert(oa_module_entry()->init(&api, &ctx) == 0);
    assert(oa_module_entry()->start(ctx) == -1);
    assert(spi_iface == NULL);
    oa_module_entry()->stop(ctx);
    oa_module_entry()->destroy(ctx);

    g_enable = 1;
    ctx = NULL;
    assert(oa_module_entry()->init(&api, &ctx) == 0);
    assert(oa_module_entry()->start(ctx) == 0);
    assert(spi_iface && spi_iface->size() == (int)sizeof content);
    unsigned char got[8] = {0};
    assert(spi_iface->read(2, 4, got) == 1);
    assert(got[0] == 30 && got[3] == 60);
    assert(spi_iface->read(7, 2, got) == 0); /* out of range */
    assert(spi_iface->read(0, 0, got) == 0); /* zero length */
    oa_module_entry()->stop(ctx);
    assert(spi_iface == NULL);
    oa_module_entry()->destroy(ctx);
    assert(unlink(g_file) == 0);
    puts("ok spi module: opt-in gate, file backend, bounded read, stop unpublishes/closes");
    return 0;
}
