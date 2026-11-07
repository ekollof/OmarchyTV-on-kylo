/* AD82587 board adapter for the injected oa_amp_init sequencer.
 *
 * amp_init expects vendor-boolean callbacks (nonzero = success). The recovered
 * native codecs (amp_i2c, amp_gpio_query, amp_stbc) return 0 on success and
 * negative errno on failure. This adapter is the explicit mapping boundary:
 * normal GPIO for reset114, STBC/MCU for mute208, I2C for the 38 register
 * writes. It owns no device and performs no I/O by itself; the caller injects
 * transports and establishes device authority. */
#ifndef OPENAURORA_AMP_BOARD_H
#define OPENAURORA_AMP_BOARD_H
#include <stdint.h>
#include "openaurora/amp_init.h"

typedef struct {
    void *context;
    /* Optional override for the two resource records. NULL returns the
     * recovered defaults below (reset114/mute208). */
    const oa_amp_gpio *(*gpio)(void *context, unsigned function);
    void (*direction)(void *context, uint32_t pin, uint32_t direction);
    /* Normal GPIO write, pairs with the reset114 record. 0 = success. */
    int (*normal_level)(void *context, uint32_t pin, uint32_t level);
    /* MCU/STBC GPIO write, pairs with mute208. 0 = success. */
    int (*stbc_level)(void *context, uint32_t pin, uint32_t level);
    /* One-byte I2C register write. 0 = success. */
    int (*i2c_write)(void *context, unsigned reg, const uint8_t *value);
    /* Optional; NULL uses a native nanosleep for the vendor delays. */
    void (*delay_ms)(void *context, unsigned milliseconds);
    uint32_t (*subwoofer_enabled)(void *context);
    void (*subwoofer_enable)(void *context, int enabled);
    void (*subwoofer_init)(void *context);
    void (*prelude)(void *context);
} oa_amp_board_io;

typedef struct {
    oa_amp_board_io io;
    oa_amp_gpio reset;
    oa_amp_gpio mute;
} oa_amp_board;

/* Fill the recovered defaults: reset {114,1,0}, mute {208,1,0}. */
void oa_amp_board_defaults(oa_amp_board *board);

/* Build an amp_init I/O table whose context is `board`. The table borrows
 * `board` and its callback context until amp_init returns; the caller must
 * keep both alive and must not run two amp_init calls concurrently with the
 * same board. Returns 0 or -EINVAL. */
int oa_amp_board_init_io(oa_amp_board *board, const oa_amp_board_io *io,
                         oa_amp_init_io *out);

#endif
