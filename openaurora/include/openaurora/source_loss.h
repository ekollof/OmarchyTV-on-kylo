#ifndef OPENAURORA_SOURCE_LOSS_H
#define OPENAURORA_SOURCE_LOSS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pure source-loss policy from the shipped InputManager.
 *
 * Recovered from aurora.elf 64c252340d3af6c5d66072611b7bad7a:
 *   InputManager::handleExternalSourceConnected    @ 0x00fb1f30
 *   InputManager::handleExternalSourceDisconnected @ 0x00fb1fbc
 *
 * The helper only decides what the caller should select. It performs no HAL,
 * device, tuner, UI or source-switch I/O. The caller supplies:
 *   - current_external: InputManager+0x68; -1 means no external source (TV);
 *   - new_external: the connected external interface index;
 *   - tuner_position: InputManager+0x90, the tuner position in the current view;
 *   - same_group: the caller's video-group comparison;
 *   - tuner_available: interface 0x13 enabled and the service list non-empty.
 *
 * Field names are recovered by use, not vendor source. In particular flag_88
 * is an opaque flag: when set and the groups match, the stock function only
 * clears it. Do not invent a friendlier meaning without more evidence.
 */
enum oa_source_loss_action {
    OA_SOURCE_LOSS_NONE = 0,
    OA_SOURCE_LOSS_SELECT_EXTERNAL,
    OA_SOURCE_LOSS_PREVIOUS_EXTERNAL,
    OA_SOURCE_LOSS_TUNER_POSITION
};

struct oa_source_loss {
    int previous_external; /* +0x94: -1 = none/TV */
    int tuner_position;    /* +0x90: current-view position, -1 = unset */
    int auto_switched;     /* +0x98: set when an external source is selected */
    int flag_88;           /* +0x88: opaque; cleared on a matching disconnect */
};

void oa_source_loss_init(struct oa_source_loss *state);

/* External-source connect. Returns SELECT_EXTERNAL with *target = new_external
 * when the source changes; NONE when it matches current_external or arguments
 * are invalid. On a change it saves previous_external = current_external,
 * tuner_position, and sets auto_switched. */
enum oa_source_loss_action oa_source_loss_connected(
    struct oa_source_loss *state, int current_external, int new_external,
    int tuner_position, int *target);

/* External-source disconnect. A matching group with flag_88 set only clears
 * flag_88. Otherwise, when auto_switched and the groups match:
 *   - previous_external != -1: return PREVIOUS_EXTERNAL and clear it;
 *   - otherwise, with tuner_available and a valid tuner_position: return
 *     TUNER_POSITION.
 * A group mismatch or invalid arguments change nothing. */
enum oa_source_loss_action oa_source_loss_disconnected(
    struct oa_source_loss *state, int same_group, int tuner_available,
    int *target);

#ifdef __cplusplus
}
#endif

#endif
