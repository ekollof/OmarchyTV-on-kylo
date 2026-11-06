#include "openaurora/nim_dtv_hw.h"

#include <string.h>

int oa_nim_dtv_init_state(uint32_t *state, size_t words)
{
    if (!state || words < OA_NIM_DTV_STATE_WORDS)
        return 0;
    memset(state, 0, OA_NIM_DTV_STATE_WORDS * sizeof(*state));
    return 1;
}

int oa_nim_dtv_connect_hal(struct oa_nim_dtv_hal_table *table,
                           const struct oa_nim_dtv_hal_ops *ops)
{
    if (!table || !ops)
        return 0;
    memset(table, 0, sizeof(*table));
    table->ops = *ops;
    table->connected = 1;
    return 1;
}
