#include "openaurora/nim_dtv.h"

int oa_nim_dtv_model_supported(int model_type)
{
    return model_type >= 0 && model_type <= 3;
}

int oa_nim_dtv_active_module(int model_type)
{
    /* DEV_NIM_DTV_Init: case 0..3 -> 3, case 4 -> 1, default -> 3. */
    return model_type == 4 ? 1 : 3;
}
