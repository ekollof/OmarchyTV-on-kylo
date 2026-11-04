#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/audio_amp.h"

static void triplet(const uint8_t *p, unsigned a, unsigned b, unsigned c)
{ assert(p[0] == a && p[1] == b && p[2] == c); }

int main(void)
{
    uint8_t baseline[OA_AD82587_REGISTERS];
    for (unsigned i = 0; i < sizeof baseline; i++) baseline[i] = (uint8_t)(0x80 + i);
    struct { uint8_t before, image[OA_AD82587_REGISTERS], after; } out;
    memset(&out, 0xa5, sizeof out);
    oa_ad82587_profile p = {
        .speaker_power = "12Watt", .prescale = 0x123, .channel1 = 0x245,
        .channel2 = UINT32_MAX, .fine_tune = 0x678, .power_clip = 0xab123456,
        .speaker_drc = 499, .state_control4 = -1, .limiter_attack = -1,
        .release_threshold = 0x123456
    };
    assert(!oa_ad82587_configure(out.image, baseline, &p));
    assert(out.before == 0xa5 && out.after == 0xa5);
    assert(out.image[3] == 0x23 && out.image[4] == 0x45 && out.image[5] == 0xff);
    assert(out.image[7] == 0xb7 && out.image[8] == 0x6a && out.image[0x17] == 0x78);
    assert(out.image[0x20] == 0 && out.image[0x21] == 0x10);
    triplet(out.image + 0x13, 0x12, 0x34, 0x56);
    triplet(out.image + 0x10, 4, 0x1e, 0xb0);
    triplet(out.image + 0x22, 4, 0x1e, 0xb0); /* release override ignored below500 */
    for (unsigned i = 0; i < sizeof baseline; i++) {
        int changed = i == 3 || i == 4 || i == 5 || i == 7 || i == 8 ||
            (i >= 0x10 && i <= 0x15) || i == 0x17 || (i >= 0x20 && i <= 0x24);
        if (!changed) assert(out.image[i] == baseline[i]);
        assert(baseline[i] == (uint8_t)(0x80 + i));
    }
    puts("ok amp image: profile byte narrowing, default DRC, exact untouched registers and guards");

    p.speaker_power = "6Watt"; p.speaker_drc = 500;
    p.state_control4 = 0x100; p.limiter_attack = -2; p.release_threshold = -1;
    assert(!oa_ad82587_configure(out.image, baseline, &p));
    assert(out.image[7] == 0 && out.image[8] == 0xfe);
    triplet(out.image + 0x13, 8, 0, 0);
    triplet(out.image + 0x10, 0, 1, 0xf4);
    triplet(out.image + 0x22, 0, 1, 0xf4);
    p.speaker_power = "8Watt_12V"; p.speaker_drc = UINT32_MAX;
    p.release_threshold = (int32_t)-2;
    assert(!oa_ad82587_configure(out.image, baseline, &p));
    triplet(out.image + 0x13, 8, 0, 0);
    triplet(out.image + 0x10, 0xff, 0xff, 0xff);
    triplet(out.image + 0x22, 0xff, 0xff, 0xfe);
    p.speaker_power = "6watt"; p.speaker_drc = 0;
    assert(!oa_ad82587_configure(out.image, baseline, &p));
    triplet(out.image + 0x13, 0x12, 0x34, 0x56); /* case-sensitive power names */
    triplet(out.image + 0x22, 4, 0x1e, 0xb0);
    puts("ok amp image: exact power exceptions, unsigned DRC boundary and signed override sentinels");

    uint8_t alias[OA_AD82587_REGISTERS];
    memcpy(alias, baseline, sizeof alias);
    assert(!oa_ad82587_configure(alias, alias, &p));
    assert(!memcmp(alias, out.image, sizeof alias));
    p.state_control4 = 0x42;
    assert(!oa_ad82587_configure(alias, alias, &p));
    assert(alias[7] == 0x42);
    p.state_control4 = -1;
    assert(!oa_ad82587_configure(alias, alias, &p));
    assert(alias[7] == 0x72); /* retained image, not a fresh0x87 baseline */
    uint8_t saved[OA_AD82587_REGISTERS];
    memcpy(saved, out.image, sizeof saved);
    assert(oa_ad82587_configure(NULL, baseline, &p) == -EINVAL);
    assert(oa_ad82587_configure(out.image, NULL, &p) == -EINVAL);
    assert(oa_ad82587_configure(out.image, baseline, NULL) == -EINVAL);
    p.speaker_power = NULL;
    assert(oa_ad82587_configure(out.image, baseline, &p) == -EINVAL);
    assert(!memcmp(saved, out.image, sizeof saved));
    puts("ok amp image: in-place output and atomic invalid-input rejection; no hardware access");
    return 0;
}
