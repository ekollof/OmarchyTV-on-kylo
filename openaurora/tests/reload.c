#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "host.h"
#include "reload_fixture.h"

static int fail_malloc;
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size)
{
    if (fail_malloc) { fail_malloc = 0; return NULL; }
    return __real_malloc(size);
}

static void point_to(const char *path, const char *dir, const char *filename)
{
    char target[512];
    int n = snprintf(target, sizeof target, "%s/%s", dir, filename);
    assert(n > 0 && (size_t)n < sizeof target);
    (void)unlink(path);
    assert(symlink(target, path) == 0);
}

static oa_mod *load(oa_host *h, struct reload_observer *o)
{
    memset(o, 0, sizeof *o);
    o->value = 73;
    o->save_need = o->save_result = sizeof(unsigned);
    oa_mod *m = oa_module_load(h, "reload_fixture");
    o->value = 0; /* replacement init must not accidentally recreate saved value */
    return m;
}
static int command(oa_host *h, const char *text)
{
    char line[80], reply[256];
    snprintf(line, sizeof line, "%s", text);
    int rc = oa_control_command(h, line, reply, sizeof reply);
    assert(strstr(reply, rc == 0 ? "ok" : "failed"));
    return rc;
}
static void no_fixture_registrations(oa_host *h)
{
    assert(!oa_host_service_exists(h, RELOAD_SERVICE));
    for (int i = 0; i < MAX_TIMERS; i++) assert(!h->timers[i].active);
    for (int i = 0; i < MAX_SUBS; i++) assert(!h->subs[i].active);
    assert(h->nposts == 0 && h->cur_module == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    oa_host h;
    struct reload_observer o;
    assert(oa_host_init(&h, argv[1], NULL) == 0);
    assert(h.api.publish(&h, RELOAD_OBSERVER, 1, &o) == 0);

    oa_mod *m = load(&h, &o);
    assert(m);
    uint32_t old = m->id;
    assert(command(&h, "reload reload_fixture") == 0);
    m = oa_host_find_module(&h, "reload_fixture");
    assert(m && m->id != old && m->state == OA_ST_LOADED);
    assert(o.init == 2 && o.start == 0 && o.stop == 0 && o.destroy == 1);
    assert(o.restore == 1 && o.restored_value == 73);
    no_fixture_registrations(&h);
    assert(oa_module_start(&h, m) == 0 && oa_module_stop(&h, m) == 0);
    assert(command(&h, "reload reload_fixture") == 0);
    m = oa_host_find_module(&h, "reload_fixture");
    assert(m && m->state == OA_ST_LOADED && o.start == 1 && o.stop == 1);
    assert(oa_module_unload(&h, "reload_fixture", 0) == 0);
    puts("  ok   never-started and stopped reload preserve loaded state without activation");

    m = load(&h, &o);
    assert(m && oa_module_start(&h, m) == 0);
    assert(command(&h, "reload reload_fixture") == 0);
    m = oa_host_find_module(&h, "reload_fixture");
    assert(m && m->state == OA_ST_STARTED && o.start == 2 && o.stop == 1);
    assert(o.restored_value == 73 && o.active_value == 73);
    assert(oa_host_service_exists(&h, RELOAD_SERVICE));
    assert(oa_module_unload(&h, "reload_fixture", 0) == 0);
    puts("  ok   running reload restores state and starts the new instance");

    m = load(&h, &o);
    assert(m && oa_module_start(&h, m) == 0);
    oa_module_suspend_all(&h);
    old = m->id;
    assert(m->state == OA_ST_SUSPENDED && o.suspend == 1);
    assert(command(&h, "reload reload_fixture") == 0);
    m = oa_host_find_module(&h, "reload_fixture");
    assert(m && m->id != old && m->state == OA_ST_SUSPENDED);
    assert(o.start == 1 && o.resume == 0 && o.suspend == 1);
    no_fixture_registrations(&h);
    oa_module_resume_all(&h);
    assert(m->state == OA_ST_STARTED && o.start == 2 && o.resume == 0);
    assert(o.active_value == 73); /* restore precedes deferred first start */
    oa_module_suspend_all(&h);
    oa_module_resume_all(&h);
    assert(m->state == OA_ST_STARTED && o.start == 2 && o.resume == 1);
    oa_module_suspend_all(&h);
    assert(command(&h, "start reload_fixture") == 0);
    assert(m->state == OA_ST_STARTED && o.start == 2 && o.resume == 2);
    oa_module_suspend_all(&h);
    o.fail_resume = 1;
    assert(command(&h, "start reload_fixture") == -1);
    assert(m->state == OA_ST_SUSPENDED && o.start == 2 && o.resume == 3);
    o.fail_resume = 0;
    oa_module_resume_all(&h);
    assert(m->state == OA_ST_STARTED && o.start == 2 && o.resume == 4);
    assert(oa_module_unload(&h, "reload_fixture", 0) == 0);
    puts("  ok   suspended reload defers start until wake; start on suspended resumes once");

    m = load(&h, &o);
    assert(m && oa_module_start(&h, m) == 0);
    oa_module_suspend_all(&h);
    assert(command(&h, "reload reload_fixture") == 0);
    m = oa_host_find_module(&h, "reload_fixture");
    o.fail_start = 1;
    oa_module_resume_all(&h);
    assert(m->state == OA_ST_SUSPENDED && o.start == 2 && o.stop == 2);
    no_fixture_registrations(&h);
    o.fail_start = 0;
    oa_module_resume_all(&h);
    assert(m->state == OA_ST_STARTED && o.start == 3 && o.resume == 0);
    oa_module_suspend_all(&h);
    assert(command(&h, "reload reload_fixture") == 0);
    unsigned stops = o.stop;
    assert(command(&h, "stop reload_fixture") == 0);
    assert(m->state == OA_ST_LOADED && o.stop == stops);
    assert(command(&h, "start reload_fixture") == 0 && m->state == OA_ST_STARTED);
    assert(oa_module_unload(&h, "reload_fixture", 0) == 0);
    puts("  ok   deferred start can fail/retry or be stopped before touching hardware");

    m = load(&h, &o);
    assert(m && oa_module_start(&h, m) == 0);
    oa_module_suspend_all(&h);
    o.post_on_restore = 1;
    assert(command(&h, "reload reload_fixture") == 0);
    assert(command(&h, "reload reload_fixture") == 0);
    m = oa_host_find_module(&h, "reload_fixture");
    assert(m && m->state == OA_ST_SUSPENDED && o.start == 1 && o.stop == 1);
    no_fixture_registrations(&h);
    oa_host_dispatch_posts(&h);
    assert(o.bad_callback == 0);
    assert(oa_module_unload(&h, "reload_fixture", 0) == 0);
    assert(o.stop == 1 && o.destroy == 3);
    puts("  ok   repeated suspended reload purges posts; unload skips stop on never-started candidate");

    m = load(&h, &o);
    assert(m && oa_module_start(&h, m) == 0);
    old = m->id;
    fail_malloc = 1;
    assert(command(&h, "reload reload_fixture") == -1 && fail_malloc == 0);
    assert(m->id == old && m->state == OA_ST_STARTED && o.destroy == 0 && o.stop == 0);
    o.save_need = MAX_STATE + 1u;
    assert(command(&h, "reload reload_fixture") == -1);
    assert(m->id == old && m->state == OA_ST_STARTED && o.destroy == 0 && o.stop == 0);
    o.save_need = sizeof(unsigned);
    o.save_result = sizeof(unsigned) + 1u;
    assert(command(&h, "reload reload_fixture") == -1);
    assert(m->id == old && m->state == OA_ST_STARTED && o.destroy == 0 && o.stop == 0);
    assert(oa_module_unload(&h, "reload_fixture", 0) == 0);
    puts("  ok   allocation/oversized/growing state failures preserve the working module");

    m = load(&h, &o);
    assert(m && oa_module_start(&h, m) == 0);
    o.fail_restore = 1;
    assert(command(&h, "reload reload_fixture") == -1);
    assert(!oa_host_find_module(&h, "reload_fixture"));
    assert(o.start == 1 && o.restore == 1 && o.destroy == 2);
    no_fixture_registrations(&h);
    oa_host_dispatch_posts(&h);
    assert(o.bad_callback == 0);
    puts("  ok   restore failure is reported and candidate registrations/posts are swept");

    m = load(&h, &o);
    assert(m && oa_module_start(&h, m) == 0);
    o.fail_start = 1;
    assert(command(&h, "reload reload_fixture") == -1);
    assert(!oa_host_find_module(&h, "reload_fixture"));
    assert(o.start == 2 && o.stop == 2 && o.destroy == 2);
    no_fixture_registrations(&h);
    puts("  ok   replacement start failure cleans the candidate instead of leaving partial state");

    /* Replace the actual on-disk path between dlclose and the next dlopen. */
    char *dir = realpath(argv[1], NULL);
    assert(dir);
    char temporary[] = "/tmp/openaurora-reload-XXXXXX", candidate[256];
    assert(mkdtemp(temporary));
    snprintf(candidate, sizeof candidate, "%s/module.so", temporary);
    for (unsigned renamed = 0; renamed < 2; renamed++) {
        memset(&o, 0, sizeof o);
        o.value = 73;
        o.save_need = o.save_result = sizeof(unsigned);
        point_to(candidate, dir, "mod_reload_fixture.so");
        m = oa_module_load(&h, candidate);
        assert(m && oa_module_start(&h, m) == 0);
        point_to(candidate, dir, renamed ? "mod_reload_renamed.so" : "mod_reload_no_restore.so");
        assert(command(&h, "reload reload_fixture") == -1);
        assert(!oa_host_find_module(&h, "reload_fixture"));
        assert(!oa_host_find_module(&h, "reload_renamed"));
        assert(o.start == 1 && o.destroy == 2 && o.restore == 0);
        no_fixture_registrations(&h);
    }
    assert(unlink(candidate) == 0 && rmdir(temporary) == 0);
    free(dir);
    puts("  ok   changed binary without restore or with a new identity is rejected and cleaned");

    close(h.selfpipe[0]); close(h.selfpipe[1]);
    pthread_mutex_destroy(&h.post_mu);
    return 0;
}
