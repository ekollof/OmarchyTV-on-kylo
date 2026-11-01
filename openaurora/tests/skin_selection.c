#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "host.h"
#include "openaurora/ui_engine.h"

static int command(oa_host *h, const char *text, char *reply, size_t cap)
{ char line[128]; snprintf(line, sizeof line, "%s", text); return oa_control_command(h, line, reply, cap); }
static void pump(oa_host *h)
{
    oa_host_dispatch_posts(h);
    uint64_t now = oa_now_ms();
    for (int i = 0; i < MAX_TIMERS; i++) if (h->timers[i].active && h->timers[i].due <= now) {
        oa_timer t = h->timers[i];
        if (t.repeat_ms) h->timers[i].due = now+t.repeat_ms; else h->timers[i].active = 0;
        h->cur_module = t.owner; t.cb(t.ud); h->cur_module = 0;
    }
    usleep(1000);
}
static void settle(oa_host *h)
{ uint64_t end = oa_now_ms()+3000; while (h->skin_timer) { assert(oa_now_ms() < end); pump(h); } }
static int forbidden_sleep(void *ud) { (void)ud; assert(!"selection must block standby entry"); return -1; }
static void select_skin(oa_host *h, const char *name, int fallback)
{
    char line[96], reply[256]; snprintf(line, sizeof line, "skin %s", name);
    assert(!command(h, line, reply, sizeof reply) && strstr(reply, "pending"));
    settle(h);
    assert(!command(h, "skin", reply, sizeof reply));
    assert(strstr(reply, "state ready"));
    char active[80]; snprintf(active, sizeof active, "active %s ", fallback ? "vendor" : name);
    assert(strstr(reply, active));
    assert(h->skin_fallback == fallback && (fallback ? h->skin_error != 0 : h->skin_error == 0));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    char config[] = "/tmp/oa-skin-policy-XXXXXX"; int fd = mkstemp(config); assert(fd >= 0);
    FILE *f = fdopen(fd, "w"); assert(f);
    assert(fputs("ui.skin=missing\nui.ok_file=\nui.fps=0\nui.skin_timeout_ms=100\n", f) >= 0 && !fclose(f));
    oa_host h; assert(!oa_host_init(&h, argv[1], config)); h.log_level = OA_LOG_ERROR;
    oa_mod *m = oa_module_load(&h, "nav_display"); assert(m && !oa_module_start(&h, m));
    m = oa_module_load(&h, "ui_engine"); assert(m && !oa_module_start(&h, m));
    assert(!oa_skin_bootstrap(&h)); settle(&h);
    char reply[256]; assert(!command(&h, "skin", reply, sizeof reply));
    assert(strstr(reply, "requested missing active vendor state ready fallback 1"));
    puts("  ok   configured missing startup skin falls back to a committed vendor frame without a readiness file");
    select_skin(&h, "omarchy", 0); select_skin(&h, "vendor", 0); select_skin(&h, "custom", 0);
    puts("  ok   runtime control switches both real skins and an external ui_<name> module");
    assert(command(&h, "skin ../vendor", reply, sizeof reply) == -1);
    assert(command(&h, "skin vendor extra", reply, sizeof reply) == -1);
    assert(!command(&h, "skin missing", reply, sizeof reply));
    assert(command(&h, "skin vendor", reply, sizeof reply) == -1 && errno == EBUSY);
    oa_sleep_result sleep_result;
    assert(oa_module_suspend_all(&h) == -1);
    assert(oa_module_standby_cycle(&h, 0, forbidden_sleep, NULL, &sleep_result) == -1);
    settle(&h);
    puts("  ok   malformed commands and competing selections cannot mutate an accepted request");
    select_skin(&h, "startfail", 1); select_skin(&h, "badenter", 1); select_skin(&h, "silent", 1);
    puts("  ok   start failure, deferred enter failure and first-frame timeout safely load the vendor fallback");
    assert(!oa_skin_request(&h, "busyfail"));
    uint64_t end = oa_now_ms()+1000;
    while (!(m = oa_host_find_module(&h, "ui_busyfail")) || !m->cleanup_pending) {
        assert(oa_now_ms() < end); pump(&h);
    }
    for (int i = 0; i < 30; i++) pump(&h);
    assert(!oa_host_find_module(&h, "ui_vendor") && h.skin_timer && m->cleanup_pending);
    const struct { void (*release)(void); } *gate = h.api.lookup(&h, "test.skin_gate", 1); assert(gate);
    gate->release(); settle(&h);
    assert(h.skin_fallback && oa_host_find_module(&h, "ui_vendor"));
    puts("  ok   fallback waits for a failed candidate's nonblocking quiescence gate, including registered skin cleanup");
    assert(!oa_skin_request(&h, "initbusy")); end = oa_now_ms()+1000;
    while (!(m = oa_host_find_module(&h, "ui_initbusy")) || !m->cleanup_pending) {
        assert(oa_now_ms() < end); pump(&h);
    }
    for (int i = 0; i < 30; i++) pump(&h);
    assert(h.skin_timer && !oa_host_find_module(&h, "ui_vendor"));
    gate = h.api.lookup(&h, "test.skin_gate", 1); assert(gate);
    gate->release(); settle(&h); assert(h.skin_fallback);
    puts("  ok   failed init returning NULL retains its busy candidate until cleanup completes");
    assert(!oa_module_suspend_all(&h));
    assert(oa_skin_request(&h, "omarchy") == -1);
    assert(!oa_module_resume_all(&h));
    assert(!oa_skin_request(&h, "missing")); h.stop_requested = 1;
    settle(&h); assert(h.skin_error == ECANCELED);
    assert(!oa_host_run(&h));
    close(h.selfpipe[0]); close(h.selfpipe[1]); pthread_mutex_destroy(&h.post_mu); assert(!unlink(config));
    puts("  ok   suspended-engine requests reject and shutdown cancels selection without starting another module");
    return 0;
}
