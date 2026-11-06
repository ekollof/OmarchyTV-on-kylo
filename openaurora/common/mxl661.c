#include "openaurora/mxl661.h"

#include <stddef.h>

const oa_mxl661_config oa_mxl661_default_config = {
    { 1, 0, 0x0b, 0, 0, 0, 6, 1, 0, 1, 1 }
};

static const uint8_t overwrite_defaults[] = {
    0x00, 0xff, 0x01,
    0x96, 0xff, 0x00,
    0x00, 0xff, 0x00,
    0x14, 0xff, 0x13,
    0x6d, 0xff, 0x8a,
    0x6d, 0xff, 0x0a,
    0xdf, 0xff, 0x19,
    0x45, 0xff, 0x1b,
    0xa9, 0xff, 0x59,
    0xaa, 0xff, 0x6a,
    0xbe, 0xff, 0x4c,
    0xcf, 0xff, 0x2a,
    0xd2, 0xff, 0xf5,
    0xd0, 0xff, 0x34,
    0x77, 0xff, 0xe7,
    0x78, 0xff, 0xe3,
    0x6f, 0xff, 0x51,
    0x7b, 0xff, 0x84,
    0x7c, 0xff, 0x9f,
    0x56, 0xff, 0x41,
    0xcd, 0xff, 0x64,
    0xc3, 0xff, 0x2c,
    0x9d, 0xff, 0x61,
    0xf7, 0xff, 0x52,
    0x58, 0xff, 0x81,
    0xb1, 0xff, 0x30,
    0x00, 0x00, 0x00
};

/* Ctrl_ProgramRegisters @0x008822dc. */
static int program_defaults(const oa_mxl661_ops *ops, void *ud, uint8_t addr)
{
    const uint8_t *t = overwrite_defaults;
    for (;;) {
        uint8_t reg = t[0];
        uint8_t mask = t[1];
        uint8_t value = t[2];
        if (reg == 0 && mask == 0 && value == 0)
            return 0;
        uint8_t old = 0;
        if (mask != 0xff) {
            if (ops->read_reg(ud, addr, reg, &old) != 0)
                return 1;
        }
        uint8_t out = (uint8_t)((old & (uint8_t)~mask) | value);
        if (ops->write_reg(ud, addr, reg, out) != 0)
            return 1;
        t += 3;
    }
}

static int wr(const oa_mxl661_ops *ops, void *ud, uint8_t addr, uint8_t reg,
              uint8_t value)
{
    return ops->write_reg(ud, addr, reg, value) != 0;
}

static int rd(const oa_mxl661_ops *ops, void *ud, uint8_t addr, uint8_t reg,
              uint8_t *value)
{
    return ops->read_reg(ud, addr, reg, value) != 0;
}

int oa_mxl661_init(const oa_mxl661_ops *ops, void *ud, uint8_t addr,
                   const oa_mxl661_config *cfg)
{
    if (!ops || !ops->write_reg || !ops->read_reg || !cfg)
        return 1;

    int fail = 0;
    uint8_t v;

    /* 1. MXL_DEV_SOFT_RESET_CFG -> Ctrl_WriteRegister(addr, 0xff, 0). */
    fail |= wr(ops, ud, addr, 0xff, 0x00);

    /* 2. MXL_DEV_OVERWRITE_DEFAULT_CFG. */
    fail |= program_defaults(ops, ud, addr);
    fail |= wr(ops, ud, addr, 0x00, 0x01);
    v = 0; /* Ctrl_ReadRegister initializes local_2d to 0 before the read. */
    fail |= rd(ops, ud, addr, 0x31, &v);
    fail |= wr(ops, ud, addr, 0x31, (uint8_t)(v | 0xd0));
    fail |= wr(ops, ud, addr, 0x00, 0x00);
    if (cfg->words[0] == 1)
        fail |= wr(ops, ud, addr, 0x0e, 0x04);

    /* 3. MXL_DEV_XTAL_SET_CFG. */
    uint8_t reg1 = (uint8_t)(((cfg->words[3] << 7) | (cfg->words[1] << 5)) &
                             0xffu);
    reg1 = (uint8_t)(reg1 | (cfg->words[8] & 0x1fu));
    fail |= wr(ops, ud, addr, 0x01, reg1);
    uint8_t reg2 = (uint8_t)((cfg->words[4] & 1u) |
                             (cfg->words[5] == 1 ? 0x40u : 0u));
    fail |= wr(ops, ud, addr, 0x02, reg2);
    fail |= wr(ops, ud, addr, 0x6d,
               cfg->words[5] == 1 ? 0x80 : 0x0a);
    if (cfg->words[0] == 1)
        fail |= wr(ops, ud, addr, 0x0e, 0x14);

    /* 4. MXL_DEV_IF_OUT_CFG. SAL reuses the command buffer, so param[4] (the
     * IF-path-enable check) still holds config[3] from the XTAL command:
     * config[3] == 1 takes the reg6/reg7 branch. The shipped default has
     * config[3] = 0, which is the else path. */
    v = 0;
    fail |= rd(ops, ud, addr, 0x04, &v);
    if (cfg->words[3] == 1) {
        fail |= wr(ops, ud, addr, 0x04, (uint8_t)(v | 0x20));
        uint32_t div = ((cfg->words[8] & 0x7ffffu) << 7) / 0xd2fu;
        fail |= wr(ops, ud, addr, 0x06, (uint8_t)(div & 0xffu));
        fail |= wr(ops, ud, addr, 0x07, (uint8_t)((div & 0xfffu) >> 8));
    } else {
        fail |= wr(ops, ud, addr, 0x04,
                   (uint8_t)((v & 0xc0) | (cfg->words[6] & 0xffu)));
    }
    uint32_t if_byte = (((cfg->words[7] & 3u) << 4) | 0x0bu) +
                       (cfg->words[8] == 1 ? 0xc0u : 0u);
    fail |= wr(ops, ud, addr, 0x05, (uint8_t)(if_byte & 0xffu));

    /* 5. MXL_TUNER_AGC_CFG. SAL passes config[10], config[9], 0x42, 0. */
    v = 0;
    fail |= rd(ops, ud, addr, 0x08, &v);
    fail |= wr(ops, ud, addr, 0x08,
               (uint8_t)((v & 0xf2) | (cfg->words[10] & 0xffu) |
                         ((cfg->words[9] & 0xffu) << 2)));
    v = 0;
    fail |= rd(ops, ud, addr, 0x09, &v);
    fail |= wr(ops, ud, addr, 0x09, (uint8_t)((v & 0x80) | 0x42));
    v = 0;
    fail |= rd(ops, ud, addr, 0x5e, &v);
    fail |= wr(ops, ud, addr, 0x5e, (uint8_t)(v & 0xef)); /* (param[5]&0xf)<<4, param[5]=0 */

    /* 6. MXL_TUNER_POWER_UP_CFG -> Ctrl_WriteRegister(addr, 0x0b, 1). */
    fail |= wr(ops, ud, addr, 0x0b, 0x01);

    return fail ? 1 : 0;
}
