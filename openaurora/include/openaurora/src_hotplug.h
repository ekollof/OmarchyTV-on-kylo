#ifndef OPENAURORA_SRC_HOTPLUG_H
#define OPENAURORA_SRC_HOTPLUG_H

#define OA_SRC_HOTPLUG_COUNT 5

/* Device-layer provider for the recovered DEV_SRC_HDMIHotPlug task.
 * descriptor() returns 1 and fills pin/active when the GPIO description
 * exists, 0 when it is absent. The caller owns all hardware access. */
typedef struct {
    void *ud;
    int (*descriptor)(void *ud, int gpio_id, int *pin, int *active);
    int (*get_level)(void *ud, int pin, int *level);
    void (*set_level)(void *ud, int pin, int level);
    void (*delay_ms)(void *ud, unsigned ms);
    void (*log)(void *ud, const char *fmt, ...);
} oa_src_hotplug_ops;

/* Runs the one-shot inactive -> 500 ms -> active pulse over GPIO IDs
 * 204,205,206,207,193 and reads MHL CD sense (192). Returns the number of
 * present hotplug descriptors toggled, or -1 for a missing required op. */
int oa_src_hdmi_hotplug_pulse(const oa_src_hotplug_ops *ops);

#endif
