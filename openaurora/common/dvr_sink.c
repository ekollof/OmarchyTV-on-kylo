#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "openaurora/dvr.h"

typedef int (*dvr_open_fn)(int, int, int);
typedef long (*dvr_write_fn)(int, const void *, size_t);
typedef int (*dvr_flush_fn)(int);
typedef int (*dvr_close_fn)(int);

static struct {
    void *handle;
    dvr_open_fn open_dvr;
    dvr_write_fn write_dvr;
    dvr_flush_fn flush_dvr;
    dvr_close_fn close_dvr;
    int loaded;
    int installed;
} g_hal;

void oa_dvr_set_hal(const struct oa_dvr_hal *hal)
{
    if (g_hal.handle) {
        dlclose(g_hal.handle);
        g_hal.handle = NULL;
    }
    if (!hal) {
        memset(&g_hal, 0, sizeof g_hal);
        return;
    }
    g_hal.open_dvr = hal->open_dvr;
    g_hal.write_dvr = hal->write_dvr;
    g_hal.flush_dvr = hal->flush_dvr;
    g_hal.close_dvr = hal->close_dvr;
    g_hal.loaded = 1;
    g_hal.installed = 1;
}

static int hal_load(void)
{
    if (g_hal.loaded) {
        if (g_hal.installed) return 0;
        return g_hal.handle ? 0 : -ENOSYS;
    }
    g_hal.loaded = 1;
    g_hal.handle = dlopen("libNDAL3.so", RTLD_NOW | RTLD_LOCAL);
    if (!g_hal.handle) return -ENOSYS;
#define DVR_SYMBOL(field, symbol) do { \
        void *address = dlsym(g_hal.handle, symbol); \
        memcpy(&g_hal.field, &address, sizeof g_hal.field); \
    } while (0)
    DVR_SYMBOL(open_dvr, "Hal_Demux_Dvb_Dvr_Open");
    DVR_SYMBOL(write_dvr, "Hal_Demux_Dvb_Dvr_Write");
    DVR_SYMBOL(flush_dvr, "Hal_Demux_Dvb_Dvr_Flush");
    DVR_SYMBOL(close_dvr, "Hal_Demux_Dvb_Dvr_Close");
#undef DVR_SYMBOL
    return 0;
}

static int linux_open(void *ctx, int adapter, int index)
{
    (void)ctx;
    if (adapter < 0 || index < 0) return -EINVAL;
    if (hal_load()) return -ENOSYS;
    if (!g_hal.open_dvr) return -ENOSYS;
    errno = 0;
    int fd = g_hal.open_dvr(adapter, index, 0);
    if (fd < 0) return errno ? -errno : -EIO;
    return fd;
}

static long linux_write(void *ctx, int fd, const void *data, size_t len)
{
    (void)ctx;
    if (fd < 0 || !data || !len) return -EINVAL;
    if (hal_load()) return -ENOSYS;
    if (!g_hal.write_dvr) return -ENOSYS;
    errno = 0;
    long written = g_hal.write_dvr(fd, data, len);
    if (written < 0) return errno ? -errno : -EIO;
    return written;
}

static int linux_flush(void *ctx, int fd)
{
    (void)ctx;
    if (fd < 0) return -EBADF;
    if (hal_load()) return -ENOSYS;
    if (!g_hal.flush_dvr) return -ENOSYS;
    return g_hal.flush_dvr(fd) < 0 ? -EIO : 0;
}

static int linux_close(void *ctx, int fd)
{
    (void)ctx;
    if (fd < 0) return -EBADF;
    if (hal_load() || !g_hal.close_dvr) {
        return close(fd) == 0 ? 0 : -errno;
    }
    return g_hal.close_dvr(fd) < 0 ? -EIO : 0;
}

static const struct oa_dvr_ops linux_ops = {
    linux_open, linux_write, linux_flush, linux_close
};

const struct oa_dvr_ops *oa_dvr_linux_ops(void)
{
    return &linux_ops;
}

static struct {
    const struct oa_dvr_ops *ops;
    void *ctx;
    int fd;
    int open;
} g_sink;

int oa_dvr_sink_open(int adapter, int index, const struct oa_dvr_ops *ops,
    void *ctx)
{
    oa_dvr_sink_close();
    if (!ops || !ops->open_dvr || !ops->write_dvr || !ops->flush_dvr ||
        !ops->close_dvr)
        return -EINVAL;
    int fd = ops->open_dvr(ctx, adapter, index);
    if (fd < 0) return fd;
    g_sink.ops = ops;
    g_sink.ctx = ctx;
    g_sink.fd = fd;
    g_sink.open = 1;
    return 0;
}

long oa_dvr_sink_write(const void *data, size_t len)
{
    if (!g_sink.open) return -ENODEV;
    if (!data || !len) return -EINVAL;
    size_t offset = 0;
    while (offset < len) {
        long written = g_sink.ops->write_dvr(g_sink.ctx, g_sink.fd,
            (const uint8_t *)data + offset, len - offset);
        if (written < 0) return written;
        if (written == 0) return -EIO;
        offset += (size_t)written;
    }
    return (long)offset;
}

int oa_dvr_sink_flush(void)
{
    if (!g_sink.open) return -ENODEV;
    return g_sink.ops->flush_dvr(g_sink.ctx, g_sink.fd);
}

void oa_dvr_sink_close(void)
{
    if (g_sink.open) g_sink.ops->close_dvr(g_sink.ctx, g_sink.fd);
    memset(&g_sink, 0, sizeof g_sink);
    g_sink.fd = -1;
}

int oa_dvr_sink_is_open(void)
{
    return g_sink.open;
}
