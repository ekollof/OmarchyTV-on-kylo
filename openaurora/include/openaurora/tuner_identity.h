/* Vendor tuner-type selection from the hwprofile.
 *
 * Recovered from aurora.elf:
 *   SAL_DEMOD_TUN_GetInitTunerType @0x0084adb8 (dispatches FUN_0084abb8)
 *   FUN_0084abb8                   @0x0084abb8 (layout/profile selection)
 *   Profile_GetTunerType           @0x00c6398c (hwprofile TunerTypes/TunerType)
 *   _convertSalTunerType2NdNimTunerType @0x0084ad44 (SAL -> NdNim table)
 *
 * FUN_0084abb8 uses DEV_COMMON_GET_PCBLAYOUTTYPE (global at 0x0175f028):
 *   layout 3,4,5,6 -> SAL type 0xf
 *   layout 8       -> SAL type 8
 *   everything else -> read hwprofile "TunerTypes/TunerType";
 *                      equal to "Si2151" -> 8, otherwise 0xd
 *
 * The shipped mb230 hwprofile contains "Si2157", so the firmware's exact
 * string compare selects 0xd (MxL661). That is a firmware fact, not a
 * guessed chip identity: keep it for parity. DEV_INI_Init @0x00853a58 sets
 * layout 14 on this boot path, which takes the profile branch.
 */
#ifndef OPENAURORA_TUNER_IDENTITY_H
#define OPENAURORA_TUNER_IDENTITY_H
#include <stdint.h>

#include "openaurora/efsl_profile.h"

#define OA_TUNER_SAL_SI2151 0x8
#define OA_TUNER_SAL_MXL661 0xd
#define OA_TUNER_SAL_EXTERNAL 0xf

#define OA_TUNER_NDNIM_SI2151 0x8
#define OA_TUNER_NDNIM_MXL661 0xc

/* Reproduce FUN_0084abb8. A missing/unreadable profile value is treated as
 * the MxL661 branch; the vendor passes an uninitialized stack buffer to
 * strcmp in that case, so its observable result is undefined. */
int oa_tuner_sal_type(uint32_t pcb_layout, const oa_efsl_profile *hwprofile);

/* Reproduce _convertSalTunerType2NdNimTunerType @0x0084ad44: the 14-entry
 * table at 0x01355c74, and 8 for every unsigned value above 0xd. */
int oa_tuner_ndnim_type(int sal_type);

#endif
