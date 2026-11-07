#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/amp_board.h"

struct fake {
    int prelude;
    int directions;
    uint32_t dir_pin[8];
    uint32_t dir_value[8];
    int normals;
    uint32_t normal_pin[8];
    uint32_t normal_value[8];
    int normal_rc;
    int stbc;
    uint32_t stbc_pin[8];
    uint32_t stbc_value[8];
    int stbc_rc;
    int writes;
    int write_fail_at;
    int write_rc;
    int delays;
    unsigned delay_total;
    int sub_enabled;
    uint32_t sub_value;
    int sub_enable;
    int sub_enable_value;
    int sub_init;
};

static void prelude(void *p) { ((struct fake *)p)->prelude++; }
static void direction(void *p, uint32_t pin, uint32_t value)
{
    struct fake *f = p;
    assert(f->directions < 8);
    f->dir_pin[f->directions] = pin;
    f->dir_value[f->directions] = value;
    f->directions++;
}
static int normal_level(void *p, uint32_t pin, uint32_t value)
{
    struct fake *f = p;
    assert(f->normals < 8);
    f->normal_pin[f->normals] = pin;
    f->normal_value[f->normals] = value;
    f->normals++;
    return f->normal_rc;
}
static int stbc_level(void *p, uint32_t pin, uint32_t value)
{
    struct fake *f = p;
    assert(f->stbc < 8);
    f->stbc_pin[f->stbc] = pin;
    f->stbc_value[f->stbc] = value;
    f->stbc++;
    return f->stbc_rc;
}
static int i2c_write(void *p, unsigned reg, const uint8_t *value)
{
    struct fake *f = p;
    (void)reg;
    (void)value;
    f->writes++;
    if (f->write_fail_at && f->writes >= f->write_fail_at)
        return f->write_rc;
    return 0;
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
    oa_amp_init_io io;
    oa_amp_init_state state;

    /* Defaults are the recovered board records. */
    memset(&board, 0xa5, sizeof board);
    oa_amp_board_defaults(&board);
    assert(board.reset.pin == 114 && board.reset.level_on == 1 &&
           board.reset.level_off == 0);
    assert(board.mute.pin == 208 && board.mute.level_on == 1 &&
           board.mute.level_off == 0);
    oa_amp_board_defaults(NULL);

    memset(&f, 0, sizeof f);
    oa_amp_board_io bio = {
        .context = &f,
        .direction = direction,
        .normal_level = normal_level,
        .stbc_level = stbc_level,
        .i2c_write = i2c_write,
        .delay_ms = delay_ms,
        .subwoofer_enabled = sub_enabled,
        .subwoofer_enable = sub_enable,
        .subwoofer_init = sub_init,
        .prelude = prelude
    };
    assert(oa_amp_board_init_io(&board, &bio, &io) == 0);
    assert(io.context == &board && io.prelude && io.gpio && io.direction &&
           io.level && io.delay_ms && io.write && io.subwoofer_enabled &&
           io.subwoofer_enable && io.subwoofer_init);

    memset(&state, 0, sizeof state);
    assert(oa_amp_init(&state, &profile, &io) == 1);
    assert(f.prelude == 1);
    assert(f.directions == 3);
    assert(f.dir_pin[0] == 208 && f.dir_pin[1] == 114 &&
           f.dir_pin[2] == 208);
    /* Initial reset drive (0,1) plus the two cached reset calls (0,1). */
    assert(f.normals == 4 && f.normal_pin[0] == 114 &&
           f.normal_value[0] == 0 && f.normal_value[1] == 1 &&
           f.normal_value[2] == 0 && f.normal_value[3] == 1);
    assert(f.stbc == 3 && f.stbc_pin[0] == 208 && f.stbc_value[0] == 0 &&
           f.stbc_value[1] == 0 && f.stbc_value[2] == 1);
    assert(f.writes == 38);
    assert(f.sub_enabled == 1 && f.sub_enable == 1 && f.sub_init == 1);
    assert(f.sub_enable_value == 0); /* sub_value 0 -> disabled */
    assert(state.reset_cached == 0 && state.mute_cached == 1);

    /* Native errno failures map to vendor false, not vendor true. */
    memset(&f, 0, sizeof f);
    f.normal_rc = -EIO;
    state = (oa_amp_init_state){ .reset_cached = 7, .mute_cached = 9 };
    assert(oa_amp_init(&state, &profile, &io) == 1);
    assert(f.normals == 4 && state.reset_cached == 7 &&
           state.mute_cached == 1);

    memset(&f, 0, sizeof f);
    f.stbc_rc = -EIO;
    state = (oa_amp_init_state){ .reset_cached = 7, .mute_cached = 9 };
    assert(oa_amp_init(&state, &profile, &io) == 1);
    assert(f.stbc == 3 && state.mute_cached == 9 &&
           state.reset_cached == 0);

    memset(&f, 0, sizeof f);
    f.write_fail_at = 1;
    f.write_rc = -EAGAIN;
    state = (oa_amp_init_state){ 0 };
    assert(oa_amp_init(&state, &profile, &io) == 0);
    assert(f.writes == 1 && f.stbc == 3 && f.stbc_value[2] == 1);
    assert(f.sub_enable == 0 && f.sub_init == 0);

    /* Missing adapter callbacks are false successes, not native fallbacks. */
    oa_amp_board_io empty = { .context = &f };
    assert(oa_amp_board_init_io(&board, &empty, &io) == 0);
    memset(&state, 0, sizeof state);
    assert(oa_amp_init(&state, &profile, &io) == 0);

    assert(oa_amp_board_init_io(NULL, &bio, &io) == -EINVAL);
    assert(oa_amp_board_init_io(&board, &bio, NULL) == -EINVAL);
    assert(oa_amp_board_init_io(&board, NULL, &io) == -EINVAL);

    puts("ok amp board: reset114/STBC208 mapping, errno->vendor boolean, "
         "write failure finish and cache/servo policy");
    return 0;
}
