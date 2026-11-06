#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/nim_hal.h"

struct fake {
    int switches;
    int opens;
    int closes;
    int programs;
    struct oa_nim_tune_channel channel;
    uint8_t locked;
    uint32_t strength;
    uint32_t quality;
};

static int f_switch(void *ud, int type)
{
    (void)ud;
    (void)type;
    ((struct fake *)ud)->switches++;
    return 0;
}
static int f_type(void *ud) { (void)ud; return 0xd; }
static int f_deliver(void *ud, uint32_t key, const void *data, int len)
{
    (void)ud;
    (void)key;
    (void)data;
    (void)len;
    return 0;
}
static int f_open(void *ud, int index)
{
    (void)index;
    ((struct fake *)ud)->opens++;
    return 1;
}
static void f_close(void *ud, int fe) { (void)fe; ((struct fake *)ud)->closes++; }
static int f_standby(void *ud, int fe, int on)
{
    (void)ud;
    (void)fe;
    (void)on;
    return 0;
}
static int f_status(void *ud, int handle, int index, uint32_t mask, void *buf)
{
    (void)handle;
    (void)index;
    struct fake *f = ud;
    uint8_t *b = buf;
    if (mask == 2)
        b[0] = f->locked;
    else if (mask == 0x40)
        memcpy(b + 0x18, &f->strength, sizeof f->strength);
    else if (mask == 0x80)
        memcpy(b + 0x24, &f->quality, sizeof f->quality);
    return 0;
}
static int f_rssi(void *ud, int type, uint8_t *rssi)
{
    (void)ud;
    (void)type;
    (void)rssi;
    return 0;
}
static int f_program(void *ud, const struct oa_nim_tune_channel *ch)
{
    struct fake *f = ud;
    f->programs++;
    f->channel = *ch;
    return 0;
}

int main(void)
{
    struct fake f = { .locked = 1, .strength = 0x1234, .quality = 0x5678 };
    const struct oa_nim_tune_ops ops = {
        f_switch, f_type, f_deliver, f_open, f_close, f_standby, f_status,
        f_rssi, f_program
    };
    oa_nim_hal_set_ops(&ops, &f);
    const struct oa_tuner_backend *b = oa_nim_hal_backend();
    assert(b && b->nim);

    uint32_t params[2] = { 0, 666000 };
    struct oa_nim_tune_result tr;
    assert(b->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &tr) == 0);
    assert(tr.frequency_khz == 666000 && tr.state == OA_NIM_TUNE_UNLOCKED);
    assert(f.switches == 1 && f.programs == 1);
    assert(f.channel.freq_hz == 666000000u && f.channel.standard == 8 &&
           f.channel.bandwidth == 0x20 && f.channel.if_khz == 5000);

    assert(b->nim->monitor(OA_NIM_PATH_TERRESTRIAL, &tr) == 0);
    assert(tr.state == OA_NIM_TUNE_LOCKED);

    struct oa_nim_signal sig;
    assert(b->nim->signal(OA_NIM_PATH_TERRESTRIAL, &sig) == 0);
    assert(sig.locked == 1 && sig.strength == 0x1234 && sig.quality == 0x5678);
    assert(sig.unc == OA_NIM_UNC_UNKNOWN);

    assert(b->nim->stop(OA_NIM_PATH_TERRESTRIAL) == 0);
    assert(f.opens == 1 && f.closes == 1);

    assert(b->nim->start(OA_NIM_PATH_CABLE, params, 2, &tr) == -ENOTSUP);
    assert(b->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 1, &tr) == -EINVAL);
    params[1] = 0;
    assert(b->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &tr) == -EINVAL);

    oa_nim_hal_set_ops(NULL, NULL);
    params[1] = 666000;
    assert(b->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &tr) == -EINVAL);

    puts("ok nim hal: backend start/monitor/signal/stop over fake tuning ops "
         "and terrestrial-only gate");
    return 0;
}
