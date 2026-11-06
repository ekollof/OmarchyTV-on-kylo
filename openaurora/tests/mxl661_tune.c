#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "openaurora/mxl661_tune.h"

int main(void)
{
    static const uint32_t std_map[8] = { 7, 8, 6, 10, 9, 0xc, 0xd, 0xb };
    for (uint32_t i = 0; i < 8; i++)
        assert(oa_mxl661_standard_from_index(i) == std_map[i]);
    assert(oa_mxl661_standard_from_index(8) == 0);
    assert(oa_mxl661_standard_from_index(0xffffffffu) == 0);

    static const uint32_t if_map[OA_MXL661_IF_TABLE_LEN] = {
        3650, 4000, 4100, 4150, 4500, 4570, 5000, 5380, 6000, 6280,
        7200, 8250, 35250, 36000, 36150, 36650, 44000
    };
    for (uint32_t i = 0; i < OA_MXL661_IF_TABLE_LEN; i++)
        assert(oa_mxl661_if_khz(i) == if_map[i]);
    assert(oa_mxl661_if_khz(OA_MXL661_IF_TABLE_LEN) == 0);
    assert(oa_mxl661_if_khz(0xffffffffu) == 0);

    puts("ok mxl661 tune: standard index map and 17-entry IF table");
    return 0;
}
