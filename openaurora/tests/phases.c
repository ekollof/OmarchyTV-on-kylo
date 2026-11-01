#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "host.h"
#include "phases.h"

static void clear_sequence(struct phase_observer *o)
{ o->length = 0; o->sequence[0] = 0; }
static int command(oa_host *h, const char *text)
{
    char line[80], reply[256];
    snprintf(line, sizeof line, "%s", text);
    int rc = oa_control_command(h, line, reply, sizeof reply);
    assert(strstr(reply, rc == 0 ? "ok" : "failed"));
    assert(h->cur_module == 0);
    return rc;
}
static void setup(oa_host *h, struct phase_observer *o, const char *dir, int no_hooks)
{
    memset(o, 0, sizeof *o);
    assert(oa_host_init(h, dir, NULL) == 0);
    assert(h->api.publish(h, PHASE_OBSERVER, 1, o) == 0);
    /* Slot order deliberately opposes dependency order. */
    oa_mod *a = oa_module_load(h, no_hooks ? "phase_no_suspend" : "phase_a");
    oa_mod *b = oa_module_load(h, "phase_b"), *c = oa_module_load(h, "phase_c");
    assert(a && b && c);
    assert(oa_module_start(h, c) == 0);
    assert(oa_module_start(h, b) == 0);
    assert(oa_module_start(h, a) == 0);
    clear_sequence(o);
}
static void finish(oa_host *h)
{
    const char *names[] = { "phase_a", "phase_b", "phase_c", "phase_c_new" };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
        if (oa_host_find_module(h, names[i])) assert(oa_module_unload(h, names[i], 0) == 0);
    close(h->selfpipe[0]); close(h->selfpipe[1]);
    pthread_mutex_destroy(&h->post_mu);
}

struct sleep_test {
    oa_host *h;
    struct phase_observer *observer;
    int entry_rc;
    int guard;
};
static int fake_entry(void *v)
{
    struct sleep_test *t = v;
    struct phase_observer *o = t->observer;
    assert(!o->running[0] && !o->running[1] && !o->running[2]);
    o->sequence[o->length++] = 'X'; o->sequence[o->length] = 0;
    if (t->guard) {
        oa_host *h = t->h;
        uint32_t owner = h->cur_module;
        oa_mod *b = oa_host_find_module(h, "phase_b");
        oa_sleep_result nested;
        assert(owner == oa_host_find_module(h, "phase_c")->id);
        assert(oa_module_sleep_cycle(h, 0, fake_entry, t, &nested) == -1);
        assert(oa_module_stop(h, b) == -1 && oa_module_start(h, b) == -1);
        assert(oa_module_unload(h, "phase_b", 1) == -1);
        assert(oa_module_reload(h, "phase_b", 1) == -1);
        assert(!oa_module_load(h, "phase_b"));
        assert(oa_module_suspend_all(h) == -1 && oa_module_resume_all(h) == -1);
        assert(h->cur_module == owner);
    }
    return t->entry_rc;
}
static void cancel_sleep(void *v)
{
    oa_host *h = v;
    assert(h->sleep_cycle);
    assert(oa_module_resume_all(h) == -1 && oa_module_suspend_all(h) == -1);
    oa_host_signal_stop(h);
}

static void sleep_cycles(oa_host *h, struct phase_observer *o, const char *dir)
{
    oa_sleep_result r;
    struct sleep_test t = { h, o, 0, 1 };
    setup(h, o, dir, 0);
    uint32_t owner = oa_host_find_module(h, "phase_c")->id;
    uint32_t caller = oa_host_find_module(h, "phase_a")->id;
    h->cur_module = caller;
    assert(oa_module_sleep_cycle(h, owner, fake_entry, &t, &r) == 0);
    assert(!strcmp(o->sequence, "abcXCBA"));
    assert(r.prepare_rc == 0 && r.entry_attempted && r.entry_rc == 0 && r.restore_rc == 0);
    assert(h->cur_module == caller && !h->sleep_cycle);
    h->cur_module = 0;
    finish(h);
    puts("  ok   sleep cycle enters last, restores automatically and pins modules/rejects nested cycles");

    setup(h, o, dir, 0); t.guard = 0;
    o->fail_suspend = 2;
    assert(oa_module_sleep_cycle(h, 0, fake_entry, &t, &r) == -1);
    assert(!strcmp(o->sequence, "abA"));
    assert(r.prepare_rc != 0 && !r.entry_attempted && r.restore_rc == 0);
    assert(o->running[0] && o->running[1] && o->running[2]);
    finish(h);
    puts("  ok   failed preparation skips entry and restores only successful transitions");

    setup(h, o, dir, 0); t.entry_rc = -7;
    assert(oa_module_sleep_cycle(h, 0, fake_entry, &t, &r) == -1);
    assert(!strcmp(o->sequence, "abcXCBA"));
    assert(r.prepare_rc == 0 && r.entry_attempted && r.entry_rc == -7 && r.restore_rc == 0);
    finish(h);
    puts("  ok   entry failure still restores modules and retains its distinct error");

    setup(h, o, dir, 0); t.entry_rc = 0;
    o->fail_resume = 2;
    assert(oa_module_sleep_cycle(h, 0, fake_entry, &t, &r) == -1);
    assert(!strcmp(o->sequence, "abcXCB"));
    assert(r.prepare_rc == 0 && r.entry_rc == 0 && r.restore_rc != 0);
    assert(!o->running[0] && !o->running[1] && o->running[2]);
    o->fail_resume = 0;
    assert(oa_module_resume_all(h) == 0);
    finish(h);
    puts("  ok   restoration failure remains visible; dependent consumers stay suspended and retry works");

    setup(h, o, dir, 0);
    assert(oa_module_suspend_all(h) == 0);
    assert(oa_module_start(h, oa_host_find_module(h, "phase_c")) == 0);
    assert(oa_module_start(h, oa_host_find_module(h, "phase_b")) == 0);
    clear_sequence(o);
    assert(oa_module_sleep_cycle(h, 0, fake_entry, &t, &r) == 0);
    assert(!strcmp(o->sequence, "bcXCB") && !o->running[0]);
    assert(oa_host_find_module(h, "phase_a")->state == OA_ST_SUSPENDED);
    finish(h);
    puts("  ok   cycle ledger preserves modules already suspended before the request");

    setup(h, o, dir, 1);
    assert(oa_module_sleep_cycle(h, 0, fake_entry, &t, &r) == -1);
    assert(!r.entry_attempted && r.prepare_rc != 0 && o->length == 0);
    finish(h);
    puts("  ok   missing hooks refuse before callbacks rather than assuming quiescence");

    setup(h, o, dir, 0);
    h->stop_requested = 1;
    assert(oa_module_sleep_cycle(h, 0, fake_entry, &t, &r) == -1 && o->length == 0);
    h->stop_requested = 0;
    assert(oa_module_sleep_cycle(h, UINT32_MAX, fake_entry, &t, &r) == -1 && o->length == 0);
    finish(h);
    puts("  ok   pending shutdown and stale backend owner reject before preparation");

    setup(h, o, dir, 0);
    o->after_suspend = cancel_sleep; o->after_suspend_ud = h;
    assert(oa_module_sleep_cycle(h, 0, fake_entry, &t, &r) == -1);
    assert(!r.entry_attempted && r.prepare_rc != 0 && r.restore_rc == 0);
    assert(!strcmp(o->sequence, "abcCBA"));
    h->stop_requested = 0;
    finish(h);
    puts("  ok   shutdown arriving during preparation cancels entry and still restores modules");

    setup(h, o, dir, 0);
    h->cur_module = oa_host_find_module(h, "phase_c")->id;
    assert(h->api.lookup(h, "phase_a_service", 1));
    h->cur_module = 0;
    assert(oa_module_sleep_cycle(h, 0, fake_entry, &t, &r) == -1);
    assert(!r.entry_attempted && o->length == 0);
    h->cur_module = oa_host_find_module(h, "phase_c")->id;
    h->api.release(h, "phase_a_service"); h->cur_module = 0;
    finish(h);
    puts("  ok   dependency cycles reject sleep before callbacks or backend entry");
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    oa_host *h = calloc(1, sizeof *h);
    struct phase_observer o;
    assert(h);

    setup(h, &o, argv[1], 0);
    assert(command(h, "suspend") == 0 && !strcmp(o.sequence, "abc"));
    assert(command(h, "suspend") == 0 && !strcmp(o.sequence, "abc"));
    assert(command(h, "resume") == 0 && !strcmp(o.sequence, "abcCBA"));
    finish(h);
    puts("  ok   declared service dependencies suspend consumers first and resume providers first");

    setup(h, &o, argv[1], 0);
    oa_mod *b = oa_host_find_module(h, "phase_b");
    uint32_t old = b->id;
    assert(command(h, "stop phase_b") == -1);
    assert(command(h, "unload phase_b") == -1);
    assert(command(h, "reload phase_b") == -1);
    assert(b->id == old && b->state == OA_ST_STARTED && o.length == 0);
    finish(h);
    puts("  ok   declared dependencies protect active consumers during stop/unload/reload");

    setup(h, &o, argv[1], 0);
    assert(command(h, "suspend") == 0);
    assert(oa_module_unload(h, "phase_a", 0) == 0); /* create an earlier hole */
    assert(command(h, "reload phase_b") == 0);
    b = oa_host_find_module(h, "phase_b");
    oa_mod *c = oa_host_find_module(h, "phase_c");
    assert(b && c && b < c && b->state == OA_ST_SUSPENDED);
    clear_sequence(&o);
    assert(command(h, "resume") == 0 && !strcmp(o.sequence, "CB"));
    assert(o.starts[1] == 2 && o.resumes[1] == 0);
    finish(h);
    puts("  ok   dependency snapshot survives service unpublication and deferred reload into a hole");

    setup(h, &o, argv[1], 0);
    o.fail_suspend = 2;
    assert(command(h, "suspend") == -1 && !strcmp(o.sequence, "ab"));
    assert(!o.running[0] && o.running[1] && o.running[2]);
    assert(oa_host_find_module(h, "phase_a")->state == OA_ST_SUSPENDED);
    assert(oa_host_find_module(h, "phase_c")->state == OA_ST_STARTED);
    o.fail_suspend = 0;
    clear_sequence(&o);
    assert(command(h, "resume") == 0 && !strcmp(o.sequence, "A"));
    finish(h);
    puts("  ok   failed consumer suspend protects its providers and reports failure through CLI");

    setup(h, &o, argv[1], 0);
    assert(command(h, "suspend") == 0);
    o.fail_resume = 3;
    clear_sequence(&o);
    assert(command(h, "resume") == -1 && !strcmp(o.sequence, "C"));
    assert(!o.running[0] && !o.running[1] && !o.running[2]);
    o.fail_resume = 0;
    clear_sequence(&o);
    assert(command(h, "resume") == 0 && !strcmp(o.sequence, "CBA"));
    finish(h);
    puts("  ok   failed provider resume keeps consumers suspended; a later resume retries");

    setup(h, &o, argv[1], 0);
    assert(command(h, "suspend") == 0);
    c = oa_host_find_module(h, "phase_c");
    int retained_iface = 0;
    h->cur_module = c->id;
    assert(h->api.publish(h, "phase_c_service", 1, &retained_iface) == 0);
    h->cur_module = 0;
    clear_sequence(&o);
    assert(command(h, "start phase_b") == -1); /* resume path */
    assert(command(h, "stop phase_b") == 0);
    assert(command(h, "start phase_b") == -1); /* fresh start path */
    assert(o.length == 0 && o.starts[1] == 1);
    h->cur_module = c->id;
    assert(h->api.unpublish(h, "phase_c_service") == 0);
    h->cur_module = 0;
    assert(command(h, "start phase_c") == 0);
    assert(command(h, "start phase_b") == 0);
    assert(command(h, "resume") == 0 && !strcmp(o.sequence, "CBA"));
    finish(h);
    puts("  ok   published interface alone cannot start/resume a consumer above a suspended provider");

    setup(h, &o, argv[1], 0);
    c = oa_host_find_module(h, "phase_c");
    h->cur_module = c->id;
    assert(h->api.lookup(h, "phase_a_service", 1)); /* C -> A closes the cycle */
    h->cur_module = 0;
    assert(command(h, "suspend") == -1 && o.length == 0);
    assert(o.running[0] && o.running[1] && o.running[2]);
    h->cur_module = c->id;
    h->api.release(h, "phase_a_service");
    h->cur_module = 0;
    assert(command(h, "suspend") == 0 && !strcmp(o.sequence, "abc"));
    assert(command(h, "resume") == 0);
    finish(h);
    puts("  ok   runtime lookup holds join the dependency graph; cycles fail before callbacks");

    setup(h, &o, argv[1], 1);
    assert(command(h, "suspend") == -1 && o.length == 0);
    assert(o.running[0] && o.running[1] && o.running[2]);
    finish(h);
    puts("  ok   consumer without a suspend hook cannot be left active above a sleeping provider");

    setup(h, &o, argv[1], 0);
    assert(command(h, "suspend") == 0);
    assert(oa_module_unload(h, "phase_c", 0) == 0);
    c = oa_module_load(h, "phase_replacement");
    assert(c && oa_module_start(h, c) == 0); /* same service, different module identity */
    clear_sequence(&o);
    assert(command(h, "resume") == 0 && !strcmp(o.sequence, "BA"));
    finish(h);
    puts("  ok   dependency edges follow a republished service to its current provider");

    sleep_cycles(h, &o, argv[1]);
    free(h);
    return 0;
}
