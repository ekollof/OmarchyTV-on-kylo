/* Host regression for the recovered DEV_AMP_ID monitor check. */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/amp_monitor.h"

static uint8_t image[OA_AD82587_REGISTERS];
static uint8_t regs[3];
static int fail_read;
static int reinit_rc, mute_rc;
static int calls[8], call_count;
static char logbuf[1024];
static size_t logat;

static void log_msg(void *ud, const char *fmt, ...)
{
    (void)ud;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(logbuf + logat, sizeof logbuf - logat, fmt, ap);
    va_end(ap);
    if (n > 0) {
        logat += (size_t)n;
        if (logat >= sizeof logbuf) logat = sizeof logbuf - 1;
        logbuf[logat] = 0;
    }
}

static int read_regs(void *ud, uint8_t first, uint8_t count, uint8_t *out)
{
    (void)ud;
    assert(first == 3 && count == 3 && out);
    calls[call_count++] = 1;
    if (fail_read) return -EIO;
    memcpy(out, regs, 3);
    return 0;
}

static int reinit(void *ud)
{
    (void)ud;
    calls[call_count++] = 2;
    return reinit_rc;
}

static int mute(void *ud, int on)
{
    (void)ud;
    assert(on == 0);
    calls[call_count++] = 3;
    return mute_rc;
}

static void reset(void)
{
    fail_read = 0;
    reinit_rc = mute_rc = 0;
    call_count = 0;
    logat = 0;
    logbuf[0] = 0;
    regs[0] = image[3];
    regs[1] = image[4];
    regs[2] = image[5];
}

int main(void)
{
    const oa_amp_monitor_ops ops = {
        .read_regs = read_regs, .reinit = reinit, .mute = mute, .log = log_msg
    };
    oa_amp_monitor m;
    for (unsigned i = 0; i < OA_AD82587_REGISTERS; i++)
        image[i] = (uint8_t)i;
    reset();

    assert(oa_amp_monitor_init(&m, &ops, image) == 0);
    assert(m.expected[3] == 3 && m.expected[5] == 5);
    assert(oa_amp_monitor_check(&m, &ops) == 0);
    assert(call_count == 1 && calls[0] == 1);
    assert(m.read_failures == 0 && m.mismatches == 0);

    /* Mismatch: Init then Mute(0), even if reinit fails. */
    reset();
    regs[0] = 0x1b; regs[1] = 0x05; regs[2] = 0x05;
    reinit_rc = -EIO;
    assert(oa_amp_monitor_check(&m, &ops) == 1);
    assert(call_count == 3 && calls[0] == 1 && calls[1] == 2 && calls[2] == 3);
    assert(m.mismatches == 1);
    assert(strstr(logbuf, "AMP monitor mismatch 1b 05 05 != 03 04 05"));
    assert(strstr(logbuf, "AMP monitor reinit failed"));

    /* Read failure: log/count, no reinit/mute. */
    reset();
    fail_read = 1;
    assert(oa_amp_monitor_check(&m, &ops) == -EIO);
    assert(call_count == 1 && calls[0] == 1);
    assert(m.read_failures == 1 && m.mismatches == 1);
    assert(strstr(logbuf, "read regs 03..05 failed"));

    /* Bad arguments. */
    assert(oa_amp_monitor_init(NULL, &ops, image) == -EINVAL);
    assert(oa_amp_monitor_init(&m, NULL, image) == -EINVAL);
    assert(oa_amp_monitor_init(&m, &ops, NULL) == -EINVAL);
    oa_amp_monitor_ops bad = ops;
    bad.read_regs = NULL;
    assert(oa_amp_monitor_init(&m, &bad, image) == -EINVAL);
    assert(oa_amp_monitor_check(&m, &bad) == -EINVAL);

    puts("ok amp monitor: regs03..05 compare, Init->Mute(0) recovery, "
         "read-failure isolation and invalid inputs");
    return 0;
}
