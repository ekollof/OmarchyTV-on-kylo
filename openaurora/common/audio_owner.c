#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include "openaurora/audio_owner.h"

struct request {
    int kind;
    const oa_audio_client_io *io;
    unsigned device;
    int timeout;
    oa_audio_opu_gain *gain;
    int rc;
    int err;
    int done;
    int asynchronous;
    oa_audio_client_io io_copy;
    oa_audio_opu_gain value;
};

struct oa_audio_owner {
    pthread_t thread;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int started;
    int shutdown;
    int exited;
    int session_open; /* published under mu; client itself is worker-only */
    oa_audio_client client;
    struct request *job;
    struct request async_job;
    oa_audio_client_io async_io; /* stable across result collection/new jobs */
    oa_audio_completion completion;
    uint64_t next_ticket;
    int async_busy;
    int async_ready;
    void (*on_settle)(void *);
    void *settle_ud;
};

enum { JOB_OPEN = OA_AUDIO_ASYNC_OPEN, JOB_GAIN = OA_AUDIO_ASYNC_GAIN,
       JOB_CLOSE = OA_AUDIO_ASYNC_CLOSE };

static int perform(oa_audio_owner *owner, struct request *job)
{
    switch (job->kind) {
    case JOB_OPEN:
        if (!job->io) { errno = ENOTSUP; return -1; }
        if (job->asynchronous) {
            /* Never overwrite the live session's copied callback table. */
            if (owner->client.fd >= 0) { errno = EBUSY; return -1; }
            owner->async_io = job->io_copy;
            return oa_audio_client_open(&owner->client, &owner->async_io);
        }
        return oa_audio_client_open(&owner->client, job->io);
    case JOB_GAIN:
        return oa_audio_client_gain(&owner->client, job->device, job->timeout, job->gain);
    case JOB_CLOSE:
        return oa_audio_client_close(&owner->client);
    default:
        errno = EINVAL;
        return -1;
    }
}

static void *worker(void *argument)
{
    oa_audio_owner *owner = argument;
    for (;;) {
        pthread_mutex_lock(&owner->mu);
        while (!owner->job && !owner->shutdown)
            pthread_cond_wait(&owner->cv, &owner->mu);
        if (owner->shutdown && !owner->job) {
            int open = owner->session_open;
            pthread_mutex_unlock(&owner->mu);
            if (open) (void)oa_audio_client_close(&owner->client);
            pthread_mutex_lock(&owner->mu);
            owner->session_open = 0;
            owner->exited = 1;
            pthread_cond_broadcast(&owner->cv);
            void (*settle)(void *) = owner->on_settle;
            void *ud = owner->settle_ud;
            pthread_mutex_unlock(&owner->mu);
            if (settle) settle(ud);
            break;
        }
        struct request *job = owner->job;
        pthread_mutex_unlock(&owner->mu);
        int rc = perform(owner, job);
        int err = rc == 0 ? 0 : errno;
        pthread_mutex_lock(&owner->mu);
        owner->session_open = owner->client.fd >= 0;
        job->rc = rc;
        job->err = err;
        job->done = 1;
        if (job->asynchronous) {
            owner->completion.operation = job->kind;
            owner->completion.rc = rc;
            owner->completion.error = err;
            owner->completion.gain = rc == 0 && job->kind == JOB_GAIN ?
                job->value : (oa_audio_opu_gain){0};
            owner->async_ready = 1;
        }
        owner->job = NULL;
        pthread_cond_broadcast(&owner->cv);
        pthread_mutex_unlock(&owner->mu);
    }
    return NULL;
}

int oa_audio_owner_create(oa_audio_owner **out, void (*on_settle)(void *), void *ud)
{
    if (!out) { errno = EINVAL; return -1; }
    oa_audio_owner *owner = calloc(1, sizeof *owner);
    if (!owner) return -1;
    owner->client = (oa_audio_client)OA_AUDIO_CLIENT_INIT;
    owner->on_settle = on_settle;
    owner->settle_ud = ud;
    if (pthread_mutex_init(&owner->mu, NULL) != 0) {
        free(owner);
        errno = ENOMEM;
        return -1;
    }
    pthread_condattr_t attr;
    if (pthread_condattr_init(&attr) != 0) {
        pthread_mutex_destroy(&owner->mu);
        free(owner);
        errno = ENOMEM;
        return -1;
    }
    if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) != 0 ||
        pthread_cond_init(&owner->cv, &attr) != 0) {
        pthread_condattr_destroy(&attr);
        pthread_mutex_destroy(&owner->mu);
        free(owner);
        errno = ENOMEM;
        return -1;
    }
    pthread_condattr_destroy(&attr);
    if (pthread_create(&owner->thread, NULL, worker, owner) != 0) {
        pthread_cond_destroy(&owner->cv);
        pthread_mutex_destroy(&owner->mu);
        free(owner);
        errno = EAGAIN;
        return -1;
    }
    owner->started = 1;
    *out = owner;
    return 0;
}

static int submit(oa_audio_owner *owner, struct request *request)
{
    if (!owner || !request) { errno = EINVAL; return -1; }
    pthread_mutex_lock(&owner->mu);
    if (owner->shutdown || owner->exited) {
        pthread_mutex_unlock(&owner->mu);
        errno = ESHUTDOWN;
        return -1;
    }
    if (owner->async_busy) {
        pthread_mutex_unlock(&owner->mu);
        errno = EBUSY;
        return -1;
    }
    while (owner->job && !owner->shutdown && !owner->async_busy)
        pthread_cond_wait(&owner->cv, &owner->mu);
    if (owner->shutdown || owner->exited) {
        pthread_mutex_unlock(&owner->mu);
        errno = ESHUTDOWN;
        return -1;
    }
    if (owner->async_busy) {
        pthread_mutex_unlock(&owner->mu);
        errno = EBUSY;
        return -1;
    }
    owner->job = request;
    pthread_cond_signal(&owner->cv);
    while (!request->done)
        pthread_cond_wait(&owner->cv, &owner->mu);
    int rc = request->rc;
    int err = request->err;
    pthread_mutex_unlock(&owner->mu);
    if (rc != 0) errno = err ? err : EIO;
    return rc;
}

int oa_audio_owner_open(oa_audio_owner *owner, const oa_audio_client_io *io)
{
    struct request job = { .kind = JOB_OPEN, .io = io };
    return submit(owner, &job);
}
int oa_audio_owner_gain(oa_audio_owner *owner, unsigned device, int timeout_ms, oa_audio_opu_gain *out)
{
    struct request job = { .kind = JOB_GAIN, .device = device, .timeout = timeout_ms, .gain = out };
    return submit(owner, &job);
}
int oa_audio_owner_close(oa_audio_owner *owner)
{
    struct request job = { .kind = JOB_CLOSE };
    return submit(owner, &job);
}

static int submit_async(oa_audio_owner *owner, struct request *request, uint64_t *ticket)
{
    if (!owner || !ticket) { errno = EINVAL; return -1; }
    pthread_mutex_lock(&owner->mu);
    int err = 0;
    if (owner->shutdown || owner->exited) err = ESHUTDOWN;
    else if (owner->job || owner->async_busy) err = EBUSY;
    else if (owner->next_ticket == UINT64_MAX) err = EOVERFLOW;
    if (err) {
        pthread_mutex_unlock(&owner->mu);
        errno = err;
        return -1;
    }
    owner->async_job = *request;
    owner->async_job.asynchronous = 1;
    if (request->kind == JOB_OPEN) owner->async_job.io = &owner->async_job.io_copy;
    if (request->kind == JOB_GAIN) owner->async_job.gain = &owner->async_job.value;
    owner->async_busy = 1;
    owner->async_ready = 0;
    owner->completion = (oa_audio_completion){ .ticket = ++owner->next_ticket };
    owner->job = &owner->async_job;
    *ticket = owner->next_ticket;
    pthread_cond_signal(&owner->cv);
    pthread_mutex_unlock(&owner->mu);
    return 0;
}
int oa_audio_owner_open_async(oa_audio_owner *owner, const oa_audio_client_io *io, uint64_t *ticket)
{
    if (!io) { errno = ENOTSUP; return -1; }
    struct request request = { .kind = JOB_OPEN, .io_copy = *io };
    return submit_async(owner, &request, ticket);
}
int oa_audio_owner_gain_async(oa_audio_owner *owner, unsigned device, int timeout_ms, uint64_t *ticket)
{
    if (device > 5 || timeout_ms < 0) { errno = EINVAL; return -1; }
    struct request request = { .kind = JOB_GAIN, .device = device, .timeout = timeout_ms };
    return submit_async(owner, &request, ticket);
}
int oa_audio_owner_close_async(oa_audio_owner *owner, uint64_t *ticket)
{
    struct request request = { .kind = JOB_CLOSE };
    return submit_async(owner, &request, ticket);
}
int oa_audio_owner_take(oa_audio_owner *owner, oa_audio_completion *out)
{
    if (!owner || !out) { errno = EINVAL; return -1; }
    pthread_mutex_lock(&owner->mu);
    if (!owner->async_ready) {
        pthread_mutex_unlock(&owner->mu);
        return 0;
    }
    *out = owner->completion;
    owner->async_ready = 0;
    owner->async_busy = 0;
    pthread_mutex_unlock(&owner->mu);
    return 1;
}

int oa_audio_owner_quiesce(oa_audio_owner *owner)
{
    if (!owner) return 0;
    pthread_mutex_lock(&owner->mu);
    owner->shutdown = 1;
    pthread_cond_broadcast(&owner->cv);
    int done = owner->exited;
    pthread_mutex_unlock(&owner->mu);
    return done ? 0 : -1;
}

int oa_audio_owner_join(oa_audio_owner *owner)
{
    if (!owner) { errno = EINVAL; return -1; }
    pthread_mutex_lock(&owner->mu);
    int exited = owner->exited;
    int started = owner->started;
    pthread_mutex_unlock(&owner->mu);
    if (!exited) { errno = EBUSY; return -1; }
    if (started && pthread_equal(owner->thread, pthread_self())) { errno = EDEADLK; return -1; }
    if (started) {
        int rc = pthread_join(owner->thread, NULL);
        if (rc != 0) { errno = rc; return -1; }
        pthread_mutex_lock(&owner->mu);
        owner->started = 0;
        pthread_mutex_unlock(&owner->mu);
    }
    return 0;
}

void oa_audio_owner_destroy(oa_audio_owner *owner)
{
    if (!owner) return;
    pthread_mutex_lock(&owner->mu);
    int exited = owner->exited;
    int started = owner->started;
    pthread_mutex_unlock(&owner->mu);
    if (!exited || started) return;
    pthread_cond_destroy(&owner->cv);
    pthread_mutex_destroy(&owner->mu);
    free(owner);
}
