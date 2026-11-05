#ifndef OPENAURORA_NIM_H
#define OPENAURORA_NIM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NIM tuner backend boundary, shaped after the recovered vendor calls:
 *
 *   DEV_NIM_DTV_StartTune(uint path, uint *params, int *status)  @0x008619e1
 *   DEV_NIM_DTV_MonitorTune(uint path, int *status)              @0x00862889
 *   DEV_NIM_DTV_StopTune / Abort
 *   DEV_NIM_DTV_GetSignalStrength/Quality/BER/SNR/UNC
 *
 * The vendor status words use 3 = unlocked and 4 = locked; frequency is
 * status[3] in kHz. Path values 1/2/3/5/7 are TERRESTRIAL/CABLE/QPSK/DTMB/
 * ISDBT; 6 and 8 were not named in the recovered evidence and are not exposed
 * here. The signal getters return uint32 strength/quality, float BER/SNR and
 * uint32 UNC with 0xffffffff meaning unavailable.
 *
 * Only an offline stub backend is implemented. The shipped DEV_NIM_DTV_*
 * functions live inside the vendor executable and are not a linkable library;
 * a live backend needs an explicit, recovery-ready integration decision.
 */

#define OA_NIM_PATH_TERRESTRIAL 1
#define OA_NIM_PATH_CABLE 2
#define OA_NIM_PATH_QPSK 3
#define OA_NIM_PATH_DTMB 5
#define OA_NIM_PATH_ISDBT 7

#define OA_NIM_TUNE_UNLOCKED 3
#define OA_NIM_TUNE_LOCKED 4

#define OA_NIM_UNC_UNKNOWN UINT32_MAX
#define OA_NIM_DEFAULT_FREQUENCY_KHZ 666000u

struct oa_nim_tune_result {
    uint32_t state;          /* OA_NIM_TUNE_UNLOCKED/LOCKED */
    uint32_t frequency_khz;  /* vendor status[3] */
};

struct oa_nim_signal {
    uint8_t locked;
    uint32_t strength;
    uint32_t quality;
    uint32_t unc;            /* OA_NIM_UNC_UNKNOWN when unavailable */
    float ber;
    float snr;
};

struct oa_nim_ops {
    int (*start)(uint8_t path, const uint32_t *params, size_t count,
        struct oa_nim_tune_result *result);
    int (*monitor)(uint8_t path, struct oa_nim_tune_result *result);
    int (*stop)(uint8_t path);
    int (*signal)(uint8_t path, struct oa_nim_signal *out);
};

struct oa_tuner_backend {
    const char *name;
    const struct oa_nim_ops *nim;
    /* Stub/source hook: returns 1 with one complete EIT section, 0 when the
     * current scan has no more output, or a negative errno. A real backend
     * would replace this with a demux/EIT section source. */
    int (*next_eit)(uint8_t *buf, size_t cap, size_t *len);
};

const struct oa_tuner_backend *oa_nim_stub_backend(void);
/* Shared stub NIM ops for replay/alternative EIT sources. */
const struct oa_nim_ops *oa_nim_stub_ops(void);

/* Offline TS replay source: opens a bounded 188-byte-packet transport stream,
 * filters PID 0x12 and returns assembled EIT sections through next_eit. The
 * NIM side stays the stub until a live tuner backend is accepted. */
int oa_nim_ts_open(const char *path);
void oa_nim_ts_close(void);
const struct oa_tuner_backend *oa_nim_ts_backend(void);

/*
 * Live-capable NDAL/DVB frontend backend. The default ops open
 * /dev/dvb/adapterN/frontendN, use FE_SET_FRONTEND/FE_READ_* for DVB-T and
 * call the shipped Nd_NIM_* setters/getters (libNDAL_EX.so) for the frontend
 * parameters they own. Tests inject the same ops table; no device is touched
 * unless the module is configured with tuner.backend=ndal.
 */
struct oa_ndal_nim_ops {
    int (*open_frontend)(void *ctx, const char *path);
    int (*close_frontend)(void *ctx, int fd);
    int (*tune)(void *ctx, int fd, uint8_t nim_path, uint32_t frequency_khz);
    int (*scan_mode)(void *ctx, int fd, uint32_t mode);
    int (*frequency)(void *ctx, int fd, uint32_t *khz);
    int (*delivery_system)(void *ctx, int fd, uint8_t *system);
    int (*symbol_rate)(void *ctx, int fd, uint32_t *ksym);
    int (*status)(void *ctx, int fd, uint32_t *status);
    int (*metrics)(void *ctx, int fd, struct oa_nim_signal *signal);
    int (*set_ts_output)(void *ctx, int fd, uint32_t value);
    int (*set_standby)(void *ctx, int fd, int on);
};

const struct oa_tuner_backend *oa_nim_ndal_backend(const char *frontend_path);
/* Test/embedding hook; NULL restores the default Linux/NDAL ops. */
void oa_nim_ndal_set_ops(const struct oa_ndal_nim_ops *ops, void *ctx);

/* Attach a DVB section-filter EIT source to nim-ndal. The source opens lazily
 * on the first next_eit() call. ops NULL selects the shipped NDAL/Linux ops;
 * tests pass mock ops. */
struct oa_eit_dvb_ops;
int oa_nim_ndal_enable_eit(int adapter, int index, uint16_t pid,
    const struct oa_eit_dvb_ops *ops, void *ctx);

#ifdef __cplusplus
}
#endif

#endif
