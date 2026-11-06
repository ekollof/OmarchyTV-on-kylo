#include "openaurora/mxl661_tune.h"

#include <stddef.h>

static const uint32_t if_khz[OA_MXL661_IF_TABLE_LEN] = {
    3650, 4000, 4100, 4150, 4500, 4570, 5000, 5380, 6000, 6280,
    7200, 8250, 35250, 36000, 36150, 36650, 44000
};

uint32_t oa_mxl661_standard_from_index(uint32_t index)
{
    switch (index) {
    case 0:
        return 7;
    case 1:
        return 8;
    case 2:
        return 6;
    case 3:
        return 10;
    case 4:
        return 9;
    case 5:
        return 0xc;
    case 6:
        return 0xd;
    case 7:
        return 0xb;
    default:
        return 0;
    }
}

uint32_t oa_mxl661_if_khz(uint32_t index)
{
    return index < OA_MXL661_IF_TABLE_LEN ? if_khz[index] : 0;
}
