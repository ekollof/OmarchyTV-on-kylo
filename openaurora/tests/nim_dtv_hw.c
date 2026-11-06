#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/nim_dtv_hw.h"

static int calls[9];

static int f_init(void *ud) { (void)ud; calls[0]++; return 0; }
static int f_open(void *ud) { (void)ud; calls[1]++; return 0; }
static int f_set(void *ud) { (void)ud; calls[2]++; return 0; }
static int f_status(void *ud) { (void)ud; calls[3]++; return 0; }
static int f_lock(void *ud) { (void)ud; calls[4]++; return 0; }
static int f_strength(void *ud) { (void)ud; calls[5]++; return 0; }
static int f_quality(void *ud) { (void)ud; calls[6]++; return 0; }
static int f_monitor(void *ud) { (void)ud; calls[7]++; return 0; }
static int f_close(void *ud) { (void)ud; calls[8]++; return 0; }

int main(void)
{
    uint32_t state[OA_NIM_DTV_STATE_WORDS + 4];
    memset(state, 0xa5, sizeof state);
    assert(oa_nim_dtv_init_state(state, OA_NIM_DTV_STATE_WORDS) == 1);
    for (size_t i = 0; i < OA_NIM_DTV_STATE_WORDS; i++)
        assert(state[i] == 0);
    /* The vendor only clears the 0x29 words. */
    assert(state[OA_NIM_DTV_STATE_WORDS] == 0xa5a5a5a5u);
    assert(oa_nim_dtv_init_state(NULL, OA_NIM_DTV_STATE_WORDS) == 0);
    assert(oa_nim_dtv_init_state(state, OA_NIM_DTV_STATE_WORDS - 1) == 0);

    struct oa_nim_dtv_hal_ops ops = {
        f_init, f_open, f_set, f_status, f_lock, f_strength, f_quality,
        f_monitor, f_close, NULL
    };
    struct oa_nim_dtv_hal_table table;
    memset(&table, 0x5a, sizeof table);
    assert(oa_nim_dtv_connect_hal(&table, &ops) == 1);
    assert(table.connected == 1);
    assert(table.ops.initialize == f_init && table.ops.open == f_open &&
           table.ops.set_channel == f_set &&
           table.ops.get_channel_status == f_status &&
           table.ops.check_lock == f_lock &&
           table.ops.get_strength == f_strength &&
           table.ops.get_quality == f_quality &&
           table.ops.monitor == f_monitor && table.ops.close == f_close);
    assert(table.ops.disconnect == NULL);
    assert(oa_nim_dtv_connect_hal(NULL, &ops) == 0);
    assert(oa_nim_dtv_connect_hal(&table, NULL) == 0);
    puts("ok nim dtv hw: 0x29-word state clear and HAL table install");
    return 0;
}
