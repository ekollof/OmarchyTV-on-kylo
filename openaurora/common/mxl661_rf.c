#include "openaurora/mxl661_rf.h"

#include <stddef.h>
#include <string.h>

static const uint8_t dvbt_props[] = {
    0x13, 0xff, 0x04, 0x0c, 0xff, 0x00, 0x53, 0xff, 0xfe, 0x57, 0xff, 0x91,
    0x62, 0xff, 0xc2, 0x6e, 0xff, 0x01, 0x6f, 0xff, 0x51, 0x87, 0xff, 0x77,
    0x88, 0xff, 0x55, 0x93, 0xff, 0x22, 0x97, 0xff, 0x02, 0xba, 0xff, 0x30,
    0x98, 0xff, 0xaf, 0x9b, 0xff, 0x20, 0x9c, 0xff, 0x1e, 0xa0, 0xff, 0x18,
    0xa5, 0xff, 0x09, 0xc2, 0xff, 0xa9, 0xc5, 0xff, 0x7c, 0xd5, 0xff, 0x03,
    0xd9, 0xff, 0x04, 0x00, 0x00, 0x00
};

static int wr(const oa_mxl661_ops *ops, void *ud, uint8_t addr, uint8_t reg,
              uint8_t value)
{
    return ops->write_reg(ud, addr, reg, value) != 0;
}

static int rd(const oa_mxl661_ops *ops, void *ud, uint8_t addr, uint8_t reg,
              uint8_t *value)
{
    *value = 0;
    return ops->read_reg(ud, addr, reg, value) != 0;
}

static void msleep(const oa_mxl661_ops *ops, void *ud, unsigned ms)
{
    if (ops->sleep_ms)
        ops->sleep_ms(ud, ms);
}

/* Ctrl_ProgramRegisters: 3-byte {reg, mask, value} entries, 00 00 00 ends. */
static int program_registers(const oa_mxl661_ops *ops, void *ud, uint8_t addr,
                             const uint8_t *table)
{
    int fail = 0;
    for (;;) {
        uint8_t reg = table[0];
        uint8_t mask = table[1];
        uint8_t value = table[2];
        if (reg == 0 && mask == 0 && value == 0)
            return fail;
        uint8_t old = 0;
        if (mask != 0xff) {
            if (rd(ops, ud, addr, reg, &old)) {
                fail = 1;
                return fail;
            }
        }
        uint8_t out = (uint8_t)((old & (uint8_t)~mask) | value);
        if (wr(ops, ud, addr, reg, out))
            fail = 1;
        table += 3;
    }
}

static int set_if_dvbt(const oa_mxl661_ops *ops, void *ud, uint8_t addr,
                       uint32_t if_khz)
{
    int fail = program_registers(ops, ud, addr, dvbt_props);
    uint8_t mode;
    if (if_khz < 0x89b2u) {
        fail |= wr(ops, ud, addr, 0x5a, 0xfe);
        fail |= wr(ops, ud, addr, 0x5b, 0x18);
        mode = 0xf1;
    } else {
        fail |= wr(ops, ud, addr, 0x5a, 0xd9);
        fail |= wr(ops, ud, addr, 0x5b, 0x16);
        mode = 0xb1;
    }
    fail |= wr(ops, ud, addr, 0x5c, mode);
    fail |= wr(ops, ud, addr, 0x8a, 0x00);
    if (fail)
        return -1; /* the vendor breaks out before the common tail */
    fail |= wr(ops, ud, addr, 0x6d, 0x0a);
    fail |= wr(ops, ud, addr, 0x03, 0x00);
    fail |= wr(ops, ud, addr, 0x03, 0x01);
    msleep(ops, ud, 50);
    return fail ? -1 : 0;
}

static int start_tune_dvbt(const oa_mxl661_ops *ops, void *ud, uint8_t addr,
                           const struct oa_mxl661_rf_params *p)
{
    int fail = 0;
    uint8_t mode;
    if (p->if_khz < 0x89b2u) {
        fail |= wr(ops, ud, addr, 0x5b, 0x18);
        mode = 0xf1;
    } else {
        fail |= wr(ops, ud, addr, 0x5b, 0x16);
        mode = 0xb1;
    }
    fail |= wr(ops, ud, addr, 0x5c, mode);
    fail |= wr(ops, ud, addr, 0x8a, 0x00);

    if (!p->start) {
        fail |= wr(ops, ud, addr, 0x7c,
                   p->freq_hz < 700000000u ? 0x1f : 0x9f);
        uint8_t v31 = (p->standard == 6 || p->standard == 9) ? 0xc1 : 0x81;
        fail |= wr(ops, ud, addr, 0x00, 0x01);
        fail |= wr(ops, ud, addr, 0x31, v31);
        fail |= wr(ops, ud, addr, 0x00, 0x00);
    }

    /* Ctrl_SetRfFreqLutReg with the constant DIG_TERR_FREQ_LUT value. */
    fail |= wr(ops, ud, addr, 0x49, 0x1c);
    fail |= wr(ops, ud, addr, 0xdb, 0x17);
    fail |= wr(ops, ud, addr, 0xea, 0x00);
    fail |= wr(ops, ud, addr, 0xeb, 0xd8);

    uint8_t v = 0;
    fail |= rd(ops, ud, addr, 0x0d, &v);
    fail |= wr(ops, ud, addr, 0x0d,
               (uint8_t)((v & 0xfe) | (p->start ? 1 : 0)));
    uint8_t f = (p->bandwidth - 0x23u < 2u) ? 0x20 : (uint8_t)p->bandwidth;
    fail |= wr(ops, ud, addr, 0x0f, f);

    uint32_t t = (uint32_t)(((uint64_t)p->freq_hz * 0x10624dd3u) >> 32);
    t &= 0xffffffc0u;
    t = (uint32_t)(((uint64_t)(t + 500u) * 0x10624dd3u) >> 32);
    fail |= wr(ops, ud, addr, 0x10, (uint8_t)((t & 0x3fff) >> 6));
    fail |= wr(ops, ud, addr, 0x11, (uint8_t)((t & 0x3fffff) >> 14));
    msleep(ops, ud, 30);
    return fail ? -1 : 0;
}

static int channel_tune(const oa_mxl661_ops *ops, void *ud, uint8_t addr)
{
    int fail = wr(ops, ud, addr, 0x12, 0x00);
    uint8_t v = 0;
    uint8_t v14 = 0;
    uint8_t v13 = 0;
    fail |= rd(ops, ud, addr, 0x6e, &v);
    fail |= wr(ops, ud, addr, 0x6e, (uint8_t)(v | 0x80));
    fail |= rd(ops, ud, addr, 0x14, &v14);
    fail |= wr(ops, ud, addr, 0x14, (uint8_t)(v14 & 0xdf));
    fail |= wr(ops, ud, addr, 0x12, 0x01);
    msleep(ops, ud, 10);
    fail |= rd(ops, ud, addr, 0x6e, &v);
    fail |= wr(ops, ud, addr, 0x6e, (uint8_t)(v & 0x7f));
    fail |= wr(ops, ud, addr, 0x14, (uint8_t)(v14 | 0x20));
    fail |= rd(ops, ud, addr, 0x13, &v13);
    fail |= wr(ops, ud, addr, 0x84, 0x83);
    fail |= wr(ops, ud, addr, 0x13, (uint8_t)(v13 & 0xfc));
    fail |= wr(ops, ud, addr, 0x13, (uint8_t)((v13 & 0xfc) | 1));
    msleep(ops, ud, 30);
    return fail ? -1 : 0;
}

int oa_mxl661_tune_dvbt(const oa_mxl661_ops *ops, void *ud, uint8_t addr,
                        const struct oa_mxl661_rf_params *params)
{
    if (!ops || !ops->write_reg || !ops->read_reg || !params)
        return -1;
    int fail = set_if_dvbt(ops, ud, addr, params->if_khz);
    fail |= start_tune_dvbt(ops, ud, addr, params);
    fail |= channel_tune(ops, ud, addr);
    return fail ? -1 : 0;
}
