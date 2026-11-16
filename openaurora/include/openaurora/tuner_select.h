#ifndef OPENAURORA_TUNER_SELECT_H
#define OPENAURORA_TUNER_SELECT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pure model of TunerManager::requestServiceSelectForDisplayInternal
 * (aurora.elf MD5 64c252340d3af6c5d66072611b7bad7a @ 0x00f771e0), the body
 * reached by the CEC 0x9D / tuner display-select path. Recovered from the
 * saved decompile and instruction dump in
 *   artifacts/analysis/source-loss-2026-11-12/tunermanager-select.{txt,dis}
 * and documented in artifacts/reports/SOURCE-LOSS-FALLBACK-2026-11-12.md.
 *
 * This helper performs NO vendor, HAL, device, tuner, source or UI I/O: every
 * effect is an injected callback, so the recovered gates and the exact vendor
 * effect order can be verified on the host. The replacement backends for most
 * of these effects do not exist yet; this is the contract they must meet, not
 * a claim of hardware or stock-UI parity.
 *
 * DAT_* globals from the firmware are mirrored as explicit input/output flags:
 *   DAT_00f775d0 (d0), DAT_00f775d4 (cached result), DAT_00f775e0 (e0),
 *   DAT_00f775e4 (e4), DAT_00f776b0 (b0), DAT_00f776b4 (b4).
 * Field names are recovered by use, not from vendor source.
 */

typedef struct {
    void *ud;
    void (*mute_on)(void *);
    void (*headphone_mute_on)(void *);
    void (*mute_scart)(void *, int on);
    void (*spdif_out)(void *, int enable);        /* enableSPDIFOut(tv, 0) */
    void (*decoder)(void *, int set);             /* selectVideoDecoder 0 or 1 */
    void (*hide_video)(void *, int hide);         /* hideVideo 0 or 1 */
    void (*subtitles_off)(void *, int on);
    void (*teletext_font)(void *);                /* setTeletextFont(-1) */
    void (*notify_selected)(void *);              /* notifyServiceSelected */
    void (*select_internal)(void *);              /* requestServiceSelectInternal */
    void (*second_select)(void *);                /* second vtable+0xd4 select */
    void (*dual_view_reset)(void *);              /* +0x98 vtable+0x3c(0) */
    void (*delay)(void *, unsigned ms);           /* FROST_Sleep(0x14) */
    void (*mute_off)(void *);
    void (*headphone_mute_off)(void *);
    void (*restore_subtitles)(void *);            /* +0xfc vtable+0x68 */
    void (*show_video)(void *, int mode);         /* 0 normal, 1 alternate */
    void (*log)(void *, const char *fmt, ...);
} oa_tuner_select_ops;

typedef struct {
    int service_count;      /* EASIServiceList+0x7c: <=0 (empty) rejects */
    int standby_requested;  /* getStandbyRequested != 0 rejects */
    int handle_valid;       /* vtable+0x54 result != -1; 0 rejects */
    int handle;             /* resolved service handle (for auth/country gate) */
    int select_result;      /* vtable+0xd4 return (uVar5) */
    int prev_select_result; /* DAT_00f775d4 cached result before this call */
    int display;            /* param_2: display/mute path */
    int post_select;        /* param_6: unmute/show phase */
    int notify_suppressed;  /* param_7: suppress notifyServiceSelected when 1 */
    int dual_view_available;/* getDualViewAvailable -> +0x98 reset */
    int dual_view_active;   /* +0x98 vtable+0x20 result (skip show when set) */
    int device_in_standby;  /* isDeviceInStandby(this) */
    int decoder_available;  /* +0x2c vtable+0x28: 0 means available */
    int authorized;         /* isServiceAlreadyAuthorisedForDisplay result */
    int current_service;    /* UserPreferences::getCurrentService */
    int subtitles_on;       /* UserPreferences::getSubtitlesOn */
    int subtitle_enabled;   /* isSubtitleEnabled */
    int country;            /* UserPreferences::getCountry */
    int member_8_zero;      /* this[8] == 0 condition */
    int pending_b0;         /* DAT_00f776b0 input */
    int pending_b4;         /* DAT_00f776b4 input */
} oa_tuner_select_inputs;

enum oa_tuner_select_reject {
    OA_TUNER_SELECT_OK = 0,
    OA_TUNER_SELECT_EMPTY_LIST,
    OA_TUNER_SELECT_STANDBY,
    OA_TUNER_SELECT_BAD_HANDLE
};

typedef struct {
    int ran;                /* 0 rejected by a gate, 1 body executed */
    int reject_reason;      /* enum oa_tuner_select_reject */
    int pre_mute_applied;   /* display mute/hide block ran */
    int pre_non_display;    /* decoder(1)/hide(1) block ran */
    int unmute_applied;     /* display unmute block ran */
    int subtitles_restored; /* restore_subtitles callback ran */
    int show_applied;       /* a showVideo call was made */
    int show_mode;          /* 0 normal, 1 alternate */
    int deferred;           /* a pending b0/b4 flag deferred the phase */
    int flag_d0;            /* final DAT_00f775d0 */
    int flag_e0;            /* final DAT_00f775e0 */
    int flag_e4;            /* final DAT_00f775e4 */
    int flag_b0;            /* final DAT_00f776b0 */
    int flag_b4;            /* final DAT_00f776b4 */
} oa_tuner_select_result;

/*
 * Pre-mute/hide gate at 0xf77588. The vendor condition is:
 *   (result & ~4) == 0                                   (result is 0 or 4)
 *   || (prev == 4 && country != 0x14 && member8 == 0)
 *   || (prev == 4 && country == 0x14
 *       && handle != current_service && member8 == 0)
 */
int oa_tuner_select_pre_mute_gate(int result, int prev, int country,
                                  int current_service, int handle,
                                  int member_8_zero);

/* Run one display-select. Returns 0 (ran or cleanly rejected), or -EINVAL for
 * null arguments. All effects are emitted through ops in firmware order. */
int oa_tuner_select_display(const oa_tuner_select_ops *ops,
                            const oa_tuner_select_inputs *in,
                            oa_tuner_select_result *out);

#ifdef __cplusplus
}
#endif

#endif
