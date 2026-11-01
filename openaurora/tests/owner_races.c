/* Hold an injected open in-flight while the lifecycle thread quiesces.
 * No real device/socket is opened. The lifecycle must not wait for the job. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "openaurora/audio_owner.h"
#include "openaurora/cec_owner.h"

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int entered, proceed;
static oa_audio_owner *audio;
static oa_cec_owner *cec;
static int callback_error;
static void settled(void *ud)
{
    int kind = *(int *)ud;
    errno = 0;
    int rc = kind ? oa_cec_owner_join(cec) : oa_audio_owner_join(audio);
    assert(rc == -1 && errno == EDEADLK);
    callback_error = errno;
}
static void blocked(void)
{
    pthread_mutex_lock(&mu);
    entered = 1;
    pthread_cond_broadcast(&cv);
    while (!proceed) pthread_cond_wait(&cv, &mu);
    pthread_mutex_unlock(&mu);
}
static int sock_open(void *ctx, int domain, int type, int protocol)
{ (void)ctx; (void)domain; (void)type; (void)protocol; blocked(); return 4; }
static int sock_bind(void *ctx, int fd, uint32_t port)
{ (void)ctx; (void)fd; (void)port; return 0; }
static ssize_t sock_send(void *ctx, int fd, const void *data, size_t size)
{ (void)ctx; (void)fd; (void)data; return (ssize_t)size; }
static int sock_wait(void *ctx, int fd, int ms)
{ (void)ctx; (void)fd; (void)ms; return 0; }
static ssize_t sock_recv(void *ctx, int fd, void *data, size_t size, uint32_t *peer, int *truncated)
{ (void)ctx; (void)fd; (void)data; (void)size; (void)peer; (void)truncated; errno = EAGAIN; return -1; }
static int close_fd(void *ctx, int fd) { (void)ctx; (void)fd; return 0; }
static const oa_audio_client_io audio_io = {
    NULL, sock_open, sock_bind, sock_send, sock_wait, sock_recv, close_fd
};
static int mirror(void *ctx, uint32_t *word) { (void)ctx; *word = 2; return 0; }
static int reg_write(void *ctx, uint16_t reg, uint8_t byte)
{ (void)ctx; (void)reg; (void)byte; assert(0); return -1; }
static int dev_open(void *ctx, const char *path, int flags)
{ (void)ctx; (void)path; (void)flags; blocked(); return 5; }
static int dev_ioctl(void *ctx, int fd, unsigned long request, void *arg)
{ (void)ctx; (void)fd; (void)request; (void)arg; return 0; }
static const oa_cec_authority_io authority = { NULL, mirror, reg_write };
static const oa_cec_device_io device = { NULL, dev_open, dev_ioctl, close_fd };
static void *open_audio(void *ud)
{ (void)ud; assert(oa_audio_owner_open(audio, &audio_io) == 0); return NULL; }
static void *open_cec(void *ud)
{
    (void)ud;
    const oa_cec_acquire request = { &authority, &device, "/fixture" };
    assert(oa_cec_owner_acquire(cec, &request) == 0);
    return NULL;
}
static double now(void)
{
    struct timespec value;
    assert(clock_gettime(CLOCK_MONOTONIC, &value) == 0);
    return (double)value.tv_sec + (double)value.tv_nsec / 1000000000.;
}
int main(void)
{
    alarm(10);
    for (int kind = 0; kind < 2; kind++) {
        entered = proceed = 0;
        callback_error = 0;
        if (kind) assert(oa_cec_owner_create(&cec, settled, &kind) == 0);
        else assert(oa_audio_owner_create(&audio, settled, &kind) == 0);
        pthread_t caller;
        assert(pthread_create(&caller, NULL, kind ? open_cec : open_audio, NULL) == 0);
        pthread_mutex_lock(&mu);
        while (!entered) pthread_cond_wait(&cv, &mu);
        pthread_mutex_unlock(&mu);
        double start = now();
        assert((kind ? oa_cec_owner_quiesce(cec) : oa_audio_owner_quiesce(audio)) == -1);
        assert(now() - start < 0.25); /* catches the former 500 ms idle wait */
        pthread_mutex_lock(&mu);
        proceed = 1;
        pthread_cond_broadcast(&cv);
        pthread_mutex_unlock(&mu);
        assert(pthread_join(caller, NULL) == 0);
        int done = 0;
        for (int i = 0; i < 1000; i++) {
            if ((kind ? oa_cec_owner_quiesce(cec) : oa_audio_owner_quiesce(audio)) == 0) {
                done = 1;
                break;
            }
            struct timespec delay = { .tv_nsec = 1000000 };
            nanosleep(&delay, NULL);
        }
        assert(done);
        if (kind) { assert(oa_cec_owner_join(cec) == 0); oa_cec_owner_destroy(cec); }
        else { assert(oa_audio_owner_join(audio) == 0); oa_audio_owner_destroy(audio); }
        assert(callback_error == EDEADLK); /* failed self-join did not consume joinability */
    }
    puts("  ok   CEC/audio in-flight opens stay worker-owned; quiesce returns before I/O completion");
    return 0;
}
