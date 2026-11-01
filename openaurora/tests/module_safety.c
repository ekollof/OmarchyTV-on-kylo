#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "host.h"
#include "post_worker.h"

static const struct post_worker_iface *worker(oa_host *h)
{
    const struct post_worker_iface *p = h->api.lookup(h, POST_WORKER_SERVICE, 1);
    assert(p);
    return p;
}
static void noop(void *ud) { (void)ud; }
static void unload_other(void *ud)
{
    oa_host *h = ud;
    assert(oa_module_unload(h, "post_worker", 0) == 0);
}
static void full_path(char *out, size_t len, const char *dir, const char *name)
{ snprintf(out, len, "%s/%s", dir, name); }

int main(int argc, char **argv)
{
    assert(argc == 3);
    oa_host h;
    assert(oa_host_init(&h, argv[1], NULL) == 0);
    char path[512];
    full_path(path, sizeof path, argv[2], "mod_abi_v1.so");
    assert(!oa_module_load(&h, path));
    full_path(path, sizeof path, argv[2], "mod_abi_v2.so");
    assert(!oa_module_load(&h, path));
    full_path(path, sizeof path, argv[2], "mod_abi_short.so");
    assert(!oa_module_load(&h, path));
    full_path(path, sizeof path, argv[2], "mod_abi_extended.so");
    oa_mod *m = oa_module_load(&h, path);
    assert(m && oa_module_start(&h, m) == 0);
    assert(oa_module_unload(&h, "abi_extended", 0) == 0);
    puts("  ok   old/short descriptors rejected before guard page; extended prefix accepted");

    full_path(path, sizeof path, argv[2], "mod_post_worker.so");
    m = oa_module_load(&h, path);
    assert(m && oa_module_start(&h, m) == 0);
    const struct post_worker_iface *p = worker(&h);
    uint32_t old = p->owner();
    assert(old == m->id && old != 0);
    assert(p->enqueue() == 0);
    assert(h.nposts == 1 && h.posts[0].owner == old);
    oa_host_dispatch_posts(&h);
    assert(p->calls() == 1);
    int owned_timer = 0;
    for (int i = 0; i < MAX_TIMERS; i++)
        if (h.timers[i].active && h.timers[i].owner == old) owned_timer++;
    assert(owned_timer == 1 && h.cur_module == 0);
    puts("  ok   real worker post dispatches under its owner; registrations inherit ownership");

    assert(p->enqueue() == 0); /* abandoned old-instance work must not run after reload */
    char command[] = "reload post_worker", reply[256];
    assert(oa_control_command(&h, command, reply, sizeof reply) == 0);
    p = worker(&h);
    assert(p->owner() != old && p->calls() == 1);
    assert(h.nposts == 0);
    assert(h.api.post(&h, old, noop, NULL) == -2);
    assert(p->enqueue() == 0);
    oa_host_dispatch_posts(&h);
    assert(p->calls() == 2);
    puts("  ok   socket/CLI dispatcher reload preserves state, cancels old work, rejects old token");

    for (int i = 0; i < MAX_POSTS; i++) assert(p->enqueue() == 0);
    assert(p->enqueue() == -1);
    assert(oa_module_stop(&h, oa_host_find_module(&h, "post_worker")) == 0);
    assert(h.nposts == 0 && p->late_post_result() == -2);
    assert(p->enqueue() == -2);
    for (int i = 0; i < MAX_TIMERS; i++) assert(!h.timers[i].active);
    assert(oa_module_start(&h, oa_host_find_module(&h, "post_worker")) == 0);
    p = worker(&h);
    assert(p->enqueue() == 0);
    oa_host_dispatch_posts(&h);
    assert(p->calls() == 3);
    puts("  ok   queue saturation, stop-time worker post rejection, cleanup and restart");

    oa_mod *ticker = oa_module_load(&h, "ticker");
    assert(ticker && oa_module_start(&h, ticker) == 0);
    assert(h.api.post(&h, ticker->id, unload_other, &h) == 0);
    assert(p->enqueue() == 0);
    oa_host_dispatch_posts(&h); /* first post unloads the library containing the second */
    assert(!oa_host_find_module(&h, "post_worker") && h.nposts == 0);
    assert(oa_module_unload(&h, "ticker", 0) == 0);
    puts("  ok   an earlier post can unload another owner without dispatching its stale callback");

    full_path(path, sizeof path, argv[2], "mod_post_fail_init.so");
    assert(!oa_module_load(&h, path) && h.nposts == 0);
    full_path(path, sizeof path, argv[2], "mod_post_fail_start.so");
    m = oa_module_load(&h, path);
    assert(m && oa_module_start(&h, m) == -1 && h.nposts == 0);
    assert(h.api.post(&h, m->id, noop, NULL) == -2);
    assert(oa_module_unload(&h, "post_worker", 0) == 0);
    oa_host_dispatch_posts(&h);
    assert(h.api.post(&h, 0, noop, NULL) == -2);
    assert(h.api.post(&h, 999999, noop, NULL) == -2);
    puts("  ok   failed init/start cancel queued module code and close post acceptance");
    close(h.selfpipe[0]); close(h.selfpipe[1]);
    pthread_mutex_destroy(&h.post_mu);
    return 0;
}
