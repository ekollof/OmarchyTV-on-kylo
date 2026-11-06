/* NIM HAL tuning-table subset (HAL_Demod_User_*).
 *
 * Recovered from aurora.elf:
 *   HAL_Demod_User_Open          @0x0086aeb8
 *   HAL_Demod_User_Close         @0x0086aff0
 *   HAL_Demod_User_Monitor       @0x0086c0cc (return 1)
 *   HAL_Demod_User_Disconnect    @0x0086c958 (return)
 *   HAL_Demod_User_CheckLock     @0x0086b8b4
 *   HAL_Demod_User_GetStrength   @0x0086ba38
 *   HAL_Demod_User_GetQuality    @0x0086b974
 *   covertModulation2HalDemod    @0x00869ad8
 *
 * Path -> HAL_Demod_GetStatus index: 1 -> 0x14, 2/7 -> 0x12, 5 -> 0x11,
 * 6 -> 0x0e, 8 -> 0x10. Path 3 uses a dynamic resolver that is not recovered.
 * CheckLock uses mask 2 and buf[0]; Strength uses mask 0x40 and the u32 at
 * buf+0x18 (path 1) or the tuner RSSI otherwise; Quality uses mask 0x80 and
 * the u32 at buf+0x24.
 *
 * SetChannel/GetChannelStatus are large NIM-channel <-> NDAL struct mappings
 * and are deliberately left -ENOTSUP rather than guessed.
 */
#ifndef OPENAURORA_NIM_TUNE_H
#define OPENAURORA_NIM_TUNE_H
#include <stdint.h>

struct oa_nim_tune_ops {
    int (*switch_tuner_type)(void *ud, int type); /* 0 = success */
    int (*get_tuner_type)(void *ud);
    int (*deliver_config)(void *ud, uint32_t key, const void *data, int len);
    int (*fe_open)(void *ud, int index);          /* >=0 handle */
    void (*fe_close)(void *ud, int fe);
    int (*set_standby)(void *ud, int fe, int on); /* 0 = success */
    int (*get_status)(void *ud, int handle, int index, uint32_t mask, void *buf);
    int (*get_rssi)(void *ud, int tuner_type, uint8_t *rssi); /* 1 = success */
};

struct oa_nim_tune_state {
    int handle;    /* state+0x04 */
    int path;      /* state+0x08 */
    int index_arg; /* state+0x24, only used by the unresolved path-3 resolver */
    int tuner_type;
};

int oa_nim_tune_path_index(const struct oa_nim_tune_state *state);
int oa_nim_tune_open(const struct oa_nim_tune_ops *ops, void *ud,
                     struct oa_nim_tune_state *state);
int oa_nim_tune_close(const struct oa_nim_tune_ops *ops, void *ud,
                      struct oa_nim_tune_state *state);
int oa_nim_tune_monitor(void);
int oa_nim_tune_disconnect(void);
int oa_nim_tune_check_lock(const struct oa_nim_tune_ops *ops, void *ud,
                           struct oa_nim_tune_state *state, uint8_t *locked);
int oa_nim_tune_get_strength(const struct oa_nim_tune_ops *ops, void *ud,
                             struct oa_nim_tune_state *state,
                             uint32_t *strength);
int oa_nim_tune_get_quality(const struct oa_nim_tune_ops *ops, void *ud,
                            struct oa_nim_tune_state *state, uint32_t *quality);

/* Unresolved NDAL struct mappings. */
int oa_nim_tune_set_channel(const struct oa_nim_tune_ops *ops, void *ud,
                            struct oa_nim_tune_state *state,
                            const void *channel);
int oa_nim_tune_get_channel_status(const struct oa_nim_tune_ops *ops,
                                   void *ud,
                                   struct oa_nim_tune_state *state,
                                   void *out);

#endif
