/* AD82587_D register-image policy recovered from AMP_AD82587_Init@874ef0.
 * Pure data only: no GPIO, MCU, I2C, delays or amplifier ownership. */
#ifndef OPENAURORA_AUDIO_AMP_H
#define OPENAURORA_AUDIO_AMP_H
#include <stdint.h>
#define OA_AD82587_REGISTERS 38u
typedef struct {
    const char *speaker_power;
    uint32_t prescale, channel1, channel2, fine_tune;
    uint32_t power_clip, speaker_drc;
    int32_t state_control4, limiter_attack, release_threshold;
} oa_ad82587_profile;
/* Supply all profile values and the current register image (initially the
 * locally extracted firmware baseline). Vendor init mutates a retained global
 * image; repeated init must feed the preceding image back in, not reset it.
 * No default register image is invented or embedded. Profile sentinels and
 * low-byte/low24-bit truncation match the shipped method; notably the release
 * override is ignored when unsigned speaker_drc <500. out may equal baseline.
 * Returns -EINVAL without changing out for invalid pointers. This is not a
 * hardware initialization routine or a UI-volume-to-amplifier conversion. */
int oa_ad82587_configure(uint8_t out[OA_AD82587_REGISTERS],
    const uint8_t baseline[OA_AD82587_REGISTERS], const oa_ad82587_profile *profile);
#endif
