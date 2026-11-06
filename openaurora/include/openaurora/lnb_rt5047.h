/* RT5047 LNB initialisation logic (SAL_LNB_Initialize @0x00877384).
 *
 * The vendor once-per-boot sequence is:
 *   desc = DEV_GPIO_GetDescription(5);   // function name "LNB_EN"
 *   if (desc) DEV_GPIO_SetLevel(desc.pin, desc.level);
 * The GPIO descriptor comes from the Novatek /dev/ntgpio ioctl; the TV
 * adapter supplies it. There is no I2C in this path.
 */
#ifndef OPENAURORA_LNB_RT5047_H
#define OPENAURORA_LNB_RT5047_H
#include <stdint.h>

typedef struct {
    /* 1 = LNB_EN present (pin/level filled), 0 = absent, <0 = error. */
    int (*get_lnb_en)(void *ud, uint32_t *pin, uint32_t *level);
    /* 0 = success. */
    int (*set_level)(void *ud, uint32_t pin, uint32_t level);
} oa_rt5047_ops;

/* 1 = initialized, 0 = no LNB_EN descriptor, <0 = error. */
int oa_rt5047_init(const oa_rt5047_ops *ops, void *ud);

#endif
