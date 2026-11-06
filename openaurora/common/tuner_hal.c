#include "openaurora/tuner_hal.h"

#include <stddef.h>

/* Exact HAL_TUNER_*Base stack context: bus, register, two zero words,
 * timeout 400, one zero word. Hal_Iic_Read/Write receives this pointer. */
struct oa_tuner_iic_ctx {
    uint32_t bus;
    uint32_t reg;
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t timeout;
    uint32_t reserved2;
};

static void resolve(uint8_t *bus, int index, oa_tuner_desc_fn get, void *ud)
{
    if (*bus != OA_TUNER_BUS_UNRESOLVED)
        return;
    const uint8_t *desc = get ? get(index, ud) : NULL;
    *bus = desc ? desc[0x19] : 0;
}

int oa_tuner_bus_ids(oa_tuner_buses *buses, oa_tuner_desc_fn get, void *ud)
{
    if (!buses)
        return -1;
    resolve(&buses->tuner, 0, get, ud);
    resolve(&buses->sat, 1, get, ud);
    resolve(&buses->ext_demod, 2, get, ud);
    resolve(&buses->lnb, 3, get, ud);
    return 0;
}

int oa_tuner_read_base(const oa_tuner_iic_ops *ops, void *ud, uint8_t bus,
                       uint8_t reg, uint8_t *buf, uint8_t len)
{
    if (!ops || !ops->iic_read || !buf)
        return 0;
    struct oa_tuner_iic_ctx ctx = {
        bus, reg, 0, 0, OA_TUNER_IIC_TIMEOUT, 0
    };
    uint8_t zero = 0;
    return ops->iic_read(&ctx, &zero, len, buf, ud) == 0;
}

int oa_tuner_write_base(const oa_tuner_iic_ops *ops, void *ud, uint8_t bus,
                        uint8_t reg, const uint8_t *buf, uint8_t len)
{
    if (!ops || !ops->iic_write || (!buf && len))
        return 0;
    struct oa_tuner_iic_ctx ctx = {
        bus, reg, 0, 0, OA_TUNER_IIC_TIMEOUT, 0
    };
    uint8_t zero = 0;
    return ops->iic_write(&ctx, &zero, len, (void *)buf, ud) == 0;
}
