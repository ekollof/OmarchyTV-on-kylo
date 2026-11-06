#include <assert.h>
#include <stdio.h>
#include "openaurora/nim_dtv.h"

int main(void)
{
    for (int m = 0; m <= 3; m++) {
        assert(oa_nim_dtv_model_supported(m) == 1);
        assert(oa_nim_dtv_active_module(m) == 3);
    }
    assert(oa_nim_dtv_model_supported(4) == 0);
    assert(oa_nim_dtv_active_module(4) == 1);
    assert(oa_nim_dtv_model_supported(5) == 0);
    assert(oa_nim_dtv_active_module(5) == 3);
    assert(oa_nim_dtv_model_supported(-1) == 0);
    assert(oa_nim_dtv_active_module(-1) == 3);
    assert(OA_NIM_DTV_TASK_ENTRY == 0x008601adu);
    assert(OA_NIM_DTV_TASK_STACK == 0x1000u);
    assert(OA_NIM_DTV_TASK_PRIORITY == 14);
    assert(OA_NIM_DTV_TERM_ACTIVE == 0xffu);
    puts("ok nim dtv: recovered model gate/active module and task constants");
    return 0;
}
