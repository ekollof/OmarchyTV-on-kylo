#ifndef OPENAURORA_AMP_CONTROLS_H
#define OPENAURORA_AMP_CONTROLS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Recovered AD82587 register controls not yet in common/audio_amp.c.
 * Source: AMP-AD82587 SetVolume @0x8752a8, GetVolume @0x87551c and SoftMute
 * @0x875148 (instruction-checked 2026-11-16). Both transports use the vendor
 * boolean convention: read/write return 1 on success, 0 on failure.
 *
 * SetVolume writes reg3 with volume&0xff then reads reg3 back; it returns 1
 * only when the byte matches. GetVolume reads reg3. SoftMute reads reg2,
 * ORs 0x0e for on or ANDs 0xf1 for off, then writes reg2 and returns the
 * write result. These are register effects, not UI volume or audible proof.
 */
typedef struct {
    void *ud;
    int (*write)(void *ud, uint8_t reg, uint8_t byte);
    int (*read)(void *ud, uint8_t reg, uint8_t count, uint8_t *out);
} oa_amp_register_io;

int oa_ad82587_set_volume(const oa_amp_register_io *io, int volume);
int oa_ad82587_get_volume(const oa_amp_register_io *io, int *out);
int oa_ad82587_soft_mute(const oa_amp_register_io *io, int on);

#ifdef __cplusplus
}
#endif

#endif
