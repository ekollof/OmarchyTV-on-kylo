/* MxL661 tuner init sequence, recovered from aurora.elf.
 *
 * SAL_TUNER_MxL661_Init @0x00872968 (called for SAL type 0xd):
 *   1. MxLWare601_API_ConfigDevice(SOFT_RESET)          -> reg 0xff = 0
 *   2. ConfigDevice(OVERWRITE_DEFAULT_CFG, config[0])   -> defaults table,
 *      reg 0 = 1, read reg 0x31, reg 0x31 |= 0xd0, reg 0 = 0, optional
 *      reg 0x0e = 4
 *   3. ConfigDevice(XTAL_SET_CFG)                       -> regs 1, 2, 0x6d,
 *      optional reg 0x0e = 0x14
 *   4. ConfigDevice(IF_OUT_CFG)                         -> read reg 4; if the
 *      reused buffer's param[4] (config[3]) is 1: reg 4 |= 0x20, regs 6/7 =
 *      divider; else reg 4 = (v & 0xc0) | config[6]; reg 5 = AGC/IF byte
 *   5. ConfigTuner(AGC_CFG)                             -> RMW regs 8, 9, 0x5e
 *   6. ConfigTuner(POWER_UP_CFG)                        -> reg 0x0b = 1
 *
 * The vendor's Ctrl_* helpers use the DEV IIC path:
 *   write: DEV_IIC_Write(0, 4, addr, 1, reg, 1, &value)
 *   read:  DEV_IIC_Write(0, 4, addr, 1, 0xfb, 1, &reg);
 *          DEV_IIC_Read(0, 4, addr, 0, 0x20000000, 1, value)
 * so the ops below expose register reads/writes at address `addr` (0xc0).
 * 0 = success, matching Ctrl_WriteRegister/Ctrl_ReadRegister.
 *
 * The defaults table is the 3-byte {reg, mask, value} stream
 * MxL_OverwriteDefaults @0x017683c4, terminated by 00 00 00. The vendor walk
 * reads the old register only when mask != 0xff, then writes
 * (old & ~mask) | value. A failed read/write stops the remaining table
 * entries, but SAL still proceeds with the later commands and ignores all
 * errors; this implementation preserves the order and returns a failure flag.
 */
#ifndef OPENAURORA_MXL661_H
#define OPENAURORA_MXL661_H
#include <stdint.h>

typedef struct {
    /* 0 = success. */
    int (*write_reg)(void *ud, uint8_t addr, uint8_t reg, uint8_t value);
    int (*read_reg)(void *ud, uint8_t addr, uint8_t reg, uint8_t *value);
} oa_mxl661_ops;

typedef struct {
    /* gTunerDefaultConfig @0x01368330. The init path uses indices 0..1,
     * 3..10; index 2 is not touched by these six commands. */
    uint32_t words[11];
} oa_mxl661_config;

extern const oa_mxl661_config oa_mxl661_default_config;

/* Run the six init commands in vendor order. Returns 0 only when every read
 * and write succeeded; 1 if any failed (the full sequence is still attempted,
 * except that a failed defaults-table entry stops the rest of that table). */
int oa_mxl661_init(const oa_mxl661_ops *ops, void *ud, uint8_t addr,
                   const oa_mxl661_config *cfg);

#endif
