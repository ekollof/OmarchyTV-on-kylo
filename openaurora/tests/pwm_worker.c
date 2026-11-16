/* Pure panel PWM worker test. No driver, TV or HAL. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "openaurora/pwm_worker.h"

static pthread_mutex_t gate_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_cv = PTHREAD_COND_INITIALIZER;
static int gate_release, gate_entered;
static int calls, last_pct, fail_rc;

static int apply_ok(void *ud, int pct)
{
    (void)ud;
    calls++;
    last_pct = pct;
    return fail_rc;
}

static int apply_gated(void *ud, int pct)
{
    (void)ud;
    pthread_mutex_lock(&gate_mu);
    gate_entered = 1;
    pthread_cond_signal(&gate_cv);
    while (!gate_release)
        pthread_cond_wait(&gate_cv, &gate_mu);
    pthread_mutex_unlock(&gate_mu);
    calls++;
    last_pct = pct;
    return 0;
}

static oa_pwm_completion wait_done(oa_pwm_worker *w)
{
    oa_pwm_completion out;
    memset(&out, 0, sizeof out);
    for (int i = 0; i < 5000; i++) {
        if (oa_pwm_worker_take(w, &out) == 1)
            return out;
        const struct timespec pause = { .tv_nsec = 1000000 };
        (void)nanosleep(&pause, NULL);
    }
    assert(0 && "pwm worker completion timed out");
    return out;
}

static void test_order_and_state(void)
{
    oa_pwm_worker *w = NULL;
    oa_pwm_worker_io io = { NULL, apply_ok };
    uint64_t ticket;
    oa_pwm_completion done;

    calls = 0; last_pct = -1; fail_rc = 0;
    assert(oa_pwm_worker_create(&w, &io) == 0 && w);
    for (int pct = 0; pct <= 100; pct += 20) {
        assert(oa_pwm_worker_submit(w, pct, &ticket) == 0);
        assert(ticket == (uint64_t)(pct / 20 + 1));
    }
    for (int i = 1; i <= 6; i++) {
        done = wait_done(w);
        assert(done.ticket == (uint64_t)i);
        assert(done.pct == (i - 1) * 20 && done.rc == 0);
    }
    assert(calls == 6 && last_pct == 100);

    fail_rc = -EIO;
    assert(oa_pwm_worker_submit(w, 50, &ticket) == 0);
    done = wait_done(w);
    assert(done.rc == -EIO && done.pct == 50);
    assert(oa_pwm_worker_take(w, &done) == 0);
    assert(oa_pwm_worker_destroy(w) == -EBUSY);
    int qr = oa_pwm_worker_quiesce(w);
    assert(qr == 0 || qr == -EAGAIN);
    (void)oa_pwm_worker_join(w);
    assert(oa_pwm_worker_destroy(w) == 0);
}

static void test_backpressure(void)
{
    oa_pwm_worker *w = NULL;
    oa_pwm_worker_io io = { NULL, apply_gated };
    uint64_t ticket;
    oa_pwm_completion done;

    gate_release = 0; gate_entered = 0; calls = 0;
    assert(oa_pwm_worker_create(&w, &io) == 0);
    assert(oa_pwm_worker_submit(w, 40, &ticket) == 0);
    for (int i = 0; i < 5000 && !gate_entered; i++) {
        const struct timespec pause = { .tv_nsec = 1000000 };
        (void)nanosleep(&pause, NULL);
    }
    assert(gate_entered);
    for (int i = 0; i < 8; i++)
        assert(oa_pwm_worker_submit(w, i, &ticket) == 0);
    assert(oa_pwm_worker_submit(w, 1, &ticket) == -EBUSY);

    pthread_mutex_lock(&gate_mu);
    gate_release = 1;
    pthread_cond_broadcast(&gate_cv);
    pthread_mutex_unlock(&gate_mu);
    for (int i = 0; i < 9; i++) {
        done = wait_done(w);
        assert(done.rc == 0);
    }
    (void)oa_pwm_worker_quiesce(w);
    (void)oa_pwm_worker_join(w);
    assert(oa_pwm_worker_destroy(w) == 0);
}

static void test_rejections(void)
{
    oa_pwm_worker *w = NULL;
    oa_pwm_worker_io io = { NULL, apply_ok };
    oa_pwm_worker_io bad = { NULL, NULL };
    uint64_t ticket;
    oa_pwm_completion done;

    assert(oa_pwm_worker_create(NULL, &io) == -EINVAL);
    assert(oa_pwm_worker_create(&w, &bad) == -EINVAL);
    assert(oa_pwm_worker_create(&w, &io) == 0);
    assert(oa_pwm_worker_submit(NULL, 1, &ticket) == -EINVAL);
    assert(oa_pwm_worker_submit(w, -1, &ticket) == -EINVAL);
    assert(oa_pwm_worker_submit(w, 101, &ticket) == -EINVAL);
    assert(oa_pwm_worker_submit(w, 10, NULL) == -EINVAL);
    assert(oa_pwm_worker_take(NULL, &done) == -EINVAL);
    int qr = oa_pwm_worker_quiesce(w);
    assert(qr == 0 || qr == -EAGAIN);
    assert(oa_pwm_worker_submit(w, 10, &ticket) == -ESHUTDOWN);
    (void)oa_pwm_worker_join(w);
    assert(oa_pwm_worker_destroy(w) == 0);
}

int main(void)
{
    test_order_and_state();
    test_backpressure();
    test_rejections();
    puts("ok pwm worker: off-loop ordered PWM writes, backpressure, "
         "failures and quiesce");
    return 0;
}
