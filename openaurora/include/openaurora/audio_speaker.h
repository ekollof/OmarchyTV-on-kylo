#ifndef OPENAURORA_AUDIO_SPEAKER_H
#define OPENAURORA_AUDIO_SPEAKER_H

#include <stdint.h>

/*
 * Speaker level/balance mapping recovered from
 * PLAT_VAUD_SetSpeakerVolumeBalance 0x8d1c3c:
 *
 *   level = min(level, 100)
 *   balance = clamp(balance, -50, 50)
 *   HAL balance control = balance + 50 (0..100)
 *   both channel volumes = level
 *
 * The vendor wrapper then calls PLAT_AOUT_SetSpeakerBalance and only writes
 * PLAT_AOUT_SetSpeakerVolume when the cached left volume differs. This helper
 * models the clamped state only; it does not select a speaker id, touch HAL,
 * ALSA, the amplifier or hardware.
 */
struct oa_audio_speaker_state {
    unsigned level;    /* 0..100 */
    unsigned balance;  /* HAL control 0..100 */
    unsigned left;     /* 0..100 */
    unsigned right;    /* 0..100 */
};

int oa_audio_speaker_map(unsigned level, int balance,
                         struct oa_audio_speaker_state *out);

#endif
