/* Blocking MB230 mixer session. Serialize under one owning worker/process,
 * outside the host loop. Source: AUDIO-NETLINK-2026-11-01. Never borrow a HAL
 * socket or compete for its same-process binding. Injected I/O is borrowed
 * through close and supports offline fixtures; no operation auto-initializes HAL.
 */
#ifndef OPENAURORA_AUDIO_CLIENT_H
#define OPENAURORA_AUDIO_CLIENT_H
#include <sys/types.h>
#include "openaurora/audio_netlink.h"
typedef struct {
    void *context;
    int (*socket)(void *, int domain, int type, int protocol);
    int (*bind)(void *, int fd, uint32_t port);
    ssize_t (*send)(void *, int fd, const void *bytes, size_t size);
    int (*wait)(void *, int fd, int timeout_ms); /* 1 readable / 0 timeout / -1 errno */
    /* Nonblocking receive; -1/EAGAIN when empty. On success set actual peer port
     * and truncation boolean. Invalid source metadata must not report port0. */
    ssize_t (*receive)(void *, int fd, void *bytes, size_t cap, uint32_t *peer, int *truncated);
    int (*close)(void *, int fd); /* Linux close consumes fd even on error. */
} oa_audio_client_io;
typedef struct { int fd; uint32_t port; const oa_audio_client_io *io; } oa_audio_client;
#define OA_AUDIO_CLIENT_INIT { .fd = -1, .port = 0, .io = NULL }
/* NULL io selects real syscalls on target little-endian ARM32 only. Opening
 * reserves this process's socket, without sending or initializing hardware. */
int oa_audio_client_open(oa_audio_client *client, const oa_audio_client_io *io);
/* Zero means DISPATCHED, not receiver/DSP success. No operation ACK exists.
 * Errors/EINTR are never replayed. Retain table memory through the whole call,
 * on errors too. The address is the caller's explicit target32-bit user address. */
int oa_audio_client_table(oa_audio_client *client, unsigned index, uint32_t address);
int oa_audio_client_select(oa_audio_client *client, unsigned device, unsigned index);
/* Direct channel gains0..0x8000; not UI volume/balance/table processing.
 * Zero is dispatch only, not application acknowledgement; never replay EINTR.
 * GetOPUGain is a lossy cache query, not a rollback snapshot for this setter.
 * No current broker/module service exposes this operation. */
int oa_audio_client_set_opu_gain(oa_audio_client *client, unsigned device, unsigned left, unsigned right);
/* Zero means matching kernel reply, not lower-operation success. Output unchanged
 * on error. Drain at most16 stale datagrams before sending; excess fails without
 * a request. timeout_ms>=0 bounds post-dispatch waiting, not execution inside send.
 * Calls after fork reject until child closes/reopens its own session. */
int oa_audio_client_gain(oa_audio_client *client, unsigned device, int timeout_ms, oa_audio_opu_gain *out);
/* Idempotent; consumes/resets owned fd even on final close error. A child may
 * close its inherited reference without changing the parent's session. */
int oa_audio_client_close(oa_audio_client *client);
#endif
