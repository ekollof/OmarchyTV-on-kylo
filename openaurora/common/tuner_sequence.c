#include "openaurora/tuner_sequence.h"

#include <string.h>

#include "openaurora/tuner_identity.h"

int oa_tuner_sequence_run(const oa_tuner_sequence_ops *ops, void *ud,
                          uint32_t sal_type, oa_tuner_sequence_result *out)
{
    if (!ops || !out)
        return -1;
    memset(out, 0, sizeof(*out));

    /* HAL_Demod_User_Initialize: always runs, even for type 0xf. */
    out->demod_rc = ops->demod_init ? ops->demod_init(ud, 0) : 0;

    /* SAL_DEMOD_TUN_Initialize only builds a tuner for 8 and 0xd. */
    if (sal_type == OA_TUNER_SAL_SI2151 || sal_type == OA_TUNER_SAL_MXL661) {
        out->tuner_type = (uint32_t)oa_tuner_ndnim_type((int)sal_type);
        out->tuner_rc = ops->tuner_init ? ops->tuner_init(ud) : 0;
        out->av2018_rc = ops->av2018_response ? ops->av2018_response(ud) : 1;
        out->dvif_handle = ops->dvif_open ? ops->dvif_open(ud) : -1;
        if (ops->set_tuner_type)
            out->set_type_rc =
                ops->set_tuner_type(ud, out->dvif_handle, out->tuner_type);
    }

    /* HAL_Demod_User_Initialize always calls SAL_LNB_Initialize afterwards. */
    if (ops->lnb_init) {
        out->lnb_called = 1;
        out->lnb_rc = ops->lnb_init(ud);
    }

    if (out->demod_rc != 0 || out->tuner_rc != 0)
        return -1;
    if (sal_type == OA_TUNER_SAL_SI2151 || sal_type == OA_TUNER_SAL_MXL661) {
        if (out->dvif_handle < 0 || out->set_type_rc != 0)
            return -1;
    }
    return 0;
}
