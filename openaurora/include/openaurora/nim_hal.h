/* NIM backend over the recovered HAL_Demod_User_* tuning ops.
 *
 * Host tests inject a fake oa_nim_tune_ops table; the TV build supplies the
 * vendor adapters (HAL_fe_*, HAL_DEMOD_EXT_*, HAL_Demod_GetStatus and the
 * MxL661 DVB-T RF/IF sequence). Only the terrestrial DVB-T path is wired.
 */
#ifndef OPENAURORA_NIM_HAL_H
#define OPENAURORA_NIM_HAL_H
#include "openaurora/nim.h"
#include "openaurora/nim_tune.h"

void oa_nim_hal_set_ops(const struct oa_nim_tune_ops *ops, void *ud);
const struct oa_tuner_backend *oa_nim_hal_backend(void);

#endif
