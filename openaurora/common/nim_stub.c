/* Offline NIM stub backend. See include/openaurora/nim.h for the recovered
 * vendor shape and the live-backend limitation. */
#include "openaurora/nim.h"

#include <errno.h>
#include <stdio.h>

#include "openaurora/eit_section.h"

struct stub {
    uint8_t path;
    uint32_t frequency_khz;
    unsigned polls;
    unsigned emitted;
    int active;
};

static struct stub g_stub;

static int stub_start(uint8_t path, const uint32_t *params, size_t count,
    struct oa_nim_tune_result *result)
{
    if (!params || count < 2 || !result) return -EINVAL;
    switch (path) {
    case 0: /* offline IP/synthetic path, not a recovered NIM path */
    case OA_NIM_PATH_TERRESTRIAL:
    case OA_NIM_PATH_CABLE:
    case OA_NIM_PATH_QPSK:
    case OA_NIM_PATH_DTMB:
    case OA_NIM_PATH_ISDBT:
        break;
    default:
        return -EINVAL;
    }
    g_stub.path = path;
    g_stub.frequency_khz = params[1] ? params[1] :
        OA_NIM_DEFAULT_FREQUENCY_KHZ;
    g_stub.polls = 0;
    g_stub.emitted = 0;
    g_stub.active = 1;
    result->state = OA_NIM_TUNE_UNLOCKED;
    result->frequency_khz = g_stub.frequency_khz;
    return 0;
}

static int stub_monitor(uint8_t path, struct oa_nim_tune_result *result)
{
    if (!result || !g_stub.active || path != g_stub.path) return -EINVAL;
    if (g_stub.polls < 10) g_stub.polls++;
    result->state = g_stub.polls >= 2 ? OA_NIM_TUNE_LOCKED :
        OA_NIM_TUNE_UNLOCKED;
    result->frequency_khz = g_stub.frequency_khz;
    return 0;
}

static int stub_stop(uint8_t path)
{
    if (!g_stub.active || path != g_stub.path) return -EINVAL;
    g_stub.active = 0;
    return 0;
}

static int stub_signal(uint8_t path, struct oa_nim_signal *out)
{
    if (!out) return -EINVAL;
    out->locked = 0;
    out->strength = 0;
    out->quality = 0;
    out->unc = OA_NIM_UNC_UNKNOWN;
    out->ber = 0.0f;
    out->snr = 0.0f;
    if (!g_stub.active || path != g_stub.path) return 0;
    if (g_stub.polls >= 2) {
        out->locked = 1;
        out->strength = 80;
        out->quality = 75;
        out->unc = 0;
        out->snr = 20.5f;
    } else {
        out->strength = 20 + 10 * g_stub.polls;
        out->quality = 10 * g_stub.polls;
    }
    return 0;
}

static void bcd_time(uint8_t out[5], unsigned hour, unsigned minute,
    unsigned second)
{
    out[0] = 0xea; /* MJD 60000: 2023-02-25 */
    out[1] = 0x60;
    out[2] = (uint8_t)(((hour / 10) << 4) | (hour % 10));
    out[3] = (uint8_t)(((minute / 10) << 4) | (minute % 10));
    out[4] = (uint8_t)(((second / 10) << 4) | (second % 10));
}

static int stub_next_eit(uint8_t *buf, size_t cap, size_t *len)
{
    if (!buf || !len) return -EINVAL;
    if (!g_stub.active || g_stub.emitted >= 12) return 0;
    unsigned index = g_stub.emitted;
    struct oa_eit_build_event event;
    char name[32];
    event.event_id = (uint16_t)(1001 + index);
    bcd_time(event.start, 12, 0, 0);
    event.duration[0] = 0x00; /* 30 minutes */
    event.duration[1] = 0x30;
    event.duration[2] = 0x00;
    event.running_status = 4;
    event.free_ca_mode = 0;
    snprintf(name, sizeof name, "Stub Service %u", index + 1);
    event.name = name;
    event.text = "Synthetic EIT for offline scan";
    int rc = oa_eit_build_section(0x4e, (uint16_t)(101 + index),
        (uint8_t)(index & 0x1f), 1, &event, 1, buf, cap, len);
    if (rc) return rc;
    g_stub.emitted++;
    return 1;
}

static const struct oa_nim_ops stub_nim = {
    stub_start, stub_monitor, stub_stop, stub_signal
};

static const struct oa_tuner_backend stub_backend = {
    "nim-stub", &stub_nim, stub_next_eit
};

const struct oa_tuner_backend *oa_nim_stub_backend(void)
{
    return &stub_backend;
}

const struct oa_nim_ops *oa_nim_stub_ops(void)
{
    return &stub_nim;
}
