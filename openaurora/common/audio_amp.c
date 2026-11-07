#include <errno.h>
#include <string.h>
#include "openaurora/audio_amp.h"

/* Initial AD82587 register image, copied from the shipping aurora.elf .data
 * initializer at virtual 0x017640e8 (file offset 0x017440e8; .data starts at
 * 0x017563c0/0x017363c0). The vendor Init/Reset/Mute code patches this
 * retained global and writes all 38 registers; this is the cold-boot
 * baseline, not the live stock image (the live 03..05 read 1b/05/05). It is
 * recovered from the firmware, not invented; FNV-1a-32 = 0xab635b86. */
const uint8_t oa_ad82587_baseline[OA_AD82587_REGISTERS] = {
    0x01, 0x01, 0x11, 0x0e, 0x0d, 0x0d, 0x81, 0x00, 0x6a, 0x0d,
    0x00, 0x0f, 0x36, 0x80, 0x00, 0x00, 0x08, 0x00, 0x00, 0x06,
    0x70, 0x00, 0x00, 0x14, 0xc0, 0x00, 0x00, 0x00, 0x1a, 0x00,
    0x00, 0x53, 0x00, 0x10, 0x02, 0x00, 0x00, 0x00
};

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

int oa_ad82587_balance_regs(int balance, uint8_t *reg4, uint8_t *reg5)
{
    if (!reg4 || !reg5 || balance < 0 || balance > 100)
        return -EINVAL;
    if (balance < 50) {
        *reg4 = 0;
        *reg5 = (uint8_t)(127 - (balance * 127) / 50);
    } else if (balance == 50) {
        *reg4 = 0;
        *reg5 = 0;
    } else {
        *reg4 = (uint8_t)(127 - ((100 - balance) * 127) / 50);
        *reg5 = 0;
    }
    return 0;
}
