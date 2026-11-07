#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/amp_vendor.h"

struct fake {
    int prelude;
    int configs;
    uint32_t config_pin[8], config_dir[8];
    int config_rc;
    int writes;
    uint32_t write_pin[8], write_level[8];
    int write_rc;
    int stbc;
    uint32_t stbc_pin[8], stbc_cmd[8];
    int stbc_rc;
    int i2c;
    unsigned i2c_reg;
    uint8_t i2c_value;
    int i2c_rc;
    int delays;
    unsigned delay_total;
    int sub_enabled;
    uint32_t sub_value;
    int sub_enable, sub_enable_value, sub_init;
};

static void prelude(void *p) { ((struct fake *)p)->prelude++; }
static int gpio_write(void *p, uint32_t pin, uint32_t level)
{
    struct fake *f = p;
    assert(f->writes < 8);
    f->write_pin[f->writes] = pin;
    f->write_level[f->writes] = level;
    f->writes++;
    return f->write_rc;
}
static int gpio_config(void *p, uint32_t pin, uint32_t dir)
{
    struct fake *f = p;
    assert(f->configs < 8);
    f->config_pin[f->configs] = pin;
    f->config_dir[f->configs] = dir;
    f->configs++;
    return f->config_rc;
}
static int stbc_set(void *p, uint32_t pin, uint32_t cmd)
{
    struct fake *f = p;
    assert(f->stbc < 8);
    f->stbc_pin[f->stbc] = pin;
    f->stbc_cmd[f->stbc] = cmd;
    f->stbc++;
    return f->stbc_rc;
}
static int i2c_write(void *p, unsigned reg, const uint8_t *value)
{
    struct fake *f = p;
    f->i2c++;
    f->i2c_reg = reg;
    f->i2c_value = *value;
    return f->i2c_rc;
}
static void delay_ms(void *p, unsigned ms)
{
    struct fake *f = p;
    f->delays++;
    f->delay_total += ms;
}
static uint32_t sub_enabled(void *p)
{
    struct fake *f = p;
    f->sub_enabled++;
    return f->sub_value;
}
static void sub_enable(void *p, int on)
{
    struct fake *f = p;
    f->sub_enable++;
    f->sub_enable_value = on;
}
static void sub_init(void *p) { ((struct fake *)p)->sub_init++; }

static const oa_ad82587_profile profile = {
    .speaker_power = "12Watt",
    .prescale = 27,
    .channel1 = 5,
    .channel2 = 5,
    .fine_tune = 128,
    .power_clip = 405736,
    .speaker_drc = 98304,
    .state_control4 = -1,
    .limiter_attack = -1,
    .release_threshold = -1
};

int main(void)
{
    struct fake f;
    oa_amp_board board;
    oa_amp_vendor vendor;
    oa_amp_init_io io;
    oa_amp_init_state state;
    uint8_t value = 0x5a;

    oa_amp_board_defaults(&board);
    memset(&f, 0, sizeof f);
    oa_amp_vendor_io vio = {
        .context = &f,
        .gpio_write = gpio_write,
        .gpio_config = gpio_config,
        .stbc_set = stbc_set,
        .i2c_write = i2c_write,
        .delay_ms = delay_ms,
        .subwoofer_enabled = sub_enabled,
        .subwoofer_enable = sub_enable,
        .subwoofer_init = sub_init,
        .prelude = prelude
    };
    assert(oa_amp_vendor_init(&vendor, &vio, NULL, &board, &io) == 0);
    assert(vendor.board == &board && vendor.i2c == NULL);
    assert(io.context == &vendor && io.prelude && io.gpio && io.direction &&
           io.level && io.delay_ms && io.write && io.subwoofer_enabled &&
           io.subwoofer_enable && io.subwoofer_init);

    /* GPIO record selection still comes from the board. */
    assert(io.gpio(io.context, 0) == &board.reset);
    assert(io.gpio(io.context, 1) == &board.mute);

    /* Pin < 200 uses the normal GPIO wrappers; direction failures are
     * ignored by the vendor void API. */
    io.direction(io.context, 114, 1);
    assert(f.configs == 1 && f.config_pin[0] == 114 &&
           f.config_dir[0] == 1);
    f.config_rc = -EIO;
    io.direction(io.context, 114, 0);
    assert(f.configs == 2 && f.config_pin[1] == 114 &&
           f.config_dir[1] == 0);

    /* Pin >= 200 maps DEV_GPIO_SetLevel's level->command mapping and the
     * (pin + 0x38) & 0xff STBC pin. It never touches the normal GPIO path. */
    f.configs = 0;
    io.direction(io.context, 208, 1);
    assert(f.configs == 0 && f.stbc == 0);
    f.stbc_rc = 1;
    assert(io.level(io.context, 208, 0) == 1);
    assert(f.stbc == 1 && f.stbc_pin[0] == 8 && f.stbc_cmd[0] == 3);
    assert(io.level(io.context, 208, 1) == 1);
    assert(f.stbc == 2 && f.stbc_pin[1] == 8 && f.stbc_cmd[1] == 2);
    f.stbc_rc = 0;
    assert(io.level(io.context, 208, 1) == 0);
    f.stbc_rc = 1;

    /* Normal level is errno-returning: only 0 is vendor true. */
    assert(io.level(io.context, 114, 0) == 1);
    assert(f.writes == 1 && f.write_pin[0] == 114 && f.write_level[0] == 0);
    f.write_rc = -EIO;
    assert(io.level(io.context, 114, 1) == 0);
    f.write_rc = 0;

    /* Register writes: injected callback is vendor boolean; absent session
     * with no callback fails closed. */
    f.i2c_rc = 1;
    assert(io.write(io.context, 3, &value) == 1);
    assert(f.i2c == 1 && f.i2c_reg == 3 && f.i2c_value == 0x5a);
    f.i2c_rc = 0;
    assert(io.write(io.context, 4, &value) == 0);
    vio.i2c_write = NULL;
    assert(oa_amp_vendor_init(&vendor, &vio, NULL, &board, &io) == 0);
    assert(io.write(io.context, 3, &value) == 0);

    /* End-to-end through the sequencer with the composed transport. */
    vio.i2c_write = i2c_write;
    assert(oa_amp_vendor_init(&vendor, &vio, NULL, &board, &io) == 0);
    memset(&f, 0, sizeof f);
    f.stbc_rc = 1;
    f.i2c_rc = 1;
    memset(&state, 0, sizeof state);
    assert(oa_amp_init(&state, &profile, &io) == 1);
    assert(f.prelude == 1);
    assert(f.configs == 1 && f.config_pin[0] == 114 &&
           f.config_dir[0] == 1); /* STBC direction 1 is the vendor no-op */
    assert(f.writes == 4); /* 114 initial 0,1 + reset pulses 0,1 */
    assert(f.stbc == 3 && f.stbc_pin[0] == 8 && f.stbc_cmd[0] == 3 &&
           f.stbc_cmd[1] == 3 && f.stbc_cmd[2] == 2);
    assert(f.i2c == 38);
    assert(f.sub_enabled == 1 && f.sub_enable == 1 && f.sub_init == 1);
    assert(f.sub_enable_value == 0);
    assert(state.reset_cached == 0 && state.mute_cached == 1);

    /* STBC failure does not stop Init; the mute cache keeps its old value. */
    memset(&f, 0, sizeof f);
    f.stbc_rc = 0;
    f.i2c_rc = 1;
    state = (oa_amp_init_state){ .reset_cached = 7, .mute_cached = 9 };
    assert(oa_amp_init(&state, &profile, &io) == 1);
    assert(f.stbc == 3 && state.mute_cached == 9 &&
           state.reset_cached == 0);

    assert(oa_amp_vendor_init(NULL, &vio, NULL, &board, &io) == -EINVAL);
    assert(oa_amp_vendor_init(&vendor, NULL, NULL, &board, &io) == -EINVAL);
    assert(oa_amp_vendor_init(&vendor, &vio, NULL, NULL, &io) == -EINVAL);
    assert(oa_amp_vendor_init(&vendor, &vio, NULL, &board, NULL) == -EINVAL);

    puts("ok amp vendor: 114 Hal_Gpio, 208 STBC command mapping, vendor "
         "boolean, 38 registers and sequencer composition");
    return 0;
}
