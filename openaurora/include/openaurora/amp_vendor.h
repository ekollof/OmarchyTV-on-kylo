/* Vendor-backed amp transport composition for oa_amp_init.
 *
 * Recovers the DEV_GPIO_SetLevel/SetDirection split:
 *   pin < 200  -> Hal_Gpio_Write(pin, level) / Hal_Gpio_Config(pin, direction)
 *   pin >= 200 -> PLAT_SYS_SetStbcGpio((pin + 0x38) & 0xff,
 *                                      level ? 2 : 3)
 * PLAT_SYS_SetStbcGpio forwards to NDAL_VideoExt_STBC_SetStbcGpio(pin, command),
 * whose return is ndcommon_v4l2_ioctl's vendor boolean (1 success, 0 failure)
 * and whose device fd is the process-global one installed by
 * NDAL_VideoExt_OpenDevice(0, index). It therefore fails closed when the
 * caller has not run the platform video open.
 *
 * This layer owns no device: every transport is injected and copied. The
 * defaults call the linkable vendor symbols on the TV build; the host build
 * returns failure. The caller establishes authority, runs the platform
 * GPIO/video prelude first, keeps the borrowed oa_amp_i2c session alive, and
 * executes oa_amp_init on a blocking worker. No register/GPIO write happens
 * here until oa_amp_init invokes the callbacks. */
#ifndef OPENAURORA_AMP_VENDOR_H
#define OPENAURORA_AMP_VENDOR_H
#include <stdint.h>
#include "openaurora/amp_board.h"
#include "openaurora/amp_i2c.h"

typedef struct {
    void *context;
    /* 0 = success, negative errno on failure (Hal_Gpio_Write/Config). */
    int (*gpio_write)(void *context, uint32_t pin, uint32_t level);
    int (*gpio_config)(void *context, uint32_t pin, uint32_t direction);
    /* Vendor boolean: nonzero = success (NDAL_VideoExt_STBC_SetStbcGpio). */
    int (*stbc_set)(void *context, uint32_t pin, uint32_t command);
    /* Vendor boolean. NULL with a session uses oa_amp_i2c_write_cb; NULL
     * without a session fails closed. */
    int (*i2c_write)(void *context, unsigned reg, const uint8_t *value);
    void (*delay_ms)(void *context, unsigned milliseconds);
    /* Subwoofer helpers are not recovered for this board; NULL leaves the
     * subwoofer disabled and performs no subwoofer I/O. */
    uint32_t (*subwoofer_enabled)(void *context);
    void (*subwoofer_enable)(void *context, int enabled);
    void (*subwoofer_init)(void *context);
    void (*prelude)(void *context);
} oa_amp_vendor_io;

typedef struct {
    oa_amp_vendor_io io;
    oa_amp_i2c *i2c;             /* borrowed through oa_amp_init's return */
    const oa_amp_board *board;   /* borrowed reset/mute records */
} oa_amp_vendor;

/* Fill `board` and `out` with the vendor composition. `vendor`, `board`,
 * `out` and any injected context stay alive and borrowed through
 * oa_amp_init's return. Returns 0 or -EINVAL. */
int oa_amp_vendor_init(oa_amp_vendor *vendor, const oa_amp_vendor_io *io,
                       oa_amp_i2c *i2c, const oa_amp_board *board,
                       oa_amp_init_io *out);

#endif
