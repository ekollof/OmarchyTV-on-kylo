#include "openaurora/nim_tune.h"

#include <errno.h>
#include <string.h>

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
                            const void *channel)
{
    (void)ops;
    (void)ud;
    (void)state;
    (void)channel;
    return -ENOTSUP;
}

int oa_nim_tune_get_channel_status(const struct oa_nim_tune_ops *ops, void *ud,
                                   struct oa_nim_tune_state *state, void *out)
{
    (void)ops;
    (void)ud;
    (void)state;
    (void)out;
    return -ENOTSUP;
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
