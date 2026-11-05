#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "host.h"
#include "openaurora/scene_broker.h"
#define require(expression) do { if (!(expression)) { \
    fprintf(stderr, "%s:%d: %s errno=%d\n", __FILE__, __LINE__, #expression, errno); exit(1); } } while (0)
static const char *helper, *renderer;
static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "graphics.broker")) return helper;
    if (!strcmp(key, "graphics.renderer")) return renderer;
    return def;
}
static void initialize(oa_host *h, const char *directory)
{ require(!oa_host_init(h, directory, NULL)); h->log_level = OA_LOG_ERROR; h->api.config_get = config; }
static void pause_tick(void) { struct timespec delay = {.tv_nsec = 1000000}; (void)nanosleep(&delay, NULL); }
static void pump(oa_host *h)
{
    for (int i = 0; i < MAX_TIMERS; i++) if (h->timers[i].active) {
        oa_timer t = h->timers[i]; h->cur_module = t.owner; t.cb(t.ud); h->cur_module = 0;
    }
    for (int i = 0; i < MAX_WATCHES; i++) if (h->watches[i].active) {
        oa_watch w = h->watches[i]; struct pollfd p = {.fd = w.fd, .events = POLLIN};
        if (poll(&p, 1, 0) == 1) { h->cur_module = w.owner; w.cb(w.ud, w.fd, OA_WATCH_READ); h->cur_module = 0; }
    }
    pause_tick();
}
static void done(oa_host *h)
{
    h->stop_requested = 1; require(!oa_host_run(h));
    int status; require(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD);
    close(h->selfpipe[0]); close(h->selfpipe[1]); pthread_mutex_destroy(&h->post_mu);
}
static const oa_display_gpu_iface *load(oa_host *h)
{
    oa_mod *m = oa_module_load(h, "graphics"); require(m && !oa_module_start(h, m));
    int watches = 0, timers = 0;
    for (int i = 0; i < MAX_WATCHES; i++) watches += h->watches[i].active && h->watches[i].owner == m->id;
    for (int i = 0; i < MAX_TIMERS; i++) timers += h->timers[i].active && h->timers[i].owner == m->id;
    require(watches == 1 && timers == 1);
    const oa_display_gpu_iface *s = h->api.lookup(h, OA_SVC_DISPLAY_GPU, 2); require(s); return s;
}
static oa_scene_reply completed(oa_host *h, const oa_display_gpu_iface *s, uint64_t ticket)
{
    oa_scene_reply reply;
    for (unsigned i = 0; i < 5000; i++) {
        pump(h); int ready = s->take(ticket, &reply); require(ready >= 0);
        if (ready) { require(reply.ticket == ticket); return reply; }
    }
    require(0); return (oa_scene_reply){0};
}
static int image(void)
{
    uint8_t *pixels = calloc(1, OA_SCENE_IMAGE_BYTES); require(pixels); pixels[0] = 42;
    int fd = oa_scene_image_create(pixels, OA_SCENE_IMAGE_BYTES); free(pixels); require(fd >= 0); return fd;
}
static void basic(const char *directory)
{
    oa_host h; initialize(&h, directory); const oa_display_gpu_iface *s = load(&h);
    uint64_t ticket, untouched = 999;
    require(s->is_open() == 0 && s->frame(7, 4, 0.5f, -1, &untouched) == -1 && untouched == 999);
    require(!s->open(&ticket) && s->open(&untouched) == -1 && errno == EBUSY);
    require(!completed(&h, s, ticket).error && s->is_open() == 1);
    int fd = image(); require(!s->frame(7, 4, 0.5f, fd, &ticket)); require(!close(fd));
    require(!s->cancel(ticket));
    oa_scene_reply reply;
    int accepted = 0;
    for (unsigned i = 0; i < 5000 && !accepted; i++) {
        pump(&h); accepted = s->frame(7, 4, 0.5f, -1, &untouched) == 0;
        if (!accepted) require(errno == EBUSY);
    }
    require(accepted && !completed(&h, s, untouched).error);
    require(s->take(ticket, &reply) == -1 && errno == ENOENT);
    oa_mod *consumer = oa_module_load(&h, "ticker"); require(consumer && !oa_module_start(&h, consumer));
    h.cur_module = consumer->id; require(h.api.lookup(&h, OA_SVC_DISPLAY_GPU, 2) == s); h.cur_module = 0;
    require(oa_module_unload(&h, "graphics", 0) == -1);
    int suspended = 0;
    for (unsigned i = 0; i < 5000 && !suspended; i++) { suspended = !oa_module_suspend_all(&h); if (!suspended) pump(&h); }
    require(suspended && s->open(&untouched) == -1 && errno == ESHUTDOWN);
    const char *saved = helper; helper = "/missing/oa-scene-broker";
    require(oa_module_resume_all(&h) == -1); helper = saved;
    require(!oa_module_resume_all(&h) && s->is_open() == 0);
    require(oa_module_unload(&h, "graphics", 0) == -1); /* hold survived */
    h.cur_module = consumer->id; h.api.release(&h, OA_SVC_DISPLAY_GPU); h.cur_module = 0;
    require(!s->open(&ticket) && !completed(&h, s, ticket).error);
    uint64_t old_ticket = ticket;
    int reloaded = 0;
    for (unsigned i = 0; i < 5000 && !reloaded; i++) { reloaded = !oa_module_reload(&h, "graphics", 0); if (!reloaded) pump(&h); }
    require(reloaded); s = h.api.lookup(&h, OA_SVC_DISPLAY_GPU, 2); require(s);
    require(!s->open(&ticket) && (ticket >> 32) != (old_ticket >> 32));
    require(s->take(old_ticket, &reply) == -1 && errno == ENOENT);
    require(!completed(&h, s, ticket).error); done(&h);
    puts("  ok   owned watch/timer, bounded submit/cancel, service holds, suspend/resume and reload-scoped tickets");
}
static void blocked_close(const char *directory)
{
    char temporary[] = "/tmp/oa-graphics-module-XXXXXX", path[160]; require(mkdtemp(temporary));
    require(snprintf(path, sizeof path, "%s/gate", temporary) > 0 && !mkfifo(path, 0600));
    require(!setenv("OA_SCENE_FIXTURE_CLOSE_GATE", path, 1));
    oa_host h; initialize(&h, directory); const oa_display_gpu_iface *s = load(&h);
    uint64_t ticket; require(!s->open(&ticket) && !completed(&h, s, ticket).error);
    require(oa_module_unload(&h, "graphics", 1) == -1);
    int writer = -1;
    for (unsigned i = 0; i < 5000 && writer < 0; i++) { writer = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC); if (writer < 0) { require(errno == ENXIO); pump(&h); } }
    require(writer >= 0);
    for (unsigned i = 0; i < 20; i++) {
        require(oa_module_unload(&h, "graphics", 1) == -1 && oa_host_find_module(&h, "graphics")); pump(&h);
    }
    require(write(writer, "x", 1) == 1 && !close(writer));
    int unloaded = 0;
    for (unsigned i = 0; i < 5000 && !unloaded; i++) { unloaded = !oa_module_unload(&h, "graphics", 1); if (!unloaded) pump(&h); }
    require(unloaded && !oa_host_service_exists(&h, OA_SVC_DISPLAY_GPU)); done(&h);
    require(!unsetenv("OA_SCENE_FIXTURE_CLOSE_GATE") && !unlink(path) && !rmdir(temporary));
    puts("  ok   blocked child cleanup retains DSO/context/timer; --force cannot bypass quiescence");
}
static void failures(const char *directory)
{
    oa_host h; const char *saved = helper; helper = "/missing/oa-scene-broker";
    initialize(&h, directory); oa_mod *m = oa_module_load(&h, "graphics");
    require(m && oa_module_start(&h, m) == -1 && !oa_host_service_exists(&h, OA_SVC_DISPLAY_GPU)); done(&h); helper = saved;
    initialize(&h, directory);
    for (unsigned i = 0; i < MAX_TIMERS; i++) h.timers[i].active = 1;
    m = oa_module_load(&h, "graphics"); require(m && oa_module_start(&h, m) == -1);
    memset(h.timers, 0, sizeof h.timers); done(&h);
    initialize(&h, directory);
    for (unsigned i = 0; i < MAX_WATCHES; i++) h.watches[i].active = 1;
    m = oa_module_load(&h, "graphics"); require(m && oa_module_start(&h, m) == -1);
    memset(h.watches, 0, sizeof h.watches); done(&h);
    puts("  ok   config/spawn/timer/watch failure retains completion path and unwinds services/children");
}
static void timeout_handler(int sig) { (void)sig; _exit(124); }
static void abnormal(const char *directory, const char *self)
{
    /* This process deliberately retains a faulted module until process exit;
     * the peer owns no GPU/device. Test child cleanup must not override a gate. */
    pid_t child = fork(); require(child >= 0);
    if (!child) {
        helper = self;
        oa_host h; initialize(&h, directory); const oa_display_gpu_iface *s = load(&h);
        uint64_t ticket; require(!s->open(&ticket));
        require(completed(&h, s, ticket).error != 0);
        for (unsigned i = 0; i < 20; i++) {
            pump(&h); require(oa_module_unload(&h, "graphics", 1) == -1);
            require(oa_host_find_module(&h, "graphics") && oa_host_service_exists(&h, OA_SVC_DISPLAY_GPU));
        }
        int status; require(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD);
        _exit(0);
    }
    int status; require(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    puts("  ok   abnormal peer death is reaped but retains a faulted module; it is not successful device cleanup");
}
int main(int argc, char **argv)
{
    require(signal(SIGALRM, timeout_handler) != SIG_ERR); alarm(30);
    if (argc == 3 && !strcmp(argv[1], "--serve")) {
        uint8_t bytes[64]; int image;
        require(oa_scene_packet_receive(STDIN_FILENO, bytes, &image, 0) == 1 && image == -1);
        (void)kill(getpid(), SIGKILL); _exit(1);
    }
    require(argc == 4);
    helper = argv[2]; renderer = argv[3];
    basic(argv[1]); blocked_close(argv[1]); failures(argv[1]);
    char *self = realpath(argv[0], NULL); require(self); abnormal(argv[1], self); free(self); return 0;
}
