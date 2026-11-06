/* Host scheduler: bounded cooperative slices on the loop thread. */
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "host.h"

static int runs;
static int nested;
static int in_outer;

static int again_step(void *ud, uint32_t *delay_ms)
{
    int *n = ud;
    (void)delay_ms;
    (*n)++;
    return (*n) >= 2 ? OA_SCHED_DONE : OA_SCHED_AGAIN;
}

static int fail_step(void *ud, uint32_t *delay_ms)
{
    int *n = ud;
    (void)delay_ms;
    (*n)++;
    return -1;
}

static int flag_step(void *ud, uint32_t *delay_ms)
{
    int *n = ud;
    (void)delay_ms;
    if (in_outer)
        nested = 1;
    (*n)++;
    return OA_SCHED_DONE;
}

static int outer_step(void *ud, uint32_t *delay_ms)
{
    oa_host *h = ud;
    (void)delay_ms;
    in_outer = 1;
    oa_host_sched_pump(h);
    in_outer = 0;
    runs++;
    return OA_SCHED_DONE;
}

static int hold_step(void *ud, uint32_t *delay_ms)
{
    (void)ud;
    if (delay_ms)
        *delay_ms = 10;
    runs++;
    return OA_SCHED_AGAIN;
}

int main(void)
{
    oa_host h;
    const oa_sched_iface *sched;
    uint32_t ids[OA_SCHED_SLOTS + 1];
    uint32_t id = 0;
    int n = 0;
    int i;
    int flags = 0;

    assert(oa_host_init(&h, NULL, NULL) == 0);
    sched = h.api.lookup(&h, OA_SVC_SCHED, 1);
    assert(sched && sched->context == &h);
    assert(sched->live(sched->context, 0) == 0);

    assert(sched->submit(sched->context, again_step, &n, 0, &id) == 0);
    assert(id != 0 && sched->live(sched->context, id));
    oa_host_sched_pump(&h);
    assert(n == 1 && sched->live(sched->context, id));
    oa_host_sched_pump(&h);
    assert(n == 2 && !sched->live(sched->context, id));
    puts("  ok   a task slices once per turn and then finishes");

    n = 0;
    assert(sched->submit(sched->context, fail_step, &n, 0, &id) == 0);
    oa_host_sched_pump(&h);
    assert(n == 1 && !sched->live(sched->context, id));
    oa_host_sched_pump(&h);
    assert(n == 1);
    puts("  ok   a negative step drops the task");

    runs = 0;
    assert(sched->submit(sched->context, hold_step, NULL, 60000, &id) == 0);
    oa_host_sched_pump(&h);
    assert(runs == 0 && sched->live(sched->context, id));
    assert(sched->wake(sched->context, id) == 0);
    oa_host_sched_pump(&h);
    assert(runs == 1 && sched->live(sched->context, id));
    assert(sched->cancel(sched->context, id) == 0);
    assert(!sched->live(sched->context, id));
    oa_host_sched_pump(&h);
    assert(runs == 1);
    assert(sched->cancel(sched->context, id) == -1);
    assert(sched->wake(sched->context, id) == -1);
    puts("  ok   delay, wake and cancel do not call a dropped step");

    h.sleep_cycle = 1;
    runs = 0;
    assert(sched->submit(sched->context, flag_step, &runs, 0, &id) == 0);
    oa_host_sched_pump(&h);
    assert(runs == 0 && sched->live(sched->context, id));
    h.sleep_cycle = 0;
    oa_host_sched_pump(&h);
    assert(runs == 1 && !sched->live(sched->context, id));
    puts("  ok   standby defers a due task");

    h.cur_module = 42;
    flags = 0;
    assert(sched->submit(sched->context, flag_step, &flags, 0, &id) == 0);
    oa_host_cleanup_owned(&h, 42);
    assert(!sched->live(sched->context, id));
    oa_host_sched_pump(&h);
    assert(flags == 0);
    h.cur_module = 0;
    puts("  ok   unload drops the owner's tasks without calling them");

    nested = 0;
    in_outer = 0;
    runs = 0;
    flags = 0;
    assert(sched->submit(sched->context, outer_step, &h, 0, &id) == 0);
    assert(sched->submit(sched->context, flag_step, &flags, 0, NULL) == 0);
    oa_host_sched_pump(&h);
    assert(runs == 1 && nested == 0 && flags == 1);
    puts("  ok   a step cannot re-enter the scheduler");

    h.cur_module = 7;
    for (i = 0; i < OA_SCHED_SLOTS; i++)
        assert(sched->submit(sched->context, hold_step, NULL, 60000, &ids[i]) == 0);
    assert(sched->submit(sched->context, hold_step, NULL, 0, &ids[OA_SCHED_SLOTS]) == -1);
    assert(sched->cancel(sched->context, ids[0]) == 0);
    assert(sched->submit(sched->context, hold_step, NULL, 60000, &id) == 0);
    oa_host_cleanup_owned(&h, 7);
    for (i = 0; i < OA_SCHED_SLOTS; i++)
        assert(!h.tasks[i].active);
    puts("  ok   the scheduler rejects a full table and accepts a slot after cancel");

    close(h.selfpipe[0]);
    close(h.selfpipe[1]);
    pthread_mutex_destroy(&h.post_mu);
    return 0;
}
