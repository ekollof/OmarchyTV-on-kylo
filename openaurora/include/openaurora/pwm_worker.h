#ifndef OPENAURORA_PWM_WORKER_H
#define OPENAURORA_PWM_WORKER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Worker for blocking panel PWM writes. mod_panel calls Hal_Pwm_Open/Set on
 * the control loop today; this worker moves the Set path off-loop while the
 * module keeps service lookup and host API calls on the loop.
 *
 * The injected apply callback is a raw function pointer copied at create
 * time; it must not call the host service API. One bounded FIFO, one worker,
 * ordered jobs, bounded completions. No device close happens here.
 */
typedef struct oa_pwm_worker oa_pwm_worker;

typedef struct {
    void *ud;
    int (*apply)(void *ud, int pct); /* 0 success, negative errno */
} oa_pwm_worker_io;

typedef struct {
    uint64_t ticket;
    int pct;
    int rc;
} oa_pwm_completion;

int oa_pwm_worker_create(oa_pwm_worker **out, const oa_pwm_worker_io *io);
/* 0 accepted, -EINVAL bad args, -EBUSY full queue, -ESHUTDOWN after quiesce. */
int oa_pwm_worker_submit(oa_pwm_worker *w, int pct, uint64_t *ticket);
/* 1 copied, 0 none ready, -EINVAL bad args. */
int oa_pwm_worker_take(oa_pwm_worker *w, oa_pwm_completion *out);
/* -EAGAIN while accepted work remains or until the thread has exited. */
int oa_pwm_worker_quiesce(oa_pwm_worker *w);
int oa_pwm_worker_join(oa_pwm_worker *w);
int oa_pwm_worker_destroy(oa_pwm_worker *w);

#ifdef __cplusplus
}
#endif

#endif
