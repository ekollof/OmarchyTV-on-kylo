#include "openaurora/tuner_identity.h"

#include <string.h>

int oa_tuner_sal_type(uint32_t pcb_layout, const oa_efsl_profile *hwprofile)
{
    switch (pcb_layout) {
    case 3:
    case 4:
    case 5:
    case 6:
        return OA_TUNER_SAL_EXTERNAL;
    case 8:
        return OA_TUNER_SAL_SI2151;
    default:
        break;
    }

    if (hwprofile) {
        oa_efsl_section root;
        oa_efsl_section types;
        char value[20] = { 0 };
        size_t len = 0;
        if (oa_efsl_root(hwprofile, &root) == 0 &&
            oa_efsl_open_section(hwprofile, &root, "TunerTypes", &types) == 0 &&
            oa_efsl_read_value(hwprofile, &types, "TunerType", value,
                               sizeof(value) - 1, &len) == 0) {
            value[len] = '\0';
            if (strcmp(value, "Si2151") == 0)
                return OA_TUNER_SAL_SI2151;
        }
    }
    return OA_TUNER_SAL_MXL661;
}

int oa_tuner_ndnim_type(int sal_type)
{
    /* Table at 0x01355c74, in firmware order. */
    static const uint8_t map[14] = {
        8, 8, 8, 0xc, 8, 5, 6, 5, 8, 8, 8, 8, 8, 0xc
    };
    unsigned type = (unsigned)sal_type;
    return type > 0xdu ? 8 : map[type];
}
