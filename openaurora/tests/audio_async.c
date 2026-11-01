/* Deterministically blocked I/O, copied requests/results, no real sockets. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "openaurora/audio_owner.h"

static struct {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int entered, release, pending, close_error, closes;
    pthread_t worker;
} hw = { .mu = PTHREAD_MUTEX_INITIALIZER, .cv = PTHREAD_COND_INITIALIZER };
static int socket_io(void *ctx, int domain, int type, int protocol)
{
    (void)domain; (void)type; (void)protocol;
    assert(ctx == &hw);
    pthread_mutex_lock(&hw.mu);
    hw.worker = pthread_self();
    hw.entered = 1;
    pthread_cond_broadcast(&hw.cv);
    while (!hw.release) pthread_cond_wait(&hw.cv, &hw.mu);
    pthread_mutex_unlock(&hw.mu);
    return 0; /* fd zero is valid */
}
static int bind_io(void *ctx, int fd, uint32_t port)
{ assert(ctx == &hw && fd == 0 && port == (uint32_t)getpid()); return 0; }
static ssize_t send_io(void *ctx, int fd, const void *bytes, size_t length)
{
    (void)bytes;
    assert(ctx == &hw && fd == 0 && pthread_equal(hw.worker, pthread_self()));
    hw.pending = 1;
    return (ssize_t)length;
}
static int wait_io(void *ctx, int fd, int timeout)
{ assert(ctx == &hw && fd == 0 && timeout == 25); return 1; }
static ssize_t receive_io(void *ctx, int fd, void *out, size_t cap, uint32_t *peer, int *truncated)
{
    assert(ctx == &hw && fd == 0 && cap >= 84);
    if (!hw.pending) { errno = EAGAIN; return -1; }
    hw.pending = 0;
    uint8_t bytes[84] = {83};
    memcpy(bytes + 16, "GetOPUGain", 10);
    bytes[26] = 5;
    bytes[30] = 0x34;
    bytes[31] = 0x12;
    bytes[35] = 0x80;
    memcpy(out, bytes, sizeof bytes);
    *peer = 0;
    *truncated = 0;
    return sizeof bytes;
}
static int close_io(void *ctx, int fd)
{
    assert(ctx == &hw && fd == 0 && pthread_equal(hw.worker, pthread_self()));
    hw.closes++;
    if (hw.close_error) { errno = EIO; return -1; }
    return 0;
}
static const oa_audio_client_io fixture = {
    &hw, socket_io, bind_io, send_io, wait_io, receive_io, close_io
};
static void pause_briefly(void)
{
    const struct timespec value = { .tv_nsec = 1000000 };
    nanosleep(&value, NULL);
}
static oa_audio_completion result(oa_audio_owner *owner, uint64_t ticket, int op, int rc, int error)
{
    oa_audio_completion value = {0};
    int ready = 0;
    for (int i = 0; i < 1000 && !ready; i++) {
        ready = oa_audio_owner_take(owner, &value);
        assert(ready >= 0);
        if (!ready) pause_briefly();
    }
    assert(ready == 1 && value.ticket == ticket && value.operation == op);
    assert(value.rc == rc && value.error == error);
    oa_audio_completion before = value;
    assert(oa_audio_owner_take(owner, &value) == 0 && !memcmp(&value, &before, sizeof value));
    return value;
}
static void finish(oa_audio_owner *owner)
{
    int done = 0;
    for (int i = 0; i < 1000 && !done; i++) {
        done = oa_audio_owner_quiesce(owner) == 0;
        if (!done) pause_briefly();
    }
    assert(done && oa_audio_owner_join(owner) == 0);
}
static void allow_open(void)
{
    pthread_mutex_lock(&hw.mu);
    hw.release = 1;
    pthread_cond_broadcast(&hw.cv);
    pthread_mutex_unlock(&hw.mu);
}
int main(void)
{
    alarm(10);
    oa_audio_owner *owner = NULL;
    assert(oa_audio_owner_create(&owner, NULL, NULL) == 0);
    uint64_t ticket = 999, untouched = 999;
    assert(oa_audio_owner_open_async(owner, NULL, &ticket) == -1 && errno == ENOTSUP && ticket == 999);
    assert(oa_audio_owner_gain_async(owner, 6, 25, &ticket) == -1 && errno == EINVAL && ticket == 999);
    assert(oa_audio_owner_gain_async(owner, 5, -1, &ticket) == -1 && errno == EINVAL);
    assert(oa_audio_owner_close_async(owner, NULL) == -1 && errno == EINVAL);
    oa_audio_client_io caller_io = fixture;
    assert(oa_audio_owner_open_async(owner, &caller_io, &ticket) == 0 && ticket == 1);
    pthread_mutex_lock(&hw.mu);
    while (!hw.entered) pthread_cond_wait(&hw.cv, &hw.mu);
    pthread_mutex_unlock(&hw.mu);
    /* Submission returned despite the worker's blocked open. No callback table
     * or output buffer is borrowed from that now-finished submission call. */
    memset(&caller_io, 0, sizeof caller_io);
    assert(oa_audio_owner_close_async(owner, &untouched) == -1 && errno == EBUSY && untouched == 999);
    assert(oa_audio_owner_close(owner) == -1 && errno == EBUSY);
    oa_audio_completion pending = { .ticket = 123 };
    assert(oa_audio_owner_take(owner, &pending) == 0 && pending.ticket == 123);
    allow_open();
    (void)result(owner, ticket, OA_AUDIO_ASYNC_OPEN, 0, 0);
    assert(!pthread_equal(hw.worker, pthread_self()));
    puts("  ok   async open returns during blocked I/O, owns callback-table copy and applies bounded backpressure");

    assert(oa_audio_owner_gain_async(owner, 5, 25, &ticket) == 0 && ticket == 2);
    /* Retain result until take: it cannot be overwritten by another request. */
    assert(oa_audio_owner_open_async(owner, &fixture, &untouched) == -1 && errno == EBUSY);
    oa_audio_completion gain = result(owner, ticket, OA_AUDIO_ASYNC_GAIN, 0, 0);
    assert(gain.gain.device == 5 && gain.gain.left == 0x1234 && gain.gain.right == 0x8000);
    assert(oa_audio_owner_open_async(owner, &fixture, &ticket) == 0);
    (void)result(owner, ticket, OA_AUDIO_ASYNC_OPEN, -1, EBUSY);
    /* A rejected second open must not damage the active session's I/O table. */
    assert(oa_audio_owner_gain_async(owner, 5, 25, &ticket) == 0);
    (void)result(owner, ticket, OA_AUDIO_ASYNC_GAIN, 0, 0);
    hw.close_error = 1; /* previous completion synchronizes worker access */
    assert(oa_audio_owner_close_async(owner, &ticket) == 0);
    (void)result(owner, ticket, OA_AUDIO_ASYNC_CLOSE, -1, EIO);
    assert(oa_audio_owner_gain_async(owner, 5, 25, &ticket) == 0);
    (void)result(owner, ticket, OA_AUDIO_ASYNC_GAIN, -1, ENODEV);
    finish(owner);
    assert(hw.closes == 1);
    oa_audio_owner_destroy(owner);
    puts("  ok   results are ticketed, owned and collected once; worker errors survive and failed OS close consumes fd");

    hw.entered = hw.release = hw.close_error = hw.closes = 0;
    assert(oa_audio_owner_create(&owner, NULL, NULL) == 0);
    assert(oa_audio_owner_open_async(owner, &fixture, &ticket) == 0);
    pthread_mutex_lock(&hw.mu);
    while (!hw.entered) pthread_cond_wait(&hw.cv, &hw.mu);
    pthread_mutex_unlock(&hw.mu);
    assert(oa_audio_owner_quiesce(owner) == -1);
    assert(oa_audio_owner_gain_async(owner, 5, 25, &untouched) == -1 && errno == ESHUTDOWN);
    allow_open();
    finish(owner);
    assert(hw.closes == 1);
    (void)result(owner, ticket, OA_AUDIO_ASYNC_OPEN, 0, 0);
    oa_audio_owner_destroy(owner);
    puts("  ok   quiesce rejects new work, drains accepted open and closes before join; result survives shutdown");
    return 0;
}
