/* MB230 AQ volume-table decoding. Pure data operation; no HAL/device access.
 * Source: AUDIO-VOLUME-2026-11-01, PLAT_VAUD_ReadAQFile/InitAQSetting/
 * SetVolumeTable. The 16-bit values are DSP table entries, not amp registers.
 */
#ifndef OPENAURORA_AUDIO_CURVE_H
#define OPENAURORA_AUDIO_CURVE_H
#include <stddef.h>
#include <stdint.h>

#define OA_AUDIO_AQ_SIZE 1668u
#define OA_AUDIO_CURVE_POINTS 101u
typedef struct { uint16_t points[OA_AUDIO_CURVE_POINTS]; } oa_audio_curve;

/* Exactly the target AQ layout. Profile VolumeCurve == 1 selects aggressive;
 * every other value selects normal, matching the vendor branch. Entries are
 * little-endian 32-bit words narrowed to 16 bits, including any high-word bits.
 * The output is unchanged on error. This decodes only the curve, not all AQ
 * processing, profile selection, default fallback or physical gain policy.
 */
int oa_audio_curve_from_aq(oa_audio_curve *out, const void *aq, size_t size, int profile_curve);
/* Clamp unsigned values above 100, as the UI-to-speaker path does. The caller
 * must supply a successfully decoded/initialized curve. No guessed defaults.
 */
int oa_audio_curve_value(const oa_audio_curve *curve, unsigned volume, uint16_t *value);
#endif
