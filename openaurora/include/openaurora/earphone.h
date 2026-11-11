#ifndef OPENAURORA_EARPHONE_H
#define OPENAURORA_EARPHONE_H

#define OA_EARPHONE_GPIO_ID 117

/* Recovered DEV_GPIO earphone task (aurora 0x00851544).
 *
 * The vendor task resolves GPIO 117 ("EARPHONE"), then repeatedly waits
 * 100 ms and reads its level. On a change it sends vendor event
 * PLAT_OS_SendEvent(0, 17, plugin, 0, 0): 1 for EN_PLAT_OS_EV_TYPE_EARPHONE_
 * PLUGIN and 0 for PLUGOUT. Profile_BoardType()==17 swaps which descriptor
 * level means plugin; every other board uses the active level as plugin.
 *
 * This helper is the detector only. The event-17 audio routing/consumer is
 * not reproduced here; the injected event callback is the seam for it.
 */
typedef struct {
    void *ud;
    /* 1 and fills pin/normal/active when GPIO 117 is described. */
    int (*descriptor)(void *ud, int gpio_id, int *pin, int *normal, int *active);
    /* 0 on success, negative errno on failure. */
    int (*get_level)(void *ud, int pin, int *level);
    void (*event)(void *ud, int plugin);
    void (*delay_ms)(void *ud, unsigned ms);
    void (*log)(void *ud, const char *fmt, ...);
} oa_earphone_ops;

typedef struct {
    int have_desc;
    int pin;
    int plugin_level;
    int plugout_level;
    int last_level;
    int initialized;
    int plugin;
    unsigned fail_count;
} oa_earphone;

/* initial_level >= 0 seeds the last level (suppresses the first event when it
 * matches); -1 emits the first observed plugin/plugout state. The vendor's
 * shared-memory initial value is not fully recovered, so callers choose. */
int oa_earphone_init(oa_earphone *e, const oa_earphone_ops *ops,
                     int board_type, int initial_level);

/* One 100 ms poll. Returns 1 when a plugin/plugout event was emitted, 0 for
 * no change/other level, or negative errno on read/argument failure. */
int oa_earphone_poll(oa_earphone *e, const oa_earphone_ops *ops);

#endif
