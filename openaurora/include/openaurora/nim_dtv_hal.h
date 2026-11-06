/* Adapter from the NIM-DTV IIC step to the vendor HAL.
 *
 * The unnamed aurora helpers call
 *   DEV_IIC_Read/Write(0, 0, 0x80, 1, code, 1, &byte)
 * which packs {bus=0, reg=0x80} and the masked code value, then calls
 * Hal_Iic_Read/Write(bus, &value, length=1, buf). Hal returns 0 on success;
 * the DEV helper returns 1 on success and 0 on failure.
 *
 * The ops callbacks use the Hal convention (0 = success) so the TV build can
 * pass Hal_Iic_Read/Write directly; the adapter inverts to the DEV return.
 */
#ifndef OPENAURORA_NIM_DTV_HAL_H
#define OPENAURORA_NIM_DTV_HAL_H
#include <stdint.h>

typedef struct {
    int (*iic_read)(const void *bus, const void *value, unsigned len,
                    void *buf, void *ud);
    int (*iic_write)(const void *bus, const void *value, unsigned len,
                     void *buf, void *ud);
} oa_nim_dtv_iic_ops;

/* Convenience context for wiring oa_nim_dtv_ops.ir_read/ir_write. Pass this as
 * the state machine's ud; the HAL callbacks use iic->ops/hal_ud. */
typedef struct {
    const oa_nim_dtv_iic_ops *iic;
    void *hal_ud;
} oa_nim_dtv_hal;

/* DEV_IIC_* return convention: 1 = full success, 0 = failure. */
int oa_nim_dtv_iic_read(const oa_nim_dtv_iic_ops *ops, int code,
                        uint8_t *value, void *ud);
int oa_nim_dtv_iic_write(const oa_nim_dtv_iic_ops *ops, int code,
                         uint8_t value, void *ud);

/* Signatures matching oa_nim_dtv_ops.ir_read/ir_write; ud is oa_nim_dtv_hal*. */
int oa_nim_dtv_hal_read(int bus, int code, uint8_t *value, void *ud);
int oa_nim_dtv_hal_write(int bus, int code, uint8_t value, void *ud);

#endif
