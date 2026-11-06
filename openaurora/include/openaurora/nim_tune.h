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

struct oa_nim_tune_channel {
    uint32_t freq_hz;
    uint32_t standard;
    uint32_t bandwidth;
    uint32_t if_khz;
};

struct oa_mxl661_analog_params;
struct oa_mxl661_atv_tables;

struct oa_nim_tune_status {
    uint8_t locked;
    uint32_t strength;
    uint32_t quality;
};

/* Raw 0x5c channel record from HAL_Demod_User_SetChannel.
 * Fields recovered from FUN_0086aab0: +0 freq (kHz), +4 p1, +8 bandwidth,
 * +0x10 p4, +0x1c standard index, +0x58 frequency override. */
struct oa_nim_raw_channel {
    uint8_t raw[0x5c];
};

/* Raw 0x180 HAL_Demod_GetStatus buffer. The verified fields are lock at +0,
 * strength at +0x18 and quality at +0x24. */
struct oa_nim_raw_status {
    uint8_t raw[0x180];
};

/* Caller prefill plus the words FUN_0086a4dc leaves in r7+8..r7+51:
 *   word[0]  frequency in kHz (object +0xc, refreshed by the type-1 read)
 *   word[1]  object +0x10
 *   word[2]  symbol rate (object +0x14)
 *   word[3..9] signal/standard enum fields
 *   word[10] bytes 40..43; the type-1 flag is byte +42 (bits 16..23)
 * The raw-status field identities behind word[3..9] are positional here, not
 * named: only the enum translations are recovered. */
struct oa_nim_core_status {
    uint32_t word[11];
};

/* HAL_Demod_User_GetChannelStatus output cases (0x86bafc), all verified at
 * instruction level. The vendor copies stack slots it never writes in some
 * cases; this model zeroes every such slot deterministically. The exception
 * is type 2/7 out[14], which the vendor explicitly preserves from the
 * caller's buffer (ldr [r9,#56] at 0x86bc1c): out[14] is an in/out slot and
 * the caller must pass an initialized 16-word array. */
int oa_nim_status_type1(const struct oa_nim_core_status *core, uint32_t out[11]);
int oa_nim_status_type2(const struct oa_nim_core_status *core, uint32_t out[16]);
int oa_nim_status_type3(const struct oa_nim_core_status *core, uint32_t out[4]);
int oa_nim_status_type6(const struct oa_nim_core_status *core, uint32_t out[4]);
int oa_nim_status_type8(const struct oa_nim_core_status *core, uint32_t out[4]);
/* Type 7 is type 2 plus out[12]: the vendor calls _hal_NIM_DeliverConfig with
 * key 0xb003 and a 1-byte value, and reports 1 only when that read succeeded
 * and returned 1 (0x86be52). A null callback models the failed read. */
int oa_nim_status_type7(const struct oa_nim_core_status *core, uint32_t out[16],
                        int (*deliver_config)(void *ud, uint32_t key,
                                              const void *data, int len),
                        void *ud);

struct oa_nim_tune_ops {
    int (*switch_tuner_type)(void *ud, int type); /* 0 = success */
    int (*get_tuner_type)(void *ud);
    int (*deliver_config)(void *ud, uint32_t key, const void *data, int len);
    int (*fe_open)(void *ud, int index);          /* >=0 handle */
    void (*fe_close)(void *ud, int fe);
    int (*set_standby)(void *ud, int fe, int on); /* 0 = success */
    int (*get_status)(void *ud, int handle, int index, uint32_t mask, void *buf);
    int (*get_rssi)(void *ud, int tuner_type, uint8_t *rssi); /* 1 = success */
    int (*program_tuner)(void *ud, const struct oa_nim_tune_channel *ch);
    /* Optional analog (ATV) tail. The caller owns the runtime-loaded table
     * set; 0 = success. NULL leaves the backend DVB-only. */
    int (*program_atv)(void *ud,
                       const struct oa_mxl661_analog_params *params,
                       const struct oa_mxl661_atv_tables *tables);
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

/* The channel calls use the typed NIM channel/status records recovered from
 * the SetChannel/GetChannelStatus first stages. The vendor's raw 0x5c channel
 * record and 0x180 status array are not exposed here. */
int oa_nim_tune_set_channel(const struct oa_nim_tune_ops *ops, void *ud,
                            struct oa_nim_tune_state *state,
                            const struct oa_nim_tune_channel *channel);
int oa_nim_tune_get_channel_status(const struct oa_nim_tune_ops *ops,
                                   void *ud,
                                   struct oa_nim_tune_state *state,
                                   struct oa_nim_tune_status *out);

/* Parse the raw 0x5c record into the typed channel. Returns 1 on success. */
int oa_nim_raw_channel_parse(const uint8_t raw[0x5c], int path,
                             struct oa_nim_tune_channel *out);
int oa_nim_tune_set_channel_raw(const struct oa_nim_tune_ops *ops, void *ud,
                                struct oa_nim_tune_state *state,
                                const struct oa_nim_raw_channel *channel);

/* Map the verified 0x180 fields into the typed status. 1 = success. */
int oa_nim_raw_status_parse(const uint8_t raw[0x180],
                            struct oa_nim_tune_status *out);

/* Adapter to the NIM-DTV typed HAL table. The channel calls stay -ENOTSUP
 * because the vendor table passes no channel/status pointer. */
struct oa_nim_tune_binding {
    const struct oa_nim_tune_ops *ops;
    void *ud;
    struct oa_nim_tune_state state;
    uint8_t locked;
    uint32_t strength;
    uint32_t quality;
};

int oa_nim_tune_hal_open(void *v);
int oa_nim_tune_hal_close(void *v);
int oa_nim_tune_hal_monitor(void *v);
int oa_nim_tune_hal_disconnect(void *v);
int oa_nim_tune_hal_check_lock(void *v);
int oa_nim_tune_hal_get_strength(void *v);
int oa_nim_tune_hal_get_quality(void *v);
int oa_nim_tune_hal_set_channel(void *v);
int oa_nim_tune_hal_get_channel_status(void *v);

#endif
