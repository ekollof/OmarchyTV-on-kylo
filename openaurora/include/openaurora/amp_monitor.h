#ifndef OPENAURORA_AMP_MONITOR_H
#define OPENAURORA_AMP_MONITOR_H

#include <stdint.h>

#include "openaurora/audio_amp.h"

/* Recovered DEV_AMP_ID factory monitor (aurora 0x84e110 / 0x875458).
 *
 * The vendor task waits 15 s, then every 2.5 s calls AMP_AD82587_IsAlive:
 * read amp registers 03..05 and compare them with the retained configured
 * image at 0x017640e8+3..5. On mismatch it calls AMP_AD82587_Init
 * (0x874ef0) and AMP_AD82587_Mute (0x874e50, level 0); the subwoofer
 * IsAlive is a literal return 1.
 *
 * The vendor read wrapper's 0/1 convention is inverted/ambiguous relative to
 * DEV_IIC_Read's 0-success contract, so this helper uses the recovered
 * intended contract: read_regs returns 0 on success, and the monitor treats a
 * read failure as a logged error rather than reinitializing on stale data.
 */
typedef struct {
    void *ud;
    /* Read count bytes starting at amp register first; 0 on success. */
    int (*read_regs)(void *ud, uint8_t first, uint8_t count, uint8_t *out);
    int (*reinit)(void *ud);
    int (*mute)(void *ud, int on);
    void (*log)(void *ud, const char *fmt, ...);
} oa_amp_monitor_ops;

typedef struct {
    uint8_t expected[OA_AD82587_REGISTERS];
    unsigned read_failures;
    unsigned mismatches;
} oa_amp_monitor;

int oa_amp_monitor_init(oa_amp_monitor *m, const oa_amp_monitor_ops *ops,
                        const uint8_t image[OA_AD82587_REGISTERS]);

/* One check. 0 = matching, 1 = mismatch and recovery attempted, -EIO = read
 * failure (no recovery), -EINVAL = bad arguments. */
int oa_amp_monitor_check(oa_amp_monitor *m, const oa_amp_monitor_ops *ops);

#endif
