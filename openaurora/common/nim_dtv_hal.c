#include "openaurora/nim_dtv_hal.h"

#include <stddef.h>

#include "openaurora/nim_dtv.h"

struct iic_bus {
    uint32_t bus;
    uint32_t reg;
};

int oa_nim_dtv_iic_read(const oa_nim_dtv_iic_ops *ops, int code,
                        uint8_t *value, void *ud)
{
    if (!ops || !ops->iic_read || !value) return 0;
    struct iic_bus bus = { 0, 0x80 };
    uint32_t addr = (uint32_t)code;
    uint8_t byte = 0;
    int rc = ops->iic_read(&bus, &addr, 1, &byte, ud);
    if (rc == 0) *value = byte;
    return rc == 0 ? 1 : 0;
}

int oa_nim_dtv_iic_write(const oa_nim_dtv_iic_ops *ops, int code,
                         uint8_t value, void *ud)
{
    if (!ops || !ops->iic_write) return 0;
    struct iic_bus bus = { 0, 0x80 };
    uint32_t addr = (uint32_t)code;
    uint8_t byte = value;
    int rc = ops->iic_write(&bus, &addr, 1, &byte, ud);
    return rc == 0 ? 1 : 0;
}

int oa_nim_dtv_hal_read(int bus, int code, uint8_t *value, void *ud)
{
    if (bus != OA_NIM_DTV_IIC_BUS || !ud) return 0;
    const oa_nim_dtv_hal *hal = ud;
    return oa_nim_dtv_iic_read(hal->iic, code, value, hal->hal_ud);
}

int oa_nim_dtv_hal_write(int bus, int code, uint8_t value, void *ud)
{
    if (bus != OA_NIM_DTV_IIC_BUS || !ud) return 0;
    const oa_nim_dtv_hal *hal = ud;
    return oa_nim_dtv_iic_write(hal->iic, code, value, hal->hal_ud);
}
