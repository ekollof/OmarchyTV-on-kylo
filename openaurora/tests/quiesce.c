#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "host.h"
#include "quiesce_fixture.h"
static oa_mod *load(oa_host *h, struct quiesce_observer *o, int fail)
{
    memset(o, 0, sizeof *o);
    o->busy = 1;
    o->fail_init = fail;
    return oa_module_load(h, "quiesce_fixture");
}
static void retained(oa_host *h, oa_mod *m, struct quiesce_observer *o)
{
    assert(m && m->dl && m->ctx && m->cleanup_pending);
    assert(o->destroy == 0 && o->stop == 0);
    assert(oa_host_service_exists(h, QUIESCE_SERVICE));
    assert(h->nposts == 1 && h->cur_module == 0);
    assert(oa_module_start(h, m) == -1);
    assert(oa_module_suspend_all(h) == -1);
    assert(oa_module_reload(h, m->name, 1) == -1);
    assert(oa_module_unload(h, m->name, 1) == -1);
    assert(m->dl && m->ctx && h->nposts == 1);
    char line[] = "status quiesce_fixture", reply[512];
    assert(oa_control_command(h, line, reply, sizeof reply) == 0);
    assert(strstr(reply, "cleanup_pending 1"));
}
struct late {
    oa_host *h;
    unsigned owner;
    struct quiesce_observer *o;
};
static void mark_done(void *ud)
{
    struct quiesce_observer *o = ud;
    assert(o->destroy == 0 && o->stop == 0);
    o->busy = 0;
    o->completion++;
}
static void *late_post(void *ud)
{
    struct late *late = ud;
    struct timespec delay = { .tv_nsec = 30000000 };
    nanosleep(&delay, NULL);
    assert(late->h->api.post(late->h, late->owner, mark_done, late->o) == 0);
    return NULL;
}
static void reset_host(oa_host *h)
{
    close(h->selfpipe[0]);
    close(h->selfpipe[1]);
    pthread_mutex_destroy(&h->post_mu);
}
static void finish(oa_host *h, struct quiesce_observer *o)
{
    oa_host_dispatch_posts(h);
    assert(o->completion == 1 && !o->busy);
    assert(oa_module_unload(h, "quiesce_fixture", 0) == 0);
    assert(o->destroy == 1 && !oa_host_find_module(h, "quiesce_fixture"));
    assert(!oa_host_service_exists(h, QUIESCE_SERVICE) && !h->nposts);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    alarm(10);
    oa_host h;
    struct quiesce_observer o;
    assert(oa_host_init(&h, argv[1], NULL) == 0);
    assert(h.api.publish(&h, QUIESCE_OBSERVER, 1, &o) == 0);
    oa_mod *m = load(&h, &o, 0);
    assert(m && oa_module_start(&h, m) == 0);
    uint32_t id = m->id;
    assert(oa_module_reload(&h, m->name, 0) == -1 && m->id == id);
    retained(&h, m, &o);
    finish(&h, &o);
    assert(o.stop == 1);
    puts("  ok   running veto retains DSO/posts/services; force cannot bypass; completion permits unload");

    m = load(&h, &o, 0);
    assert(m && oa_module_unload(&h, m->name, 0) == -1);
    retained(&h, m, &o);
    finish(&h, &o);
    puts("  ok   never-started init resources protected by gate");

    assert(!load(&h, &o, 1));
    m = oa_host_find_module(&h, "quiesce_fixture");
    assert(m && m->init_failed);
    retained(&h, m, &o);
    oa_host_dispatch_posts(&h);
    assert(oa_module_stop(&h, m) == 0);
    assert(oa_module_start(&h, m) == -1 && o.stop == 0);
    assert(oa_module_unload(&h, m->name, 0) == 0 && o.destroy == 1);
    puts("  ok   failed init retained and discoverable for cleanup, never activated");

    m = load(&h, &o, 0);
    o.fail_start = 1;
    assert(m && oa_module_start(&h, m) == -1);
    retained(&h, m, &o);
    finish(&h, &o);
    assert(o.stop == 1);
    puts("  ok   failed partial start retains callback code until completion");

    m = load(&h, &o, 0);
    assert(m && oa_module_start(&h, m) == 0);
    assert(oa_module_suspend_all(&h) == 0);
    assert(oa_module_stop(&h, m) == -1);
    retained(&h, m, &o);
    assert(oa_module_resume_all(&h) == -1);
    finish(&h, &o);
    puts("  ok   suspended instance cannot resume while cleanup is pending");

    m = load(&h, &o, 0);
    assert(m && oa_module_start(&h, m) == 0);
    oa_host_dispatch_posts(&h);
    assert(oa_module_suspend_all(&h) == 0);
    assert(oa_module_reload(&h, m->name, 0) == 0);
    m = oa_host_find_module(&h, "quiesce_fixture");
    assert(m && m->start_on_resume);
    o.busy = o.fail_start = o.post_start = 1;
    o.completion = o.destroy = o.stop = 0;
    assert(oa_module_resume_all(&h) == -1);
    retained(&h, m, &o);
    finish(&h, &o);
    assert(o.stop == 1);
    puts("  ok   failed deferred activation retains resources and eventually runs stop");

    m = load(&h, &o, 0);
    assert(m && oa_module_start(&h, m) == 0);
    oa_host_dispatch_posts(&h);
    assert(o.completion == 1 && !o.busy);
    o.busy = 1;
    o.defer = 1;
    o.gate = o.stop = o.destroy = o.completion = 0;
    h.stop_requested = 1;
    assert(oa_host_run(&h) == 0);
    assert(o.gate == 4 && o.completion == 1 && o.stop == 1 && o.destroy == 1);
    assert(!oa_host_find_module(&h, "quiesce_fixture"));
    assert(h.shutting_down && !oa_module_load(&h, "quiesce_fixture"));
    puts("  ok   shutdown dispatches the gate completion and retries unload before return");

    reset_host(&h);
    assert(oa_host_init(&h, argv[1], NULL) == 0);
    assert(h.api.publish(&h, QUIESCE_OBSERVER, 1, &o) == 0);
    m = load(&h, &o, 0);
    assert(m && oa_module_start(&h, m) == 0);
    oa_host_dispatch_posts(&h);
    o.busy = 1;
    o.completion = o.gate = o.stop = o.destroy = 0;
    struct late late = { &h, o.owner, &o };
    pthread_t thread;
    assert(pthread_create(&thread, NULL, late_post, &late) == 0);
    h.stop_requested = 1;
    assert(oa_host_run(&h) == 0);
    assert(pthread_join(thread, NULL) == 0);
    assert(o.completion == 1 && o.stop == 1 && o.destroy == 1);
    assert(!oa_host_find_module(&h, "quiesce_fixture") && !h.nposts);
    puts("  ok   shutdown waits for a worker completion post before unload");

    reset_host(&h);
    assert(oa_host_init(&h, argv[1], NULL) == 0);
    assert(h.api.publish(&h, QUIESCE_OBSERVER, 1, &o) == 0);
    m = load(&h, &o, 0);
    assert(m && oa_module_start(&h, m) == 0);
    h.cur_module = 999;
    assert(h.api.lookup(&h, QUIESCE_SERVICE, 1));
    h.cur_module = 0;
    h.stop_requested = 1;
    assert(oa_host_run(&h) == -1);
    assert(oa_host_find_module(&h, "quiesce_fixture") && m->dl && o.destroy == 0 && o.stop == 0);
    h.cur_module = 999;
    h.api.release(&h, QUIESCE_SERVICE);
    h.cur_module = 0;
    assert(oa_module_unload(&h, "quiesce_fixture", 0) == 0 && o.destroy == 1);
    puts("  ok   shutdown reports a held module and does not dlclose it");

    close(h.selfpipe[0]); close(h.selfpipe[1]);
    return 0;
}
