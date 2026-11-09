/* Audio-source HAL seam for the recovered ICE_AudioStart external branch.
 *
 * The vendor code for this route is inside aurora.elf (PLAT_AIN_Connect,
 * PLAT_AOUT_*, PLAT_VAUD_SetSoftMute, plat_adec_connect, plat_audio_connect);
 * only the libNDAL3 calls are exported. The module mirrors the PLAT functions
 * and uses this seam for the raw HAL calls, so the exact order and arguments
 * are host-testable without a TV.
 *
 * Firmware evidence: AUDIO-SOURCE-TRANSPORT-2026-11-09 and
 * SOURCE-PATH-2026-11-09. All pointers are caller-owned.
 */
#ifndef OPENAURORA_AUDIO_SOURCE_HAL_H
#define OPENAURORA_AUDIO_SOURCE_HAL_H

#include <stdint.h>

struct oa_audio_source_hal {
    /* Hal_Audio_Ctrl(op, params): params is a 0x38-byte payload whose word0
     * is the sub-command and word1 the value. Used with op 0x2a for PIP. */
    int (*audio_ctrl)(int op, void *params38);
    /* Hal_Audio_EasySelect(path, id, records): records is an array of
     * {int id; char name[0x28]} entries; the HAL matches id, compares the
     * record name with an ALSA control name and writes its index. */
    int (*easy_select)(int path, int id, const void *records);
    /* Hal_Audio_EasySet(item, value, zero). */
    int (*easy_set)(int item, const void *value, int zero);
    /* Hal_Audio_KItem(code) followed by Hal_Audio_SetControl(kitem,
     * {code, value, 0}). */
    int (*kitem_set)(int code, const void *value);
    /* Hal_Audio_Send(name, params, len): PLAT_AIN_SetPathConfig sends
     * "MuxInput" with a 0x38-byte {source, port} payload. */
    int (*audio_send)(const char *name, void *params, int len);
    void (*delay_ms)(unsigned ms);
};

const struct oa_audio_source_hal *oa_audio_source_hal(void);

#endif
