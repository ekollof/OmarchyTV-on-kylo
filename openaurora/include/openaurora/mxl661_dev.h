/* DEV_IIC-level MxL661 register adapter.
 *
 * aurora.elf's Ctrl_WriteRegister/Ctrl_ReadRegister use its internal DEV_IIC
 * wrappers, which are not exported by a shared library (only the executable
 * defines DEV_IIC_Read/Write). The wrappers build a 24-byte Hal_Iic context
 * (see hal_iic.h) and:
 *
 *   write: DEV_IIC_Write(0, 4, addr, 1, reg, 1, &value)
 *   read:  DEV_IIC_Write(0, 4, addr, 1, 0xfb, 1, &reg)
 *          DEV_IIC_Read (0, 4, addr, 0, 0x20000000, 1, value)
 *
 * Those become Hal_Iic_Write/Read with subaddress length 1 (write, and the
 * 0xfb pointer write) or 0 (the read), timeout 400 and the 8-bit address.
 * The TV adapter supplies Hal_Iic_Write/Read; DEV_IIC's 1 = success is
 * inverted here to the MxL661 ops' 0 = success.
 */
#ifndef OPENAURORA_MXL661_DEV_H
#define OPENAURORA_MXL661_DEV_H
#include <stdint.h>

#include "openaurora/mxl661.h"

typedef struct {
    int (*hal_write)(const void *ctx, const void *value, unsigned len,
                     const void *buf, void *ud);
    int (*hal_read)(const void *ctx, const void *value, unsigned len,
                    void *buf, void *ud);
    void *ud;
} oa_mxl661_hal_ops;

/* oa_mxl661_ops signatures: 0 = success, nonzero = failure. */
int oa_mxl661_dev_write(void *ud, uint8_t addr, uint8_t reg, uint8_t value);
int oa_mxl661_dev_read(void *ud, uint8_t addr, uint8_t reg, uint8_t *value);

#endif
