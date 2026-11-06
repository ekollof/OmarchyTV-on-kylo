/* Recovered DEV_NIM_DTV_Init model gate and task-creation constants.
 *
 * DEV_NIM_DTV_Init @0x00861678 is idempotent on a global initialized byte; it
 * accepts model types 0..3, marks type 4 unsupported with active module 1,
 * and leaves the default/other models at active module 3. It then initializes
 * g_InitMutex and calls
 *   PLAT_OS_CreateTask(entry 0x008601ad, arg 0, stack 0x1000, priority 14,
 *                      params, 1)
 * before marking the stage initialized. DEV_NIM_DTV_Term clears the guard and
 * sets 0xff; it does not join the task in the recovered body.
 *
 * This header exposes only the verified gate/constants. The task body performs
 * NIM/hardware work and is not implemented here.
 */
#ifndef OPENAURORA_NIM_DTV_H
#define OPENAURORA_NIM_DTV_H
#include <stdint.h>

#define OA_NIM_DTV_TASK_ENTRY 0x008601adu
#define OA_NIM_DTV_TASK_STACK 0x1000u
#define OA_NIM_DTV_TASK_PRIORITY 14
#define OA_NIM_DTV_TERM_ACTIVE 0xffu

/* The two helpers at 0x0085fd04/0x0085fc80 are _dev_nim_dtv_IIC_Read/Write
 * (the names come from their own debug strings). They call
 * DEV_IIC_Read/Write(0, 0, 0x80, 1, code, 1, &byte):
 *   - bus 0 and the fixed 8-bit slave address 0x80,
 *   - one subaddress byte (the per-code value 21/17/64),
 *   - one data byte.
 * The helper's first argument selects the DEV_IIC clock table entry; the
 * one-shot task passes 0 (79 kHz). Keep the macro value even though the
 * current kernel transfer ignores the clock field. */
#define OA_NIM_DTV_IIC_BUS 0
#define OA_NIM_DTV_IIC_DEV 0
#define OA_NIM_DTV_IIC_REG 0x80
#define OA_NIM_DTV_IIC_SIZE 1
#define OA_NIM_DTV_IIC_LEN 1

/* 1 for model types 0..3 (supported); 0 otherwise. */
int oa_nim_dtv_model_supported(int model_type);

/* Vendor gu32NimActiveModule: 1 for model type 4, 3 for 0..3 and the default
 * branch (including other/negative values). */
int oa_nim_dtv_active_module(int model_type);

/* PLAT_DRV_GetModuleBufInfo @0x0089b79c is a literal `return 0` stub in this
 * firmware. Keep the equivalent, including its argument contract, so the
 * one-shot task logs the same "not found" branch. */
int oa_nim_dtv_module_buf_info(int module, void *ud);

/* Ordered one-shot task body recovered from 0x008601ac. The real body calls
 * PLAT_DRV_GetModuleBufInfo(9), NIM init, optional HAL demod init/connect and
 * the UART IR system-code sequence 21/17/64 (masks 8/8/2), then active 0xff
 * and a condition signal. These callbacks stand in for that hardware path. */
enum {
    OA_NIM_DTV_STEP_LOCK = 0,
    OA_NIM_DTV_STEP_MODULE_BUF,
    OA_NIM_DTV_STEP_INIT_NIM,
    OA_NIM_DTV_STEP_HAL_DEMOD,
    OA_NIM_DTV_STEP_CONNECT_HAL,
    OA_NIM_DTV_STEP_IR21,
    OA_NIM_DTV_STEP_IR17,
    OA_NIM_DTV_STEP_IR64,
    OA_NIM_DTV_STEP_ACTIVE,
    OA_NIM_DTV_STEP_SIGNAL,
    OA_NIM_DTV_STEP_UNLOCK,
    OA_NIM_DTV_STEP_COUNT
};

typedef struct {
    int (*module_buf_info)(int module, void *ud); /* vendor 1 = found */
    int (*init_nim_func)(void *ud);
    int (*hal_demod_initialize)(void *ud);
    int (*connect_hal)(void *ud);
    /* DEV_IIC_Read/Write(0, 0, 0x80, 1, code, 1, &byte), then the per-code
     * bit update. Both vendor helpers return 1 only when the IIC call returns 0. */
    int (*ir_read)(int bus, int code, uint8_t *value, void *ud);
    int (*ir_write)(int bus, int code, uint8_t value, void *ud);
    void (*signal_done)(void *ud);
    void (*step)(int step, void *ud); /* test/observability hook, may be NULL */
} oa_nim_dtv_ops;

typedef struct {
    int module_buf_rc;
    int initialized;
    int active_module;
    int failed_step; /* -1 = none, else the IR step that failed */
    int failed_io;   /* -1 none, 0 read, 1 write */
    int hal_used;
    uint8_t ir_value[3]; /* final buffer bytes after OR masks */
} oa_nim_dtv_result;

/* Runs the one-shot sequence. Returns 0 when it reached SIGNAL/UNLOCK (even if
 * an IR code was rejected, mirroring the vendor finish path), -EINVAL for bad
 * arguments. Missing optional callbacks are skipped. */
int oa_nim_dtv_task_run(int model_type, int hal_requested,
                        const oa_nim_dtv_ops *ops, void *ud,
                        oa_nim_dtv_result *out);

#endif
