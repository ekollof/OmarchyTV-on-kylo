#ifndef OPENAURORA_TUNER_VENDOR_H
#define OPENAURORA_TUNER_VENDOR_H
#include <stdint.h>

#include "openaurora/mxl661.h"

#ifndef OA_NO_VENDOR
/* TV-only bindings over libNDAL3/libNDAL_EX/unresolved lib scenarios. */
int oa_tuner_vendor_mxl661_init(uint8_t addr, const oa_mxl661_config *cfg);
int oa_tuner_vendor_demod_init(void *ud, int index);
int oa_tuner_vendor_dvif_open(void *ud);
int oa_tuner_vendor_set_tuner_type(void *ud, int handle, uint32_t type);
int oa_tuner_vendor_av2018_response(void *ud);
int oa_tuner_vendor_hal_iic_read(const void *ctx, const void *value,
                                 unsigned len, void *buf, void *ud);
int oa_tuner_vendor_hal_iic_write(const void *ctx, const void *value,
                                  unsigned len, void *buf, void *ud);
#endif

#endif
