/* Recovered DEV_GPIO earphone detector (aurora 0x00851544).
 *
 * Firmware behavior: resolve GPIO 117 once; then each loop waits 100 ms,
 * reads the level, and on a level change sends PLAT_OS_SendEvent(0, 17,
 * plugin, 0, 0). A failed read is counted and logged only for the first
 * failure; the loop continues. Profile_BoardType()==17 maps plugin to the
 * descriptor's normal level, otherwise to its active level.
 *
 * The audio routing triggered by event 17 is not implemented here.
 */
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/earphone.h"

static void log_msg(const oa_earphone_ops *ops, const char *fmt, ...)
{
    if (!ops || !ops->log)
        return;
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    ops->log(ops->ud, "%s", buf);
}

int oa_earphone_init(oa_earphone *e, const oa_earphone_ops *ops,
                     int board_type, int initial_level)
{
    if (!e || !ops || !ops->descriptor || !ops->get_level ||
        !ops->event || !ops->delay_ms)
        return -EINVAL;
    memset(e, 0, sizeof *e);

    int pin = 0, normal = 0, active = 0;
    if (ops->descriptor(ops->ud, OA_EARPHONE_GPIO_ID,
                        &pin, &normal, &active) != 1) {
        log_msg(ops, "EARPHONE GPIO %d descriptor missing", OA_EARPHONE_GPIO_ID);
        return -ENODEV;
    }
    e->have_desc = 1;
    e->pin = pin;
    /* 0x008515d4: board type 17 uses the normal level for plugin; every
     * other board uses the active level. */
    if (board_type == 17) {
        e->plugin_level = normal;
        e->plugout_level = active;
    } else {
        e->plugin_level = active;
        e->plugout_level = normal;
    }
    if (initial_level >= 0) {
        e->last_level = initial_level;
        e->initialized = 1;
    } else {
        e->last_level = -1;
    }
    return 0;
}

int oa_earphone_poll(oa_earphone *e, const oa_earphone_ops *ops)
{
    if (!e || !e->have_desc || !ops || !ops->get_level ||
        !ops->event || !ops->delay_ms)
        return -EINVAL;

    ops->delay_ms(ops->ud, 100);
    int level = 0;
    if (ops->get_level(ops->ud, e->pin, &level) != 0) {
        if (e->fail_count == 0)
            log_msg(ops, "EARPHONE GPIO %d read failed", e->pin);
        e->fail_count++;
        return -EIO;
    }
    e->fail_count = 0;
    if (e->initialized && level == e->last_level)
        return 0;

    int plugin;
    if (level == e->plugin_level)
        plugin = 1;
    else if (level == e->plugout_level)
        plugin = 0;
    else
        return 0; /* neither recovered descriptor level: no event */

    e->last_level = level;
    e->initialized = 1;
    e->plugin = plugin;
    ops->event(ops->ud, plugin);
    log_msg(ops, "EARPHONE %s", plugin ? "plugin" : "plugout");
    return 1;
}
