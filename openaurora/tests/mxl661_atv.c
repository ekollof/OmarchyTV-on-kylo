/* MxL661 analog (ATV) sequence. The synthetic fixtures prove the case-9/0xd/
 * 0xb order and LUT selection; the optional argv[1] path loads the real
 * vendor tables from the private aurora.elf (never compiled in). */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/mxl661_atv.h"

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

static const uint8_t props[] = { 0x13, 0xff, 0x04, 0x00, 0x00, 0x00 };
static const uint8_t regs[] = { 0x4a, 0xff, 0x1c, 0x00, 0x00, 0x00 };
static const uint32_t lut_default[] = { 1, 0xd800171c, 0, 0 };
static const uint32_t lut_24m[] = { 1, 0x9000171d, 0, 0 };
static const uint32_t lut_band[] = {
    1, 0xd800171c, 666000000u, 0x11223344u, 0, 0
};

static void tables_synthetic(struct oa_mxl661_atv_tables *t)
{
    memset(t, 0, sizeof *t);
    const uint8_t *p = props;
    const uint8_t *r = regs;
    const uint32_t *l = lut_default;
    t->ntsc_props = p;
    t->pal_props = p;
    t->secam_props = p;
    t->ntsc_16m_regs = r;
    t->ntsc_24m_regs = r;
    t->ntsc_hrc_regs = r;
    t->ntsc_hif_regs = r;
    t->pal_bg_7_hif_regs = r;
    t->pal_bg_8_hif_regs = r;
    t->pal_d_hif_regs = r;
    t->pal_i_hif_regs = r;
    t->pal_lif_regs = r;
    t->ntsc_lif_lut = l;
    t->ntsc_hif_lut = l;
    t->pal_bg_7_16hif_lut = l;
    t->pal_bg_7_16lif_lut = l;
    t->pal_bg_8_16hif_lut = l;
    t->pal_bg_8_16lif_lut = l;
    t->pal_bg_7_24_lut = l;
    t->pal_bg_8_24_lut = l;
    t->pal_i_24_lut = l;
    t->pal_i_16hif_lut = l;
    t->pal_i_16lif_lut = l;
    t->pal_d_24_lut = l;
    t->pal_d_16hif_lut = l;
    t->pal_d_16lif_lut = l;
    t->secam_l_24_lut = l;
    t->secam_l_16_lut = l;
}

static void tune(struct fake *f, const struct oa_mxl661_atv_tables *t,
                 int standard, uint32_t freq, uint32_t if_khz, int xtal,
                 int start)
{
    struct oa_mxl661_analog_params p = {
        .freq_hz = freq,
        .if_khz = if_khz,
        .bandwidth = 0,
        .standard = standard,
        .start = start,
        .xtal_24mhz = xtal
    };
    assert(oa_mxl661_tune_analog(&ops, f, 0xc0, &p, t) == 0);
}

static void test_ntsc_lif(void)
{
    struct fake f;
    struct oa_mxl661_atv_tables t;
    memset(&f, 0, sizeof f);
    tables_synthetic(&t);
    tune(&f, &t, OA_MXL661_ATV_NTSC, 666000000u, 5000, 0, 0);

    assert(f.regs[0x13] == 0x05); /* props 4, then channel tune sets bit0 */
    assert(f.regs[0x5a] == 0x7e && f.regs[0x5b] == 0x10);
    assert(f.regs[0x8a] == 0x00 && f.regs[0xdd] == 0x00);
    assert(f.regs[0x4a] == 0x1c);
    assert(f.regs[0x7c] == 0x1f && f.regs[0x31] == 0x81);
    assert(f.regs[0x49] == 0x1c && f.regs[0xdb] == 0x17);
    assert(f.regs[0xea] == 0x00 && f.regs[0xeb] == 0xd8);
    assert(f.regs[0x0d] == 0x00 && f.regs[0x0f] == 0x02);
    assert(f.regs[0x10] == 0x80 && f.regs[0x11] == 0xa6);
    assert(f.regs[0x6d] == 0x80);
    assert(f.regs[0x12] == 0x01 && f.regs[0x14] == 0x20);
    assert(f.regs[0x84] == 0x83);
    assert(f.sleep_total == 120);
}

static void test_ntsc_hif_and_band(void)
{
    struct fake f;
    struct oa_mxl661_atv_tables t;
    memset(&f, 0, sizeof f);
    tables_synthetic(&t);
    t.ntsc_hif_lut = lut_band;
    tune(&f, &t, OA_MXL661_ATV_NTSC, 666000000u, 36650, 0, 0);
    assert(f.regs[0x5a] == 0xd9 && f.regs[0x5b] == 0x16);
    assert(f.regs[0x49] == 0x44 && f.regs[0xdb] == 0x33);
    assert(f.regs[0xea] == 0x22 && f.regs[0xeb] == 0x11);

    memset(&f, 0, sizeof f);
    tune(&f, &t, OA_MXL661_ATV_NTSC, 600000000u, 36650, 0, 0);
    assert(f.regs[0x49] == 0x1c && f.regs[0xeb] == 0xd8);
}

static void test_pal_d(void)
{
    struct fake f;
    struct oa_mxl661_atv_tables t;
    memset(&f, 0, sizeof f);
    tables_synthetic(&t);
    tune(&f, &t, OA_MXL661_ATV_PAL_D, 300000000u, 5000, 0, 0);
    assert(f.regs[0x0c] == 0x00);
    assert(f.regs[0x93] == 0x44 && f.regs[0x94] == 0x55);
    assert(f.regs[0xba] == 0x50 && f.regs[0x5d] == 0x0f);
    assert(f.regs[0x10] == 0x00 && f.regs[0x11] == 0x4b);

    memset(&f, 0, sizeof f);
    tune(&f, &t, OA_MXL661_ATV_PAL_D, 400000000u, 5000, 0, 0);
    assert(f.regs[0x93] == 0x33 && f.regs[0x94] == 0x44);
    assert(f.regs[0xba] == 0x40 && f.regs[0x5d] == 0x0e);
}

static void test_pal_i_and_secam(void)
{
    struct fake f;
    struct oa_mxl661_atv_tables t;
    memset(&f, 0, sizeof f);
    tables_synthetic(&t);
    tune(&f, &t, OA_MXL661_ATV_PAL_I, 474000000u, 5000, 0, 0);
    assert(f.regs[0x0c] == 0x0b && f.regs[0x8a] == 0x28);

    memset(&f, 0, sizeof f);
    tune(&f, &t, OA_MXL661_ATV_SECAM_L, 474000000u, 5000, 0, 0);
    assert(f.regs[0x0c] == 0x0c && f.regs[0x8a] == 0x00);

    memset(&f, 0, sizeof f);
    tune(&f, &t, OA_MXL661_ATV_SECAM, 474000000u, 5000, 0, 0);
    assert(f.regs[0x0c] == 0x0b);
}

static void test_xtal_and_pal_1a(void)
{
    struct fake f;
    struct oa_mxl661_atv_tables t;
    memset(&f, 0, sizeof f);
    tables_synthetic(&t);
    t.ntsc_lif_lut = lut_24m;
    tune(&f, &t, OA_MXL661_ATV_NTSC, 474000000u, 5000, 1, 0);
    assert(f.regs[0xdd] == 0x01);
    assert(f.regs[0xeb] == 0x90); /* 24 MHz default image */

    memset(&f, 0, sizeof f);
    f.regs[0x1a] = 0x04;
    f.regs[0x58] = 0xff;
    f.regs[0x57] = 0xff;
    tune(&f, &t, OA_MXL661_ATV_PAL, 474000000u, 5000, 0, 0);
    assert(f.regs[0x58] == 0xfb && f.regs[0x57] == 0xf7);
    assert(f.regs[0x69] == 0x00); /* case 9 writes 1, case 0xd PAL writes 0 */
    assert(f.regs[0x0c] == 0x0a);
}

static void test_failures(void)
{
    struct fake f;
    struct oa_mxl661_atv_tables t;
    struct oa_mxl661_analog_params p = {
        .freq_hz = 474000000u,
        .if_khz = 5000,
        .standard = OA_MXL661_ATV_PAL
    };
    memset(&f, 0, sizeof f);
    tables_synthetic(&t);
    f.fail_write_at = 1;
    assert(oa_mxl661_tune_analog(&ops, &f, 0xc0, &p, &t) == -1);

    memset(&f, 0, sizeof f);
    f.fail_read_at = 1;
    assert(oa_mxl661_tune_analog(&ops, &f, 0xc0, &p, &t) == -1);

    assert(oa_mxl661_tune_analog(NULL, &f, 0xc0, &p, &t) == -1);
    assert(oa_mxl661_tune_analog(&ops, &f, 0xc0, NULL, &t) == -1);
    assert(oa_mxl661_tune_analog(&ops, &f, 0xc0, &p, NULL) == -1);
    p.standard = 9;
    assert(oa_mxl661_tune_analog(&ops, &f, 0xc0, &p, &t) == -1);

    assert(oa_mxl661_atv_load("/nonexistent/aurora.elf", &t) == -1);
    assert(oa_mxl661_atv_load(NULL, &t) == -1);
    assert(oa_mxl661_atv_load("/nonexistent/aurora.elf", NULL) == -1);
}

/* Explicit corpus run: uses the private aurora.elf the caller supplies. */
static void test_corpus(const char *elf)
{
    struct fake f;
    struct oa_mxl661_atv_tables t;
    assert(oa_mxl661_atv_load(elf, &t) == 0);

    memset(&f, 0, sizeof f);
    tune(&f, &t, OA_MXL661_ATV_NTSC, 666000000u, 5000, 0, 0);
    assert(f.regs[0x0c] == 0x08 && f.regs[0x13] == 0x01);
    assert(f.regs[0x4a] == 0x1c); /* RF HRC table overwrites case 9 */
    assert(f.regs[0x49] == 0x1c && f.regs[0xeb] == 0xd8);

    memset(&f, 0, sizeof f);
    tune(&f, &t, OA_MXL661_ATV_PAL, 474000000u, 5000, 0, 0);
    assert(f.regs[0x13] == 0x01 && f.regs[0x0c] == 0x0a);
    assert(f.regs[0x49] == 0x1d && f.regs[0xeb] == 0xd8);

    memset(&f, 0, sizeof f);
    tune(&f, &t, OA_MXL661_ATV_PAL_D, 300000000u, 5000, 1, 0);
    assert(f.regs[0x49] == 0x1d && f.regs[0xeb] == 0x90);
    assert(f.regs[0x5d] == 0x0f);
    puts("  ok   mxl661 atv corpus: loader CRC + real NTSC/PAL/PAL-D sequences");
}

int main(int argc, char **argv)
{
    test_ntsc_lif();
    test_ntsc_hif_and_band();
    test_pal_d();
    test_pal_i_and_secam();
    test_xtal_and_pal_1a();
    test_failures();
    if (argc > 1)
        test_corpus(argv[1]);
    puts("ok mxl661 atv: analog case 9/0xd/0xb, LUT selection, PAL/SECAM/NTSC");
    return 0;
}
