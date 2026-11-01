/* Blocking real-display diagnostic adapter. Exercises the actual reloadable
 * provider/watch/timer; never used by production UI or a quiescence gate. */
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "host.h"
#include "scene-probe-client.h"
struct scene_probe { oa_host host; const oa_display_gpu_iface *service; uint64_t revision; int initialized, error; };
static const char *helper_path, *renderer_path;
static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "graphics.broker")) return helper_path;
    if (!strcmp(key, "graphics.renderer")) return renderer_path;
    return def;
}
static void pump(scene_probe *s)
{
    oa_host *h = &s->host;
    oa_host_dispatch_posts(h);
    uint64_t now = oa_now_ms();
    for (int i = 0; i < MAX_TIMERS; i++) if (h->timers[i].active && h->timers[i].due <= now) {
        oa_timer t = h->timers[i];
        if (t.repeat_ms) h->timers[i].due = now+t.repeat_ms; else h->timers[i].active = 0;
        h->cur_module = t.owner; t.cb(t.ud); h->cur_module = 0;
    }
    for (int i = 0; i < MAX_WATCHES; i++) if (h->watches[i].active) {
        oa_watch w = h->watches[i]; struct pollfd p = {.fd = w.fd, .events = POLLIN};
        if (poll(&p, 1, 0) == 1) { h->cur_module = w.owner; w.cb(w.ud, w.fd, OA_WATCH_READ); h->cur_module = 0; }
    }
}
static int completion(scene_probe *s, uint64_t ticket)
{
    uint64_t deadline = oa_now_ms()+5000;
    for (;;) {
        pump(s);
        oa_scene_reply reply;
        int taken = s->service->take(ticket, &reply);
        if (taken == 1) { if (!reply.error) return 0; errno = reply.error; break; }
        if (taken < 0) break;
        if (oa_now_ms() >= deadline) { errno = ETIMEDOUT; break; }
        struct timespec pause = {.tv_nsec = 1000000}; (void)nanosleep(&pause, NULL);
    }
    s->error = errno; return -1;
}
int probe_open(scene_probe **out, unsigned width, unsigned height, const char *helper, const char *renderer)
{
    const char *modules = getenv("OA_SCENE_PROBE_MODULES");
    if (!out || *out || !modules || modules[0] != '/' || width != OA_SCENE_WIDTH || height != OA_SCENE_HEIGHT) {
        errno = EINVAL; return -1;
    }
    scene_probe *s = calloc(1, sizeof *s); if (!s) return -1; *out = s;
    helper_path = helper; renderer_path = renderer;
    if (oa_host_init(&s->host, modules, NULL)) return -1;
    s->initialized = 1; s->host.log_level = OA_LOG_ERROR; s->host.api.config_get = config;
    oa_mod *module = oa_module_load(&s->host, "graphics");
    if (!module || oa_module_start(&s->host, module)) { s->error = errno; return -1; }
    s->service = s->host.api.lookup(&s->host, OA_SVC_DISPLAY_GPU, 1);
    if (!s->service) { errno = ENODEV; s->error = errno; return -1; }
    uint64_t ticket;
    if (s->service->open(&ticket)) { s->error = errno; return -1; }
    printf("graphics module id=%u service=%s OPEN ticket=%llu\n", module->id, OA_SVC_DISPLAY_GPU, (unsigned long long)ticket);
    return completion(s, ticket);
}
int probe_render(scene_probe *s, const uint8_t *rgba, size_t size, int type, float progress)
{
    if (!s || s->error || !s->service) { errno = s && s->error ? s->error : ENODEV; return -1; }
    int image = -1;
    if (rgba) {
        image = oa_scene_image_create(rgba, size); if (image < 0) return -1;
        s->revision++;
    } else if (size) { errno = EINVAL; return -1; }
    uint64_t ticket;
    int result = s->service->frame(s->revision, (unsigned)type, progress, image, &ticket), primary = errno;
    if (image >= 0) (void)close(image);
    if (result) { s->error = primary; errno = primary; return -1; }
    return completion(s, ticket);
}
int probe_close(scene_probe **out)
{
    if (!out) { errno = EINVAL; return -1; }
    scene_probe *s = *out; if (!s) return 0;
    if (s->initialized) {
        s->service = NULL; s->host.stop_requested = 1;
        if (oa_host_run(&s->host)) return -1;
        close(s->host.selfpipe[0]); close(s->host.selfpipe[1]); pthread_mutex_destroy(&s->host.post_mu);
        s->initialized = 0;
        puts("graphics host drained and unloaded provider");
    }
    free(s); *out = NULL; return 0;
}
void probe_error(const scene_probe *s, char *out, size_t size)
{ if (out && size) snprintf(out, size, "graphics module error=%d", s ? s->error : errno); }
