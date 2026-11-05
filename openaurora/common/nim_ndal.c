/*
 * Live-capable NDAL/DVB frontend backend.
 *
 * Default ops open /dev/dvb/adapterN/frontendN, tune DVB-T with the Linux DVB
 * API and use the shipped Nd_NIM_* getters/setters from libNDAL_EX.so for the
 * parameters they own. Tests replace the whole ops table; selecting this
 * backend is explicit (tuner.backend=ndal) and no device is opened otherwise.
 */
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/dvb/frontend.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "openaurora/nim.h"

typedef void (*ndal_set_one)(int, uint32_t);
typedef void (*ndal_set_int)(int, int);
typedef void (*ndal_get_u32)(int, uint32_t *);
typedef void (*ndal_get_u8)(int, uint8_t *);

static struct {
    void *handle;
    ndal_set_one set_scan_mode;
    ndal_get_u32 get_frequency;
    ndal_get_u8 get_delivery_system;
    ndal_get_u32 get_symbol_rate;
    ndal_set_one set_ts_output;
    ndal_set_int set_standby;
    int loaded;
} g_ndal;

static int ndal_load(void)
{
    if (g_ndal.loaded) return g_ndal.handle ? 0 : -ENOSYS;
    g_ndal.loaded = 1;
    g_ndal.handle = dlopen("libNDAL_EX.so", RTLD_NOW | RTLD_LOCAL);
    if (!g_ndal.handle) return -ENOSYS;
    /* ISO C does not permit an object-to-function pointer cast. Copy the
     * dlsym result through the typed storage; all target ABIs use equal
     * pointer sizes. */
#define NDAL_SYMBOL(field, symbol) do { \
        void *address = dlsym(g_ndal.handle, symbol); \
        memcpy(&g_ndal.field, &address, sizeof g_ndal.field); \
    } while (0)
    NDAL_SYMBOL(set_scan_mode, "Nd_NIM_SetScanMode");
    NDAL_SYMBOL(get_frequency, "Nd_NIM_GetFrequency");
    NDAL_SYMBOL(get_delivery_system, "Nd_NIM_GetDeliverySystem");
    NDAL_SYMBOL(get_symbol_rate, "Nd_NIM_GetSymbolRate");
    NDAL_SYMBOL(set_ts_output, "Nd_NIM_SetTsOutput");
    NDAL_SYMBOL(set_standby, "Nd_NIM_SetStandy");
#undef NDAL_SYMBOL
    return 0;
}

static int linux_open_frontend(void *ctx, const char *path)
{
    (void)ctx;
    if (!path || !*path) return -EINVAL;
    int fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    return fd < 0 ? -errno : fd;
}

static int linux_close_frontend(void *ctx, int fd)
{
    (void)ctx;
    if (fd < 0) return -EBADF;
    return close(fd) == 0 ? 0 : -errno;
}

static int linux_tune(void *ctx, int fd, uint8_t nim_path,
    uint32_t frequency_khz)
{
    (void)ctx;
    if (fd < 0 || !frequency_khz) return -EINVAL;
    if (nim_path != OA_NIM_PATH_TERRESTRIAL) return -ENOTSUP;
    struct dvb_frontend_parameters fe;
    memset(&fe, 0, sizeof fe);
    fe.frequency = frequency_khz * 1000u;
    fe.inversion = INVERSION_AUTO;
    fe.u.ofdm.bandwidth = BANDWIDTH_8_MHZ;
    fe.u.ofdm.code_rate_HP = FEC_AUTO;
    fe.u.ofdm.code_rate_LP = FEC_AUTO;
    fe.u.ofdm.constellation = QAM_AUTO;
    fe.u.ofdm.transmission_mode = TRANSMISSION_MODE_AUTO;
    fe.u.ofdm.guard_interval = GUARD_INTERVAL_AUTO;
    fe.u.ofdm.hierarchy_information = HIERARCHY_AUTO;
    return ioctl(fd, FE_SET_FRONTEND, &fe) == 0 ? 0 : -errno;
}

static int linux_scan_mode(void *ctx, int fd, uint32_t mode)
{
    (void)ctx;
    if (fd < 0) return -EBADF;
    if (ndal_load()) return -ENOSYS;
    if (!g_ndal.set_scan_mode) return -ENOSYS;
    g_ndal.set_scan_mode(fd, mode);
    return 0;
}

static int linux_frequency(void *ctx, int fd, uint32_t *khz)
{
    (void)ctx;
    if (fd < 0 || !khz) return -EINVAL;
    if (ndal_load()) return -ENOSYS;
    if (!g_ndal.get_frequency) return -ENOSYS;
    uint32_t value = 0;
    g_ndal.get_frequency(fd, &value);
    *khz = value;
    return 0;
}

static int linux_delivery_system(void *ctx, int fd, uint8_t *system)
{
    (void)ctx;
    if (fd < 0 || !system) return -EINVAL;
    if (ndal_load()) return -ENOSYS;
    if (!g_ndal.get_delivery_system) return -ENOSYS;
    uint8_t value = 0;
    g_ndal.get_delivery_system(fd, &value);
    *system = value;
    return 0;
}

static int linux_symbol_rate(void *ctx, int fd, uint32_t *ksym)
{
    (void)ctx;
    if (fd < 0 || !ksym) return -EINVAL;
    if (ndal_load()) return -ENOSYS;
    if (!g_ndal.get_symbol_rate) return -ENOSYS;
    uint32_t value = 0;
    g_ndal.get_symbol_rate(fd, &value);
    *ksym = value;
    return 0;
}

static int linux_status(void *ctx, int fd, uint32_t *status)
{
    (void)ctx;
    if (fd < 0 || !status) return -EINVAL;
    fe_status_t value = 0;
    if (ioctl(fd, FE_READ_STATUS, &value) != 0) return -errno;
    *status = value;
    return 0;
}

static uint32_t scale_percent(uint16_t value)
{
    return (uint32_t)((uint32_t)value * 100u / 65535u);
}

static int linux_metrics(void *ctx, int fd, struct oa_nim_signal *signal)
{
    (void)ctx;
    if (fd < 0 || !signal) return -EINVAL;
    struct oa_nim_signal value;
    memset(&value, 0, sizeof value);
    value.unc = OA_NIM_UNC_UNKNOWN;
    fe_status_t status = 0;
    uint16_t strength = 0, snr = 0;
    uint32_t ber = 0, unc = 0;
    if (ioctl(fd, FE_READ_STATUS, &status) == 0)
        value.locked = (status & FE_HAS_LOCK) != 0;
    if (ioctl(fd, FE_READ_SIGNAL_STRENGTH, &strength) == 0)
        value.strength = scale_percent(strength);
    if (ioctl(fd, FE_READ_SNR, &snr) == 0)
        value.snr = scale_percent(snr);
    if (ioctl(fd, FE_READ_BER, &ber) == 0)
        value.ber = (float)ber;
    if (ioctl(fd, FE_READ_UNCORRECTED_BLOCKS, &unc) == 0)
        value.unc = unc;
    *signal = value;
    return 0;
}

static int linux_set_ts_output(void *ctx, int fd, uint32_t value)
{
    (void)ctx;
    if (fd < 0) return -EBADF;
    if (ndal_load()) return -ENOSYS;
    if (!g_ndal.set_ts_output) return -ENOSYS;
    g_ndal.set_ts_output(fd, value);
    return 0;
}

static int linux_set_standby(void *ctx, int fd, int on)
{
    (void)ctx;
    if (fd < 0) return -EBADF;
    if (ndal_load()) return -ENOSYS;
    if (!g_ndal.set_standby) return -ENOSYS;
    g_ndal.set_standby(fd, on);
    return 0;
}

static const struct oa_ndal_nim_ops linux_ops = {
    .open_frontend = linux_open_frontend,
    .close_frontend = linux_close_frontend,
    .tune = linux_tune,
    .scan_mode = linux_scan_mode,
    .frequency = linux_frequency,
    .delivery_system = linux_delivery_system,
    .symbol_rate = linux_symbol_rate,
    .status = linux_status,
    .metrics = linux_metrics,
    .set_ts_output = linux_set_ts_output,
    .set_standby = linux_set_standby
};

static struct {
    const struct oa_ndal_nim_ops *ops;
    void *ops_ctx;
    char frontend[256];
    int fd;
    int open;
    uint8_t path;
    uint32_t frequency;
} g_ndal_state = { .ops = &linux_ops, .fd = -1 };

void oa_nim_ndal_set_ops(const struct oa_ndal_nim_ops *ops, void *ctx)
{
    if (g_ndal_state.open) return;
    g_ndal_state.ops = ops ? ops : &linux_ops;
    g_ndal_state.ops_ctx = ops ? ctx : NULL;
}

static int ndal_start(uint8_t path, const uint32_t *params, size_t count,
    struct oa_nim_tune_result *result)
{
    if (!result || !params || count < 2 || !params[1]) return -EINVAL;
    if (!g_ndal_state.open) {
        int fd = g_ndal_state.ops->open_frontend(g_ndal_state.ops_ctx,
            g_ndal_state.frontend);
        if (fd < 0) return fd;
        g_ndal_state.fd = fd;
        g_ndal_state.open = 1;
    }
    uint32_t frequency = params[1];
    int rc = g_ndal_state.ops->tune(g_ndal_state.ops_ctx, g_ndal_state.fd,
        path, frequency);
    if (rc < 0) {
        g_ndal_state.ops->close_frontend(g_ndal_state.ops_ctx,
            g_ndal_state.fd);
        g_ndal_state.fd = -1;
        g_ndal_state.open = 0;
        return rc;
    }
    /* The Nd_NIM scan-mode setter is advisory for this backend; a missing
     * vendor library does not prevent the standard DVB tune/status path. */
    if (g_ndal_state.ops->scan_mode)
        (void)g_ndal_state.ops->scan_mode(g_ndal_state.ops_ctx,
            g_ndal_state.fd, path);
    uint32_t actual = frequency;
    if (g_ndal_state.ops->frequency &&
        g_ndal_state.ops->frequency(g_ndal_state.ops_ctx, g_ndal_state.fd,
            &actual) == 0 && actual)
        frequency = actual;
    g_ndal_state.path = path;
    g_ndal_state.frequency = frequency;
    result->state = OA_NIM_TUNE_UNLOCKED;
    result->frequency_khz = frequency;
    return 0;
}

static int ndal_monitor(uint8_t path, struct oa_nim_tune_result *result)
{
    (void)path;
    if (!result || !g_ndal_state.open) return -EINVAL;
    uint32_t status = 0;
    int rc = g_ndal_state.ops->status(g_ndal_state.ops_ctx,
        g_ndal_state.fd, &status);
    if (rc < 0) return rc;
    uint32_t frequency = g_ndal_state.frequency;
    if (g_ndal_state.ops->frequency) {
        uint32_t actual = 0;
        if (g_ndal_state.ops->frequency(g_ndal_state.ops_ctx,
                g_ndal_state.fd, &actual) == 0 && actual)
            frequency = actual;
    }
    g_ndal_state.frequency = frequency;
    result->state = (status & FE_HAS_LOCK) ? OA_NIM_TUNE_LOCKED :
        OA_NIM_TUNE_UNLOCKED;
    result->frequency_khz = frequency;
    return 0;
}

static int ndal_stop(uint8_t path)
{
    (void)path;
    if (!g_ndal_state.open) return -EINVAL;
    int rc = g_ndal_state.ops->close_frontend(g_ndal_state.ops_ctx,
        g_ndal_state.fd);
    g_ndal_state.fd = -1;
    g_ndal_state.open = 0;
    return rc;
}

static int ndal_signal(uint8_t path, struct oa_nim_signal *out)
{
    (void)path;
    if (!out || !g_ndal_state.open) return -EINVAL;
    return g_ndal_state.ops->metrics(g_ndal_state.ops_ctx, g_ndal_state.fd,
        out);
}

static const struct oa_nim_ops ndal_ops = {
    ndal_start, ndal_monitor, ndal_stop, ndal_signal
};

const struct oa_tuner_backend *oa_nim_ndal_backend(const char *frontend_path)
{
    static struct oa_tuner_backend backend;
    if (g_ndal_state.open) return NULL;
    if (!g_ndal_state.ops) g_ndal_state.ops = &linux_ops;
    if (frontend_path && *frontend_path) {
        size_t length = strlen(frontend_path);
        if (length >= sizeof g_ndal_state.frontend) return NULL;
        memcpy(g_ndal_state.frontend, frontend_path, length + 1);
    } else {
        snprintf(g_ndal_state.frontend, sizeof g_ndal_state.frontend,
            "/dev/dvb/adapter0/frontend0");
    }
    backend.name = "nim-ndal";
    backend.nim = &ndal_ops;
    backend.next_eit = NULL;
    return &backend;
}
