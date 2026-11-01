/* Injected raw CEC only. No device, physical mirror or CEC bus is touched. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "openaurora/cec_owner.h"
#include "openaurora/vendor_cec.h"

enum { BLOCK_OPEN = 1, BLOCK_RECEIVE };
static struct {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int block, entered, proceed, reads, opens, closes, empty;
    atomic_int attempts, fail_close, settled;
    pthread_t worker;
} hw = { .mu = PTHREAD_MUTEX_INITIALIZER, .cv = PTHREAD_COND_INITIALIZER };
static void pause_briefly(void)
{
    const struct timespec value = { .tv_nsec = 1000000 };
    nanosleep(&value, NULL);
}
static void block(int operation)
{
    pthread_mutex_lock(&hw.mu);
    if (hw.block == operation) {
        hw.entered = 1;
        pthread_cond_broadcast(&hw.cv);
        while (!hw.proceed) pthread_cond_wait(&hw.cv, &hw.mu);
    }
    pthread_mutex_unlock(&hw.mu);
}
static void wait_entered(void)
{
    pthread_mutex_lock(&hw.mu);
    while (!hw.entered) pthread_cond_wait(&hw.cv, &hw.mu);
    pthread_mutex_unlock(&hw.mu);
}
static void allow(void)
{
    pthread_mutex_lock(&hw.mu);
    hw.proceed = 1;
    pthread_cond_broadcast(&hw.cv);
    pthread_mutex_unlock(&hw.mu);
}
static int read_mirror(void *ctx, uint32_t *word)
{ assert(ctx == &hw); hw.reads++; *word = 2; return 0; }
static int write_reg(void *ctx, uint16_t reg, uint8_t byte)
{ (void)ctx; (void)reg; (void)byte; assert(0); return -1; }
static int open_io(void *ctx, const char *path, int flags)
{
    (void)flags;
    assert(ctx == &hw);
    hw.worker = pthread_self();
    hw.opens++;
    block(BLOCK_OPEN);
    assert(!strcmp(path, "/fixture/ntkcec"));
    return 0;
}
static int close_io(void *ctx, int fd)
{ assert(ctx == &hw && fd == 0); hw.closes++; return 0; }
static int ioctl_io(void *ctx, int fd, unsigned long request, void *arg)
{
    assert(ctx == &hw && fd == 0 && pthread_equal(hw.worker, pthread_self()));
    if (request == 0x401c4301) return 0;
    if (request == 0x80014302) {
        atomic_fetch_add(&hw.attempts, 1);
        if (atomic_load(&hw.fail_close)) { errno = EIO; return -1; }
        return 0;
    }
    oa_cec_raw_packet *p = arg;
    if (request == 0xc0104304) {
        const uint8_t expected[] = { 0x05, 0x44, 0x41 };
        assert(p->length == sizeof expected && !memcmp(p->bytes, expected, sizeof expected));
        return 0;
    }
    assert(request == 0x80104303);
    block(BLOCK_RECEIVE);
    assert(p->bytes && !p->status && !p->length);
    if (hw.empty) return 0;
    const uint8_t bytes[] = { 0x50, 0x7a, 0xa8 };
    memcpy(p->bytes, bytes, sizeof bytes);
    p->length = sizeof bytes;
    p->status = 1;
    errno = EIO; /* positive receive must not turn this into a result error */
    return 0;
}
static const oa_cec_authority_io authority = { &hw, read_mirror, write_reg };
static const oa_cec_device_io io = { &hw, open_io, ioctl_io, close_io };
static const oa_cec_acquire acquisition = { &authority, &io, "/fixture/ntkcec" };
static void settled(void *ud) { assert(ud == &hw); atomic_fetch_add(&hw.settled, 1); }
static oa_cec_completion result(oa_cec_owner *owner, uint64_t ticket, int op, int rc, int err)
{
    oa_cec_completion value = {0};
    int ready = 0;
    for (int i = 0; i < 1000 && !ready; i++) {
        ready = oa_cec_owner_take(owner, &value);
        assert(ready >= 0);
        if (!ready) pause_briefly();
    }
    assert(ready == 1 && value.ticket == ticket && value.operation == op && value.rc == rc && value.error == err);
    oa_cec_completion before = value;
    assert(oa_cec_owner_take(owner, &value) == 0 && !memcmp(&value, &before, sizeof value));
    return value;
}
static void finish(oa_cec_owner *owner)
{
    int done = 0;
    for (int i = 0; i < 1000 && !done; i++) {
        done = oa_cec_owner_quiesce(owner) == 0;
        if (!done) pause_briefly();
    }
    assert(done && oa_cec_owner_join(owner) == 0);
}
static void wait_attempts(int count)
{
    for (int i = 0; i < 1000 && atomic_load(&hw.attempts) < count; i++) pause_briefly();
    assert(atomic_load(&hw.attempts) >= count);
}
int main(void)
{
    alarm(10);
    oa_cec_owner *owner = NULL;
    assert(oa_cec_owner_create(&owner, settled, &hw) == 0);
    uint64_t ticket = 999, untouched = 999;
    assert(oa_cec_owner_acquire_async(owner, NULL, &ticket) == -1 && errno == EINVAL && ticket == 999);
    char long_path[OA_CEC_OWNER_PATH_MAX + 1];
    memset(long_path, 'x', sizeof long_path - 1); long_path[sizeof long_path - 1] = 0;
    oa_cec_acquire request = acquisition; request.path = long_path;
    assert(oa_cec_owner_acquire_async(owner, &request, &ticket) == -1 && errno == ENAMETOOLONG);
    oa_cec_device_io bad_io = io; bad_io.ioctl = NULL;
    request = acquisition; request.device = &bad_io;
    assert(oa_cec_owner_acquire_async(owner, &request, &ticket) == 0);
    (void)result(owner, ticket, OA_CEC_ASYNC_ACQUIRE, -1, EINVAL);
    assert(hw.reads == 0 && hw.opens == 0);

    hw.block = BLOCK_OPEN;
    char path[] = "/fixture/ntkcec";
    oa_cec_authority_io caller_authority = authority;
    oa_cec_device_io caller_io = io;
    request = (oa_cec_acquire){ &caller_authority, &caller_io, path };
    assert(oa_cec_owner_acquire_async(owner, &request, &ticket) == 0);
    wait_entered();
    memset(path, 'x', sizeof path - 1);
    memset(&caller_io, 0, sizeof caller_io); memset(&caller_authority, 0, sizeof caller_authority);
    memset(&request, 0, sizeof request);
    assert(oa_cec_owner_release_async(owner, &untouched) == -1 && errno == EBUSY && untouched == 999);
    assert(oa_cec_owner_release(owner) == -1 && errno == EBUSY);
    oa_cec_completion pending = { .ticket = 123 };
    assert(oa_cec_owner_take(owner, &pending) == 0 && pending.ticket == 123);
    allow();
    (void)result(owner, ticket, OA_CEC_ASYNC_ACQUIRE, 0, 0);
    assert(!pthread_equal(hw.worker, pthread_self()));
    puts("  ok   blocked acquire is nonblocking; path/tables are copied; invalid I/O never touches authority");

    oa_cec_message message = { .destination = 5, .opcode = 0x44, .count = 1, .operands = {0x41} };
    message.initiator = 4;
    assert(oa_cec_owner_send_async(owner, &message, &untouched) == -1 && errno == EADDRNOTAVAIL);
    message.initiator = 0;
    assert(oa_cec_owner_send_async(owner, &message, &ticket) == 0);
    memset(&message, 0, sizeof message);
    (void)result(owner, ticket, OA_CEC_ASYNC_SEND, 0, 0);
    assert(oa_cec_owner_receive_async(owner, &ticket) == 0);
    oa_cec_completion received = result(owner, ticket, OA_CEC_ASYNC_RECEIVE, 1, 0);
    assert(received.message.initiator == 5 && received.message.destination == 0 &&
           received.message.opcode == 0x7a && received.message.count == 1 && received.message.operands[0] == 0xa8);
    hw.empty = 1;
    assert(oa_cec_owner_receive_async(owner, &ticket) == 0);
    oa_cec_completion empty = result(owner, ticket, OA_CEC_ASYNC_RECEIVE, 0, 0);
    assert(empty.message.count == 0 && empty.message.opcode == 0 && received.message.operands[0] == 0xa8);
    puts("  ok   send owns its frame; decoded/empty receives have distinct ticketed copied results");

    atomic_store(&hw.fail_close, 1);
    assert(oa_cec_owner_release_async(owner, &ticket) == 0);
    (void)result(owner, ticket, OA_CEC_ASYNC_RELEASE, -1, EIO);
    assert(hw.closes == 0);
    int reads = hw.reads;
    assert(oa_cec_owner_acquire_async(owner, &acquisition, &ticket) == 0);
    (void)result(owner, ticket, OA_CEC_ASYNC_ACQUIRE, -1, EBUSY);
    assert(hw.reads == reads);
    atomic_store(&hw.fail_close, 0);
    assert(oa_cec_owner_release_async(owner, &ticket) == 0);
    (void)result(owner, ticket, OA_CEC_ASYNC_RELEASE, 0, 0);
    assert(hw.closes == 1);
    finish(owner); oa_cec_owner_destroy(owner);
    puts("  ok   failed hardware release retains fd and copied backend until an explicit successful retry");

    hw.empty = hw.entered = hw.proceed = hw.closes = 0;
    hw.block = BLOCK_RECEIVE;
    atomic_store(&hw.attempts, 0); atomic_store(&hw.fail_close, 1); atomic_store(&hw.settled, 0);
    assert(oa_cec_owner_create(&owner, settled, &hw) == 0);
    assert(oa_cec_owner_acquire_async(owner, &acquisition, &ticket) == 0);
    (void)result(owner, ticket, OA_CEC_ASYNC_ACQUIRE, 0, 0);
    assert(oa_cec_owner_receive_async(owner, &ticket) == 0);
    wait_entered();
    assert(oa_cec_owner_quiesce(owner) == -1);
    assert(oa_cec_owner_release_async(owner, &untouched) == -1 && errno == ESHUTDOWN);
    allow(); wait_attempts(1);
    assert(!hw.closes && !atomic_load(&hw.settled));
    assert(oa_cec_owner_quiesce(owner) == -1);
    wait_attempts(2);
    assert(!hw.closes && !atomic_load(&hw.settled));
    atomic_store(&hw.fail_close, 0);
    finish(owner);
    assert(hw.closes == 1 && atomic_load(&hw.settled) == 1);
    (void)result(owner, ticket, OA_CEC_ASYNC_RECEIVE, 1, 0);
    oa_cec_owner_destroy(owner);
    puts("  ok   shutdown drains blocked receive; failed-close generations retain session and untaken result until join");
    return 0;
}
