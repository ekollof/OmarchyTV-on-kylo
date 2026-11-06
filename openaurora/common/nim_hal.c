#include "openaurora/nim_hal.h"

#include <errno.h>
#include <string.h>

#include "openaurora/mxl661_atv.h"

struct hal {
    const struct oa_nim_tune_ops *ops;
    void *ud;
    struct oa_nim_tune_state tune;
    uint8_t path;
    uint32_t frequency_khz;
    int active;
};

static struct hal g_hal;
static const struct oa_nim_tune_ops *g_ops;
static void *g_ud;
static const struct oa_mxl661_atv_tables *g_atv_tables;
static int g_atv_standard;
static uint32_t g_atv_if_khz;
static int g_atv_xtal24;
static int g_vendor_channel;

void oa_nim_hal_set_ops(const struct oa_nim_tune_ops *ops, void *ud)
{
    g_ops = ops;
    g_ud = ud;
}

void oa_nim_hal_set_vendor_channel(int enabled)
{
    g_vendor_channel = enabled != 0;
}

int oa_nim_hal_set_atv(const struct oa_mxl661_atv_tables *tables,
                       int standard, uint32_t if_khz, int xtal_24mhz)
{
    if (!tables) {
        g_atv_tables = NULL;
        return 0;
    }
    if (standard < OA_MXL661_ATV_NTSC || standard > OA_MXL661_ATV_SECAM_L)
        return -EINVAL;
    if (if_khz == 0)
        return -EINVAL;
    g_atv_tables = tables;
    g_atv_standard = standard;
    g_atv_if_khz = if_khz;
    g_atv_xtal24 = xtal_24mhz != 0;
    return 0;
}

static int hal_start(uint8_t path, const uint32_t *params, size_t count,
                     struct oa_nim_tune_result *result)
{
    if (!g_ops || !params || count < 2 || !result)
        return -EINVAL;
    if (path != OA_NIM_PATH_TERRESTRIAL)
        return -ENOTSUP; /* only the DVB-T RF/IF tail is recovered */
    if (!params[1])
        return -EINVAL;

    memset(&g_hal, 0, sizeof g_hal);
    g_hal.ops = g_ops;
    g_hal.ud = g_ud;
    g_hal.path = path;
    g_hal.frequency_khz = params[1];
    g_hal.tune.path = path;
    g_hal.tune.handle = 0;
    g_hal.tune.tuner_type = 0xd;

    if (!oa_nim_tune_open(g_ops, g_ud, &g_hal.tune))
        return -EIO;
    if (g_atv_tables) {
        if (!g_ops->program_atv)
            return -ENOTSUP;
        struct oa_mxl661_analog_params p = {
            .freq_hz = params[1] * 1000u,
            .if_khz = g_atv_if_khz,
            .bandwidth = 2, /* vendor SetRF analog bandwidth */
            .standard = g_atv_standard,
            .start = 0,
            .xtal_24mhz = g_atv_xtal24
        };
        if (g_ops->program_atv(g_ud, &p, g_atv_tables) != 0)
            return -EIO;
    } else {
        const struct oa_nim_tune_channel channel = {
            .freq_hz = params[1] * 1000u,
            .standard = 8,     /* DVB-T */
            .bandwidth = 0x22, /* 8 MHz, DEV_NIM_DTV_StartTune's default */
            .if_khz = 5000
        };
        if (g_vendor_channel && g_ops->set_channel) {
            struct oa_nim_raw_channel record;
            /* StartTune's normal COFDM record: 8 MHz, IF mode 1. */
            if (oa_nim_raw_channel_build_dvbt(&record, params[1], 8, 1) != 0)
                return -EIO;
            if (g_ops->set_channel(g_ud, 2, &record) != 0)
                return -EIO;
        } else if (!oa_nim_tune_set_channel(g_ops, g_ud, &g_hal.tune,
                                            &channel)) {
            return -EIO;
        }
    }
    g_hal.active = 1;
    result->state = OA_NIM_TUNE_UNLOCKED;
    result->frequency_khz = g_hal.frequency_khz;
    return 0;
}

static int hal_monitor(uint8_t path, struct oa_nim_tune_result *result)
{
    if (!result || !g_hal.active || path != g_hal.path)
        return -EINVAL;
    uint8_t locked = 0;
    if (g_atv_tables) {
        if (!g_ops->get_atv_status)
            return -ENOTSUP;
        if (g_ops->get_atv_status(g_ud, path, &locked) != 0)
            return -EIO;
    } else if (!oa_nim_tune_check_lock(g_hal.ops, g_hal.ud, &g_hal.tune,
                                       &locked)) {
        return -EIO;
    }
    result->state = locked ? OA_NIM_TUNE_LOCKED : OA_NIM_TUNE_UNLOCKED;
    result->frequency_khz = g_hal.frequency_khz;
    return 0;
}

static int hal_stop(uint8_t path)
{
    if (!g_hal.active || path != g_hal.path)
        return -EINVAL;
    (void)oa_nim_tune_close(g_hal.ops, g_hal.ud, &g_hal.tune);
    g_hal.active = 0;
    return 0;
}

static int hal_signal(uint8_t path, struct oa_nim_signal *out)
{
    if (!out)
        return -EINVAL;
    memset(out, 0, sizeof *out);
    out->unc = OA_NIM_UNC_UNKNOWN;
    if (!g_hal.active || path != g_hal.path)
        return 0;
    if (g_atv_tables) {
        uint8_t locked = 0;
        if (!g_ops->get_atv_status)
            return -ENOTSUP;
        if (g_ops->get_atv_status(g_ud, path, &locked) != 0)
            return -EIO;
        out->locked = locked;
        /* SAL_TUNER_MxL661_GetATVStatus returns lock only; strength/quality
         * are not part of that contract. */
        return 0;
    }
    struct oa_nim_tune_status status;
    if (!oa_nim_tune_get_channel_status(g_hal.ops, g_hal.ud, &g_hal.tune,
                                        &status))
        return -EIO;
    out->locked = status.locked ? 1 : 0;
    out->strength = status.strength;
    out->quality = status.quality;
    return 0;
}

static const struct oa_nim_ops hal_nim = {
    hal_start, hal_monitor, hal_stop, hal_signal
};

static const struct oa_tuner_backend hal_backend = {
    "nim-hal", &hal_nim, NULL
};

const struct oa_tuner_backend *oa_nim_hal_backend(void)
{
    return &hal_backend;
}
