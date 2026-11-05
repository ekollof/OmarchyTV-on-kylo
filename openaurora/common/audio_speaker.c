/* Speaker level/balance clamp policy. See audio_speaker.h. No device access. */
#include "openaurora/audio_speaker.h"

#include <errno.h>

int oa_audio_speaker_map(unsigned level, int balance,
                         struct oa_audio_speaker_state *out)
{
    if (!out) return -EINVAL;
    unsigned clamped = level > 99 ? 100 : level;
    int b = balance;
    if (b < -50) b = -50;
    if (b > 50) b = 50;
    out->level = clamped;
    out->balance = (unsigned)(b + 50);
    out->left = clamped;
    out->right = clamped;
    return 0;
}
