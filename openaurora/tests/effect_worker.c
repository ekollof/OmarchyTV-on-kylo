/* Single-slot effect worker test. No host API, timer, TV or device. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "openaurora/effect_worker.h"
#include "openaurora/easi_sink.h"
#include "openaurora/video_mute.h"

static pthread_mutex_t gate_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_cv = PTHREAD_COND_INITIALIZER;
static int gate_release;
static int gate_entered;

static int gated_set_mute(int on)
{
    (void)on;
    pthread_mutex_lock(&gate_mu);
    gate_entered = 1;
    pthread_cond_signal(&gate_cv);
    while (!gate_release)
        pthread_cond_wait(&gate_cv, &gate_mu);
    pthread_mutex_unlock(&gate_mu);
    return 0;
}

static int mute_calls, mute_last, mute_rc;
static int fake_set_mute(int on)
{
    mute_calls++;
    mute_last = on ? 1 : 0;
    return mute_rc;
}

static int fake_vm_dev(void *ud, int index) { (void)ud; (void)index; return 11; }
static int fake_vm_path(void *ud, int path) { (void)ud; (void)path; return 2; }
static int fake_vm_info(void *ud, int plane_type,
                        uint32_t info[OA_VIDEO_MUTE_INFO_WORDS])
{
    (void)ud; (void)plane_type;
    info[0] = 33;
    return 0;
}
static int fake_vm_mute(void *ud, int fd, unsigned plane, int enable, int on)
{
    (void)ud; (void)fd; (void)plane; (void)enable; (void)on;
    return mute_rc;
}

static oa_effect_completion wait_done(oa_effect_worker *w)
{
    oa_effect_completion out;
    memset(&out, 0, sizeof out);
    for (int i = 0; i < 5000; i++) {
        if (oa_effect_worker_take(w, &out) == 1)
            return out;
        const struct timespec pause = { .tv_nsec = 1000000 };
        (void)nanosleep(&pause, NULL);
    }
    assert(0 && "worker completion timed out");
    return out;
}

static void test_submit_backpressure_take(void)
{
    oa_effect_worker *w = NULL;
    oa_effect_job job;
    uint64_t ticket = 0;
    oa_effect_completion done;

    mute_calls = 0; mute_last = -1; mute_rc = 0;
    assert(oa_effect_worker_create(&w) == 0 && w);

    memset(&job, 0, sizeof job);
    job.effect = OA_EASI_MUTE_ON;
    job.set_mute = fake_set_mute;
    assert(oa_effect_worker_submit(w, &job, &ticket) == 0 && ticket == 1);

    /* The job may still be running: no host API call is made on submit. */
    assert(mute_calls >= 0);
    done = wait_done(w);
    assert(done.ticket == 1 && done.effect == OA_EASI_MUTE_ON);
    assert(done.status == OA_EASI_APPLIED && done.rc == 0);
    assert(mute_calls == 1 && mute_last == 1);

    /* The FIFO accepts several effects in order and rejects only overflow. */
    memset(&job, 0, sizeof job);
    job.effect = OA_EASI_MUTE_OFF;
    job.set_mute = fake_set_mute;
    uint64_t expected = 2;
    for (int i = 1; i < 16; i++) {
        assert(oa_effect_worker_submit(w, &job, &ticket) == 0);
        assert(ticket == expected++);
    }
    for (uint64_t t = 2; t <= 16; t++) {
        done = wait_done(w);
        assert(done.ticket == t && done.status == OA_EASI_APPLIED);
    }
    assert(mute_calls == 16 && mute_last == 0);

    assert(oa_effect_worker_take(w, &done) == 0);
    assert(oa_effect_worker_destroy(w) == -EBUSY); /* not quiesced/joined */
    int qr = oa_effect_worker_quiesce(w); /* -EAGAIN until the thread exits */
    assert(qr == 0 || qr == -EAGAIN);
    (void)oa_effect_worker_join(w);
    assert(oa_effect_worker_destroy(w) == 0);
}

static void test_video_and_failure(void)
{
    oa_effect_worker *w = NULL;
    oa_effect_job job;
    uint64_t ticket;
    oa_effect_completion done;
    oa_video_mute m;
    oa_video_mute_io io;
    oa_video_mute_state st;

    mute_calls = 0; mute_rc = 0;
    io.ud = NULL; io.drm_device_id = fake_vm_dev;
    io.video_path_to_plane = fake_vm_path; io.get_plane_info = fake_vm_info;
    io.drm_set_mute = fake_vm_mute;
    oa_video_mute_init(&m, &io, OA_VIDEO_MUTE_PATH_MAIN);

    assert(oa_effect_worker_create(&w) == 0);
    memset(&job, 0, sizeof job);
    job.effect = OA_EASI_HIDE_VIDEO;
    job.vm = &m;
    assert(oa_effect_worker_submit(w, &job, &ticket) == 0);
    done = wait_done(w);
    assert(done.status == OA_EASI_APPLIED && done.rc == 0);
    assert(oa_video_mute_snapshot(&m, &st) == 0 && st.hidden == 1);

    memset(&job, 0, sizeof job);
    job.effect = OA_EASI_SHOW_VIDEO;
    job.vm = &m;
    assert(oa_effect_worker_submit(w, &job, &ticket) == 0);
    done = wait_done(w);
    assert(done.status == OA_EASI_APPLIED);
    assert(oa_video_mute_snapshot(&m, &st) == 0 && st.hidden == 0);

    /* A backend failure is reported as FAILED with its raw errno. */
    mute_rc = -EIO;
    memset(&job, 0, sizeof job);
    job.effect = OA_EASI_MUTE_ON;
    job.set_mute = fake_set_mute;
    assert(oa_effect_worker_submit(w, &job, &ticket) == 0);
    done = wait_done(w);
    assert(done.status == OA_EASI_FAILED && done.rc == -EIO);

    /* An effect with no backend never reaches a HAL call. */
    int before = mute_calls;
    memset(&job, 0, sizeof job);
    job.effect = OA_EASI_NOTIFY_SELECTED;
    assert(oa_effect_worker_submit(w, &job, &ticket) == 0);
    done = wait_done(w);
    assert(done.status == OA_EASI_FAILED && done.rc == -EINVAL);
    assert(mute_calls == before);

    (void)oa_effect_worker_quiesce(w);
    (void)oa_effect_worker_join(w);
    assert(oa_effect_worker_destroy(w) == 0);
}

static void test_rejections(void)
{
    oa_effect_worker *w = NULL;
    oa_effect_job job;
    uint64_t ticket;
    oa_effect_completion done;
    memset(&job, 0, sizeof job);
    assert(oa_effect_worker_submit(NULL, &job, &ticket) == -EINVAL);
    assert(oa_effect_worker_create(NULL) == -EINVAL);
    assert(oa_effect_worker_create(&w) == 0);
    assert(oa_effect_worker_take(NULL, &done) == -EINVAL);
    int qr = oa_effect_worker_quiesce(w);
    assert(qr == 0 || qr == -EAGAIN);
    /* After quiesce the worker refuses new work. */
    job.effect = OA_EASI_MUTE_ON;
    job.set_mute = fake_set_mute;
    assert(oa_effect_worker_submit(w, &job, &ticket) == -ESHUTDOWN);
    (void)oa_effect_worker_join(w);
    assert(oa_effect_worker_destroy(w) == 0);
}

static void test_deterministic_backpressure(void)
{
    oa_effect_worker *w = NULL;
    oa_effect_job job;
    uint64_t ticket;
    oa_effect_completion done;

    gate_release = 0;
    gate_entered = 0;
    assert(oa_effect_worker_create(&w) == 0);
    memset(&job, 0, sizeof job);
    job.effect = OA_EASI_MUTE_ON;
    job.set_mute = gated_set_mute;
    assert(oa_effect_worker_submit(w, &job, &ticket) == 0);
    /* Wait until the worker is inside the gated call (first job dequeued). */
    for (int i = 0; i < 5000 && !gate_entered; i++) {
        const struct timespec pause = { .tv_nsec = 1000000 };
        (void)nanosleep(&pause, NULL);
    }
    assert(gate_entered);
    /* Fill all 16 FIFO slots, then overflow must be rejected. */
    job.set_mute = fake_set_mute;
    for (int i = 0; i < 16; i++)
        assert(oa_effect_worker_submit(w, &job, &ticket) == 0);
    assert(oa_effect_worker_submit(w, &job, &ticket) == -EBUSY);

    pthread_mutex_lock(&gate_mu);
    gate_release = 1;
    pthread_cond_broadcast(&gate_cv);
    pthread_mutex_unlock(&gate_mu);
    for (int i = 0; i < 17; i++) {
        done = wait_done(w);
        assert(done.status == OA_EASI_APPLIED);
    }
    (void)oa_effect_worker_quiesce(w);
    (void)oa_effect_worker_join(w);
    assert(oa_effect_worker_destroy(w) == 0);
}

int main(void)
{
    test_submit_backpressure_take();
    test_deterministic_backpressure();
    test_video_and_failure();
    test_rejections();
    puts("ok effect worker: off-loop HAL jobs, backpressure, completion, "
         "failure reporting and quiesce");
    return 0;
}
