/* Recovered DEV_AMP_ID factory monitor (aurora 0x84e110 / 0x875458). */
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/amp_monitor.h"

static void log_msg(const oa_amp_monitor_ops *ops, const char *fmt, ...)
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

int oa_amp_monitor_init(oa_amp_monitor *m, const oa_amp_monitor_ops *ops,
                        const uint8_t image[OA_AD82587_REGISTERS])
{
    if (!m || !ops || !ops->read_regs || !ops->reinit || !ops->mute ||
        !image)
        return -EINVAL;
    memset(m, 0, sizeof *m);
    memcpy(m->expected, image, sizeof m->expected);
    return 0;
}

int oa_amp_monitor_check(oa_amp_monitor *m, const oa_amp_monitor_ops *ops)
{
    if (!m || !ops || !ops->read_regs || !ops->reinit || !ops->mute)
        return -EINVAL;

    uint8_t buf[3];
    if (ops->read_regs(ops->ud, 3, 3, buf) != 0) {
        m->read_failures++;
        log_msg(ops, "AMP monitor read regs 03..05 failed");
        return -EIO;
    }
    if (!memcmp(buf, m->expected + 3, sizeof buf))
        return 0;

    m->mismatches++;
    log_msg(ops, "AMP monitor mismatch %02x %02x %02x != %02x %02x %02x",
            buf[0], buf[1], buf[2],
            m->expected[3], m->expected[4], m->expected[5]);
    /* 0x8754ce -> object +0x44 then +0x70(obj,0); both are called even if
     * the first returns an error. */
    int rc = ops->reinit(ops->ud);
    if (rc != 0)
        log_msg(ops, "AMP monitor reinit failed (%d)", rc);
    rc = ops->mute(ops->ud, 0);
    if (rc != 0)
        log_msg(ops, "AMP monitor unmute failed (%d)", rc);
    return 1;
}
