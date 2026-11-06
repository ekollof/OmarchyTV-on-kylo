/* SAL/HAL tuner bring-up order after the profile-selected tuner type.
 *
 * Recovered from HAL_Demod_User_Initialize @0x0086ade4 and
 * SAL_DEMOD_TUN_Initialize @0x0084c01c:
 *
 *   HAL_Demod_Init(0)                 // opens/caches the dvif handle
 *   HAL_TUNER_Init()                  // bus IDs (handled separately)
 *   tuner class Init()                // SAL type 8 (SI2151) or 0xd (MxL661)
 *   AV2018 GetTunerResponse()         // literal return 1 in this firmware
 *   HAL_dvif_open()                   // cached handle
 *   HAL_DEMOD_EXT_DVIF_SetTunerType(handle, table[type])
 *   SAL_LNB_Initialize()              // RT5047 -> LNB_EN GPIO
 *
 * SAL type 0xf (external/unknown) skips the tuner/AV2018/dvif block inside
 * SAL_DEMOD_TUN_Initialize, but HAL_Demod_User_Initialize still calls
 * SAL_LNB_Initialize afterwards.
 *
 * The callbacks keep host tests independent of the vendor libraries. The TV
 * binding supplies HAL_Demod_Init/HAL_dvif_open/SetTunerType and Hal_Iic;
 * RT5047's LNB_EN GPIO is left unbound because /dev/ntgpio is owned by stock
 * Aurora and the Novatek GPIO descriptor path is not implemented here.
 */
#ifndef OPENAURORA_TUNER_SEQUENCE_H
#define OPENAURORA_TUNER_SEQUENCE_H
#include <stdint.h>

typedef struct {
    int (*demod_init)(void *ud, int index); /* 0 = success */
    int (*tuner_init)(void *ud);            /* 0 = success */
    int (*av2018_response)(void *ud);       /* vendor returns 1 */
    int (*dvif_open)(void *ud);             /* handle, negative = failure */
    int (*set_tuner_type)(void *ud, int handle, uint32_t type); /* 0 = ok */
    void (*lnb_init)(void *ud);             /* may be NULL (unowned GPIO) */
} oa_tuner_sequence_ops;

typedef struct {
    int demod_rc;
    int tuner_rc;
    int av2018_rc;
    int dvif_handle;
    int set_type_rc;
    int lnb_called;
    uint32_t tuner_type;
} oa_tuner_sequence_result;

/* Execute the recovered order. The vendor ignores most errors and continues;
 * this runs every applicable step, then returns 0 only if the demod, tuner,
 * dvif handle and SetTunerType all succeeded. */
int oa_tuner_sequence_run(const oa_tuner_sequence_ops *ops, void *ud,
                          uint32_t sal_type, oa_tuner_sequence_result *out);

#endif
