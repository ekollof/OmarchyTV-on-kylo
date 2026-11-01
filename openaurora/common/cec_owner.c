#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "openaurora/cec_owner.h"

struct request {
    int kind;
    const oa_cec_acquire *acquire;
    const oa_cec_message *outgoing;
    oa_cec_message *incoming;
    int rc;
    int err;
    int done;
    int asynchronous;
    oa_cec_acquire acquire_copy;
    oa_cec_authority_io authority_copy;
    oa_cec_device_io device_copy;
    char path[OA_CEC_OWNER_PATH_MAX];
    oa_cec_message value;
};

struct oa_cec_owner {
    pthread_t thread;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int started;
    int shutdown;
    int exited;
    int session_open; /* published under mu; device itself is worker-only */
    unsigned generation;
    oa_cec_device device;
    struct request *job;
    struct request async_job;
    oa_cec_device_io async_io; /* stable while the device retains its fd */
    oa_cec_completion completion;
    uint64_t next_ticket;
    int async_busy;
    int async_ready;
    void (*on_settle)(void *);
    void *settle_ud;
};

enum { JOB_ACQUIRE = OA_CEC_ASYNC_ACQUIRE, JOB_SEND = OA_CEC_ASYNC_SEND,
       JOB_RECEIVE = OA_CEC_ASYNC_RECEIVE, JOB_RELEASE = OA_CEC_ASYNC_RELEASE };

static int perform(oa_cec_owner *owner, struct request *job)
{
    switch (job->kind) {
    case JOB_ACQUIRE: {
        const oa_cec_acquire *request = job->acquire;
        if (!request || !request->authority || !request->device || !request->path || !*request->path ||
            !request->authority->read_mirror || !request->authority->write_register ||
            !request->device->open || !request->device->ioctl || !request->device->close) {
            errno = EINVAL;
            return -1;
        }
        /* Reject a second acquisition before touching authority registers. */
        if (owner->device.initialized) { errno = EBUSY; return -1; }
        if (oa_cec_authority_ensure(request->authority) != 0) return -1;
        if (job->asynchronous) {
            owner->async_io = job->device_copy;
            return oa_cec_device_open(&owner->device, request->path, &owner->async_io);
        }
        return oa_cec_device_open(&owner->device, request->path, request->device);
    }
    case JOB_SEND:
        return oa_cec_device_send(&owner->device, job->outgoing);
    case JOB_RECEIVE:
        return oa_cec_device_receive(&owner->device, job->incoming);
    case JOB_RELEASE:
        return oa_cec_device_close(&owner->device);
    default:
        errno = EINVAL;
        return -1;
    }
}

static void *worker(void *argument)
{
    oa_cec_owner *owner = argument;
    for (;;) {
        pthread_mutex_lock(&owner->mu);
        while (!owner->job && !owner->shutdown)
            pthread_cond_wait(&owner->cv, &owner->mu);
        if (owner->shutdown && !owner->job) {
            unsigned generation = owner->generation;
            int open = owner->session_open;
            pthread_mutex_unlock(&owner->mu);
            if (open) {
                int rc = oa_cec_device_close(&owner->device);
                pthread_mutex_lock(&owner->mu);
                if (rc != 0) {
                    while (owner->shutdown && owner->generation == generation)
                        pthread_cond_wait(&owner->cv, &owner->mu);
                    pthread_mutex_unlock(&owner->mu);
                    continue;
                }
                pthread_mutex_unlock(&owner->mu);
            }
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
        /* receive returns 1 for a decoded frame, 0 for no frame. */
        int err = rc < 0 ? errno : 0;
        pthread_mutex_lock(&owner->mu);
        owner->session_open = owner->device.initialized;
        job->rc = rc;
        job->err = err;
        job->done = 1;
        if (job->asynchronous) {
            owner->completion.operation = job->kind;
            owner->completion.rc = rc;
            owner->completion.error = err;
            owner->completion.message = job->kind == JOB_RECEIVE && rc == 1 ?
                job->value : (oa_cec_message){0};
            owner->async_ready = 1;
        }
        owner->job = NULL;
        pthread_cond_broadcast(&owner->cv);
        pthread_mutex_unlock(&owner->mu);
    }
    return NULL;
}

int oa_cec_owner_create(oa_cec_owner **out, void (*on_settle)(void *), void *ud)
{
    if (!out) { errno = EINVAL; return -1; }
    oa_cec_owner *owner = calloc(1, sizeof *owner);
    if (!owner) return -1;
    owner->device = (oa_cec_device)OA_CEC_DEVICE_INIT;
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

static int submit(oa_cec_owner *owner, struct request *request)
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
    if (rc < 0) errno = err ? err : EIO;
    return rc;
}

int oa_cec_owner_acquire(oa_cec_owner *owner, const oa_cec_acquire *request)
{
    struct request job = { .kind = JOB_ACQUIRE, .acquire = request };
    return submit(owner, &job);
}
int oa_cec_owner_send(oa_cec_owner *owner, const oa_cec_message *message)
{
    struct request job = { .kind = JOB_SEND, .outgoing = message };
    return submit(owner, &job);
}
int oa_cec_owner_receive(oa_cec_owner *owner, oa_cec_message *message)
{
    struct request job = { .kind = JOB_RECEIVE, .incoming = message };
    return submit(owner, &job);
}
int oa_cec_owner_release(oa_cec_owner *owner)
{
    struct request job = { .kind = JOB_RELEASE };
    return submit(owner, &job);
}

static int submit_async(oa_cec_owner *owner, const struct request *request, uint64_t *ticket)
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
    struct request *job = &owner->async_job;
    job->asynchronous = 1;
    job->acquire_copy.authority = &job->authority_copy;
    job->acquire_copy.device = &job->device_copy;
    job->acquire_copy.path = job->path;
    job->acquire = &job->acquire_copy;
    job->outgoing = &job->value;
    job->incoming = &job->value;
    owner->async_busy = 1;
    owner->async_ready = 0;
    owner->completion = (oa_cec_completion){ .ticket = ++owner->next_ticket };
    owner->job = job;
    *ticket = owner->next_ticket;
    pthread_cond_signal(&owner->cv);
    pthread_mutex_unlock(&owner->mu);
    return 0;
}
int oa_cec_owner_acquire_async(oa_cec_owner *owner, const oa_cec_acquire *acquire, uint64_t *ticket)
{
    if (!acquire || !acquire->authority || !acquire->device || !acquire->path || !*acquire->path) {
        errno = EINVAL;
        return -1;
    }
    size_t length = strnlen(acquire->path, OA_CEC_OWNER_PATH_MAX);
    if (length == OA_CEC_OWNER_PATH_MAX) { errno = ENAMETOOLONG; return -1; }
    struct request request = { .kind = JOB_ACQUIRE,
        .authority_copy = *acquire->authority, .device_copy = *acquire->device };
    memcpy(request.path, acquire->path, length + 1);
    return submit_async(owner, &request, ticket);
}
int oa_cec_owner_send_async(oa_cec_owner *owner, const oa_cec_message *message, uint64_t *ticket)
{
    if (!message) { errno = EINVAL; return -1; }
    uint8_t frame[OA_CEC_MAX_FRAME];
    size_t length;
    if (oa_cec_encode(message, frame, sizeof frame, &length)) { errno = EINVAL; return -1; }
    if (message->initiator != 0) { errno = EADDRNOTAVAIL; return -1; }
    struct request request = { .kind = JOB_SEND, .value = *message };
    return submit_async(owner, &request, ticket);
}
int oa_cec_owner_receive_async(oa_cec_owner *owner, uint64_t *ticket)
{
    const struct request request = { .kind = JOB_RECEIVE };
    return submit_async(owner, &request, ticket);
}
int oa_cec_owner_release_async(oa_cec_owner *owner, uint64_t *ticket)
{
    const struct request request = { .kind = JOB_RELEASE };
    return submit_async(owner, &request, ticket);
}
int oa_cec_owner_take(oa_cec_owner *owner, oa_cec_completion *out)
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

int oa_cec_owner_quiesce(oa_cec_owner *owner)
{
    if (!owner) return 0;
    pthread_mutex_lock(&owner->mu);
    owner->shutdown = 1;
    owner->generation++;
    pthread_cond_broadcast(&owner->cv);
    int done = owner->exited;
    pthread_mutex_unlock(&owner->mu);
    return done ? 0 : -1;
}

int oa_cec_owner_join(oa_cec_owner *owner)
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

void oa_cec_owner_destroy(oa_cec_owner *owner)
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
