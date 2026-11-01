#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "host.h"
#include "openaurora/ui_time.h"

static uint64_t raw_ms;
static uint64_t raw_now(void) { return raw_ms; }
static const oa_clock_iface raw = { raw_now };
static unsigned timers(oa_host *h, uint32_t owner)
{
    unsigned n = 0;
    for (int i = 0; i < MAX_TIMERS; i++)
        if (h->timers[i].active && h->timers[i].owner == owner) n++;
    return n;
}
static void init(oa_host *h, const char *dir, int publish)
{
    raw_ms = 0;
    assert(oa_host_init(h, dir, NULL) == 0);
    h->log_level = OA_LOG_ERROR;
    if (publish) assert(h->api.publish(h, OA_SVC_CLOCK, 1, &raw) == 0);
}
static void done(oa_host *h)
{
    if (oa_host_find_module(h, "ui_clock")) assert(oa_module_unload(h, "ui_clock", 0) == 0);
    if (oa_host_find_module(h, "ticker")) assert(oa_module_unload(h, "ticker", 0) == 0);
    close(h->selfpipe[0]); close(h->selfpipe[1]); pthread_mutex_destroy(&h->post_mu);
}
static const oa_ui_clock_iface *start(oa_host *h)
{
    oa_mod *m = oa_module_load(h, "ui_clock");
    assert(m && oa_module_start(h, m) == 0 && timers(h, m->id) == 1);
    const oa_ui_clock_iface *clock = h->api.lookup(h, OA_SVC_UI_CLOCK, 1);
    assert(clock); return clock;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    oa_host *h = calloc(1, sizeof *h); assert(h);
    init(h, argv[1], 1);
    const oa_ui_clock_iface *clock = start(h);
    /* Golden traces derived from ARM constants and strict branches. In
     * particular the correction does not advance at an exact quantum. */
    const uint64_t input[] = { 0, 12343, 12344, 24686, 24687, 90000000, 90000001, 90007290, 90007293 };
    const uint32_t expected[] = { 0, 12343, 12343, 24685, 24685, 89992709, 89992710, 89999998, 1 };
    for (unsigned i = 0; i < sizeof input / sizeof input[0]; i++) {
        raw_ms = input[i]; assert(clock->now_ms() == expected[i]);
    }
    assert(clock->elapsed(1, 89999998) == 3 && clock->elapsed(5, 89999995) == 10);
    puts("  ok   FROST quantum boundaries, correction carry and raw/corrected 90000000-ms rollover");

    oa_mod *m = oa_host_find_module(h, "ui_clock"); uint32_t old = m->id;
    assert(oa_module_reload(h, "ui_clock", 0) == 0);
    m = oa_host_find_module(h, "ui_clock");
    clock = h->api.lookup(h, OA_SVC_UI_CLOCK, 1);
    assert(clock && m->id != old && clock->now_ms() == 1);
    assert(timers(h, old) == 0 && timers(h, m->id) == 1);
    uint32_t state[4], after[4];
    assert(m->desc->save(m->ctx, state, sizeof state) == sizeof state);
    for (unsigned i = 0; i < 4; i++) {
        uint32_t bad[4]; memcpy(bad, state, sizeof bad);
        bad[i] = i == 0 ? 2 : (i == 2 ? OA_UI_TIME_QUANTUM : OA_UI_TIME_PERIOD) + 1;
        assert(m->desc->restore(m->ctx, bad, sizeof bad) == -1);
        assert(m->desc->save(m->ctx, after, sizeof after) == sizeof after && !memcmp(state, after, sizeof state));
    }
    assert(m->desc->restore(m->ctx, state, sizeof state - 1) == -1);
    puts("  ok   real DSO reload preserves clock state, replaces owned timer and rejects invalid snapshots atomically");

    assert(oa_module_suspend_all(h) == 0 && timers(h, m->id) == 0);
    raw_ms += 10;
    for (int i = 0; i < MAX_TIMERS; i++) h->timers[i].active = 1;
    assert(oa_module_resume_all(h) == -1 && m->state == OA_ST_SUSPENDED);
    memset(h->timers, 0, sizeof h->timers);
    assert(oa_module_resume_all(h) == 0 && timers(h, m->id) == 1);
    assert(clock->now_ms() == 11);
    done(h);
    puts("  ok   suspend removes timer; failed resume can retry without losing elapsed time/correction");

    init(h, argv[1], 1); raw_ms = UINT64_C(4294967301); clock = start(h);
    assert(clock->now_ms() == 5); done(h);
    init(h, argv[1], 1); raw_ms = OA_UI_TIME_PERIOD; clock = start(h);
    m = oa_host_find_module(h, "ui_clock");
    assert(m->desc->save(m->ctx, state, sizeof state) == sizeof state && state[1] == OA_UI_TIME_PERIOD);
    assert(clock->now_ms() == 89992709); done(h);
    puts("  ok   ARM raw 32-bit truncation and strict > period comparison are preserved");

    init(h, argv[1], 0);
    m = oa_module_load(h, "ui_clock"); assert(m && oa_module_start(h, m) == -1);
    assert(!oa_host_service_exists(h, OA_SVC_UI_CLOCK) && !timers(h, m->id)); done(h);
    init(h, argv[1], 1);
    m = oa_module_load(h, "ui_clock"); assert(m);
    for (int i = 0; i < MAX_TIMERS; i++) h->timers[i].active = 1;
    assert(oa_module_start(h, m) == -1 && !oa_host_service_exists(h, OA_SVC_UI_CLOCK));
    for (int i = 0; i < MAX_HOLDS; i++) assert(!h->holds[i].used || h->holds[i].holder != m->id);
    memset(h->timers, 0, sizeof h->timers);
    int peer = 1;
    assert(h->api.publish(h, OA_SVC_UI_CLOCK, 1, &peer) == 0);
    assert(oa_module_start(h, m) == -1 && h->api.lookup(h, OA_SVC_UI_CLOCK, 1) == &peer);
    assert(!timers(h, m->id));
    assert(h->api.unpublish(h, OA_SVC_UI_CLOCK) == 0);
    assert(oa_module_start(h, m) == 0); done(h);
    puts("  ok   missing raw clock, full timer table and publication collision clean up; start can retry");

    init(h, argv[1], 0);
    oa_mod *ticker = oa_module_load(h, "ticker"); assert(ticker && oa_module_start(h, ticker) == 0);
    clock = start(h); m = oa_host_find_module(h, "ui_clock");
    assert(clock->now_ms() <= OA_UI_TIME_PERIOD);
    assert(oa_module_stop(h, ticker) == -1);
    assert(oa_module_suspend_all(h) == 0 && ticker->state == OA_ST_SUSPENDED && m->state == OA_ST_SUSPENDED);
    assert(timers(h, ticker->id) == 0 && timers(h, m->id) == 0);
    assert(oa_module_resume_all(h) == 0 && ticker->state == OA_ST_STARTED && m->state == OA_ST_STARTED);
    assert(timers(h, ticker->id) == 1 && timers(h, m->id) == 1);
    done(h);
    puts("  ok   real ticker/ui_clock dependency chain quiesces both timers and resumes in provider order");

    init(h, argv[1], 1); clock = start(h); m = oa_host_find_module(h, "ui_clock");
    raw_ms = 1000;
    for (int i = 0; i < MAX_TIMERS; i++) if (h->timers[i].active && h->timers[i].owner == m->id) {
        assert(h->timers[i].repeat_ms == 1000);
        h->cur_module = m->id;
        h->timers[i].cb(h->timers[i].ud);
        h->cur_module = 0;
    }
    assert(m->desc->save(m->ctx, state, sizeof state) == sizeof state && state[1] == 1000);
    done(h);
    puts("  ok   owned one-second sampling advances idle clock state without a client query");
    free(h); return 0;
}
