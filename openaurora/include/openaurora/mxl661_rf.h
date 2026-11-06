/* MxL661 DVB-T RF/IF sequence.
 *
 * Recovered from:
 *   SAL_DEMOD_TUN_ProgramTuner @0x0084b0c8
 *   SAL_TUNER_MxL661_SetIF   @0x00872c98  -> ConfigTuner case 9
 *   SAL_TUNER_MxL661_SetRF   @0x00872f7c  -> case 0xd then 0xb
 *   Ctrl_SetRfFreqLutReg     @0x008823f8
 *   Ctrl_ProgramRegisters    @0x008822dc
 *
 * For the digital standards the five DIG_*_FREQ_LUT tables all contain the
 * single default entry {1, 0xd800171c}; Ctrl_SetRfFreqLutReg therefore writes
 * the four MxL601 spur registers 0x49=0x1c, 0xdb=0x17, 0xea=0x00, 0xeb=0xd8.
 *
 * This implements the DVB-T path (standard 8) with the DigitalDvbt property
 * table. Analog and cable RF LUTs/tails remain separate.
 */
#ifndef OPENAURORA_MXL661_RF_H
#define OPENAURORA_MXL661_RF_H
#include <stdint.h>

#include "openaurora/mxl661.h"

struct oa_mxl661_rf_params {
    uint32_t freq_hz;
    uint32_t standard;   /* ConfigTuner standard number; 8 = DVB-T */
    uint32_t bandwidth;  /* 0x20 = 6 MHz, 0x21 = 7 MHz, else 8 MHz */
    uint32_t if_khz;
    int start;           /* case 0xd param[2] */
};

/* Program the digital IF mode (case 9), start tune (0xd) and channel tune
 * (0xb) for one DVB-T channel. 0 = success. */
int oa_mxl661_tune_dvbt(const oa_mxl661_ops *ops, void *ud, uint8_t addr,
                        const struct oa_mxl661_rf_params *params);

#endif
