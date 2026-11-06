/* NIM backend over the recovered HAL_Demod_User_* tuning ops.
 *
 * Host tests inject a fake oa_nim_tune_ops table; the TV build supplies the
 * vendor adapters (HAL_fe_*, HAL_DEMOD_EXT_*, HAL_Demod_GetStatus and the
 * MxL661 DVB-T RF/IF sequence). oa_nim_hal_set_atv() switches the start path
 * to the analog tail with a caller-owned runtime table set; without it the
 * backend stays DVB-T only.
 */
#ifndef OPENAURORA_NIM_HAL_H
#define OPENAURORA_NIM_HAL_H
#include "openaurora/nim.h"
#include "openaurora/nim_tune.h"

void oa_nim_hal_set_ops(const struct oa_nim_tune_ops *ops, void *ud);
/* Use the vendor HAL_Demod_User_SetChannel path for DVB-T start when the ops
 * table provides set_channel. 0 keeps the typed program_tuner fallback. */
void oa_nim_hal_set_vendor_channel(int enabled);
/* Enable analog start with the loaded table set (standard 0..5, xtal flag).
 * NULL tables clears analog mode. 0 = accepted, -EINVAL = bad arguments. */
int oa_nim_hal_set_atv(const struct oa_mxl661_atv_tables *tables,
                       int standard, uint32_t if_khz, int xtal_24mhz);
const struct oa_tuner_backend *oa_nim_hal_backend(void);

#endif
