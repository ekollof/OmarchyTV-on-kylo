/*
 * Event topics and their payloads.
 *
 * Payload pointers are only valid for the duration of emit() — copy what you keep.
 * A module may treat payloads as opaque bytes and only agree on the topic string.
 */
#ifndef OPENAURORA_EVENTS_H
#define OPENAURORA_EVENTS_H

#include <stdint.h>

/* input/key — remote control. id is the stable key identifier (for this chassis the
 * NEC command byte; see artifacts/reports/IR-UART-2026-10-30.md). */
#define OA_TOPIC_KEY "input/key"
enum { OA_KEY_PRESS = 0, OA_KEY_REPEAT = 1, OA_KEY_RELEASE = 2 };
typedef struct {
    uint8_t id;
    uint8_t kind;   /* OA_KEY_* */
    uint16_t flags; /* reserved */
    uint64_t ts_ms;
} oa_key_event;

/* clock/tick — one per second from the clock service; cheap heartbeat for UIs. */
#define OA_TOPIC_TICK "clock/tick"
typedef struct {
    uint64_t ms;
} oa_tick_event;

/* power/state — standby integration. */
#define OA_TOPIC_POWER "power/state"
enum { OA_POWER_ON = 0, OA_POWER_STANDBY = 1, OA_POWER_RESUMING = 2 };
typedef struct {
    uint8_t state; /* OA_POWER_* */
} oa_power_event;

/* ui/action — what the user asked for, decided by a UI module. */
#define OA_TOPIC_ACTION "ui/action"
enum { OA_ACTION_SELECT = 0, OA_ACTION_HOME = 1, OA_ACTION_BACK = 2, OA_ACTION_SOURCE = 3 };
typedef struct {
    uint8_t kind; /* OA_ACTION_* */
    uint8_t index;
    uint8_t reserved[2];
    const char *name; /* may be NULL */
} oa_action_event;

/* service/removed — a module unloaded; drop cached pointers for `name`. */
#define OA_TOPIC_SERVICE_REMOVED "service/removed"
typedef struct {
    const char *name;
} oa_service_removed_event;

/* module/changed — lifecycle notification from the host. */
#define OA_TOPIC_MODULE_CHANGED "module/changed"
enum { OA_MOD_LOADED_EV = 0, OA_MOD_STARTED_EV = 1, OA_MOD_STOPPED_EV = 2, OA_MOD_UNLOADED_EV = 3 };
typedef struct {
    const char *name;
    uint8_t what; /* OA_MOD_*_EV */
} oa_module_event;

#endif /* OPENAURORA_EVENTS_H */
