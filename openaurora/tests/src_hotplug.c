/* Host regression for the recovered DEV_SRC_HDMIHotPlug one-shot pulse.
 * Uses injected descriptors and levels; no GPIO or device access.
 *
 * Recovered behavior (aurora 0x008fc3b0):
 *   - GPIO 204..207 and 193, inactive -> 500 ms -> active;
 *   - MHL CD sense 192 is checked at index 0 in both loops;
 *   - MHL high skips GPIO 204 in that loop and is re-read after the delay;
 *   - a missing descriptor logs "Not found HDMI%d";
 *   - the active level is written verbatim.
 */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/src_hotplug.h"

struct event { int pin, level; };
struct fake_desc { int id, pin, active, present; };
struct fake_level { int level, fail; };

static struct event ev[32];
static unsigned nev, delays;
static struct fake_desc descs[8];
static unsigned ndesc;
static struct fake_level mhl[4];
static unsigned nmhl, mhl_at;
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
        logbuf[logat] = 0;
    }
}

static void reset(void)
{
    nev = delays = 0;
    logat = 0;
    logbuf[0] = 0;
    mhl_at = 0;
    ndesc = 0;
    nmhl = 0;
}

static void add_desc(int id, int pin, int active)
{
    assert(ndesc < sizeof descs / sizeof descs[0]);
    descs[ndesc++] = (struct fake_desc){ id, pin, active, 1 };
}

static int desc(void *ud, int id, int *pin, int *active)
{
    (void)ud;
    for (unsigned i = 0; i < ndesc; i++)
        if (descs[i].id == id && descs[i].present) {
            *pin = descs[i].pin;
            *active = descs[i].active;
            return 1;
        }
    return 0;
}

static int get_level(void *ud, int pin, int *level)
{
    (void)ud;
    assert(pin == 300);
    if (mhl_at >= nmhl) return -1;
    if (mhl[mhl_at].fail) {
        mhl_at++;
        return -1;
    }
    *level = mhl[mhl_at++].level;
    return 0;
}

static void set_level(void *ud, int pin, int level)
{
    (void)ud;
    assert(nev < sizeof ev / sizeof ev[0]);
    ev[nev++] = (struct event){ pin, level };
}

static void delay_ms(void *ud, unsigned ms)
{
    (void)ud;
    assert(ms == 500);
    delays++;
}

static void stock(void)
{
    reset();
    add_desc(204, 274, 0);
    add_desc(205, 275, 0);
    add_desc(206, 276, 0);
    add_desc(207, 277, 0);
    add_desc(193, 0, 0);
    descs[4].present = 0; /* HDMI_HOTPLUG_5 absent */
    add_desc(192, 300, 0);
    descs[5].present = 0; /* MHL_CDSENSE absent */
}

static void expect(const struct event *want, unsigned n)
{
    assert(nev == n);
    for (unsigned i = 0; i < n; i++)
        assert(ev[i].pin == want[i].pin && ev[i].level == want[i].level);
}

int main(void)
{
    const oa_src_hotplug_ops ops = {
        .descriptor = desc, .get_level = get_level,
        .set_level = set_level, .delay_ms = delay_ms, .log = log_msg
    };

    /* Stock-like mapping from the live TV: HDMI1..4 -> 274..277, active low;
     * MHL_CDSENSE and HDMI_HOTPLUG_5 are absent. */
    stock();
    assert(oa_src_hdmi_hotplug_pulse(&ops) == 4);
    assert(delays == 1);
    static const struct event stock_want[] = {
        {274, 1}, {275, 1}, {276, 1}, {277, 1},
        {274, 0}, {275, 0}, {276, 0}, {277, 0},
    };
    expect(stock_want, 8);
    assert(strstr(logbuf, "#DEV-SRC#Not found HDMI5 HotPlug Definition"));
    assert(strstr(logbuf, "End toggle PIN=274, Active level=0"));
    assert(strstr(logbuf, "#DEV-SRC#Toogle HDMI1 HotPlug Successfully"));
    assert(!strstr(logbuf, "MHL CD_SENSE"));

    /* MHL high in both loops: GPIO 204 is never pulsed. */
    stock();
    descs[5].present = 1;
    mhl[0] = (struct fake_level){ 1, 0 };
    mhl[1] = (struct fake_level){ 1, 0 };
    nmhl = 2;
    assert(oa_src_hdmi_hotplug_pulse(&ops) == 3);
    static const struct event mhl_high_want[] = {
        {275, 1}, {276, 1}, {277, 1},
        {275, 0}, {276, 0}, {277, 0},
    };
    expect(mhl_high_want, 6);
    assert(strstr(logbuf, "#DEV-SRC#MHL CD_SENSE Is High"));

    /* MHL becomes high after the delay: 204 is driven inactive but not
     * active, matching the re-read. */
    stock();
    descs[5].present = 1;
    mhl[0] = (struct fake_level){ 0, 0 };
    mhl[1] = (struct fake_level){ 1, 0 };
    nmhl = 2;
    assert(oa_src_hdmi_hotplug_pulse(&ops) == 3);
    static const struct event mhl_late_want[] = {
        {274, 1}, {275, 1}, {276, 1}, {277, 1},
        {275, 0}, {276, 0}, {277, 0},
    };
    expect(mhl_late_want, 7);

    /* MHL read failure keeps processing 204; the second read is high. */
    stock();
    descs[5].present = 1;
    mhl[0] = (struct fake_level){ 0, 1 };
    mhl[1] = (struct fake_level){ 1, 0 };
    nmhl = 2;
    assert(oa_src_hdmi_hotplug_pulse(&ops) == 3);
    expect(mhl_late_want, 7);
    assert(strstr(logbuf, "DEV_SRC_HDMIHotPlug,Get CDSense level of pin 300 failed"));
    assert(strstr(logbuf, "#DEV-SRC#MHL CD_SENSE Is High"));

    /* Active-high and "other" descriptors: inactive is 0, active is written
     * verbatim; the device layer later rejects "other" (2). */
    stock();
    descs[0].active = 1;
    descs[1].active = 2;
    descs[2].present = 0; /* HDMI_HOTPLUG_3 missing */
    assert(oa_src_hdmi_hotplug_pulse(&ops) == 3);
    static const struct event mixed_want[] = {
        {274, 0}, {275, 0}, {277, 1},
        {274, 1}, {275, 2}, {277, 0},
    };
    expect(mixed_want, 6);
    assert(strstr(logbuf, "#DEV-SRC#Not found HDMI3 HotPlug Definition"));
    assert(strstr(logbuf, "End toggle PIN=275, Active level=2"));
    assert(strstr(logbuf, "#DEV-SRC#Toogle HDMI4 HotPlug Successfully"));

    /* Missing required operations are rejected before any hardware call. */
    oa_src_hotplug_ops bad = ops;
    bad.descriptor = NULL;
    assert(oa_src_hdmi_hotplug_pulse(&bad) == -1);
    bad = ops;
    bad.set_level = NULL;
    assert(oa_src_hdmi_hotplug_pulse(&bad) == -1);
    bad = ops;
    bad.delay_ms = NULL;
    assert(oa_src_hdmi_hotplug_pulse(&bad) == -1);
    assert(oa_src_hdmi_hotplug_pulse(NULL) == -1);

    puts("ok src hotplug: stock 274..277 active-low pulse, MHL 192 high/re-read/"
         "failure skip of GPIO 204, missing descriptors, active-high/other and "
         "invalid ops");
    return 0;
}
