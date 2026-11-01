#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "host.h"
static const char *broker;
static int native_mode;
static int descriptors(void)
{
    DIR *dir = opendir("/proc/self/fd"); assert(dir);
    int count = 0; while (readdir(dir)) count++;
    closedir(dir); return count;
}
static const char *config(void *ud, const char *key, const char *def)
{ (void)ud; return !strcmp(key, "audio_mixer.broker") ? broker : def; }
static void initialize(oa_host *h, const char *dir, const char *mode)
{
    assert(setenv("OA_MIXER_TEST_MODE", mode, 1) == 0);
    assert(oa_host_init(h, dir, NULL) == 0);
    h->log_level = OA_LOG_ERROR; h->api.config_get = config;
}
/* Deterministic loop pumping for direct service tests. Timer first also covers
 * a child that exited with a final reply still buffered. */
static void pump(oa_host *h)
{
    for (int i = 0; i < MAX_TIMERS; i++) if (h->timers[i].active) {
        oa_timer t = h->timers[i]; h->cur_module = t.owner; t.cb(t.ud); h->cur_module = 0;
    }
    for (int i = 0; i < MAX_WATCHES; i++) if (h->watches[i].active) {
        oa_watch w = h->watches[i]; struct pollfd p = { .fd = w.fd, .events = POLLIN };
        if (poll(&p, 1, 0) == 1) { h->cur_module = w.owner; w.cb(w.ud, w.fd, OA_WATCH_READ); h->cur_module = 0; }
    }
    const struct timespec pause = { .tv_nsec = 1000000 }; nanosleep(&pause, NULL);
}
static void done(oa_host *h)
{
    h->stop_requested = 1;
    assert(oa_host_run(h) == 0);
    int status;
    assert(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD);
    close(h->selfpipe[0]); close(h->selfpipe[1]); pthread_mutex_destroy(&h->post_mu);
}
static const oa_audio_mixer_iface *load(oa_host *h)
{
    oa_mod *m = oa_module_load(h, "audio_mixer");
    assert(m && oa_module_start(h, m) == 0);
    int watches = 0, timers = 0;
    for (int i = 0; i < MAX_WATCHES; i++) watches += h->watches[i].active && h->watches[i].owner == m->id;
    for (int i = 0; i < MAX_TIMERS; i++) timers += h->timers[i].active && h->timers[i].owner == m->id;
    assert(watches == 1 && timers == 1);
    const oa_audio_mixer_iface *service = h->api.lookup(h, OA_SVC_AUDIO_MIXER, 1);
    assert(service); return service;
}
static oa_audio_broker_reply collect(oa_host *h, const oa_audio_mixer_iface *s, uint64_t ticket)
{
    oa_audio_broker_reply result = {0}; int rc = 0;
    for (int i = 0; i < 1000 && !rc; i++) { pump(h); rc = s->take(ticket, &result); }
    assert(rc == 1 && result.ticket == ticket);
    oa_audio_broker_reply before = result;
    assert(s->take(ticket, &result) == -1 && errno == ENOENT && !memcmp(&before, &result, sizeof result));
    return result;
}
static void basic(const char *dir)
{
    oa_host h; initialize(&h, dir, "echo");
    const oa_audio_mixer_iface *s = load(&h);
    uint64_t ticket = 99, untouched = 99;
    assert(s->gain(6, &ticket) == -1 && errno == EINVAL && ticket == 99);
    assert(s->gain(0, &ticket) == 0);
    assert(s->gain(1, &untouched) == -1 && errno == EBUSY && untouched == 99);
    oa_audio_broker_reply r = collect(&h, s, ticket);
    assert(!r.error && r.gain.device == 0 && r.gain.left == 123 && r.gain.right == 456);
    assert(s->gain(1, &ticket) == 0 && s->cancel(ticket) == 0);
    for (int i = 0; i < 1000; i++) {
        pump(&h);
        if (s->gain(2, &untouched) == 0) break;
        assert(errno == EBUSY);
    }
    assert(untouched != 99);
    (void)collect(&h, s, untouched);
    oa_mod *consumer = oa_module_load(&h, "ticker");
    assert(consumer && oa_module_start(&h, consumer) == 0);
    h.cur_module = consumer->id;
    assert(h.api.lookup(&h, OA_SVC_AUDIO_MIXER, 1) == s);
    h.cur_module = 0;
    assert(oa_module_unload(&h, "audio_mixer", 0) == -1);
    int rc = -1;
    for (int i = 0; i < 1000 && rc; i++) { rc = oa_module_suspend_all(&h); if (rc) pump(&h); }
    assert(rc == 0);
    assert(s->gain(0, &untouched) == -1 && errno == ESHUTDOWN);
    int count = descriptors();
    const char *saved = broker; broker = "/does/not/exist/oa-audio-broker";
    int resume_rc = oa_module_resume_all(&h), after = descriptors();
    if (resume_rc != -1 || after != count)
        fprintf(stderr, "failed-resume diagnostic: rc=%d fds-before=%d after=%d\n", resume_rc, count, after);
    assert(resume_rc == -1 && after == count); broker = saved;
    for (int i = 0; i < MAX_WATCHES; i++) h.watches[i].active = 1;
    assert(oa_module_resume_all(&h) == -1 && descriptors() == count);
    memset(h.watches, 0, sizeof h.watches);
    assert(oa_module_resume_all(&h) == 0);
    assert(oa_module_unload(&h, "audio_mixer", 0) == -1); /* hold survived resume */
    h.cur_module = consumer->id; h.api.release(&h, OA_SVC_AUDIO_MIXER); h.cur_module = 0;
    assert(s->gain(3, &ticket) == 0); (void)collect(&h, s, ticket);
    uint32_t old_id = oa_host_find_module(&h, "audio_mixer")->id;
    rc = -1;
    for (int i = 0; i < 1000 && rc; i++) { rc = oa_module_reload(&h, "audio_mixer", 0); if (rc) pump(&h); }
    assert(rc == 0 && oa_host_find_module(&h, "audio_mixer")->id != old_id);
    s = h.api.lookup(&h, OA_SVC_AUDIO_MIXER, 1);
    assert(s && s->take(ticket, &r) == -1 && errno == ENOENT);
    done(&h);
    puts("  ok   real DSO owns watches/timer, bounds requests, cancels/drains, preserves holds across suspend and invalidates reload tickets");
}
static void failures(const char *dir)
{
    const char *modes[] = { "mismatch", "oversize", "exit", "error", "replyexit" };
    const int errors[] = { EPROTO, EPROTO, ECONNRESET, EIO, 0 };
    for (unsigned i = 0; i < sizeof errors / sizeof errors[0]; i++) {
        oa_host h; initialize(&h, dir, modes[i]);
        const oa_audio_mixer_iface *s = load(&h); uint64_t ticket;
        assert(s->gain(4, &ticket) == 0);
        oa_audio_broker_reply r = collect(&h, s, ticket);
        assert(r.error == errors[i] && r.gain.device == 4);
        done(&h);
    }
    oa_host h; initialize(&h, dir, "hang");
    const oa_audio_mixer_iface *s = load(&h); uint64_t ticket;
    assert(s->gain(0, &ticket) == 0);
    for (int i = 0; i < 10; i++) pump(&h);
    oa_audio_broker_reply r = { .ticket = 88 };
    assert(s->take(ticket, &r) == 0 && r.ticket == 88);
    done(&h); /* close IPC wakes the peer; host loop reaps before unloading */
    puts("  ok   mismatched/oversize/EOF/error replies, final buffered reply and shutdown during a hung request release every child");
}
static void start_failures(const char *dir)
{
    oa_host h; const char *saved = broker; broker = "/does/not/exist/oa-audio-broker";
    initialize(&h, dir, "echo"); oa_mod *m = oa_module_load(&h, "audio_mixer");
    assert(m && oa_module_start(&h, m) == -1 && !oa_host_service_exists(&h, OA_SVC_AUDIO_MIXER));
    done(&h); broker = saved;
    initialize(&h, dir, "echo");
    for (int i = 0; i < MAX_TIMERS; i++) h.timers[i].active = 1;
    m = oa_module_load(&h, "audio_mixer"); assert(m && oa_module_start(&h, m) == -1);
    memset(h.timers, 0, sizeof h.timers); done(&h);
    initialize(&h, dir, "echo");
    for (int i = 0; i < MAX_WATCHES; i++) h.watches[i].active = 1;
    m = oa_module_load(&h, "audio_mixer"); assert(m && oa_module_start(&h, m) == -1);
    memset(h.watches, 0, sizeof h.watches); done(&h);
    puts("  ok   spawn/timer/watch failures unwind without a child or stale service");
}
static void late_exec_failure(const char *dir, const char *bad_exec)
{
    const char *saved = broker; broker = bad_exec;
    oa_host h; initialize(&h, dir, "echo");
    oa_mod *m = oa_module_load(&h, "audio_mixer"); assert(m);
    int rc = oa_module_start(&h, m);
    if (rc == 0) {
        const oa_audio_mixer_iface *s = h.api.lookup(&h, OA_SVC_AUDIO_MIXER, 1); assert(s);
        uint64_t ticket;
        if (s->gain(0, &ticket) == 0) {
            oa_audio_broker_reply result = collect(&h, s, ticket);
            assert(result.error == ECONNRESET || result.error == EPIPE);
        } else assert(errno == ENOTCONN || errno == EPIPE || errno == ECONNRESET);
    } else assert(!oa_host_service_exists(&h, OA_SVC_AUDIO_MIXER));
    done(&h); broker = saved;
    puts("  ok   missing exec interpreter is safe for immediate or asynchronous libc spawn failure; child is reaped");
}
static char socket_path[108];
static void rpc(int fd, const char *command, char *reply, size_t cap)
{
    char line[160]; int length = snprintf(line, sizeof line, "%s\n", command);
    assert(send(fd, line, (size_t)length, MSG_NOSIGNAL) == length);
    size_t used = 0;
    while (used + 1 < cap) {
        struct pollfd p = { .fd = fd, .events = POLLIN };
        assert(poll(&p, 1, 5000) == 1);
        ssize_t n = recv(fd, reply + used, 1, 0); assert(n == 1);
        if (reply[used++] == '\n') { reply[used] = 0; return; }
    }
    assert(0);
}
static void *client(void *ud)
{
    (void)ud;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0); assert(fd >= 0);
    struct sockaddr_un a = { .sun_family = AF_UNIX }; snprintf(a.sun_path, sizeof a.sun_path, "%s", socket_path);
    assert(connect(fd, (struct sockaddr *)&a, sizeof a) == 0);
    char reply[512], command[128]; uint64_t old = 0;
    const char *invalid[] = { "mixer-query -1", "mixer-query 6", "mixer-query 0 extra", "mixer-result 0",
        "mixer-result 18446744073709551616", "mixer-cancel +1" };
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        rpc(fd, invalid[i], reply, sizeof reply); assert(strstr(reply, "failed errno"));
    }
    for (int round = 0; round < 2; round++) {
        rpc(fd, "mixer-query 0", reply, sizeof reply);
        uint64_t ticket; assert(sscanf(reply, "ticket %" SCNu64, &ticket) == 1 && ticket != old);
        snprintf(command, sizeof command, "mixer-result %" PRIu64, ticket);
        int ready = 0;
        for (int i = 0; i < 1000 && !ready; i++) {
            rpc(fd, command, reply, sizeof reply);
            ready = !strncmp(reply, "result ", 7);
            if (!ready) { assert(!strncmp(reply, "pending ", 8)); const struct timespec p = {0,1000000}; nanosleep(&p,NULL); }
        }
        assert(ready && strstr(reply, "error 0 device 0 left "));
        if (native_mode) printf("  native socket %s", reply);
        else assert(strstr(reply, "left 123 right 456"));
        old = ticket;
        int reloaded = 0;
        for (int i = 0; i < 1000 && !reloaded; i++) {
            rpc(fd, "reload audio_mixer", reply, sizeof reply); reloaded = !strcmp(reply, "ok\n");
        }
        assert(reloaded);
        rpc(fd, command, reply, sizeof reply); assert(strstr(reply, "failed errno"));
    }
    rpc(fd, "quit", reply, sizeof reply); assert(!strcmp(reply, "bye\n")); close(fd);
    return NULL;
}
int main(int argc, char **argv)
{
    assert(argc == 4 || (argc == 5 && !strcmp(argv[4], "--native")));
    alarm(30); broker = argv[2]; native_mode = argc == 5;
    if (!native_mode) {
        basic(argv[1]); failures(argv[1]); start_failures(argv[1]);
        late_exec_failure(argv[1], argv[3]);
    }
    oa_host h; initialize(&h, argv[1], "echo"); (void)load(&h);
    snprintf(socket_path, sizeof socket_path, "/tmp/oa-mixer-%ld.sock", (long)getpid());
    assert(oa_control_start(&h, socket_path) == 0);
    pthread_t thread; assert(pthread_create(&thread, NULL, client, NULL) == 0);
    assert(oa_host_run(&h) == 0 && pthread_join(thread, NULL) == 0);
    int status; assert(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD);
    close(h.selfpipe[0]); close(h.selfpipe[1]); pthread_mutex_destroy(&h.post_mu);
    puts("  ok   real control socket queries/collects and reloads the DSO while the host reactor stays responsive");
    return 0;
}
