#include "openaurora/tuner_vendor.h"

#ifndef OA_NO_VENDOR

#include <stddef.h>

#include "openaurora/mxl661_dev.h"

/* libNDAL3 exports. Hal_Iic_Read/Write return 0 on success;
 * HAL_DEMOD_EXT_DVIF_SetTunerType returns 0 on success. */
extern int Hal_Iic_Write(const void *ctx, const void *value, unsigned len,
                         const void *buf);
extern int Hal_Iic_Read(const void *ctx, const void *value, unsigned len,
                        void *buf);
extern int HAL_Demod_Init(int index);
extern int HAL_dvif_open(void);
extern int HAL_DEMOD_EXT_DVIF_SetTunerType(int handle, int type);

static int hal_write(const void *ctx, const void *value, unsigned len,
                     const void *buf, void *ud)
{
    (void)ud;
    return Hal_Iic_Write(ctx, value, len, buf);
}

static int hal_read(const void *ctx, const void *value, unsigned len,
                    void *buf, void *ud)
{
    (void)ud;
    return Hal_Iic_Read(ctx, value, len, buf);
}

static const oa_mxl661_hal_ops hal_ops = { hal_write, hal_read, NULL };

int oa_tuner_vendor_mxl661_init(uint8_t addr, const oa_mxl661_config *cfg)
{
    oa_mxl661_ops ops = { oa_mxl661_dev_write, oa_mxl661_dev_read };
    return oa_mxl661_init(&ops, (void *)&hal_ops, addr, cfg);
}

int oa_tuner_vendor_demod_init(void *ud, int index)
{
    (void)ud;
    return HAL_Demod_Init(index);
}

int oa_tuner_vendor_dvif_open(void *ud)
{
    (void)ud;
    return HAL_dvif_open();
}

int oa_tuner_vendor_set_tuner_type(void *ud, int handle, uint32_t type)
{
    (void)ud;
    return HAL_DEMOD_EXT_DVIF_SetTunerType(handle, (int)type);
}

int oa_tuner_vendor_av2018_response(void *ud)
{
    (void)ud;
    /* C_SalTunerAV2018::GetTunerResponse @0x0084d8cc is `return 1`. */
    return 1;
}

int oa_tuner_vendor_hal_iic_read(const void *ctx, const void *value,
                                 unsigned len, void *buf, void *ud)
{
    (void)ud;
    return Hal_Iic_Read(ctx, value, len, buf);
}

int oa_tuner_vendor_hal_iic_write(const void *ctx, const void *value,
                                  unsigned len, void *buf, void *ud)
{
    (void)ud;
    return Hal_Iic_Write(ctx, value, len, buf);
}

#endif /* !OA_NO_VENDOR */
