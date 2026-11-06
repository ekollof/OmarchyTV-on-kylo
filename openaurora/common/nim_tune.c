#include "openaurora/nim_tune.h"

#include <errno.h>
#include <string.h>

#include "openaurora/mxl661_tune.h"

int oa_nim_tune_path_index(const struct oa_nim_tune_state *state)
{
    if (!state)
        return 0;
    switch (state->path) {
    case 1:
        return 0x14;
    case 2:
    case 7:
        return 0x12;
    case 5:
        return 0x11;
    case 6:
        return 0x0e;
    case 8:
        return 0x10;
    case 3:
        return -1; /* dynamic resolver FUN_0086999c not recovered */
    default:
        return 0;
    }
}

int oa_nim_tune_open(const struct oa_nim_tune_ops *ops, void *ud,
                     struct oa_nim_tune_state *state)
{
    if (!ops || !state || !ops->switch_tuner_type || !ops->get_tuner_type)
        return 0;
    int type = state->path == 1 ? 1 : 2;
    if (ops->switch_tuner_type(ud, type) != 0)
        return 0;
    int tuner = ops->get_tuner_type(ud);
    state->tuner_type = tuner;
    int mapped;
    switch (tuner) {
    case 5:
    case 6:
    case 7:
    case 8:
    case 10:
    case 11:
        mapped = tuner;
        break;
    case 0xd:
        mapped = 0xc;
        break;
    default:
        return 1; /* the vendor skips delivery for other types */
    }
    if (ops->deliver_config &&
        ops->deliver_config(ud, 0xa000, &mapped, 1) != 0)
        return 0;
    return 1;
}

int oa_nim_tune_close(const struct oa_nim_tune_ops *ops, void *ud,
                      struct oa_nim_tune_state *state)
{
    (void)state;
    if (!ops || !ops->fe_open || !ops->set_standby || !ops->fe_close)
        return 0;
    int fe = ops->fe_open(ud, 0);
    if (fe < 0)
        return 0;
    /* The vendor prints a standby failure but still closes the frontend. */
    (void)ops->set_standby(ud, fe, 1);
    ops->fe_close(ud, fe);
    return 1;
}

int oa_nim_tune_monitor(void)
{
    return 1;
}

int oa_nim_tune_disconnect(void)
{
    return 0;
}

int oa_nim_tune_check_lock(const struct oa_nim_tune_ops *ops, void *ud,
                           struct oa_nim_tune_state *state, uint8_t *locked)
{
    if (!ops || !ops->get_status || !state || !locked)
        return 0;
    int index = oa_nim_tune_path_index(state);
    if (index < 0)
        return 0;
    uint8_t buf[0x180];
    memset(buf, 0, sizeof buf);
    if (ops->get_status(ud, state->handle, index, 2, buf) != 0)
        buf[0] = 0;
    *locked = buf[0];
    return 1;
}

int oa_nim_tune_get_strength(const struct oa_nim_tune_ops *ops, void *ud,
                             struct oa_nim_tune_state *state,
                             uint32_t *strength)
{
    if (!ops || !state || !strength)
        return 0;
    if (state->path == 1) {
        int index = oa_nim_tune_path_index(state);
        if (index < 0 || !ops->get_status)
            return 0;
        uint8_t buf[0x180];
        memset(buf, 0, sizeof buf);
        if (ops->get_status(ud, state->handle, index, 0x40, buf) != 0) {
            *strength = 0;
            return 1;
        }
        uint32_t value;
        memcpy(&value, buf + 0x18, sizeof value);
        *strength = value;
        return 1;
    }
    if (!ops->get_rssi)
        return 0;
    uint8_t rssi = 100;
    if (ops->get_rssi(ud, state->tuner_type, &rssi) == 0)
        *strength = 0;
    else
        *strength = rssi;
    return 1;
}

int oa_nim_tune_get_quality(const struct oa_nim_tune_ops *ops, void *ud,
                            struct oa_nim_tune_state *state, uint32_t *quality)
{
    if (!ops || !ops->get_status || !state || !quality)
        return 0;
    int index = oa_nim_tune_path_index(state);
    if (index < 0)
        return 0;
    uint8_t buf[0x180];
    memset(buf, 0, sizeof buf);
    if (ops->get_status(ud, state->handle, index, 0x80, buf) == 0) {
        uint32_t value;
        memcpy(&value, buf + 0x24, sizeof value);
        *quality = value;
    }
    return 1;
}

int oa_nim_tune_set_channel(const struct oa_nim_tune_ops *ops, void *ud,
                            struct oa_nim_tune_state *state,
                            const struct oa_nim_tune_channel *channel)
{
    if (!ops || !ops->program_tuner || !state || !channel)
        return 0;
    return ops->program_tuner(ud, channel) == 0 ? 1 : 0;
}

int oa_nim_tune_get_channel_status(const struct oa_nim_tune_ops *ops, void *ud,
                                   struct oa_nim_tune_state *state,
                                   struct oa_nim_tune_status *out)
{
    if (!ops || !state || !out)
        return 0;
    if (!oa_nim_tune_check_lock(ops, ud, state, &out->locked))
        return 0;
    if (!oa_nim_tune_get_strength(ops, ud, state, &out->strength))
        return 0;
    if (!oa_nim_tune_get_quality(ops, ud, state, &out->quality))
        return 0;
    return 1;
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

int oa_nim_raw_channel_parse(const uint8_t raw[0x5c], int path,
                             struct oa_nim_tune_channel *out)
{
    if (!raw || !out)
        return 0;
    uint32_t standard = oa_mxl661_standard_from_index(rd32(raw + 0x1c));
    if (!standard)
        return 0;
    uint32_t freq = rd32(raw + 0);
    uint32_t bandwidth = rd32(raw + 8);
    uint32_t p4 = rd32(raw + 0x10);
    uint32_t override = rd32(raw + 0x58);
    if (path == 1) {
        if (override)
            freq = override;
        if (bandwidth == 0 || p4 == 3)
            bandwidth = 40000;
    }
    out->freq_hz = freq * 1000u;
    out->standard = standard;
    out->bandwidth = bandwidth;
    out->if_khz = 0;
    return 1;
}

int oa_nim_tune_set_channel_raw(const struct oa_nim_tune_ops *ops, void *ud,
                                struct oa_nim_tune_state *state,
                                const struct oa_nim_raw_channel *channel)
{
    if (!state || !channel)
        return 0;
    struct oa_nim_tune_channel parsed;
    if (!oa_nim_raw_channel_parse(channel->raw, state->path, &parsed))
        return 0;
    return oa_nim_tune_set_channel(ops, ud, state, &parsed);
}

int oa_nim_raw_status_parse(const uint8_t raw[0x180],
                            struct oa_nim_tune_status *out)
{
    if (!raw || !out)
        return 0;
    out->locked = raw[0];
    out->strength = rd32(raw + 0x18);
    out->quality = rd32(raw + 0x24);
    return 1;
}

/* The vendor's raw-status fields behind core word[3..9] are not named here;
 * these helpers are the recovered one-stage enum translations only. */
static uint32_t status_map_core4(uint32_t v)
{
    switch (v) {
    case 1: return 0;
    case 2: return 1;
    case 3: return 2;
    case 4: return 3;
    case 5: return 5;
    case 6: return 7;
    default: return 0xff;
    }
}

static uint32_t status_map_core3(uint32_t v)
{
    switch (v) {
    case 3: return 0;
    case 7: return 1;
    case 9: return 2;
    case 11: return 6;
    default: return 0xff;
    }
}

static uint32_t status_map_core78(uint32_t v)
{
    switch (v) {
    case 0: return 1;
    case 1: return 0;
    case 2: return 2;
    case 3: return 6;
    case 4: return 7;
    case 5: return 3;
    case 6: return 4;
    default: return 0xff;
    }
}

static uint32_t status_map_core5(uint32_t v)
{
    switch (v) {
    case 1: return 1;
    case 2: return 0;
    case 3: return 3;
    case 4: return 2;
    case 5: return 8;
    case 6: return 9;
    case 7: return 7;
    default: return 0xff;
    }
}

static uint32_t status_map_core6(uint32_t v)
{
    switch (v) {
    case 0: return 0;
    case 1: return 1;
    default: return 2;
    }
}

static uint32_t status_map_core9(uint32_t v)
{
    switch (v) {
    case 1: return 1;
    case 2: return 2;
    case 3: return 3;
    default: return 0xff;
    }
}

int oa_nim_status_type6(const struct oa_nim_core_status *core, uint32_t out[4])
{
    if (!core || !out)
        return 0;
    /* 0x86bbda: out[3]=0 and the common copy at 0x86bbe8. out[1]/out[2] are
     * stack slots the vendor never writes here. */
    out[0] = core->word[0] * 1000u;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;
    return 1;
}

int oa_nim_status_type8(const struct oa_nim_core_status *core, uint32_t out[4])
{
    if (!core || !out)
        return 0;
    /* 0x86bbba: out[3] from core[3] (9 -> 1, 11 -> 2, else 0xff). */
    out[0] = core->word[0] * 1000u;
    out[1] = 0;
    out[2] = 0;
    switch (core->word[3]) {
    case 9:
        out[3] = 1;
        break;
    case 11:
        out[3] = 2;
        break;
    default:
        out[3] = 0xff;
        break;
    }
    return 1;
}

int oa_nim_status_type3(const struct oa_nim_core_status *core, uint32_t out[4])
{
    if (!core || !out)
        return 0;
    /* 0x86bc06: out[0]=freq*1000, out[1]=symbol rate (object +0x14 is core[2]),
     * out[3]=standard enum 7..11 -> 1..5, else 6. out[2] is vendor stack. */
    out[0] = core->word[0] * 1000u;
    out[1] = core->word[2];
    out[2] = 0;
    switch (core->word[3]) {
    case 7: out[3] = 1; break;
    case 8: out[3] = 2; break;
    case 9: out[3] = 3; break;
    case 10: out[3] = 4; break;
    case 11: out[3] = 5; break;
    default: out[3] = 6; break;
    }
    return 1;
}

int oa_nim_status_type2(const struct oa_nim_core_status *core, uint32_t out[16])
{
    if (!core || !out)
        return 0;
    /* 0x86bc1a..0x86bdfc: 16-word copy. out[14] is read from the caller's
     * buffer before the copy (0x86bc1c); the other undefined slots zero. */
    uint32_t keep14 = out[14];
    memset(out, 0, 16 * sizeof(*out));
    out[1] = core->word[0] * 1000u;
    out[2] = status_map_core4(core->word[4]);
    out[3] = core->word[1] == 6 ? 2 : core->word[1] == 7 ? 1 : 0;
    out[6] = status_map_core3(core->word[3]);
    out[7] = status_map_core6(core->word[6]);
    out[8] = status_map_core9(core->word[9]);
    out[9] = status_map_core78(core->word[8]);
    out[10] = status_map_core78(core->word[7]);
    out[11] = status_map_core5(core->word[5]);
    out[14] = keep14;
    return 1;
}

int oa_nim_status_type7(const struct oa_nim_core_status *core, uint32_t out[16],
                        int (*deliver_config)(void *ud, uint32_t key,
                                              const void *data, int len),
                        void *ud)
{
    if (!oa_nim_status_type2(core, out))
        return 0;
    uint32_t value = 0;
    int ok = deliver_config && deliver_config(ud, 0xb003u, &value, 1) == 0;
    out[12] = ok && value == 1 ? 1 : 0;
    return 1;
}

int oa_nim_status_type1(const struct oa_nim_core_status *core, uint32_t out[11])
{
    if (!core || !out)
        return 0;
    /* The vendor copies 11 words; only these fields are initialized. */
    memset(out, 0, 11 * sizeof(*out));
    out[1] = core->word[0];
    out[2] = core->word[2];
    out[9] = ((core->word[10] >> 16) & 0xffu) == 0 ? 1 : 9;
    return 1;
}

int oa_nim_tune_hal_open(void *v)
{
    struct oa_nim_tune_binding *b = v;
    return oa_nim_tune_open(b->ops, b->ud, &b->state);
}

int oa_nim_tune_hal_close(void *v)
{
    struct oa_nim_tune_binding *b = v;
    return oa_nim_tune_close(b->ops, b->ud, &b->state);
}

int oa_nim_tune_hal_monitor(void *v)
{
    (void)v;
    return oa_nim_tune_monitor();
}

int oa_nim_tune_hal_disconnect(void *v)
{
    (void)v;
    return oa_nim_tune_disconnect();
}

int oa_nim_tune_hal_check_lock(void *v)
{
    struct oa_nim_tune_binding *b = v;
    return oa_nim_tune_check_lock(b->ops, b->ud, &b->state, &b->locked);
}

int oa_nim_tune_hal_get_strength(void *v)
{
    struct oa_nim_tune_binding *b = v;
    return oa_nim_tune_get_strength(b->ops, b->ud, &b->state, &b->strength);
}

int oa_nim_tune_hal_get_quality(void *v)
{
    struct oa_nim_tune_binding *b = v;
    return oa_nim_tune_get_quality(b->ops, b->ud, &b->state, &b->quality);
}

int oa_nim_tune_hal_set_channel(void *v)
{
    (void)v;
    return -ENOTSUP;
}

int oa_nim_tune_hal_get_channel_status(void *v)
{
    (void)v;
    return -ENOTSUP;
}
