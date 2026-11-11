/* Host regression for the recovered earphone detector (aurora 0x00851544). */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/earphone.h"

static int levels[16];
static int level_count, level_at;
static int fail_reads;
static int desc_present;
static int desc_pin = 300, desc_normal, desc_active = 1;
static int events[8], event_count;
static unsigned delays;
static char logbuf[2048];
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

static int descriptor(void *ud, int id, int *pin, int *normal, int *active)
{
    (void)ud;
    assert(id == OA_EARPHONE_GPIO_ID);
    if (!desc_present) return 0;
    *pin = desc_pin;
    *normal = desc_normal;
    *active = desc_active;
    return 1;
}

static int get_level(void *ud, int pin, int *level)
{
    (void)ud;
    assert(pin == desc_pin);
    if (fail_reads) {
        fail_reads--;
        return -EIO;
    }
    assert(level_at < level_count);
    *level = levels[level_at++];
    return 0;
}

static void event(void *ud, int plugin)
{
    (void)ud;
    assert(event_count < (int)(sizeof events / sizeof events[0]));
    events[event_count++] = plugin;
}

static void delay_ms(void *ud, unsigned ms)
{
    (void)ud;
    assert(ms == 100);
    delays++;
}

static void reset(void)
{
    level_count = level_at = 0;
    fail_reads = 0;
    desc_present = 1;
    desc_normal = 0;
    desc_active = 1;
    event_count = 0;
    delays = 0;
    logat = 0;
    logbuf[0] = 0;
}

static void add(int level)
{
    assert(level_count < (int)(sizeof levels / sizeof levels[0]));
    levels[level_count++] = level;
}

int main(void)
{
    const oa_earphone_ops ops = {
        .descriptor = descriptor, .get_level = get_level,
        .event = event, .delay_ms = delay_ms, .log = log_msg
    };
    oa_earphone e;

    /* Default board: plugin = active level 1, plugout = normal 0. */
    reset();
    add(1); add(1); add(0); add(0);
    assert(oa_earphone_init(&e, &ops, 0, -1) == 0);
    assert(oa_earphone_poll(&e, &ops) == 1);
    assert(oa_earphone_poll(&e, &ops) == 0);
    assert(oa_earphone_poll(&e, &ops) == 1);
    assert(oa_earphone_poll(&e, &ops) == 0);
    assert(event_count == 2 && events[0] == 1 && events[1] == 0);
    assert(delays == 4 && level_at == 4);
    assert(strstr(logbuf, "EARPHONE plugin") && strstr(logbuf, "EARPHONE plugout"));

    /* Board type 17 swaps plugin/plugout levels. */
    reset();
    add(0); add(1);
    assert(oa_earphone_init(&e, &ops, 17, -1) == 0);
    assert(oa_earphone_poll(&e, &ops) == 1);
    assert(oa_earphone_poll(&e, &ops) == 1);
    assert(event_count == 2 && events[0] == 1 && events[1] == 0);

    /* Read failure is counted and logged once, then recovery works. */
    reset();
    fail_reads = 2;
    add(1);
    assert(oa_earphone_init(&e, &ops, 0, -1) == 0);
    assert(oa_earphone_poll(&e, &ops) == -EIO);
    assert(oa_earphone_poll(&e, &ops) == -EIO);
    assert(e.fail_count == 2);
    assert(oa_earphone_poll(&e, &ops) == 1);
    assert(e.fail_count == 0 && event_count == 1 && events[0] == 1);
    assert(strstr(logbuf, "EARPHONE GPIO 300 read failed"));
    assert(!strstr(logbuf, "read failed\nEARPHONE GPIO 300 read failed"));

    /* An "other" level is ignored and does not update the last level. */
    reset();
    add(2); add(1);
    assert(oa_earphone_init(&e, &ops, 0, -1) == 0);
    assert(oa_earphone_poll(&e, &ops) == 0);
    assert(oa_earphone_poll(&e, &ops) == 1);
    assert(event_count == 1 && events[0] == 1);

    /* Seeding the initial level suppresses the matching first event. */
    reset();
    add(0); add(1);
    assert(oa_earphone_init(&e, &ops, 0, 0) == 0);
    assert(oa_earphone_poll(&e, &ops) == 0);
    assert(oa_earphone_poll(&e, &ops) == 1);
    assert(event_count == 1 && events[0] == 1);

    /* Missing descriptor and bad arguments fail closed. */
    reset();
    desc_present = 0;
    assert(oa_earphone_init(&e, &ops, 0, -1) == -ENODEV);
    assert(strstr(logbuf, "descriptor missing"));
    assert(oa_earphone_init(NULL, &ops, 0, -1) == -EINVAL);
    assert(oa_earphone_init(&e, NULL, 0, -1) == -EINVAL);
    assert(oa_earphone_poll(&e, &ops) == -EINVAL);
    oa_earphone_ops bad = ops;
    bad.event = NULL;
    assert(oa_earphone_init(&e, &bad, 0, -1) == -EINVAL);

    puts("ok earphone: GPIO117 descriptor, 100 ms debounce, board17 swap, "
         "plugin/plugout event, read-failure log/recovery, other-level and "
         "initial-level handling");
    return 0;
}
