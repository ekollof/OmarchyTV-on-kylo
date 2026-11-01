#include "openaurora/audio_curve.h"

int oa_audio_curve_from_aq(oa_audio_curve *out, const void *aq, size_t size, int profile_curve)
{
    if (!out || !aq || size != OA_AUDIO_AQ_SIZE) return -1;
    const uint8_t *bytes = aq;
    size_t offset = profile_curve == 1 ? 0x1b8u : 0x24u;
    oa_audio_curve decoded;
    for (size_t i = 0; i < OA_AUDIO_CURVE_POINTS; i++) {
        const uint8_t *p = bytes + offset + 4 * i;
        /* Equivalent to the instruction-verified word load / halfword store;
         * no alignment requirement or host-endian dependency. */
        decoded.points[i] = (uint16_t)((unsigned)p[0] | (unsigned)p[1] << 8);
    }
    *out = decoded;
    return 0;
}

int oa_audio_curve_value(const oa_audio_curve *curve, unsigned volume, uint16_t *value)
{
    if (!curve || !value) return -1;
    *value = curve->points[volume > 100 ? 100 : volume];
    return 0;
}
