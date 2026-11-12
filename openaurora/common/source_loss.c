#include <errno.h>
#include <string.h>

#include "openaurora/source_loss.h"

void oa_source_loss_init(struct oa_source_loss *state)
{
    if (!state) return;
    memset(state, 0, sizeof *state);
    state->previous_external = -1;
    state->tuner_position = -1;
}

enum oa_source_loss_action oa_source_loss_connected(
    struct oa_source_loss *state, int current_external, int new_external,
    int tuner_position, int *target)
{
    if (!state || !target || new_external < 0) return OA_SOURCE_LOSS_NONE;
    if (new_external == current_external) return OA_SOURCE_LOSS_NONE;

    /* Stock handleExternalSourceConnected: save +0x68 to +0x94, save the
     * tuner navigation position to +0x90, then select the new external. */
    state->previous_external = current_external;
    state->tuner_position = tuner_position;
    state->auto_switched = 1;
    *target = new_external;
    return OA_SOURCE_LOSS_SELECT_EXTERNAL;
}

enum oa_source_loss_action oa_source_loss_disconnected(
    struct oa_source_loss *state, int same_group, int tuner_available,
    int *target)
{
    if (!state || !target) return OA_SOURCE_LOSS_NONE;
    if (!same_group) return OA_SOURCE_LOSS_NONE;

    /* +0x88 set and the groups match: the stock code only clears it. */
    if (state->flag_88) {
        state->flag_88 = 0;
        return OA_SOURCE_LOSS_NONE;
    }
    if (!state->auto_switched) return OA_SOURCE_LOSS_NONE;

    /* +0x94 is restored first; the +0x90 tuner position is used only when
     * no previous external source was recorded (-1, e.g. TV was current). */
    if (state->previous_external != -1) {
        *target = state->previous_external;
        state->previous_external = -1;
        return OA_SOURCE_LOSS_PREVIOUS_EXTERNAL;
    }
    if (tuner_available && state->tuner_position >= 0) {
        *target = state->tuner_position;
        return OA_SOURCE_LOSS_TUNER_POSITION;
    }
    return OA_SOURCE_LOSS_NONE;
}

int oa_source_loss_inactive_source(const struct oa_source_loss_worker *worker,
                                   int enabled, int displayed_external,
                                   int saved_tuner_position)
{
    if (!worker || !worker->select_dtv || !worker->select_tuner_position)
        return -EINVAL;
    if (!enabled || !displayed_external || saved_tuner_position < 0)
        return OA_SOURCE_LOSS_NONE;

    int rc = worker->select_dtv(worker->context);
    if (rc < 0) return rc;
    rc = worker->select_tuner_position(worker->context, saved_tuner_position);
    if (rc < 0) return rc;
    return OA_SOURCE_LOSS_DTV_TUNER;
}
