#include <errno.h>
#include <string.h>
#include "openaurora/audio_amp.h"

static void be24(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 16);
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)value;
}

int oa_ad82587_configure(uint8_t out[OA_AD82587_REGISTERS],
    const uint8_t baseline[OA_AD82587_REGISTERS], const oa_ad82587_profile *p)
{
    if (!out || !baseline || !p || !p->speaker_power) return -EINVAL;
    uint8_t image[OA_AD82587_REGISTERS];
    memcpy(image, baseline, sizeof image);
    image[3] = (uint8_t)p->prescale;
    image[4] = (uint8_t)p->channel1;
    image[5] = (uint8_t)p->channel2;
    image[0x17] = (uint8_t)p->fine_tune;
    image[7] |= 0x30;
    if (p->state_control4 != -1) image[7] = (uint8_t)p->state_control4;
    image[8] = p->limiter_attack == -1 ? 0x6a : (uint8_t)p->limiter_attack;
    be24(image + 0x13, p->power_clip);
    if (!strcmp(p->speaker_power, "6Watt") || !strcmp(p->speaker_power, "8Watt_12V"))
        be24(image + 0x13, 0x080000);
    image[0x20] = 0;
    image[0x21] = 0x10;
    uint32_t drc = p->speaker_drc < 500 ? 0x041eb0 : p->speaker_drc;
    be24(image + 0x10, drc);
    if (p->speaker_drc >= 500 && p->release_threshold != -1)
        drc = (uint32_t)p->release_threshold;
    be24(image + 0x22, drc);
    memcpy(out, image, sizeof image);
    return 0;
}
