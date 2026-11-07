#include <errno.h>
#include <string.h>
#include <time.h>
#include "openaurora/amp_board.h"

static const oa_amp_gpio *board_gpio(void *context, unsigned function)
{
    oa_amp_board *b = context;
    if (b->io.gpio)
        return b->io.gpio(b->io.context, function);
    return function ? &b->mute : &b->reset;
}

static void board_direction(void *context, uint32_t pin, uint32_t direction)
{
    oa_amp_board *b = context;
    if (b->io.direction)
        b->io.direction(b->io.context, pin, direction);
}

static int board_level(void *context, uint32_t pin, uint32_t value)
{
    oa_amp_board *b = context;
    int rc = -ENOTSUP;
    if (pin == 208 && b->io.stbc_level)
        rc = b->io.stbc_level(b->io.context, pin, value);
    else if (pin != 208 && b->io.normal_level)
        rc = b->io.normal_level(b->io.context, pin, value);
    return rc == 0 ? 1 : 0;
}

static void board_delay(void *context, unsigned milliseconds)
{
    oa_amp_board *b = context;
    if (b->io.delay_ms) {
        b->io.delay_ms(b->io.context, milliseconds);
        return;
    }
    if (milliseconds) {
        const struct timespec pause = {
            .tv_sec = milliseconds / 1000u,
            .tv_nsec = (long)(milliseconds % 1000u) * 1000000L
        };
        (void)nanosleep(&pause, NULL);
    }
}

static int board_write(void *context, unsigned reg, const uint8_t *value)
{
    oa_amp_board *b = context;
    return b->io.i2c_write &&
           b->io.i2c_write(b->io.context, reg, value) == 0 ? 1 : 0;
}

static uint32_t board_sub_enabled(void *context)
{
    oa_amp_board *b = context;
    return b->io.subwoofer_enabled ?
           b->io.subwoofer_enabled(b->io.context) : 0;
}

static void board_sub_enable(void *context, int enabled)
{
    oa_amp_board *b = context;
    if (b->io.subwoofer_enable)
        b->io.subwoofer_enable(b->io.context, enabled);
}

static void board_sub_init(void *context)
{
    oa_amp_board *b = context;
    if (b->io.subwoofer_init)
        b->io.subwoofer_init(b->io.context);
}

static void board_prelude(void *context)
{
    oa_amp_board *b = context;
    if (b->io.prelude)
        b->io.prelude(b->io.context);
}

void oa_amp_board_defaults(oa_amp_board *board)
{
    if (!board)
        return;
    memset(board, 0, sizeof *board);
    board->reset = (oa_amp_gpio){ 114, 1, 0 };
    board->mute = (oa_amp_gpio){ 208, 1, 0 };
}

int oa_amp_board_init_io(oa_amp_board *board, const oa_amp_board_io *io,
                         oa_amp_init_io *out)
{
    if (!board || !io || !out)
        return -EINVAL;
    board->io = *io;
    memset(out, 0, sizeof *out);
    out->context = board;
    out->prelude = board_prelude;
    out->gpio = board_gpio;
    out->direction = board_direction;
    out->level = board_level;
    out->delay_ms = board_delay;
    out->write = board_write;
    out->subwoofer_enabled = board_sub_enabled;
    out->subwoofer_enable = board_sub_enable;
    out->subwoofer_init = board_sub_init;
    return 0;
}
