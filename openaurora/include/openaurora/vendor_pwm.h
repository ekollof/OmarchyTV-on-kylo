/* Fixed firmware PWM ABI: decompilation plus ARM return-path checks.
 * See artifacts/reports/PWM-CONTRACT-RECHECK-2026-10-31.md.
 * scripts/fetch-vendor.sh copies this file to vendor/lib/vendor-hal.h.
 */
#ifndef OPENAURORA_VENDOR_PWM_H
#define OPENAURORA_VENDOR_PWM_H

#include <stddef.h>
#include <stdint.h>

struct pwm_open_req {
    uint32_t channel;
    uint32_t freq;
    uint32_t duty;
    uint32_t shift;
    uint32_t mode;
    uint32_t extra;
};
struct pwm_set_req {
    uint32_t freq;
    uint32_t duty;
    uint32_t shift;
    uint32_t mode;
};
_Static_assert(sizeof(struct pwm_open_req) == 24, "PWM Open reads six words");
_Static_assert(offsetof(struct pwm_open_req, extra) == 20, "PWM Open word 5");
_Static_assert(sizeof(struct pwm_set_req) == 16, "PWM Set reads four words");
_Static_assert(offsetof(struct pwm_set_req, mode) == 12, "PWM Set mode word");

#ifndef OA_NO_VENDOR
/* Scalar results are not panel-level readback or proof that an ioctl ran.
 * Open/Set: 0 = definite failure (bad argument/fd/ioctl), nonzero = the
 * vendor success path (the GetLock shortcut in this lib always returns 1).
 * Init: 1 = initialized now, 0 = already initialized; both are success. */
int Hal_Pwm_Init(void);
int Hal_Pwm_Open(struct pwm_open_req *request);
int Hal_Pwm_Set_ByChannel(unsigned channel, struct pwm_set_req *request);
void Hal_Pwm_Close(void);
int Hal_Pwm_Terminate(void);
#endif

#endif
