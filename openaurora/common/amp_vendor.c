#include <errno.h>
#include <string.h>
#include <time.h>
#include "openaurora/amp_vendor.h"

#ifndef OA_NO_VENDOR
/* Linkable vendor wrappers. Hal_Gpio_Write/Config return 0 on success and -1
 * on failure; DEV_GPIO_SetLevel maps that to a vendor boolean. They issue the
 * vendor 670a claim before 670e/670c, so they are not view-only and must run
 * only under a caller that owns the GPIO device. */
extern int Hal_Gpio_Write(unsigned pin, int level);
extern int Hal_Gpio_Config(unsigned pin, int direction);
/* Returns the ndcommon v4l2 ioctl boolean: nonzero success, 0 failure. */
extern int NDAL_VideoExt_STBC_SetStbcGpio(unsigned pin, unsigned command);
#endif

static int default_gpio_write(void *context, uint32_t pin, uint32_t level)
{
    (void)context;
#ifndef OA_NO_VENDOR
    return Hal_Gpio_Write(pin, (int)level) == 0 ? 0 : -EIO;
#else
    (void)pin;
    (void)level;
    return -ENOTSUP;
#endif
}

static int default_gpio_config(void *context, uint32_t pin, uint32_t direction)
{
    (void)context;
#ifndef OA_NO_VENDOR
    return Hal_Gpio_Config(pin, (int)direction) == 0 ? 0 : -EIO;
#else
    (void)pin;
    (void)direction;
    return -ENOTSUP;
#endif
}

static int default_stbc_set(void *context, uint32_t pin, uint32_t command)
{
    (void)context;
#ifndef OA_NO_VENDOR
    return NDAL_VideoExt_STBC_SetStbcGpio(pin, command);
#else
    (void)pin;
    (void)command;
    return 0;
#endif
}

static void default_delay(void *context, unsigned milliseconds)
{
    (void)context;
    if (!milliseconds)
        return;
    const struct timespec pause = {
        .tv_sec = milliseconds / 1000u,
        .tv_nsec = (long)(milliseconds % 1000u) * 1000000L
    };
    (void)nanosleep(&pause, NULL);
}

/* oa_amp_board_io callbacks. The board context is oa_amp_vendor. */
static const oa_amp_gpio *v_gpio(void *context, unsigned function)
{
    oa_amp_vendor *v = context;
    return function ? &v->board->mute : &v->board->reset;
}

static void v_prelude(void *context)
{
    oa_amp_vendor *v = context;
    if (v->io.prelude)
        v->io.prelude(v->io.context);
}

static void v_direction(void *context, uint32_t pin, uint32_t direction)
{
    oa_amp_vendor *v = context;
    if (pin >= 200u) {
        /* DEV_GPIO_SetDirection selects the STBC path only for direction 0;
         * the direction-1 path is a vendor no-op success. The direction-0
         * STBC call's second argument is not recovered, so it is not issued.
         * oa_amp_init only requests direction 1 for the mute pin. */
        return;
    }
    (void)v->io.gpio_config(v->io.context, pin, direction);
}

static int v_level(void *context, uint32_t pin, uint32_t value)
{
    oa_amp_vendor *v = context;
    if (pin >= 200u) {
        /* DEV_GPIO_SetLevel: level 0 -> command 3, level 1 -> command 2. */
        int command = value ? 2 : 3;
        int ok = v->io.stbc_set(v->io.context,
                                (pin + 0x38u) & 0xffu, (uint32_t)command);
        return ok ? 1 : 0;
    }
    return v->io.gpio_write(v->io.context, pin, value) == 0 ? 1 : 0;
}

static void v_delay(void *context, unsigned milliseconds)
{
    oa_amp_vendor *v = context;
    if (v->io.delay_ms)
        v->io.delay_ms(v->io.context, milliseconds);
    else
        default_delay(v->io.context, milliseconds);
}

static int v_write(void *context, unsigned reg, const uint8_t *value)
{
    oa_amp_vendor *v = context;
    if (v->io.i2c_write)
        return v->io.i2c_write(v->io.context, reg, value) ? 1 : 0;
    if (v->i2c)
        return oa_amp_i2c_write_cb(v->i2c, reg, value);
    return 0;
}

static uint32_t v_sub_enabled(void *context)
{
    oa_amp_vendor *v = context;
    return v->io.subwoofer_enabled ?
           v->io.subwoofer_enabled(v->io.context) : 0;
}

static void v_sub_enable(void *context, int enabled)
{
    oa_amp_vendor *v = context;
    if (v->io.subwoofer_enable)
        v->io.subwoofer_enable(v->io.context, enabled);
}

static void v_sub_init(void *context)
{
    oa_amp_vendor *v = context;
    if (v->io.subwoofer_init)
        v->io.subwoofer_init(v->io.context);
}

int oa_amp_vendor_init(oa_amp_vendor *vendor, const oa_amp_vendor_io *io,
                       oa_amp_i2c *i2c, const oa_amp_board *board,
                       oa_amp_init_io *out)
{
    if (!vendor || !io || !board || !out)
        return -EINVAL;
    vendor->io = *io;
    vendor->i2c = i2c;
    vendor->board = board;
    if (!vendor->io.gpio_write)
        vendor->io.gpio_write = default_gpio_write;
    if (!vendor->io.gpio_config)
        vendor->io.gpio_config = default_gpio_config;
    if (!vendor->io.stbc_set)
        vendor->io.stbc_set = default_stbc_set;

    memset(out, 0, sizeof *out);
    out->context = vendor;
    out->prelude = v_prelude;
    out->gpio = v_gpio;
    out->direction = v_direction;
    out->level = v_level;
    out->delay_ms = v_delay;
    out->write = v_write;
    out->subwoofer_enabled = v_sub_enabled;
    out->subwoofer_enable = v_sub_enable;
    out->subwoofer_init = v_sub_init;
    return 0;
}
