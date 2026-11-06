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

/* 1 for model types 0..3 (supported); 0 otherwise. */
int oa_nim_dtv_model_supported(int model_type);

/* Vendor gu32NimActiveModule: 1 for model type 4, 3 for 0..3 and the default
 * branch (including other/negative values). */
int oa_nim_dtv_active_module(int model_type);

#endif
