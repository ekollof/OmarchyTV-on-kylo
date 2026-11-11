/* Recovered DEV_SRC_HDMIHotPlug task (aurora 0x008fc3b0).
 *
 * dev_src_init (0x008fc654) creates this one-shot task with stack 0x1000 and
 * priority 0x10. It resolves GPIO 204..207 (HDMI_HOTPLUG_1..4) and 193
 * (HDMI_HOTPLUG_5), drives each present pin inactive, reads MHL CD sense 192,
 * waits 500 ms, then drives each present pin active.
 *
 * The task checks MHL at index 0 in both loops. When MHL CD_SENSE is high it
 * logs and skips GPIO 204 (HDMI_HOTPLUG_1) for that loop, so an MHL source on
 * HDMI1 is not pulsed. The MHL level is re-read after the delay; a level that
 * changes between the loops changes whether 204 is pulsed active.
 *
 * Inactive level is (active == 0) ? 1 : 0. The active level is written
 * verbatim; DEV_GPIO_SetLevel rejects values above 1, so an "other" active
 * descriptor is logged by the task but the device layer refuses the write.
 *
 * Descriptor pins/active levels are runtime data from
 * DEV_GPIO_GetDescription; this helper never guesses them. Instructions:
 * aurora.dis 0x008fc3b0..0x008fc582. GPIO ids/names: table 0x0139ee38 and
 * au8GPIOFuncName 0x0175f350.
 */
#include <stddef.h>

#include "openaurora/src_hotplug.h"

static const int hotplug_gpio[OA_SRC_HOTPLUG_COUNT] = {
    204, 205, 206, 207, 193
};

struct desc {
    int found;
    int pin;
    int active;
};

/* Returns 1 when MHL CD_SENSE reads high, logging either the high level or a
 * read failure. A missing getter is treated as "no level", not a failure. */
static int mhl_high(const oa_src_hotplug_ops *ops, int mhl_pin)
{
    if (!ops->get_level)
        return 0;
    int level = 0;
    if (ops->get_level(ops->ud, mhl_pin, &level) != 0) {
        if (ops->log)
            ops->log(ops->ud, "%s,Get CDSense level of pin %d failed",
                     "DEV_SRC_HDMIHotPlug", mhl_pin);
        return 0;
    }
    if (level == 1) {
        if (ops->log)
            ops->log(ops->ud, "#DEV-SRC#MHL CD_SENSE Is High");
        return 1;
    }
    return 0;
}

int oa_src_hdmi_hotplug_pulse(const oa_src_hotplug_ops *ops)
{
    if (!ops || !ops->descriptor || !ops->set_level || !ops->delay_ms)
        return -1;

    struct desc d[OA_SRC_HOTPLUG_COUNT];
    for (int i = 0; i < OA_SRC_HOTPLUG_COUNT; i++)
        d[i] = (struct desc){ 0, 0, 0 };

    int mhl_found = 0, mhl_pin = 0, mhl_active = 0;
    for (int i = 0; i < OA_SRC_HOTPLUG_COUNT; i++) {
        if (i == 0) {
            if (ops->descriptor(ops->ud, 192, &mhl_pin, &mhl_active) == 1) {
                mhl_found = 1;
                if (mhl_high(ops, mhl_pin))
                    continue; /* firmware skips HDMI_HOTPLUG_1 here */
            }
        }
        int pin, active;
        if (ops->descriptor(ops->ud, hotplug_gpio[i], &pin, &active) != 1) {
            if (ops->log)
                ops->log(ops->ud,
                         "#DEV-SRC#Not found HDMI%d HotPlug Definition", i + 1);
            continue;
        }
        d[i].found = 1;
        d[i].pin = pin;
        d[i].active = active;
        (void)ops->set_level(ops->ud, pin, active == 0 ? 1 : 0);
    }

    ops->delay_ms(ops->ud, 500);

    int toggled = 0;
    for (int i = 0; i < OA_SRC_HOTPLUG_COUNT; i++) {
        if (i == 0 && mhl_found && mhl_high(ops, mhl_pin))
            continue; /* MHL high after the delay: still do not pulse 204 */
        if (!d[i].found)
            continue;
        if (ops->log)
            ops->log(ops->ud, "End toggle PIN=%d, Active level=%d",
                     d[i].pin, d[i].active);
        (void)ops->set_level(ops->ud, d[i].pin, d[i].active);
        toggled++;
        if (ops->log)
            ops->log(ops->ud, "#DEV-SRC#Toogle HDMI%d HotPlug Successfully",
                     i + 1);
    }
    return toggled;
}
