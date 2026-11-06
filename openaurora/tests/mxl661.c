#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/mxl661.h"

#define MAX_EV 128

struct ev {
    uint8_t addr;
    uint8_t reg;
    uint8_t value;
    int is_read;
};

struct fake {
    uint8_t regs[256];
    struct ev ev[MAX_EV];
    unsigned n;
    unsigned writes;
    unsigned reads;
    int write_fail_at; /* 1-based write index, 0 = never */
    int read_fail_at;  /* 1-based read index, 0 = never */
};

static int fake_write(void *ud, uint8_t addr, uint8_t reg, uint8_t value)
{
    struct fake *f = ud;
    assert(f->n < MAX_EV);
    f->ev[f->n++] = (struct ev){ addr, reg, value, 0 };
    f->writes++;
    if (f->write_fail_at == (int)f->writes)
        return -1;
    f->regs[reg] = value;
    return 0;
}

static int fake_read(void *ud, uint8_t addr, uint8_t reg, uint8_t *value)
{
    struct fake *f = ud;
    assert(f->n < MAX_EV);
    f->ev[f->n++] = (struct ev){ addr, reg, f->regs[reg], 1 };
    f->reads++;
    if (f->read_fail_at == (int)f->reads)
        return -1;
    *value = f->regs[reg];
    return 0;
}

static const oa_mxl661_ops fake_ops = { fake_write, fake_read, NULL };

static void init_nonzero_regs(struct fake *f)
{
    memset(f, 0, sizeof(*f));
    f->regs[0x31] = 0x2f;
    f->regs[0x04] = 0xff;
    f->regs[0x08] = 0xff;
    f->regs[0x09] = 0xff;
    f->regs[0x5e] = 0xff;
}

/* Exact vendor order for the default config. */
static const uint8_t expect_writes[41][2] = {
    { 0xff, 0x00 }, { 0x00, 0x01 }, { 0x96, 0x00 }, { 0x00, 0x00 },
    { 0x14, 0x13 }, { 0x6d, 0x8a }, { 0x6d, 0x0a }, { 0xdf, 0x19 },
    { 0x45, 0x1b }, { 0xa9, 0x59 }, { 0xaa, 0x6a }, { 0xbe, 0x4c },
    { 0xcf, 0x2a }, { 0xd2, 0xf5 }, { 0xd0, 0x34 }, { 0x77, 0xe7 },
    { 0x78, 0xe3 }, { 0x6f, 0x51 }, { 0x7b, 0x84 }, { 0x7c, 0x9f },
    { 0x56, 0x41 }, { 0xcd, 0x64 }, { 0xc3, 0x2c }, { 0x9d, 0x61 },
    { 0xf7, 0x52 }, { 0x58, 0x81 }, { 0xb1, 0x30 }, { 0x00, 0x01 },
    { 0x31, 0xff }, { 0x00, 0x00 }, { 0x0e, 0x04 }, { 0x01, 0x00 },
    { 0x02, 0x00 }, { 0x6d, 0x0a }, { 0x0e, 0x14 }, { 0x04, 0xc6 },
    { 0x05, 0x1b }, { 0x08, 0xf7 }, { 0x09, 0xc2 }, { 0x5e, 0xef },
    { 0x0b, 0x01 }
};
static const uint8_t expect_reads[5] = { 0x31, 0x04, 0x08, 0x09, 0x5e };

static void check_log(const struct fake *f)
{
    unsigned wi = 0;
    unsigned ri = 0;
    for (unsigned i = 0; i < f->n; i++) {
        assert(f->ev[i].addr == 0xc0);
        if (f->ev[i].is_read) {
            assert(ri < 5 && f->ev[i].reg == expect_reads[ri]);
            ri++;
        } else {
            assert(wi < 41 && f->ev[i].reg == expect_writes[wi][0]);
            assert(f->ev[i].value == expect_writes[wi][1]);
            wi++;
        }
    }
    assert(wi == 41 && ri == 5);
}

static void test_default_sequence(void)
{
    struct fake f;
    init_nonzero_regs(&f);
    assert(oa_mxl661_init(&fake_ops, &f, 0xc0, &oa_mxl661_default_config) == 0);
    assert(f.writes == 41 && f.reads == 5);
    check_log(&f);

    /* Final register file, including the non-zero read-modify-write cases. */
    assert(f.regs[0x31] == 0xff);
    assert(f.regs[0x04] == 0xc6);
    assert(f.regs[0x05] == 0x1b);
    assert(f.regs[0x08] == 0xf7);
    assert(f.regs[0x09] == 0xc2);
    assert(f.regs[0x5e] == 0xef);
    assert(f.regs[0x0b] == 0x01);
    assert(f.regs[0x0e] == 0x14); /* last write wins */
}

static void test_custom_config(void)
{
    oa_mxl661_config cfg = oa_mxl661_default_config;
    cfg.words[0] = 0;  /* no 0x0e writes */
    cfg.words[1] = 2;  /* xtal freq sel */
    cfg.words[3] = 1;  /* clk out enable -> IF-out reg6/reg7 branch */
    cfg.words[4] = 1;  /* clk out div */
    cfg.words[5] = 1;  /* xtal sharing -> 0x80 and reg2 bit 6 */
    cfg.words[6] = 9;  /* IF path (else branch only) */
    cfg.words[7] = 2;  /* AGC type */
    cfg.words[8] = 3;  /* xtal cap / IF-out divider */
    cfg.words[9] = 2;  /* AGC id */
    cfg.words[10] = 0; /* AGC type */
    struct fake f;
    init_nonzero_regs(&f);
    assert(oa_mxl661_init(&fake_ops, &f, 0xc0, &cfg) == 0);

    assert(f.regs[0x01] == 0xc3);
    assert(f.regs[0x02] == 0x41);
    assert(f.regs[0x6d] == 0x80); /* cmd2 overrides the defaults table */
    /* IF-out takes the vendor's reg6/reg7 branch; 0xff | 0x20 = 0xff and
     * (3 << 7) / 0xd2f = 0. */
    assert(f.regs[0x04] == 0xff);
    assert(f.regs[0x06] == 0x00);
    assert(f.regs[0x07] == 0x00);
    assert(f.regs[0x05] == 0x2b);
    assert(f.regs[0x08] == 0xfa);
    assert(f.regs[0x09] == 0xc2);
    assert(f.regs[0x5e] == 0xef);
    assert(f.regs[0x0e] == 0x00); /* never written */
    for (unsigned i = 0; i < f.n; i++)
        if (!f.ev[i].is_read)
            assert(!(f.ev[i].reg == 0x0e && f.ev[i].value != 0));
    assert(f.writes == 41 && f.reads == 5);
}

static void test_write_failure_stops_table_only(void)
{
    struct fake f;
    init_nonzero_regs(&f);
    f.write_fail_at = 2; /* 0x96 = 0 in the defaults table */
    assert(oa_mxl661_init(&fake_ops, &f, 0xc0, &oa_mxl661_default_config) == 1);
    assert(f.writes == 16 && f.reads == 5);
    /* The remaining table entries are skipped ... */
    for (unsigned i = 0; i < f.n; i++)
        assert(f.ev[i].reg != 0x14 && f.ev[i].reg != 0xdf);
    /* ... but the later commands still run. */
    int saw_post = 0;
    for (unsigned i = 2; i < f.n; i++)
        if (!f.ev[i].is_read && f.ev[i].reg == 0x00 && f.ev[i].value == 0x01)
            saw_post = 1;
    assert(saw_post);
    assert(f.regs[0x31] == 0xff); /* read 0x2f, write |0xd0 */
}

static void test_read_failure_still_writes(void)
{
    struct fake f;
    init_nonzero_regs(&f);
    f.read_fail_at = 1; /* reg 0x31 */
    assert(oa_mxl661_init(&fake_ops, &f, 0xc0, &oa_mxl661_default_config) == 1);
    assert(f.writes == 41 && f.reads == 5);
    assert(f.regs[0x31] == 0xd0); /* v stayed 0 */

    init_nonzero_regs(&f);
    f.read_fail_at = 5; /* reg 0x5e */
    assert(oa_mxl661_init(&fake_ops, &f, 0xc0, &oa_mxl661_default_config) == 1);
    assert(f.writes == 41 && f.reads == 5);
    assert(f.regs[0x5e] == 0x00);
}

static void test_write_failure_late_and_last(void)
{
    struct fake f;
    init_nonzero_regs(&f);
    f.write_fail_at = 29; /* 0x31 write after a successful read */
    assert(oa_mxl661_init(&fake_ops, &f, 0xc0, &oa_mxl661_default_config) == 1);
    assert(f.writes == 41 && f.reads == 5);
    assert(f.regs[0x31] == 0x2f); /* failed write did not update */

    init_nonzero_regs(&f);
    f.write_fail_at = 41; /* power-up */
    assert(oa_mxl661_init(&fake_ops, &f, 0xc0, &oa_mxl661_default_config) == 1);
    assert(f.writes == 41 && f.reads == 5);
    assert(f.regs[0x0b] == 0x00);
}

static void test_bad_args(void)
{
    struct fake f;
    init_nonzero_regs(&f);
    assert(oa_mxl661_init(NULL, &f, 0xc0, &oa_mxl661_default_config) == 1);
    assert(oa_mxl661_init(&fake_ops, &f, 0xc0, NULL) == 1);
}

int main(void)
{
    test_default_sequence();
    test_custom_config();
    test_write_failure_stops_table_only();
    test_read_failure_still_writes();
    test_write_failure_late_and_last();
    test_bad_args();
    puts("ok mxl661: default/custom init order, RMW masks, defaults table and "
         "failure behavior");
    return 0;
}
