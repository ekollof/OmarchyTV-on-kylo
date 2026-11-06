/* MxL661 RF/IF preparation tables.
 *
 * SAL_DEMOD_TUN_ProgramTuner @0x0084b0c8 maps the channel standard index:
 *   0->7, 1->8, 2->6, 3->10, 4->9, 5->0xc, 6->0xd, 7->0xb, other->failure.
 *
 * SAL_TUNER_MxL661_SetIF @0x00872c98 / SetRF @0x00872f7c index the 17-word
 * IF table at 0x01367d38 with gTunerConfig[6]:
 *   3650,4000,4100,4150,4500,4570,5000,5380,6000,6280,7200,8250,
 *   35250,36000,36150,36650,44000 kHz.
 *
 * The MxLWare ConfigTuner cases 9/0xd/0xb and the 3-byte standard property
 * tables still need porting; this module only carries the verified mapping.
 */
#ifndef OPENAURORA_MXL661_TUNE_H
#define OPENAURORA_MXL661_TUNE_H
#include <stdint.h>

#define OA_MXL661_IF_TABLE_LEN 17u

uint32_t oa_mxl661_standard_from_index(uint32_t index);
uint32_t oa_mxl661_if_khz(uint32_t index);

#endif
