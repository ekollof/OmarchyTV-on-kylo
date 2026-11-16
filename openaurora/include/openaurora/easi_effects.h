#ifndef OPENAURORA_EASI_EFFECTS_H
#define OPENAURORA_EASI_EFFECTS_H

#include "openaurora/tuner_select.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pure replacement state for the EASI effect interface used by
 * TunerManager::requestServiceSelectForDisplayInternal @ 0xf771e0.
 *
 * The vendor methods (EASIControlInterface::muteOn @ 0x975d58,
 * EASITVControlInterface::headphoneMuteOn @ 0xa23224, muteScartOut @ 0xa243f8,
 * enableSPDIFOut @ 0xa23b4c, selectVideoDecoder @ 0x975f24, hideVideo @
 * 0x977230, showVideo @ 0x9771fc, subtitlesOff @ 0x975010) are C++ methods on
 * live EASIControlTask/UIAppClient object instances; a replacement process
 * cannot call them without recreating the whole object graph. This backend
 * models their observable state and emits every effect, in firmware order,
 * through an injected callback. It performs no device, HAL, decode or UI I/O.
 * The emitter is the seam a future backend or diagnostic can attach to.
 */
enum oa_easi_effect {
    OA_EASI_NOTIFY_SELECTED = 0,
    OA_EASI_MUTE_ON,
    OA_EASI_MUTE_OFF,
    OA_EASI_HEADPHONE_MUTE_ON,
    OA_EASI_HEADPHONE_MUTE_OFF,
    OA_EASI_SCART_MUTE,       /* arg = on */
    OA_EASI_SPDIF_OUT,        /* arg = enable */
    OA_EASI_DECODER_SELECT,   /* arg = 0/1 */
    OA_EASI_HIDE_VIDEO,       /* arg = 0/1 */
    OA_EASI_SHOW_VIDEO,       /* arg = 0/1 */
    OA_EASI_SUBTITLES_OFF,    /* arg = on */
    OA_EASI_TELETEXT_FONT,    /* arg = -1 */
    OA_EASI_SUBTITLES_RESTORE,
    OA_EASI_SELECT_INTERNAL,
    OA_EASI_SELECT_SECOND,
    OA_EASI_DUAL_VIEW_RESET,
    OA_EASI_DELAY             /* arg = milliseconds */
};

typedef struct {
    void *ud;
    void (*emit)(void *ud, enum oa_easi_effect effect, int arg);
} oa_easi_effects_io;

typedef struct {
    int muted;           /* -1 unknown, 0 unmuted, 1 muted */
    int headphone_muted; /* -1 unknown, 0/1 */
    int scart_muted;     /* -1 unknown, 0/1 */
    int spdif_enabled;   /* -1 unknown, 0/1 */
    int decoder;         /* -1 unknown, 0/1 */
    int video_hidden;    /* -1 unknown, 0 shown, 1 hidden */
    int subtitles_off;   /* -1 unknown, 0/1 */
    int teletext_font;   /* -1 unknown, last requested value */
    unsigned effects;    /* count of emitted effects */
} oa_easi_effects_state;

typedef struct {
    oa_easi_effects_io io;
    oa_easi_effects_state state;
} oa_easi_effects;

/* Initialise with all state unknown (-1) and zero emitted effects. A null io
 * or null emit callback leaves the backend without an emitter (state only). */
void oa_easi_effects_init(oa_easi_effects *e, const oa_easi_effects_io *io);

/* Copy the current observable effect state. Returns -EINVAL for null args. */
int oa_easi_effects_snapshot(const oa_easi_effects *e,
                             oa_easi_effects_state *out);

/* Fill ops with callbacks bound to e. ops->ud points at e. This is the bridge
 * that lets oa_tuner_select_display drive the replacement effects backend. */
void oa_easi_effects_bind(oa_easi_effects *e, oa_tuner_select_ops *ops);

#ifdef __cplusplus
}
#endif

#endif
