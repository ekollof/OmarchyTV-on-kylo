#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/mxl661_atv.h"
#include "openaurora/nim_hal.h"

struct fake {
    int switches;
    int opens;
    int closes;
    int programs;
    int atv_programs;
    int atv_status_calls;
    int atv_path;
    uint8_t atv_locked;
    struct oa_nim_tune_channel channel;
    struct oa_mxl661_analog_params atv_params;
    const struct oa_mxl661_atv_tables *atv_tables;
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
static int f_program_atv(void *ud,
                         const struct oa_mxl661_analog_params *params,
                         const struct oa_mxl661_atv_tables *tables)
{
    struct fake *f = ud;
    f->atv_programs++;
    f->atv_params = *params;
    f->atv_tables = tables;
    return 0;
}
static int f_atv_status(void *ud, int path, uint8_t *locked)
{
    struct fake *f = ud;
    f->atv_status_calls++;
    f->atv_path = path;
    *locked = f->atv_locked;
    return 0;
}

int main(void)
{
    struct fake f = { .locked = 1, .strength = 0x1234, .quality = 0x5678 };
    const struct oa_nim_tune_ops ops = {
        f_switch, f_type, f_deliver, f_open, f_close, f_standby, f_status,
        f_rssi, f_program, f_program_atv, f_atv_status
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

    /* Analog mode consumes the caller-owned runtime table set. */
    static struct oa_mxl661_atv_tables atv;
    assert(oa_nim_hal_set_atv(&atv, OA_MXL661_ATV_PAL, 5000, 1) == 0);
    assert(b->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &tr) == 0);
    assert(f.atv_programs == 1 && f.programs == 1);
    assert(f.atv_tables == &atv);
    assert(f.atv_params.freq_hz == 666000000u &&
           f.atv_params.if_khz == 5000 &&
           f.atv_params.standard == OA_MXL661_ATV_PAL &&
           f.atv_params.bandwidth == 2 && f.atv_params.start == 0 &&
           f.atv_params.xtal_24mhz == 1);

    f.atv_locked = 1;
    assert(b->nim->monitor(OA_NIM_PATH_TERRESTRIAL, &tr) == 0);
    assert(tr.state == OA_NIM_TUNE_LOCKED);
    assert(f.atv_status_calls == 1 && f.atv_path == OA_NIM_PATH_TERRESTRIAL);
    assert(b->nim->signal(OA_NIM_PATH_TERRESTRIAL, &sig) == 0);
    assert(sig.locked == 1 && sig.strength == 0 && sig.quality == 0 &&
           sig.unc == OA_NIM_UNC_UNKNOWN);
    f.atv_locked = 0;
    assert(b->nim->monitor(OA_NIM_PATH_TERRESTRIAL, &tr) == 0);
    assert(tr.state == OA_NIM_TUNE_UNLOCKED);
    assert(b->nim->stop(OA_NIM_PATH_TERRESTRIAL) == 0);
    assert(oa_nim_hal_set_atv(&atv, 6, 5000, 0) == -EINVAL);
    assert(oa_nim_hal_set_atv(&atv, OA_MXL661_ATV_NTSC, 0, 0) == -EINVAL);
    assert(oa_nim_hal_set_atv(NULL, 0, 0, 0) == 0);

    /* Clearing analog restores the DVB-T program/status path. */
    assert(b->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &tr) == 0);
    assert(f.programs == 2 && f.atv_programs == 1);
    assert(b->nim->monitor(OA_NIM_PATH_TERRESTRIAL, &tr) == 0);
    assert(tr.state == OA_NIM_TUNE_LOCKED);
    assert(b->nim->signal(OA_NIM_PATH_TERRESTRIAL, &sig) == 0);
    assert(sig.strength == 0x1234 && sig.quality == 0x5678);
    assert(b->nim->stop(OA_NIM_PATH_TERRESTRIAL) == 0);

    assert(b->nim->start(OA_NIM_PATH_CABLE, params, 2, &tr) == -ENOTSUP);
    assert(b->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 1, &tr) == -EINVAL);
    params[1] = 0;
    assert(b->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &tr) == -EINVAL);

    oa_nim_hal_set_ops(NULL, NULL);
    params[1] = 666000;
    assert(b->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &tr) == -EINVAL);

    puts("ok nim hal: backend start/monitor/signal/stop, DVB-T default, "
         "analog table path and ATV lock status");
    return 0;
}
