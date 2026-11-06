#include "openaurora/mxl661_dev.h"

#include <stddef.h>

#include "openaurora/hal_iic.h"

int oa_mxl661_dev_write(void *ud, uint8_t addr, uint8_t reg, uint8_t value)
{
    const oa_mxl661_hal_ops *ops = ud;
    if (!ops || !ops->hal_write)
        return -1;
    oa_hal_iic_ctx ctx = { 0, addr, 1, 0, OA_HAL_IIC_TIMEOUT_TUNER, 0 };
    uint8_t subaddr = reg;
    uint8_t data = value;
    return ops->hal_write(&ctx, &subaddr, 1, &data, ops->ud) == 0 ? 0 : -1;
}

int oa_mxl661_dev_read(void *ud, uint8_t addr, uint8_t reg, uint8_t *value)
{
    const oa_mxl661_hal_ops *ops = ud;
    if (!ops || !ops->hal_write || !ops->hal_read || !value)
        return -1;

    /* Ctrl_ReadRegister: set the 0xfb register pointer to the target reg. */
    oa_hal_iic_ctx wctx = { 0, addr, 1, 0, OA_HAL_IIC_TIMEOUT_TUNER, 0 };
    uint8_t subaddr = 0xfb;
    uint8_t pointer = reg;
    if (ops->hal_write(&wctx, &subaddr, 1, &pointer, ops->ud) != 0)
        return -1;

    /* Then read one byte with a zero subaddress. */
    oa_hal_iic_ctx rctx = { 0, addr, 0, 0, OA_HAL_IIC_TIMEOUT_TUNER, 0 };
    uint8_t zero = 0;
    if (ops->hal_read(&rctx, &zero, 1, value, ops->ud) != 0)
        return -1;
    return 0;
}
