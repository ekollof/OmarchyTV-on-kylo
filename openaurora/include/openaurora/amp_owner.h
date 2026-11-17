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
    OA_AMP_JOB_BALANCE = 2,
    OA_AMP_JOB_MONITOR = 3,
    OA_AMP_JOB_VOLUME = 4,
    OA_AMP_JOB_SOFTMUTE = 5,
    OA_AMP_JOB_GET_VOLUME = 6,
    OA_AMP_JOB_HP_MUTE = 7
};
typedef struct {
    uint64_t ticket;
    int kind;          /* OA_AMP_JOB_* */
    /* Init: 1 complete/0 first write failed. mute/balance: 0 or negative errno.
     * MONITOR: 0 match, 1 mismatch recovered (Init then Mute(0)), -EIO read
     * failure (no reinit). The counter/log fields are MONITOR-only and zero
     * for other kinds. */
    int native_result;
    int value;         /* mute0/1 or balance0..100, copied on success */
    oa_amp_init_state state; /* copied after execution, including failed-write state */
    unsigned monitor_read_failures, monitor_mismatches;
    char monitor_log[160]; /* bounded worker message; logged by the module on the loop */
} oa_amp_completion;
/* APIs return0 or negative errno, except take's1/0 copied/not-ready result.
 * create copies initial state and I/O table. Callback code, context and GPIO
 * records remain borrowed until successful quiesce/join, not merely take().
 * Initial state is explicit; no guessed baseline or native I/O fallback.
 * Nothing is executed until submit. Output handles/tickets stay unchanged on
 * error. Handles are process-local; inherited handles reject ECHILD after fork.
 * Serialize quiesce/join/destroy; do not destroy concurrently with other APIs. */
int oa_amp_owner_create(oa_amp_owner **out,const oa_amp_init_state *initial,const oa_amp_init_io *io);
/* Copy the configured profile used by the MONITOR reinit path. The owner keeps
 * its own bounded copy of the power name. Call at module start before any job;
 * a later INIT submit replaces it with the submitted profile. Returns 0 or a
 * negative errno, and never touches hardware or the retained image. */
int oa_amp_owner_set_profile(oa_amp_owner *owner,const oa_ad82587_profile *profile);
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
/* Submit one DEV_AMP_ID monitor check on the same worker. read_regs is copied
 * with its context and called only on the worker as read_regs(ctx,3,3,out);
 * the owner supplies reinit/Mute(0) from its retained state/io. No I2C or GPIO
 * access happens in this call. Backpressure matches the other job kinds. */
int oa_amp_owner_submit_monitor(oa_amp_owner *owner,
    int (*read_regs)(void *ctx,uint8_t first,uint8_t count,uint8_t *out),
    void *read_ctx,uint64_t *ticket);
/* Register read callback for the VOLUME/SOFTMUTE/GET_VOLUME jobs. Vendor
 * boolean: 1 success, 0 failure. Copied with its context; called only on the
 * worker. Without it those jobs fail with -ENOTSUP. */
int oa_amp_owner_set_register_read(oa_amp_owner *owner,
    int (*read_regs)(void *ctx,uint8_t first,uint8_t count,uint8_t *out),
    void *read_ctx);
/* Submit recovered AD82587 reg3 volume (0..255), reg2 soft mute (0/1) and
 * reg3 read jobs. Completion value is the requested volume/mute or the byte
 * read by GET_VOLUME; native_result is 0 or a negative errno. */
int oa_amp_owner_submit_volume(oa_amp_owner *owner,int volume,uint64_t *ticket);
int oa_amp_owner_submit_soft_mute(oa_amp_owner *owner,int on,uint64_t *ticket);
int oa_amp_owner_submit_get_volume(oa_amp_owner *owner,uint64_t *ticket);
/* Submit recovered HP_Mute (GPIO function 0xc3, record selector 2). The
 * worker writes level_on for on==1 and level_off otherwise through the same
 * level dispatch as Init; the vendor ignores the GPIO result, so a
 * native_result of 0 does not prove electrical effect. 0 accepted. */
int oa_amp_owner_submit_hp_mute(oa_amp_owner *owner,int on,uint64_t *ticket);
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
