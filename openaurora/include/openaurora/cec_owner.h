/* One worker thread owns blocking raw CEC calls. Legacy acquire/send/receive/
 * release are synchronous RPCs and must not run on the host loop. Use *_async
 * submissions and take() results there. Quiesce is the nonblocking lifecycle entry.
 * No function here selects the
 * real ntkcec node or a physical-memory authority backend. */
#ifndef OPENAURORA_CEC_OWNER_H
#define OPENAURORA_CEC_OWNER_H
#include "openaurora/cec.h"
#include "openaurora/cec_authority.h"
#include "openaurora/cec_device.h"
typedef struct oa_cec_owner oa_cec_owner;
#define OA_CEC_OWNER_PATH_MAX 256u /* including the terminating NUL */
enum { OA_CEC_ASYNC_ACQUIRE = 1, OA_CEC_ASYNC_SEND, OA_CEC_ASYNC_RECEIVE, OA_CEC_ASYNC_RELEASE };
typedef struct {
    uint64_t ticket;
    int operation;
    int rc; /* receive: 1 frame / 0 empty / -1 error; other operations: 0 / -1 */
    int error; /* worker errno only on negative rc */
    oa_cec_message message; /* valid only for receive rc1 */
} oa_cec_completion;
typedef struct {
    const oa_cec_authority_io *authority;
    const oa_cec_device_io *device;
    const char *path;
} oa_cec_acquire;
/* on_settle runs on the worker after its I/O loop has finished, with no owner
 * lock held. It may post() to the host. It is not a license to unload. */
int oa_cec_owner_create(oa_cec_owner **out, void (*on_settle)(void *), void *ud);
int oa_cec_owner_acquire(oa_cec_owner *owner, const oa_cec_acquire *request);
int oa_cec_owner_send(oa_cec_owner *owner, const oa_cec_message *message);
int oa_cec_owner_receive(oa_cec_owner *owner, oa_cec_message *message);
int oa_cec_owner_release(oa_cec_owner *owner);
/* Nonblocking submissions: one request or uncollected result at a time.
 * Accepted requests write a nonzero instance-scoped ticket. Other outputs are
 * never borrowed. Acquire copies path and I/O tables; send copies its frame.
 * I/O context/code remain borrowed through release or successful quiesce+join,
 * including failed hardware closes. NULL never selects native I/O. Tickets
 * cannot cross destruction/reload. Synchronous RPCs reject while the slot is
 * occupied. No user completion callback is retained. */
int oa_cec_owner_acquire_async(oa_cec_owner *owner, const oa_cec_acquire *request, uint64_t *ticket);
int oa_cec_owner_send_async(oa_cec_owner *owner, const oa_cec_message *message, uint64_t *ticket);
int oa_cec_owner_receive_async(oa_cec_owner *owner, uint64_t *ticket);
int oa_cec_owner_release_async(oa_cec_owner *owner, uint64_t *ticket);
/* 1 copied/consumed result; 0 not ready (output unchanged); -1 invalid args.
 * Quiesce drains accepted work before close, rejects new jobs and preserves an
 * untaken value result until destruction. Failed hardware close retains owner
 * and fd for another quiesce generation, even after result collection. */
int oa_cec_owner_take(oa_cec_owner *owner, oa_cec_completion *out);
/* 0 means the worker has left its loop and join is allowed. An idle or open
 * session returns busy without waiting, including after a failed hardware close.
 * Retry after completion; each call is a
 * new retry generation for a retained close. */
int oa_cec_owner_quiesce(oa_cec_owner *owner);
int oa_cec_owner_join(oa_cec_owner *owner);
void oa_cec_owner_destroy(oa_cec_owner *owner);
#endif
