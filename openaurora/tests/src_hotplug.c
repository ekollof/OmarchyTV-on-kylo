/* Host regression for the recovered DEV_SRC_HDMIHotPlug one-shot pulse.
 * Uses injected descriptors; no GPIO or device access. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/src_hotplug.h"

struct ev { int pin, level; };
static struct ev ev[32];
static unsigned nev, delays;
static int mhl_level = 1, mhl_fail;
static char logbuf[4096];
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
    }
}

static int desc(void *ud, int gpio, int *pin, int *active)
{
    (void)ud;
    switch (gpio) {
    case 204: *pin = 10; *active = 1; return 1;
    case 205: *pin = 11; *active = 0; return 1;
    case 206: return 0;
    case 207: *pin = 13; *active = 1; return 1;
    case 193: *pin = 14; *active = 0; return 1;
    case 192: *pin = 20; *active = 0; return 1;
    default: return 0;
    }
}

static int get_level(void *ud, int pin, int *level)
{
    (void)ud;
    if (pin != 20 || mhl_fail) return -1;
    *level = mhl_level;
    return 0;
}

static void set_level(void *ud, int pin, int level)
{
    (void)ud;
    assert(nev < sizeof ev / sizeof ev[0]);
    ev[nev++] = (struct ev){pin, level};
}

static void delay_ms(void *ud, unsigned ms)
{
    (void)ud;
    assert(ms == 500);
    delays++;
}

int main(void)
{
    const oa_src_hotplug_ops ops = {
        .descriptor = desc, .get_level = get_level,
        .set_level = set_level, .delay_ms = delay_ms, .log = log_msg
    };
    assert(oa_src_hdmi_hotplug_pulse(&ops) == 4);
    assert(nev == 8 && delays == 1);
    static const struct ev want[] = {
        {10, 0}, {11, 1}, {13, 0}, {14, 1},
        {10, 1}, {11, 0}, {13, 1}, {14, 0},
    };
    for (unsigned i = 0; i < nev; i++) {
        assert(ev[i].pin == want[i].pin && ev[i].level == want[i].level);
    }
    assert(strstr(logbuf, "Not found HDMI3 HotPlug Definition"));
    assert(strstr(logbuf, "MHL CD_SENSE Is High"));
    assert(strstr(logbuf, "End toggle PIN=10, Active level=1"));
    assert(strstr(logbuf, "Toogle HDMI1 HotPlug Successfully"));

    nev = delays = 0;
    logat = 0;
    logbuf[0] = 0;
    mhl_fail = 1;
    assert(oa_src_hdmi_hotplug_pulse(&ops) == 4);
    assert(strstr(logbuf, "Get CDSense level of pin 20 failed"));

    mhl_fail = 0;
    mhl_level = 0;
    nev = delays = 0;
    assert(oa_src_hdmi_hotplug_pulse(&ops) == 4);
    assert(!strstr(logbuf, "MHL CD_SENSE Is High"));

    oa_src_hotplug_ops bad = ops;
    bad.descriptor = NULL;
    assert(oa_src_hdmi_hotplug_pulse(&bad) == -1);
    puts("ok src hotplug: inactive -> 500 ms -> active over GPIO 204/205/206/207/193, "
         "MHL 192 level/log and missing-descriptor branches");
    return 0;
}
