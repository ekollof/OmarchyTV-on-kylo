/* Bounded FIFO worker for blocking panel PWM writes. */
#include "openaurora/pwm_worker.h"

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define OA_PWM_QUEUE 8
#define OA_PWM_DONE_QUEUE 16

struct oa_pwm_worker {
    pthread_t thread;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    oa_pwm_worker_io io;
    struct { uint64_t ticket; int pct; } jobs[OA_PWM_QUEUE];
    unsigned head, tail, count;
    oa_pwm_completion done[OA_PWM_DONE_QUEUE];
    unsigned dhead, dtail, dcount, dropped;
    int stopping, stopped;
    uint64_t next_ticket;
};

static void finish(oa_pwm_worker *w, oa_pwm_completion *out)
{
    pthread_mutex_lock(&w->mu);
    if (w->dcount == OA_PWM_DONE_QUEUE) {
        w->dhead = (w->dhead + 1) % OA_PWM_DONE_QUEUE;
        w->dcount--;
        w->dropped++;
    }
    w->done[w->dtail] = *out;
    w->dtail = (w->dtail + 1) % OA_PWM_DONE_QUEUE;
    w->dcount++;
    pthread_cond_signal(&w->cv);
    pthread_mutex_unlock(&w->mu);
}

static void *worker(void *v)
{
    oa_pwm_worker *w = v;
    for (;;) {
        uint64_t ticket;
        int pct;
        oa_pwm_completion out;
        pthread_mutex_lock(&w->mu);
        while (!w->count && !w->stopping)
            pthread_cond_wait(&w->cv, &w->mu);
        if (!w->count && w->stopping) {
            w->stopped = 1;
            pthread_mutex_unlock(&w->mu);
            return NULL;
        }
        ticket = w->jobs[w->head].ticket;
        pct = w->jobs[w->head].pct;
        w->head = (w->head + 1) % OA_PWM_QUEUE;
        w->count--;
        pthread_mutex_unlock(&w->mu);

        out.ticket = ticket;
        out.pct = pct;
        out.rc = w->io.apply(w->io.ud, pct);
        finish(w, &out);
    }
}

int oa_pwm_worker_create(oa_pwm_worker **out, const oa_pwm_worker_io *io)
{
    oa_pwm_worker *w;
    int rc;
    if (!out || !io || !io->apply)
        return -EINVAL;
    *out = NULL;
    w = calloc(1, sizeof *w);
    if (!w)
        return -ENOMEM;
    w->io = *io;
    rc = pthread_mutex_init(&w->mu, NULL);
    if (rc) { free(w); return -rc; }
    rc = pthread_cond_init(&w->cv, NULL);
    if (rc) { pthread_mutex_destroy(&w->mu); free(w); return -rc; }
    w->next_ticket = 1;
    rc = pthread_create(&w->thread, NULL, worker, w);
    if (rc) {
        pthread_cond_destroy(&w->cv);
        pthread_mutex_destroy(&w->mu);
        free(w);
        return -rc;
    }
    *out = w;
    return 0;
}

int oa_pwm_worker_submit(oa_pwm_worker *w, int pct, uint64_t *ticket)
{
    int rc = 0;
    if (!w || !ticket || pct < 0 || pct > 100)
        return -EINVAL;
    pthread_mutex_lock(&w->mu);
    if (w->stopping)
        rc = -ESHUTDOWN;
    else if (w->count == OA_PWM_QUEUE)
        rc = -EBUSY;
    else {
        w->jobs[w->tail].ticket = w->next_ticket++;
        w->jobs[w->tail].pct = pct;
        w->tail = (w->tail + 1) % OA_PWM_QUEUE;
        w->count++;
        *ticket = w->next_ticket - 1;
        pthread_cond_signal(&w->cv);
    }
    pthread_mutex_unlock(&w->mu);
    return rc;
}

int oa_pwm_worker_take(oa_pwm_worker *w, oa_pwm_completion *out)
{
    if (!w || !out)
        return -EINVAL;
    pthread_mutex_lock(&w->mu);
    if (!w->dcount) {
        pthread_mutex_unlock(&w->mu);
        return 0;
    }
    *out = w->done[w->dhead];
    w->dhead = (w->dhead + 1) % OA_PWM_DONE_QUEUE;
    w->dcount--;
    pthread_mutex_unlock(&w->mu);
    return 1;
}

int oa_pwm_worker_quiesce(oa_pwm_worker *w)
{
    if (!w)
        return -EINVAL;
    pthread_mutex_lock(&w->mu);
    w->stopping = 1;
    pthread_cond_signal(&w->cv);
    int busy = w->count || !w->stopped;
    pthread_mutex_unlock(&w->mu);
    return busy ? -EAGAIN : 0;
}

int oa_pwm_worker_join(oa_pwm_worker *w)
{
    if (!w)
        return -EINVAL;
    pthread_join(w->thread, NULL);
    return 0;
}

int oa_pwm_worker_destroy(oa_pwm_worker *w)
{
    if (!w)
        return -EINVAL;
    pthread_mutex_lock(&w->mu);
    int busy = !w->stopped || w->count;
    pthread_mutex_unlock(&w->mu);
    if (busy)
        return -EBUSY;
    pthread_cond_destroy(&w->cv);
    pthread_mutex_destroy(&w->mu);
    free(w);
    return 0;
}
