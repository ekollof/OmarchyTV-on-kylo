/* Pure PLAT_VAUD_SetSpeakerVolumeBalance clamp/state policy. No HAL/device. */
#include "openaurora/audio_speaker.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>

static void check(unsigned level, int balance, unsigned want_level,
                  unsigned want_balance)
{
    struct oa_audio_speaker_state state;
    assert(!oa_audio_speaker_map(level, balance, &state));
    assert(state.level == want_level && state.balance == want_balance &&
        state.left == want_level && state.right == want_level);
}

int main(void)
{
    check(0, -50, 0, 0);
    check(0, -100, 0, 0);
    check(50, -51, 50, 0);
    check(50, 0, 50, 50);
    check(99, -1, 99, 49);
    check(100, 1, 100, 51);
    check(101, 50, 100, 100);
    check(UINT_MAX, 100, 100, 100);
    check(50, -50, 50, 0);
    assert(oa_audio_speaker_map(50, 0, NULL) == -EINVAL);
    puts("ok audio speaker: level 0..100, balance -50..50, HAL balance 0..100, equal channels");
    return 0;
}
