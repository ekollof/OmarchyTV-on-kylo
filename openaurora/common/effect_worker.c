/* Bounded FIFO worker for blocking replacement-effect HAL calls. */
#include "openaurora/effect_worker.h"

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/easi_sink.h"
#include "openaurora/video_mute.h"

#define OA_EFFECT_QUEUE 16
#define OA_EFFECT_DONE_QUEUE (OA_EFFECT_QUEUE * 2)

struct oa_effect_worker {
    pthread_t thread;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    oa_effect_job jobs[OA_EFFECT_QUEUE];
    unsigned head, tail, count;
    oa_effect_completion done[OA_EFFECT_DONE_QUEUE];
    unsigned dhead, dtail, dcount, dropped;
    int stopping;
    int stopped;
    uint64_t next_ticket;
};

/* No host API calls here: set_mute is a raw copied pointer and vm is the
 * injected backend. Both are resolved/copied on the loop at submit time. */
static int execute(oa_effect_job *j)
{
    int on = (j->effect == OA_EASI_MUTE_ON || j->effect == OA_EASI_HIDE_VIDEO);
    if (j->effect == OA_EASI_MUTE_ON || j->effect == OA_EASI_MUTE_OFF) {
        if (!j->set_mute)
            return -ENODEV;
        return j->set_mute(on ? 1 : 0);
    }
    if (j->effect == OA_EASI_HIDE_VIDEO || j->effect == OA_EASI_SHOW_VIDEO) {
        if (!j->vm)
            return -ENODEV;
        return oa_video_mute_set(j->vm, on ? 1 : 0);
    }
    return -EINVAL;
}

static void finish(oa_effect_worker *w, oa_effect_completion *out)
{
    pthread_mutex_lock(&w->mu);
    if (w->dcount == OA_EFFECT_DONE_QUEUE) {
        /* Loop did not collect in time: drop the oldest completion, never
         * block the worker. The count is diagnostic only. */
        w->dhead = (w->dhead + 1) % OA_EFFECT_DONE_QUEUE;
        w->dcount--;
        w->dropped++;
    }
    w->done[w->dtail] = *out;
    w->dtail = (w->dtail + 1) % OA_EFFECT_DONE_QUEUE;
    w->dcount++;
    pthread_cond_signal(&w->cv);
    pthread_mutex_unlock(&w->mu);
}

static void *worker(void *v)
{
    oa_effect_worker *w = v;
    for (;;) {
        oa_effect_job job;
        oa_effect_completion out;
        pthread_mutex_lock(&w->mu);
        while (!w->count && !w->stopping)
            pthread_cond_wait(&w->cv, &w->mu);
        if (!w->count && w->stopping) {
            w->stopped = 1;
            pthread_mutex_unlock(&w->mu);
            return NULL;
        }
        job = w->jobs[w->head];
        w->head = (w->head + 1) % OA_EFFECT_QUEUE;
        w->count--;
        pthread_mutex_unlock(&w->mu);

        out.ticket = job.ticket;
        out.effect = job.effect;
        out.rc = execute(&job);
        out.status = out.rc == 0 ? OA_EASI_APPLIED : OA_EASI_FAILED;
        finish(w, &out);
    }
}

int oa_effect_worker_create(oa_effect_worker **out)
{
    oa_effect_worker *w;
    int rc;
    if (!out)
        return -EINVAL;
    *out = NULL;
    w = calloc(1, sizeof *w);
    if (!w)
        return -ENOMEM;
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

int oa_effect_worker_submit(oa_effect_worker *w, const oa_effect_job *job,
                            uint64_t *ticket)
{
    int rc = 0;
    if (!w || !job || !ticket)
        return -EINVAL;
    pthread_mutex_lock(&w->mu);
    if (w->stopping)
        rc = -ESHUTDOWN;
    else if (w->count == OA_EFFECT_QUEUE)
        rc = -EBUSY;
    else {
        oa_effect_job *slot = &w->jobs[w->tail];
        *slot = *job;
        slot->ticket = w->next_ticket++;
        w->tail = (w->tail + 1) % OA_EFFECT_QUEUE;
        w->count++;
        *ticket = slot->ticket;
        pthread_cond_signal(&w->cv);
    }
    pthread_mutex_unlock(&w->mu);
    return rc;
}

int oa_effect_worker_take(oa_effect_worker *w, oa_effect_completion *out)
{
    if (!w || !out)
        return -EINVAL;
    pthread_mutex_lock(&w->mu);
    if (!w->dcount) {
        pthread_mutex_unlock(&w->mu);
        return 0;
    }
    *out = w->done[w->dhead];
    w->dhead = (w->dhead + 1) % OA_EFFECT_DONE_QUEUE;
    w->dcount--;
    pthread_mutex_unlock(&w->mu);
    return 1;
}

int oa_effect_worker_quiesce(oa_effect_worker *w)
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

int oa_effect_worker_join(oa_effect_worker *w)
{
    if (!w)
        return -EINVAL;
    pthread_join(w->thread, NULL);
    return 0;
}

int oa_effect_worker_destroy(oa_effect_worker *w)
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
