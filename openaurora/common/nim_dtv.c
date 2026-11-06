#include "openaurora/nim_dtv.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

int oa_nim_dtv_model_supported(int model_type)
{
    return model_type >= 0 && model_type <= 3;
}

int oa_nim_dtv_active_module(int model_type)
{
    /* DEV_NIM_DTV_Init: case 0..3 -> 3, case 4 -> 1, default -> 3. */
    return model_type == 4 ? 1 : 3;
}

int oa_nim_dtv_module_buf_info(int module, void *ud)
{
    (void)module;
    (void)ud;
    /* PLAT_DRV_GetModuleBufInfo @0x0089b79c: movs r0,#0; return. */
    return 0;
}

static void step(const oa_nim_dtv_ops *ops, void *ud, int value)
{
    if (ops->step) ops->step(value, ud);
}

int oa_nim_dtv_task_run(int model_type, int hal_requested,
                        const oa_nim_dtv_ops *ops, void *ud,
                        oa_nim_dtv_result *out)
{
    if (!ops || !out) return -EINVAL;
    out->module_buf_rc = 0;
    out->initialized = 0;
    out->active_module = oa_nim_dtv_active_module(model_type);
    out->failed_step = -1;
    out->failed_io = -1;
    out->hal_used = 0;
    memset(out->ir_value, 0, sizeof out->ir_value);

    step(ops, ud, OA_NIM_DTV_STEP_LOCK);
    if (ops->module_buf_info)
        out->module_buf_rc = ops->module_buf_info(9, ud);
    step(ops, ud, OA_NIM_DTV_STEP_MODULE_BUF);

    /* The vendor ignores these scalar results and proceeds to the IR steps. */
    if (ops->init_nim_func) (void)ops->init_nim_func(ud);
    step(ops, ud, OA_NIM_DTV_STEP_INIT_NIM);

    if (hal_requested) {
        out->hal_used = 1;
        if (ops->hal_demod_initialize) (void)ops->hal_demod_initialize(ud);
        step(ops, ud, OA_NIM_DTV_STEP_HAL_DEMOD);
        if (ops->connect_hal) (void)ops->connect_hal(ud);
        step(ops, ud, OA_NIM_DTV_STEP_CONNECT_HAL);
    }

    static const int codes[3] = { 21, 17, 64 };
    static const uint8_t masks[3] = { 8, 8, 2 };
    static const int steps[3] = {
        OA_NIM_DTV_STEP_IR21, OA_NIM_DTV_STEP_IR17, OA_NIM_DTV_STEP_IR64
    };
    for (int i = 0; i < 3; i++) {
        step(ops, ud, steps[i]);
        uint8_t value = 0;
        int ok = ops->ir_read ? ops->ir_read(OA_NIM_DTV_IIC_BUS, codes[i], &value, ud) : 1;
        if (!ok) {
            out->failed_step = steps[i];
            out->failed_io = 0;
            break;
        }
        /* Decompiled order: 0x15 clears bit3, 0x11 sets bit3, 0x40 sets bit1. */
        if (i == 0) value &= (uint8_t)~masks[i];
        else value |= masks[i];
        out->ir_value[i] = value;
        ok = ops->ir_write ? ops->ir_write(OA_NIM_DTV_IIC_BUS, codes[i], value, ud) : 1;
        if (!ok && codes[i] != 64) {
            out->failed_step = steps[i];
            out->failed_io = 1;
            break; /* vendor jumps to the finish path */
        }
        /* Code 64's write return is not checked in the vendor body. */
    }

    out->active_module = OA_NIM_DTV_TERM_ACTIVE;
    out->initialized = 1;
    step(ops, ud, OA_NIM_DTV_STEP_ACTIVE);
    if (ops->signal_done) ops->signal_done(ud);
    step(ops, ud, OA_NIM_DTV_STEP_SIGNAL);
    step(ops, ud, OA_NIM_DTV_STEP_UNLOCK);
    return 0;
}
