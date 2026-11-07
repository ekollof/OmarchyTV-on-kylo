/* One worker owns retained amp image/cache state and executes blocking Init.
 * Injection only: NULL I/O never selects native devices, even on ARM. */
#ifndef OPENAURORA_AMP_OWNER_H
#define OPENAURORA_AMP_OWNER_H
#include "openaurora/amp_init.h"
typedef struct oa_amp_owner oa_amp_owner;
/* Worker job kinds. A submission is ordered FIFO by the single worker, and a
 * second submission is rejected with -EBUSY until the pending job's result is
 * taken, so mute/balance cannot overtake a pending Init. */
enum {
    OA_AMP_JOB_INIT = 0,
    OA_AMP_JOB_MUTE = 1,
    OA_AMP_JOB_BALANCE = 2
};
typedef struct {
    uint64_t ticket;
    int kind;          /* OA_AMP_JOB_* */
    int native_result; /* Init's1/0; mute/balance 0 success or negative errno */
    int value;         /* mute0/1 or balance0..100, copied on success */
    oa_amp_init_state state; /* copied after execution, including failed-write state */
} oa_amp_completion;
/* APIs return0 or negative errno, except take's1/0 copied/not-ready result.
 * create copies initial state and I/O table. Callback code, context and GPIO
 * records remain borrowed until successful quiesce/join, not merely take().
 * Initial state is explicit; no guessed baseline or native I/O fallback.
 * Nothing is executed until submit. Output handles/tickets stay unchanged on
 * error. Handles are process-local; inherited handles reject ECHILD after fork.
 * Serialize quiesce/join/destroy; do not destroy concurrently with other APIs. */
int oa_amp_owner_create(oa_amp_owner **out,const oa_amp_init_state *initial,const oa_amp_init_io *io);
/* One pending request OR untaken result. Copies scalar inputs and a bounded
 * NUL-terminated power name (max31 bytes). Caller data can change after return.
 * Tickets are instance-scoped, not reusable across owner/module replacement. */
int oa_amp_owner_submit(oa_amp_owner *owner,const oa_ad82587_profile *profile,uint64_t *ticket);
/* Submit one mute/balance job on the same worker. on must be0/1; balance must
 * be0..100 (the register formula is applied on the worker). Rejections are the
 * same as submit: -EBUSY while a job is pending or its result is untaken,
 * -ESHUTDOWN after quiesce, -EOVERFLOW at ticket exhaustion. The vendor GPIO
 * record and I2C write run only on the worker; no hardware write happens in
 * these calls. */
int oa_amp_owner_submit_mute(oa_amp_owner *owner,int on,uint64_t *ticket);
int oa_amp_owner_submit_balance(oa_amp_owner *owner,int balance,uint64_t *ticket);
int oa_amp_owner_take(oa_amp_owner *owner,oa_amp_completion *out);
/* Quiesce rejects new jobs, drains accepted work without cancellation/replay,
 * then attempts a nonblocking join. Returns-EBUSY until fully terminated/joined,
 * including TLS destructors. No backend/device close or paired shutdown occurs.
 * Completion stays available after join. join alone never requests shutdown. */
int oa_amp_owner_quiesce(oa_amp_owner *owner);
int oa_amp_owner_join(oa_amp_owner *owner);
/* Only after joined; otherwise-EBUSY. Discards an explicitly untaken result. */
int oa_amp_owner_destroy(oa_amp_owner *owner);
#endif
