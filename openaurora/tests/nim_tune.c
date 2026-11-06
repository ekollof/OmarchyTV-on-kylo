#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/nim_tune.h"

struct fake {
    int switch_calls;
    int last_switch;
    int tuner_type;
    int deliver_calls;
    uint32_t deliver_key;
    uint8_t deliver_value;
    int deliver_len;
    int fe_open_rc;
    int fe_open_calls;
    int fe_close_calls;
    int standby_calls;
    int standby_on;
    int last_fe;
    int status_calls;
    int status_rc;
    int last_handle;
    int last_index;
    uint32_t last_mask;
    uint8_t status[0x180];
    int rssi_calls;
    int rssi_rc;
    uint8_t rssi;
    int last_tuner;
    int program_calls;
    int program_rc;
    struct oa_nim_tune_channel program_ch;
};

static int switch_type(void *ud, int type)
{
    struct fake *f = ud;
    f->switch_calls++;
    f->last_switch = type;
    return 0;
}

static int get_tuner_type(void *ud)
{
    return ((struct fake *)ud)->tuner_type;
}

static int deliver_config(void *ud, uint32_t key, const void *data, int len)
{
    struct fake *f = ud;
    f->deliver_calls++;
    f->deliver_key = key;
    f->deliver_value = *(const uint8_t *)data;
    f->deliver_len = len;
    return 0;
}

static int fe_open(void *ud, int index)
{
    struct fake *f = ud;
    (void)index;
    f->fe_open_calls++;
    return f->fe_open_rc;
}

static void fe_close(void *ud, int fe)
{
    struct fake *f = ud;
    f->fe_close_calls++;
    f->last_fe = fe;
}

static int set_standby(void *ud, int fe, int on)
{
    struct fake *f = ud;
    f->standby_calls++;
    f->standby_on = on;
    f->last_fe = fe;
    return 0;
}

static int get_status(void *ud, int handle, int index, uint32_t mask, void *buf)
{
    struct fake *f = ud;
    f->status_calls++;
    f->last_handle = handle;
    f->last_index = index;
    f->last_mask = mask;
    if (f->status_rc == 0)
        memcpy(buf, f->status, sizeof f->status);
    return f->status_rc;
}

static int get_rssi(void *ud, int tuner_type, uint8_t *rssi)
{
    struct fake *f = ud;
    f->rssi_calls++;
    f->last_tuner = tuner_type;
    if (f->rssi_rc)
        *rssi = f->rssi;
    return f->rssi_rc;
}

static int program_tune(void *ud, const struct oa_nim_tune_channel *ch)
{
    struct fake *f = ud;
    f->program_calls++;
    f->program_ch = *ch;
    return f->program_rc;
}

static const struct oa_nim_tune_ops ops = {
    switch_type, get_tuner_type, deliver_config, fe_open, fe_close,
    set_standby, get_status, get_rssi, program_tune
};

static void test_path_index(void)
{
    struct oa_nim_tune_state s = { 0 };
    static const struct { int path, index; } map[] = {
        { 1, 0x14 }, { 2, 0x12 }, { 7, 0x12 }, { 5, 0x11 }, { 6, 0x0e },
        { 8, 0x10 }, { 3, -1 }, { 0, 0 }, { 4, 0 }
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++) {
        s.path = map[i].path;
        assert(oa_nim_tune_path_index(&s) == map[i].index);
    }
    assert(oa_nim_tune_path_index(NULL) == 0);
}

static void test_open(void)
{
    struct fake f = { .tuner_type = 0xd };
    struct oa_nim_tune_state s = { .path = 1 };

    assert(oa_nim_tune_open(&ops, &f, &s) == 1);
    assert(f.switch_calls == 1 && f.last_switch == 1);
    assert(f.deliver_calls == 1 && f.deliver_key == 0xa000 &&
           f.deliver_value == 0xc && f.deliver_len == 1);
    assert(s.tuner_type == 0xd);

    memset(&f, 0, sizeof f);
    f.tuner_type = 8;
    s.path = 2;
    assert(oa_nim_tune_open(&ops, &f, &s) == 1);
    assert(f.last_switch == 2 && f.deliver_value == 8);

    memset(&f, 0, sizeof f);
    f.tuner_type = 0xf;
    assert(oa_nim_tune_open(&ops, &f, &s) == 1);
    assert(f.deliver_calls == 0);
}

static void test_close_monitor(void)
{
    struct fake f = { .fe_open_rc = 4 };
    struct oa_nim_tune_state s = { 0 };
    assert(oa_nim_tune_close(&ops, &f, &s) == 1);
    assert(f.fe_open_calls == 1 && f.standby_calls == 1 &&
           f.standby_on == 1 && f.fe_close_calls == 1 && f.last_fe == 4);

    memset(&f, 0, sizeof f);
    f.fe_open_rc = -1;
    assert(oa_nim_tune_close(&ops, &f, &s) == 0);
    assert(f.fe_open_calls == 1 && f.standby_calls == 0 &&
           f.fe_close_calls == 0);

    assert(oa_nim_tune_monitor() == 1);
    assert(oa_nim_tune_disconnect() == 0);
}

static void test_lock_strength_quality(void)
{
    struct fake f = { 0 };
    struct oa_nim_tune_state s = { .handle = 9, .path = 1, .tuner_type = 0xd };
    uint8_t locked = 0;

    f.status[0] = 1;
    assert(oa_nim_tune_check_lock(&ops, &f, &s, &locked) == 1);
    assert(locked == 1 && f.last_handle == 9 && f.last_index == 0x14 &&
           f.last_mask == 2);

    f.status_rc = 1;
    assert(oa_nim_tune_check_lock(&ops, &f, &s, &locked) == 1);
    assert(locked == 0);

    memset(&f, 0, sizeof f);
    uint32_t value = 0x12345678;
    memcpy(f.status + 0x18, &value, sizeof value);
    uint32_t strength = 0;
    assert(oa_nim_tune_get_strength(&ops, &f, &s, &strength) == 1);
    assert(strength == 0x12345678 && f.last_index == 0x14 &&
           f.last_mask == 0x40);

    s.path = 2;
    f.rssi = 0x55;
    f.rssi_rc = 1;
    assert(oa_nim_tune_get_strength(&ops, &f, &s, &strength) == 1);
    assert(strength == 0x55 && f.rssi_calls == 1 && f.last_tuner == 0xd);
    f.rssi_rc = 0;
    assert(oa_nim_tune_get_strength(&ops, &f, &s, &strength) == 1);
    assert(strength == 0);

    memset(&f, 0, sizeof f);
    s.path = 1;
    value = 0xabcdef01;
    memcpy(f.status + 0x24, &value, sizeof value);
    uint32_t quality = 0;
    assert(oa_nim_tune_get_quality(&ops, &f, &s, &quality) == 1);
    assert(quality == 0xabcdef01 && f.last_mask == 0x80 &&
           f.last_index == 0x14);

    assert(oa_nim_tune_check_lock(NULL, &f, &s, &locked) == 0);
    assert(oa_nim_tune_get_strength(&ops, &f, &s, NULL) == 0);
    assert(oa_nim_tune_get_quality(&ops, &f, &s, NULL) == 0);
}

static void test_channel_mapping(void)
{
    struct fake f = { 0 };
    struct oa_nim_tune_state s = { .handle = 9, .path = 1, .tuner_type = 0xd };
    const struct oa_nim_tune_channel ch = { 666000000u, 8, 0x20, 5000 };
    assert(oa_nim_tune_set_channel(&ops, &f, &s, &ch) == 1);
    assert(f.program_calls == 1 && f.program_ch.freq_hz == 666000000u &&
           f.program_ch.standard == 8 && f.program_ch.bandwidth == 0x20 &&
           f.program_ch.if_khz == 5000);
    f.program_rc = -1;
    assert(oa_nim_tune_set_channel(&ops, &f, &s, &ch) == 0);
    assert(oa_nim_tune_set_channel(&ops, &f, &s, NULL) == 0);

    f.status[0] = 1;
    uint32_t value = 0x1234;
    memcpy(f.status + 0x18, &value, sizeof value);
    value = 0x5678;
    memcpy(f.status + 0x24, &value, sizeof value);
    struct oa_nim_tune_status st;
    assert(oa_nim_tune_get_channel_status(&ops, &f, &s, &st) == 1);
    assert(st.locked == 1 && st.strength == 0x1234 && st.quality == 0x5678);

    assert(oa_nim_tune_open(NULL, &f, &s) == 0);
    assert(oa_nim_tune_close(NULL, &f, &s) == 0);
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void test_raw_channel(void)
{
    uint8_t raw[0x5c];
    memset(raw, 0, sizeof raw);
    put32(raw + 0, 666000);
    put32(raw + 8, 0x20);
    put32(raw + 0x1c, 1); /* standard index 1 -> DVB-T 8 */
    struct oa_nim_tune_channel ch;
    assert(oa_nim_raw_channel_parse(raw, 1, &ch) == 1);
    assert(ch.freq_hz == 666000000u && ch.standard == 8 &&
           ch.bandwidth == 0x20);

    put32(raw + 0x58, 700000);
    assert(oa_nim_raw_channel_parse(raw, 1, &ch) == 1);
    assert(ch.freq_hz == 700000000u);

    put32(raw + 8, 0);
    assert(oa_nim_raw_channel_parse(raw, 1, &ch) == 1);
    assert(ch.bandwidth == 40000);

    put32(raw + 0x1c, 8);
    assert(oa_nim_raw_channel_parse(raw, 1, &ch) == 0);

    memset(raw, 0, sizeof raw);
    put32(raw + 0, 666000);
    put32(raw + 8, 0x20);
    put32(raw + 0x1c, 1);
    struct oa_nim_raw_channel rc;
    memcpy(rc.raw, raw, sizeof raw);
    struct fake f = { 0 };
    struct oa_nim_tune_state s = { .path = 1, .tuner_type = 0xd };
    assert(oa_nim_tune_set_channel_raw(&ops, &f, &s, &rc) == 1);
    assert(f.program_calls == 1 && f.program_ch.freq_hz == 666000000u &&
           f.program_ch.standard == 8 && f.program_ch.bandwidth == 0x20);
}

static void test_raw_status(void)
{
    uint8_t raw[0x180];
    memset(raw, 0, sizeof raw);
    raw[0] = 1;
    put32(raw + 0x18, 0x11223344u);
    put32(raw + 0x24, 0x55667788u);
    struct oa_nim_tune_status st;
    assert(oa_nim_raw_status_parse(raw, &st) == 1);
    assert(st.locked == 1 && st.strength == 0x11223344u &&
           st.quality == 0x55667788u);
    assert(oa_nim_raw_status_parse(NULL, &st) == 0);
}

static void test_status_cases(void)
{
    struct oa_nim_core_status c;
    memset(&c, 0, sizeof c);
    c.word[1] = 666000;
    c.word[12] = 0x11;
    c.word[13] = 0x22;
    uint32_t out[4];
    assert(oa_nim_status_type6(&c, out) == 1);
    assert(out[0] == 666000000u && out[1] == 0x11 && out[2] == 0x22 &&
           out[3] == 0);

    c.word[3] = 9;
    assert(oa_nim_status_type8(&c, out) == 1);
    assert(out[0] == 666000000u && out[3] == 1);
    c.word[3] = 11;
    assert(oa_nim_status_type8(&c, out) == 1 && out[3] == 2);
    c.word[3] = 0;
    assert(oa_nim_status_type8(&c, out) == 1 && out[3] == 0xff);

    assert(oa_nim_status_type6(NULL, out) == 0);
    assert(oa_nim_status_type8(&c, NULL) == 0);
}

static void test_status_type1(void)
{
    struct oa_nim_core_status c;
    memset(&c, 0, sizeof c);
    c.word[0] = 0x11;
    c.word[2] = 0x22;
    uint32_t out[11];
    assert(oa_nim_status_type1(&c, out) == 1);
    assert(out[1] == 0x11 && out[2] == 0x22 && out[9] == 1);
    assert(out[0] == 0 && out[3] == 0 && out[4] == 0 && out[5] == 0 &&
           out[6] == 0 && out[7] == 0 && out[8] == 0 && out[10] == 0);
    c.word[10] = 0x00010000u; /* byte +42 nonzero */
    assert(oa_nim_status_type1(&c, out) == 1 && out[9] == 9);
    assert(oa_nim_status_type1(NULL, out) == 0);
}

static void test_binding(void)
{
    struct fake f = { .tuner_type = 0xd };
    struct oa_nim_tune_binding b = {
        .ops = &ops,
        .ud = &f,
        .state = { .handle = 9, .path = 1, .tuner_type = 0xd }
    };
    f.status[0] = 1;
    uint32_t value = 0x11223344;
    memcpy(f.status + 0x18, &value, sizeof value);
    value = 0x55667788;
    memcpy(f.status + 0x24, &value, sizeof value);

    assert(oa_nim_tune_hal_open(&b) == 1);
    assert(f.last_switch == 1 && f.deliver_value == 0xc);
    assert(oa_nim_tune_hal_monitor(&b) == 1);
    assert(oa_nim_tune_hal_check_lock(&b) == 1 && b.locked == 1);
    assert(oa_nim_tune_hal_get_strength(&b) == 1 &&
           b.strength == 0x11223344);
    assert(oa_nim_tune_hal_get_quality(&b) == 1 && b.quality == 0x55667788);
    assert(oa_nim_tune_hal_set_channel(&b) == -ENOTSUP);
    assert(oa_nim_tune_hal_get_channel_status(&b) == -ENOTSUP);
    assert(oa_nim_tune_hal_disconnect(&b) == 0);
    assert(oa_nim_tune_hal_close(&b) == 1);
}

int main(void)
{
    test_path_index();
    test_open();
    test_close_monitor();
    test_lock_strength_quality();
    test_channel_mapping();
    test_raw_channel();
    test_raw_status();
    test_status_cases();
    test_status_type1();
    test_binding();
    puts("ok nim tune: path/index map, open/close, lock/strength/quality "
         "masks and unresolved channel mappings");
    return 0;
}
