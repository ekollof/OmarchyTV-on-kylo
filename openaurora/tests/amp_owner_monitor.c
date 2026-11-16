/* Amp owner MONITOR job: the recovered DEV_AMP_ID check runs on the worker
 * with an injected I2C read. Match is quiet, mismatch runs Init then Mute(0),
 * and a read failure reports -EIO without reinitializing from stale data. */
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
static pthread_t main_thread;
static int on_worker(void)
{
    return !pthread_equal(pthread_self(), main_thread);
}
static void prelude(void *v) { (void)v; assert(on_worker()); }
static const oa_amp_gpio *gpio(void *v, unsigned f)
{
    (void)v;
    return f == 1 ? &mute_record : NULL;
}
static void direction(void *v, uint32_t p, uint32_t d)
{
    (void)v; (void)p; (void)d; assert(on_worker());
}
static int level(void *v, uint32_t p, uint32_t value)
{
    (void)v; assert(on_worker() && p == 208);
    assert(value == mute_record.level_off); /* monitor Mute(0) */
    return 1;
}
static void delay_ms(void *v, unsigned ms) { (void)v; (void)ms; assert(on_worker()); }
static int write_reg(void *v, unsigned reg, const uint8_t *val)
{
    (void)v; assert(on_worker() && val);
    return reg != 3; /* mismatch Init fails its first write, as in firmware */
}
static uint32_t sub_enabled(void *v) { (void)v; return 0; }
static void sub_enable(void *v, int on) { (void)v; (void)on; }
static void sub_init(void *v) { (void)v; }

static uint8_t regs[3], read_first, read_count, read_calls;
static int read_fail;
static int mon_read(void *ctx, uint8_t first, uint8_t count, uint8_t *out)
{
    (void)ctx;
    assert(on_worker() && out && count == 3);
    read_first = first;
    read_count = count;
    read_calls++;
    if (read_fail) return -EIO;
    memcpy(out, regs, 3);
    return 0;
}

static void take_wait(oa_amp_owner *o, oa_amp_completion *out)
{
    for (int i = 0; i < 5000; i++) {
        int rc = oa_amp_owner_take(o, out);
        if (rc == 1) return;
        assert(rc == 0);
        const struct timespec pause = { .tv_nsec = 1000000 };
        (void)nanosleep(&pause, NULL);
    }
    assert(0 && "monitor job did not complete");
}

int main(void)
{
    main_thread = pthread_self();
    oa_amp_init_state initial;
    memset(&initial, 0, sizeof initial);
    for (unsigned i = 0; i < OA_AD82587_REGISTERS; i++)
        initial.image[i] = (uint8_t)(0x40 + i);
    oa_amp_init_io io = { NULL, prelude, gpio, direction, level, delay_ms,
                          write_reg, sub_enabled, sub_enable, sub_init };
    oa_amp_owner *o = NULL;
    assert(!oa_amp_owner_create(&o, &initial, &io));
    uint64_t ticket = 0;

    /* Match: only the read runs, result 0, counters zero. */
    regs[0] = initial.image[3]; regs[1] = initial.image[4];
    regs[2] = initial.image[5];
    read_fail = 0; read_calls = 0;
    assert(!oa_amp_owner_submit_monitor(o, mon_read, NULL, &ticket));
    oa_amp_completion result;
    take_wait(o, &result);
    assert(result.kind == OA_AMP_JOB_MONITOR && result.ticket == ticket);
    assert(result.native_result == 0 && read_calls == 1);
    assert(read_first == 3 && read_count == 3);
    assert(result.monitor_read_failures == 0 && result.monitor_mismatches == 0);
    assert(result.monitor_log[0] == 0);

    /* Mismatch: Init (write fails first reg) then Mute(0) still run; both on
     * the worker. The result is 1 with a mismatch counted. */
    regs[0] = 0x7f;
    read_calls = 0;
    assert(!oa_amp_owner_submit_monitor(o, mon_read, NULL, &ticket));
    take_wait(o, &result);
    assert(result.kind == OA_AMP_JOB_MONITOR && result.native_result == 1);
    assert(read_calls == 1 && result.monitor_mismatches == 1);
    assert(result.monitor_read_failures == 0);
    assert(strstr(result.monitor_log, "mismatch") != NULL);

    /* Read failure: -EIO, no reinit, no mute, no stale-data recovery. */
    read_fail = 1; read_calls = 0;
    assert(!oa_amp_owner_submit_monitor(o, mon_read, NULL, &ticket));
    take_wait(o, &result);
    assert(result.kind == OA_AMP_JOB_MONITOR && result.native_result == -EIO);
    assert(read_calls == 1 && result.monitor_read_failures == 1);
    assert(result.monitor_mismatches == 0); /* fresh check; no stale compare */
    assert(strstr(result.monitor_log, "failed") != NULL);

    /* NULL read and null ticket reject before submitting. */
    uint64_t rejected = 777;
    assert(oa_amp_owner_submit_monitor(o, NULL, NULL, &rejected) == -EINVAL);
    assert(rejected == 777);
    assert(oa_amp_owner_submit_monitor(o, mon_read, NULL, NULL) == -EINVAL);

    for (int i = 0; i < 5000; i++) {
        int rc = oa_amp_owner_quiesce(o);
        if (!rc) break;
        assert(rc == -EBUSY);
        const struct timespec pause = { .tv_nsec = 1000000 };
        (void)nanosleep(&pause, NULL);
    }
    assert(!oa_amp_owner_destroy(o));
    puts("ok amp owner monitor: worker read, match, Init->Mute(0) mismatch, "
         "read-failure isolation and rejection");
    return 0;
}
