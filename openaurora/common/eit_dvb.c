#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/dvb/dmx.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "openaurora/eit_dvb.h"
#include "openaurora/eit_ts.h"

typedef int (*hal_open_fn)(int, int);
typedef int (*hal_filter_fn)(int, uint16_t, size_t, void *, void *, void *,
    char);
typedef int (*hal_close_fn)(int);

static struct {
    void *handle;
    hal_open_fn open_demux;
    hal_filter_fn set_filter;
    hal_close_fn close_demux;
    int loaded;
    int installed;
} g_hal;

void oa_eit_dvb_set_hal(const struct oa_eit_dvb_hal *hal)
{
    if (g_hal.handle) {
        dlclose(g_hal.handle);
        g_hal.handle = NULL;
    }
    if (!hal) {
        memset(&g_hal, 0, sizeof g_hal);
        return;
    }
    g_hal.open_demux = hal->open_demux;
    g_hal.set_filter = hal->set_section_filter;
    g_hal.close_demux = hal->close_demux;
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
#define HAL_SYMBOL(field, symbol) do { \
        void *address = dlsym(g_hal.handle, symbol); \
        memcpy(&g_hal.field, &address, sizeof g_hal.field); \
    } while (0)
    HAL_SYMBOL(open_demux, "Hal_Demux_Dvb_Open");
    HAL_SYMBOL(set_filter, "Hal_Demux_Dvb_Set_Sectionfilter");
    HAL_SYMBOL(close_demux, "Hal_Demux_Dvb_Close");
#undef HAL_SYMBOL
    return 0;
}

static int linux_open(void *ctx, int adapter, int index)
{
    (void)ctx;
    if (adapter < 0 || index < 0) return -EINVAL;
    if (hal_load() == 0 && g_hal.open_demux) {
        errno = 0;
        int fd = g_hal.open_demux(adapter, index);
        if (fd < 0) return errno ? -errno : -EIO;
        return fd;
    }
    char path[64];
    snprintf(path, sizeof path, "/dev/dvb/adapter%d/demux%d", adapter, index);
    int fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    return fd < 0 ? -errno : fd;
}

static int linux_filter(void *ctx, int fd, uint16_t pid)
{
    (void)ctx;
    if (fd < 0) return -EBADF;
    if (hal_load() == 0 && g_hal.set_filter) {
        if (g_hal.set_filter(fd, pid, 0, NULL, NULL, NULL, 1) < 0)
            return -EIO;
        return 0;
    }
    struct dmx_sct_filter_params params;
    memset(&params, 0, sizeof params);
    params.pid = pid;
    params.flags = DMX_CHECK_CRC;
    if (ioctl(fd, DMX_SET_FILTER, &params) < 0) return -errno;
    (void)ioctl(fd, DMX_START, 0);
    return 0;
}

static long linux_read(void *ctx, int fd, uint8_t *buf, size_t cap)
{
    (void)ctx;
    if (fd < 0 || !buf || !cap) return -EINVAL;
    long got = (long)read(fd, buf, cap);
    if (got < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -errno;
    }
    return got;
}

static int linux_close(void *ctx, int fd)
{
    (void)ctx;
    if (fd < 0) return -EBADF;
    if (hal_load() == 0 && g_hal.close_demux)
        return g_hal.close_demux(fd) < 0 ? -EIO : 0;
    (void)ioctl(fd, DMX_STOP, 0);
    return close(fd) == 0 ? 0 : -errno;
}

static const struct oa_eit_dvb_ops linux_ops = {
    linux_open, linux_filter, linux_read, linux_close
};

const struct oa_eit_dvb_ops *oa_eit_dvb_linux_ops(void)
{
    return &linux_ops;
}

static struct {
    const struct oa_eit_dvb_ops *ops;
    void *ctx;
    int fd;
    int open;
} g_eit;

int oa_eit_dvb_open(int adapter, int index, uint16_t pid,
    const struct oa_eit_dvb_ops *ops, void *ctx)
{
    oa_eit_dvb_close();
    if (!ops || !ops->open_demux || !ops->set_section_filter ||
        !ops->read_section || !ops->close_demux)
        return -EINVAL;
    int fd = ops->open_demux(ctx, adapter, index);
    if (fd < 0) return fd;
    int rc = ops->set_section_filter(ctx, fd, pid);
    if (rc < 0) {
        ops->close_demux(ctx, fd);
        return rc;
    }
    g_eit.ops = ops;
    g_eit.ctx = ctx;
    g_eit.fd = fd;
    g_eit.open = 1;
    return 0;
}

void oa_eit_dvb_close(void)
{
    if (g_eit.open) g_eit.ops->close_demux(g_eit.ctx, g_eit.fd);
    memset(&g_eit, 0, sizeof g_eit);
    g_eit.fd = -1;
}

int oa_eit_dvb_next(uint8_t *buf, size_t cap, size_t *len)
{
    if (!len) return -EINVAL;
    *len = 0;
    if (!g_eit.open) return 0;
    if (!buf || !cap) return -EINVAL;
    long got = g_eit.ops->read_section(g_eit.ctx, g_eit.fd, buf, cap);
    if (got < 0) return (int)got;
    if (got == 0) return 0;
    if ((size_t)got > cap) return -E2BIG;
    if ((size_t)got < 3) return -EINVAL;
    size_t section_length = ((size_t)(buf[1] & 0x0fu) << 8) | buf[2];
    if (section_length < 4 || section_length > OA_EIT_TS_SECTION_MAX - 3)
        return -EINVAL;
    size_t total = 3 + section_length;
    if ((size_t)got != total) return -EINVAL;
    *len = total;
    return 1;
}
