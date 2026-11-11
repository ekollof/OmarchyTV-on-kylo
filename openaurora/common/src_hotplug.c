/* Recovered DEV_SRC_HDMIHotPlug task (aurora 0x008fc3b0).
 *
 * The task is a one-shot pulse, not a continuous watcher:
 *   1. resolve GPIO descriptors 204..207 (HDMI_HOTPLUG_1..4) and 193
 *      (HDMI_HOTPLUG_5);
 *   2. drive each present hotplug pin to its inactive level
 *      (active_level ? 0 : 1);
 *   3. read MHL CD sense GPIO 192 and log a high level;
 *   4. wait 500 ms;
 *   5. drive each present pin to its active level and log the toggle.
 *
 * The descriptor carries the pin and active level. That is runtime device
 * state, not a firmware constant; callers must provide it from the device
 * layer. This helper does not guess a level or touch hardware itself.
 * Instructions: aurora.dis 0x008fc3b0..0x008fc582; GPIO names from the
 * vendor inventory table (204..207, 193, 192).
 */
#include <stddef.h>

#include "openaurora/src_hotplug.h"

static const int hotplug_gpio[OA_SRC_HOTPLUG_COUNT] = {
    204, 205, 206, 207, 193
};

int oa_src_hdmi_hotplug_pulse(const oa_src_hotplug_ops *ops)
{
    if (!ops || !ops->descriptor || !ops->set_level || !ops->delay_ms)
        return -1;

    struct desc { int found, pin, active; } d[OA_SRC_HOTPLUG_COUNT];
    for (int i = 0; i < OA_SRC_HOTPLUG_COUNT; i++) {
        d[i].found = d[i].pin = d[i].active = 0;
        int pin, active;
        if (ops->descriptor(ops->ud, hotplug_gpio[i], &pin, &active) != 1) {
            if (ops->log)
                ops->log(ops->ud, "#DEV-SRC#Not found HDMI%d HotPlug Definition",
                         i + 1);
            continue;
        }
        d[i].found = 1;
        d[i].pin = pin;
        d[i].active = active;
        ops->set_level(ops->ud, pin, active ? 0 : 1);
    }

    int mhl_pin, mhl_active;
    if (ops->descriptor(ops->ud, 192, &mhl_pin, &mhl_active) == 1 &&
        ops->get_level) {
        int level = 0;
        if (ops->get_level(ops->ud, mhl_pin, &level) == 0) {
            if (level == 1 && ops->log)
                ops->log(ops->ud, "#DEV-SRC#MHL CD_SENSE Is High");
        } else if (ops->log) {
            ops->log(ops->ud, "%s,Get CDSense level of pin %d failed",
                     "DEV_SRC_HDMIHotPlug", mhl_pin);
        }
    }

    ops->delay_ms(ops->ud, 500);

    int toggled = 0;
    for (int i = 0; i < OA_SRC_HOTPLUG_COUNT; i++) {
        if (!d[i].found)
            continue;
        if (ops->log)
            ops->log(ops->ud, "End toggle PIN=%d, Active level=%d",
                     d[i].pin, d[i].active);
        ops->set_level(ops->ud, d[i].pin, d[i].active);
        toggled++;
        if (ops->log)
            ops->log(ops->ud, "#DEV-SRC#Toogle HDMI%d HotPlug Successfully",
                     i + 1);
    }
    return toggled;
}
