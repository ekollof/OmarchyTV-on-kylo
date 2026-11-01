/* One worker thread owns a blocking mixer session. Real netlink is not selected:
 * a NULL io is rejected, including on ARM, so this cannot bind beside the vendor
 * HAL socket by accident. Shutdown closes the session, which consumes its fd.
 * Open/gain/close are synchronous RPCs and block the caller. Loop callers use
 * the bounded *_async submissions and take() value completions below. */
#ifndef OPENAURORA_AUDIO_OWNER_H
#define OPENAURORA_AUDIO_OWNER_H
#include "openaurora/audio_client.h"
typedef struct oa_audio_owner oa_audio_owner;
enum { OA_AUDIO_ASYNC_OPEN = 1, OA_AUDIO_ASYNC_GAIN, OA_AUDIO_ASYNC_CLOSE };
typedef struct {
    uint64_t ticket;
    int operation;
    int rc;
    int error; /* captured worker errno, zero on success */
    oa_audio_opu_gain gain; /* valid only for a successful gain result */
} oa_audio_completion;
int oa_audio_owner_create(oa_audio_owner **out, void (*on_settle)(void *), void *ud);
int oa_audio_owner_open(oa_audio_owner *owner, const oa_audio_client_io *io);
int oa_audio_owner_gain(oa_audio_owner *owner, unsigned device, int timeout_ms, oa_audio_opu_gain *out);
int oa_audio_owner_close(oa_audio_owner *owner);
/* Nonblocking, bounded submission: one request OR uncollected result at a time.
 * 0 accepts and writes a nonzero instance-scoped ticket; -1/EBUSY leaves caller
 * output alone. Tickets must not cross owner destruction/reload.
 * Open copies the I/O table; its context/code remain borrowed through close
 * or quiesce+join. NULL never selects the system socket. No completion callback
 * or borrowed output pointer survives submission. Poll take() on the loop.
 * Quiesce drains an accepted job before closing; destroy discards an untaken
 * value result. Keep the owner alive until join, and serialize destruction.
 * Synchronous RPCs reject while an async request/result occupies the slot. */
int oa_audio_owner_open_async(oa_audio_owner *owner, const oa_audio_client_io *io, uint64_t *ticket);
int oa_audio_owner_gain_async(oa_audio_owner *owner, unsigned device, int timeout_ms, uint64_t *ticket);
int oa_audio_owner_close_async(oa_audio_owner *owner, uint64_t *ticket);
/* 1 copied/consumed a result, 0 not ready (output unchanged), -1 invalid args.
 * Result rc is transport/query status, never DSP/audible acceptance. */
int oa_audio_owner_take(oa_audio_owner *owner, oa_audio_completion *out);
int oa_audio_owner_quiesce(oa_audio_owner *owner);
int oa_audio_owner_join(oa_audio_owner *owner);
void oa_audio_owner_destroy(oa_audio_owner *owner);
#endif
