/* Equivalents of the aurora-internal NIM-DTV HAL hooks.
 *
 * _dev_nim_dtv_InitNimFunc @0x00860114 zeroes a 0x29-word NIM state block and
 * returns 1. It performs no I2C or state query.
 *
 * _dev_nim_dtv_Connect2HAL @0x0086fea8 fills a 0x29-entry table with the
 * HAL_Demod_User_* function pointers (Initialize, Open, SetChannel, Monitor,
 * Close, GetChannelStatus, CheckLock, GetStrength, GetQuality, GetSNR, ...)
 * and leaves entry 0x12 zero. The implementation here keeps the typed subset
 * needed by the init gate; unimplemented tuning entries are NULL rather than
 * invented.
 */
#ifndef OPENAURORA_NIM_DTV_HW_H
#define OPENAURORA_NIM_DTV_HW_H
#include <stddef.h>
#include <stdint.h>

#define OA_NIM_DTV_STATE_WORDS 0x29u

/* Zero the state block. Returns 1 on success (vendor convention), 0 on a
 * bad/short buffer. */
int oa_nim_dtv_init_state(uint32_t *state, size_t words);

struct oa_nim_dtv_hal_ops {
    int (*initialize)(void *ud);
    int (*open)(void *ud);
    int (*set_channel)(void *ud);
    int (*get_channel_status)(void *ud);
    int (*check_lock)(void *ud);
    int (*get_strength)(void *ud);
    int (*get_quality)(void *ud);
    int (*monitor)(void *ud);
    int (*close)(void *ud);
    int (*disconnect)(void *ud);
};

struct oa_nim_dtv_hal_table {
    struct oa_nim_dtv_hal_ops ops;
    int connected;
};

/* Install the available HAL ops. Returns 1 on success, 0 on bad arguments. */
int oa_nim_dtv_connect_hal(struct oa_nim_dtv_hal_table *table,
                           const struct oa_nim_dtv_hal_ops *ops);

#endif
