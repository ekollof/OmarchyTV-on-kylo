/* Ordered amp owner jobs: Init, mute and balance share one worker and one
 * slot. Verifies FIFO/backpressure, worker-only callbacks, result kinds and
 * that failed jobs report an error without mutating retained Init state. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/amp_owner.h"

static const oa_amp_gpio mute_record = { 208, 1, 0 };

static struct {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    pthread_t main_thread, worker;
    unsigned preludes, levels, writes;
    uint32_t level_pin, level_value;
    unsigned char regs[64], values[64];
    int block, released, provide_mute, level_fail, fail_write_reg;
} hw = {
    .mu = PTHREAD_MUTEX_INITIALIZER,
    .cv = PTHREAD_COND_INITIALIZER,
    .fail_write_reg = -1
};

static void on_worker(void)
{
    assert(!pthread_equal(pthread_self(), hw.main_thread));
    hw.worker = pthread_self();
}

static void prelude(void *v)
{
    assert(v == &hw);
    on_worker();
    pthread_mutex_lock(&hw.mu);
    hw.preludes++;
    pthread_cond_broadcast(&hw.cv);
    while (hw.block && !hw.released)
        pthread_cond_wait(&hw.cv, &hw.mu);
    pthread_mutex_unlock(&hw.mu);
}

static const oa_amp_gpio *gpio(void *v, unsigned function)
{
    assert(v == &hw && function < 2);
    /* Init runs with the mute record absent (as on the fixture); the mute job
     * explicitly enables it so the GPIO path is exercised. */
    return function == 1 && hw.provide_mute ? &mute_record : NULL;
}

static void direction(void *v, uint32_t pin, uint32_t dir)
{
    (void)pin;
    (void)dir;
    assert(v == &hw);
    on_worker();
}

static int level(void *v, uint32_t pin, uint32_t value)
{
    assert(v == &hw);
    on_worker();
    pthread_mutex_lock(&hw.mu);
    hw.levels++;
    hw.level_pin = pin;
    hw.level_value = value;
    pthread_cond_broadcast(&hw.cv);
    pthread_mutex_unlock(&hw.mu);
    return hw.level_fail ? 0 : 7;
}

static void delay_ms(void *v, unsigned ms)
{
    (void)ms;
    assert(v == &hw);
    on_worker();
}

static int write_reg(void *v, unsigned reg, const uint8_t *value)
{
    assert(v == &hw && value && hw.writes < 64);
    on_worker();
    hw.regs[hw.writes] = (unsigned char)reg;
    hw.values[hw.writes] = *value;
    hw.writes++;
    return reg != (unsigned)hw.fail_write_reg;
}

static uint32_t sub_enabled(void *v) { assert(v == &hw); on_worker(); return 0; }
static void sub_enable(void *v, int on) { (void)on; assert(v == &hw); on_worker(); }
static void sub_init(void *v) { assert(v == &hw); on_worker(); }

static void wait_until(unsigned *value, unsigned threshold)
{
    struct timespec until;
    assert(!clock_gettime(CLOCK_REALTIME, &until));
    until.tv_sec += 5;
    pthread_mutex_lock(&hw.mu);
    while (*value < threshold)
        assert(!pthread_cond_timedwait(&hw.cv, &hw.mu, &until));
    pthread_mutex_unlock(&hw.mu);
}

static void release_worker(void)
{
    pthread_mutex_lock(&hw.mu);
    hw.released = 1;
    pthread_cond_broadcast(&hw.cv);
    pthread_mutex_unlock(&hw.mu);
}

static void take_wait(oa_amp_owner *o, oa_amp_completion *out)
{
    for (int i = 0; i < 5000; i++) {
        int rc = oa_amp_owner_take(o, out);
        if (rc == 1)
            return;
        assert(rc == 0);
        const struct timespec pause = { .tv_nsec = 1000000 };
        (void)nanosleep(&pause, NULL);
    }
    assert(0 && "amp job did not complete");
}

static void settle(void)
{
    const struct timespec pause = { .tv_nsec = 20000000 };
    (void)nanosleep(&pause, NULL);
}

int main(void)
{
    hw.main_thread = pthread_self();
    oa_amp_init_state initial;
    memset(&initial, 0, sizeof initial);
    initial.reset_cached = 7;
    initial.mute_cached = 9;
    for (unsigned i = 0; i < OA_AD82587_REGISTERS; i++)
        initial.image[i] = (uint8_t)(0x40 + i);

    oa_amp_init_io io = { &hw, prelude, gpio, direction, level, delay_ms,
                          write_reg, sub_enabled, sub_enable, sub_init };
    oa_amp_owner *o = NULL;
    assert(!oa_amp_owner_create(&o, &initial, &io));

    uint64_t ticket = 0, rejected = 777;
    assert(oa_amp_owner_submit_mute(o, 2, &ticket) == -EINVAL && ticket == 0);
    assert(oa_amp_owner_submit_balance(o, 101, &ticket) == -EINVAL && ticket == 0);
    assert(oa_amp_owner_submit_mute(o, 1, NULL) == -EINVAL);
    assert(oa_amp_owner_submit_balance(o, 50, NULL) == -EINVAL);

    /* A pending Init blocks mute/balance submission: they cannot overtake it. */
    char power[32] = "6Watt";
    oa_ad82587_profile p = { .speaker_power = power, .prescale = 0x11,
        .state_control4 = 0x42, .power_clip = 0x010203,
        .limiter_attack = -1, .release_threshold = -1 };
    hw.block = 1;
    assert(!oa_amp_owner_submit(o, &p, &ticket) && ticket == 1);
    wait_until(&hw.preludes, 1);
    assert(oa_amp_owner_submit_mute(o, 1, &rejected) == -EBUSY && rejected == 777);
    assert(oa_amp_owner_submit_balance(o, 50, &rejected) == -EBUSY && rejected == 777);
    release_worker();

    oa_amp_completion result;
    take_wait(o, &result);
    assert(result.kind == OA_AMP_JOB_INIT && result.ticket == 1);
    assert(result.native_result == 1);
    assert(result.state.image[3] == 0x11 && result.state.image[7] == 0x42);
    assert(hw.preludes == 1);
    assert(!pthread_equal(hw.worker, hw.main_thread));

    /* Mute: queued only after the Init result is taken, executed on the worker
     * with the board mute record and the vendor level mapping. */
    hw.block = 0;
    hw.provide_mute = 1;
    hw.levels = 0;
    hw.level_fail = 0;
    assert(!oa_amp_owner_submit_mute(o, 1, &ticket) && ticket == 2);
    assert(oa_amp_owner_take(o, &result) == 0); /* pending, not ready */
    assert(oa_amp_owner_submit_mute(o, 0, &rejected) == -EBUSY && rejected == 777);
    take_wait(o, &result);
    assert(result.kind == OA_AMP_JOB_MUTE && result.ticket == 2);
    assert(result.value == 1 && result.native_result == 0);
    assert(hw.levels == 1 && hw.level_pin == 208 && hw.level_value == 1);
    assert(result.state.image[3] == 0x11); /* Init state retained */
    puts("ok amp owner jobs: mute is ordered after Init and runs on the worker");

    /* A failed level write is reported but never fabricates success. */
    hw.level_fail = 1;
    hw.levels = 0;
    assert(!oa_amp_owner_submit_mute(o, 0, &ticket) && ticket == 3);
    take_wait(o, &result);
    assert(result.kind == OA_AMP_JOB_MUTE && result.value == 0 &&
           result.native_result == -EIO);
    assert(hw.levels == 1 && hw.level_value == 0);
    hw.level_fail = 0;
    puts("ok amp owner jobs: failed mute returns -EIO on the worker");

    /* Balance 25 -> reg4=0, reg5=64 in vendor order; then 50 -> 0/0. */
    hw.writes = 0;
    hw.fail_write_reg = -1;
    assert(!oa_amp_owner_submit_balance(o, 25, &ticket) && ticket == 4);
    take_wait(o, &result);
    assert(result.kind == OA_AMP_JOB_BALANCE && result.value == 25 &&
           result.native_result == 0);
    assert(hw.writes == 2 && hw.regs[0] == 4 && hw.regs[1] == 5);
    assert(hw.values[0] == 0 && hw.values[1] == 64);
    hw.writes = 0;
    assert(!oa_amp_owner_submit_balance(o, 50, &ticket) && ticket == 5);
    take_wait(o, &result);
    assert(hw.writes == 2 && hw.values[0] == 0 && hw.values[1] == 0);
    assert(result.value == 50 && result.native_result == 0);
    puts("ok amp owner jobs: balance writes reg4 then reg5 on the worker");

    /* reg5 failure reports -EIO after the partial reg4 write. */
    hw.writes = 0;
    hw.fail_write_reg = 5;
    assert(!oa_amp_owner_submit_balance(o, 50, &ticket) && ticket == 6);
    take_wait(o, &result);
    assert(result.kind == OA_AMP_JOB_BALANCE && result.native_result == -EIO);
    assert(hw.writes == 2 && hw.regs[0] == 4 && hw.regs[1] == 5);
    assert(hw.values[0] == 0 && hw.values[1] == 0);
    hw.fail_write_reg = -1;
    puts("ok amp owner jobs: failed balance leaves the partial write visible and reports -EIO");

    /* An untaken result also blocks the next submission (backpressure). */
    hw.levels = 0;
    assert(!oa_amp_owner_submit_mute(o, 1, &ticket) && ticket == 7);
    wait_until(&hw.levels, 1);
    settle();
    assert(oa_amp_owner_submit_mute(o, 0, &rejected) == -EBUSY && rejected == 777);
    take_wait(o, &result);
    assert(result.ticket == 7 && result.kind == OA_AMP_JOB_MUTE);
    puts("ok amp owner jobs: untaken result keeps the single-slot backpressure");

    for (unsigned i = 0; i < 5000; i++) {
        int rc = oa_amp_owner_quiesce(o);
        if (!rc)
            break;
        assert(rc == -EBUSY);
        const struct timespec pause = { .tv_nsec = 1000000 };
        (void)nanosleep(&pause, NULL);
    }
    assert(!oa_amp_owner_destroy(o));
    puts("all amp owner job tests passed");
    return 0;
}
