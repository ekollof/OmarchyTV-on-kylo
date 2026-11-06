/* Exact Hal_Iic_Read/Write context recovered from the DEV_IIC wrappers.
 *
 * DEV_IIC_Read/Write and HAL_TUNER_ReadBase/WriteBase build a 24-byte context
 * before calling Hal_Iic_Read/Write:
 *
 *   +0x00  bus/device index (byte)
 *   +0x04  8-bit I2C address (word; _i2c_RdWr_Transfer shifts it to 7-bit)
 *   +0x08  subaddress length 0..4
 *   +0x0c  zero
 *   +0x10  timeout: 100 for DEV type 0, 400 for DEV type 4 / HAL_TUNER
 *   +0x14  zero
 *
 * The subaddress bytes are passed separately as the `value` argument; its
 * length is the field at +0x08, not the data length argument.
 */
#ifndef OPENAURORA_HAL_IIC_H
#define OPENAURORA_HAL_IIC_H
#include <stdint.h>

#define OA_HAL_IIC_TIMEOUT_NIM 100u
#define OA_HAL_IIC_TIMEOUT_TUNER 400u

typedef struct {
    uint32_t bus;
    uint32_t addr;
    uint32_t subaddr_len;
    uint32_t reserved0;
    uint32_t timeout;
    uint32_t reserved1;
} oa_hal_iic_ctx;

#endif
