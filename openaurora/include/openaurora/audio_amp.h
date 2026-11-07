/* AD82587_D register-image policy recovered from AMP_AD82587_Init@874ef0.
 * Pure data only: no GPIO, MCU, I2C, delays or amplifier ownership. */
#ifndef OPENAURORA_AUDIO_AMP_H
#define OPENAURORA_AUDIO_AMP_H
#include <stdint.h>
#include <stddef.h>
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
 * This function invents no baseline; the recovered cold-boot initializer is
 * the separate oa_ad82587_baseline below. Profile sentinels and
 * low-byte/low24-bit truncation match the shipped method; notably the release
 * override is ignored when unsigned speaker_drc <500. out may equal baseline.
 * Returns -EINVAL without changing out for invalid pointers. This is not a
 * hardware initialization routine or a UI-volume-to-amplifier conversion. */
int oa_ad82587_configure(uint8_t out[OA_AD82587_REGISTERS],
    const uint8_t baseline[OA_AD82587_REGISTERS], const oa_ad82587_profile *profile);
/* AMP_AD82587_SetBalance @0x875204 register formula for balance 0..100:
 *   0..49   -> reg4=0, reg5=127 - (balance*127)/50
 *   50      -> reg4=0, reg5=0
 *   51..100 -> reg4=127 - ((100-balance)*127)/50, reg5=0
 * Returns 0 or -EINVAL (outside 0..100). The vendor writes reg4 first and
 * reg5 only on success; this helper computes values only. */
int oa_ad82587_balance_regs(int balance, uint8_t *reg4, uint8_t *reg5);
/* aurora.elf .data initializer at virtual 0x017640e8 (file 0x017440e8), the
 * retained global the vendor Init patches and writes. Not the live stock
 * image; read-only data with no GPIO/MCU/I2C access. */
extern const uint8_t oa_ad82587_baseline[OA_AD82587_REGISTERS];
/* Fresh immutable hwprofile snapshot using recovered power/section/default
 * rules. out->speaker_power borrows the caller's32-byte power buffer, never
 * input data or temporary stack storage. Both outputs unchanged on error.
 * Returns positive errno, like profile readers. No vendor global getter cache
 * is shared; retain this snapshot until explicit reload. Malformed/nonterminated
 * power data and out-of-range legacy enums reject instead of vendor OOB reads. */
int oa_ad82587_profile_from_hw(const void *data,size_t size,
    oa_ad82587_profile *out,char power[32]);
#endif
