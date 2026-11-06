#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/mxl661_rf.h"

struct fake {
    uint8_t regs[256];
    unsigned writes;
    unsigned reads;
    unsigned sleeps;
    unsigned sleep_total;
    int fail_write_at;
    int fail_read_at;
};

static int fw(void *ud, uint8_t addr, uint8_t reg, uint8_t value)
{
    struct fake *f = ud;
    assert(addr == 0xc0);
    f->writes++;
    if (f->fail_write_at == (int)f->writes)
        return -1;
    f->regs[reg] = value;
    return 0;
}

static int fr(void *ud, uint8_t addr, uint8_t reg, uint8_t *value)
{
    struct fake *f = ud;
    assert(addr == 0xc0);
    f->reads++;
    *value = f->regs[reg];
    if (f->fail_read_at == (int)f->reads)
        return -1;
    return 0;
}

static void fs(void *ud, unsigned ms)
{
    struct fake *f = ud;
    f->sleeps++;
    f->sleep_total += ms;
}

static const oa_mxl661_ops ops = { fw, fr, fs };

static void test_dvbt_666mhz(void)
{
    struct fake f;
    memset(&f, 0, sizeof f);
    const struct oa_mxl661_rf_params p = {
        .freq_hz = 666000000u,
        .standard = 8,
        .bandwidth = 0x20,
        .if_khz = 5000,
        .start = 0
    };
    assert(oa_mxl661_tune_dvbt(&ops, &f, 0xc0, &p) == 0);

    /* DigitalDvbt property table (0x13 is later RMW'd by channel tune). */
    assert(f.regs[0x0c] == 0x00 && f.regs[0x53] == 0xfe);
    assert(f.regs[0x57] == 0x91 && f.regs[0x62] == 0xc2);
    assert(f.regs[0x6e] == 0x01 && f.regs[0x6f] == 0x51);
    assert(f.regs[0x87] == 0x77 && f.regs[0x88] == 0x55);
    assert(f.regs[0x93] == 0x22 && f.regs[0x97] == 0x02);
    assert(f.regs[0xba] == 0x30 && f.regs[0x98] == 0xaf);
    assert(f.regs[0x9b] == 0x20 && f.regs[0x9c] == 0x1e);
    assert(f.regs[0xa0] == 0x18 && f.regs[0xa5] == 0x09);
    assert(f.regs[0xc2] == 0xa9 && f.regs[0xc5] == 0x7c);
    assert(f.regs[0xd5] == 0x03 && f.regs[0xd9] == 0x04);

    /* Case 9/0xd fixed writes. */
    assert(f.regs[0x5a] == 0xfe && f.regs[0x5b] == 0x18);
    assert(f.regs[0x5c] == 0xf1 && f.regs[0x8a] == 0x00);
    assert(f.regs[0x6d] == 0x0a && f.regs[0x03] == 0x01);
    assert(f.regs[0x7c] == 0x1f && f.regs[0x31] == 0x81);
    assert(f.regs[0x49] == 0x1c && f.regs[0xdb] == 0x17);
    assert(f.regs[0xea] == 0x00 && f.regs[0xeb] == 0xd8);
    assert(f.regs[0x0d] == 0x00 && f.regs[0x0f] == 0x20);
    assert(f.regs[0x10] == 0x80 && f.regs[0x11] == 0xa6);

    /* Case 0xb channel tune / lock sequence. */
    assert(f.regs[0x12] == 0x01);
    assert(f.regs[0x6e] == 0x01);
    assert(f.regs[0x14] == 0x20);
    assert(f.regs[0x84] == 0x83);
    assert(f.regs[0x13] == 0x05);
    assert(f.sleeps == 4 && f.sleep_total == 120);
}

static void test_high_if_and_failure(void)
{
    struct fake f;
    memset(&f, 0, sizeof f);
    const struct oa_mxl661_rf_params p = {
        .freq_hz = 474000000u,
        .standard = 8,
        .bandwidth = 0x21,
        .if_khz = 36650,
        .start = 1
    };
    assert(oa_mxl661_tune_dvbt(&ops, &f, 0xc0, &p) == 0);
    assert(f.regs[0x5a] == 0xd9 && f.regs[0x5b] == 0x16);
    assert(f.regs[0x5c] == 0xb1);
    /* start=1 skips the 0x7c/0x31 pair. */
    assert(f.regs[0x7c] == 0 && f.regs[0x31] == 0);
    assert(f.regs[0x0d] == 0x01);

    memset(&f, 0, sizeof f);
    f.fail_write_at = 3;
    assert(oa_mxl661_tune_dvbt(&ops, &f, 0xc0, &p) == -1);

    memset(&f, 0, sizeof f);
    f.fail_read_at = 1;
    assert(oa_mxl661_tune_dvbt(&ops, &f, 0xc0, &p) == -1);

    assert(oa_mxl661_tune_dvbt(NULL, &f, 0xc0, &p) == -1);
    assert(oa_mxl661_tune_dvbt(&ops, &f, 0xc0, NULL) == -1);
}

int main(void)
{
    test_dvbt_666mhz();
    test_high_if_and_failure();
    puts("ok mxl661 rf: DVB-T case 9/0xd/0xb order, DigitalDvbt table, "
         "spur registers, divider and lock sequence");
    return 0;
}
