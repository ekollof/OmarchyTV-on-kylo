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
#include "openaurora/events.h"
#include "scene-probe-client.h"
struct scene_probe {
    oa_host host;
    const oa_display_gpu_iface *service;
    const oa_display_iface *display;
    const oa_display_async_iface *asynchronous;
    uint64_t revision;
    uint64_t last_completion;
    unsigned completions;
    int initialized, error;
};
static const char *helper_path, *renderer_path;
static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "graphics.broker")) return helper_path;
    if (!strcmp(key, "graphics.renderer")) return renderer_path;
    if (!strcmp(key, "ui.dir")) {
        const char *assets = getenv("OA_SCENE_PROBE_UI_DIR");
        return assets ? assets : "/buffer/openaurora/ui";
    }
    if (!strcmp(key, "ui.ok_file")) return "/tmp/oa-scene-helper-20261101/ui.ready";
    if (!strcmp(key, "ui.anim_enable")) return "1"; /* explicit lab bootstrap, not recovered vendor policy */
    if (!strcmp(key, "ui.fps")) return "0";
    if (!strcmp(key, "ui.launch_anim")) {
        const char *lab = getenv("OA_SCENE_PROBE_LAUNCH_ANIM");
        return lab && !strcmp(lab, "1") ? "1" : "0";
    }
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
        int taken, error = 0;
        if (s->asynchronous) {
            oa_display_result result;
            taken = s->asynchronous->latest(&result);
            if (taken == 1) {
                if (result.serial != s->last_completion) { s->last_completion = result.serial; s->completions++; }
                if (result.serial < ticket) taken = 0;
                else if (result.serial != ticket) { errno = EPROTO; taken = -1; }
                else error = result.error;
            }
        } else {
            oa_scene_reply reply;
            taken = s->service->take(ticket, &reply);
            if (taken == 1) error = reply.error;
        }
        if (taken == 1) { if (!error) return 0; errno = error; break; }
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
    const char *canvas = getenv("OA_SCENE_PROBE_ASYNC");
    if (canvas && !strcmp(canvas, "1")) {
        module = oa_module_load(&s->host, "scene_display");
        if (!module || oa_module_start(&s->host, module)) { s->error = errno; return -1; }
        s->display = s->host.api.lookup(&s->host, OA_SVC_DISPLAY, 2);
        s->asynchronous = s->host.api.lookup(&s->host, OA_SVC_DISPLAY_ASYNC, 1);
        if (!s->display || !s->asynchronous) { errno = ENODEV; s->error = errno; return -1; }
        printf("scene display module id=%u service=%s\n", module->id, OA_SVC_DISPLAY_ASYNC);
        return 0; /* OPEN/window initialization is asynchronous */
    }
    s->service = s->host.api.lookup(&s->host, OA_SVC_DISPLAY_GPU, 1);
    if (!s->service) { errno = ENODEV; s->error = errno; return -1; }
    uint64_t ticket;
    if (s->service->open(&ticket)) { s->error = errno; return -1; }
    printf("graphics module id=%u service=%s OPEN ticket=%llu\n", module->id, OA_SVC_DISPLAY_GPU, (unsigned long long)ticket);
    return completion(s, ticket);
}
int probe_render(scene_probe *s, const uint8_t *rgba, size_t size, int type, float progress)
{
    if (!s || s->error || (!s->service && !s->asynchronous)) { errno = s && s->error ? s->error : ENODEV; return -1; }
    if (s->asynchronous) {
        uint64_t serial;
        if ((rgba && (size != OA_SCENE_IMAGE_BYTES || s->display->blit(0, 0, OA_SCENE_WIDTH, OA_SCENE_HEIGHT, rgba))) ||
            (!rgba && size) || s->display->set_scene_transform(type, progress) || s->asynchronous->request(&serial)) {
            s->error = errno ? errno : EINVAL; errno = s->error; return -1;
        }
        return completion(s, serial);
    }
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
        /* Grid-mode completion() owns this counter. UI mode samples receipts
         * inside ui_vendor instead; a zero grid counter is not a UI frame count. */
        if (s->asynchronous && s->completions) printf("scene display observed frame completions=%u\n", s->completions);
        s->service = NULL; s->display = NULL; s->asynchronous = NULL; s->host.stop_requested = 1;
        if (oa_host_run(&s->host)) return -1;
        close(s->host.selfpipe[0]); close(s->host.selfpipe[1]); pthread_mutex_destroy(&s->host.post_mu);
        s->initialized = 0;
        puts("graphics host drained and unloaded provider");
    }
    free(s); *out = NULL; return 0;
}
int probe_coalesce(scene_probe *s)
{
    if (!s || !s->asynchronous) return 0;
    uint64_t first = 0, last = 0;
    for (unsigned i = 0; i <= 100; i++) {
        if (s->display->set_scene_transform(i == 100 ? 0 : 4, (float)i/100) ||
            s->asynchronous->request(&last)) { s->error = errno; return -1; }
        if (!i) first = last;
    }
    if (completion(s, last)) return -1;
    oa_display_result receipt;
    if (s->asynchronous->latest(&receipt) != 1 || receipt.serial != last || receipt.type != 0 || receipt.error) {
        s->error = EPROTO; errno = EPROTO; return -1;
    }
    printf("coalesced101 desired frames first=%llu final=%llu; final identity receipt=%llu\n",
        (unsigned long long)first, (unsigned long long)last, (unsigned long long)receipt.serial);
    return 0;
}
static int load_ui_module(scene_probe *s, const char *name)
{
    oa_mod *m = oa_module_load(&s->host, name);
    return m && !oa_module_start(&s->host, m) ? 0 : -1;
}
static void run_for(scene_probe *s, unsigned ms)
{
    uint64_t until = oa_now_ms()+ms;
    do {
        pump(s);
        struct timespec delay = {.tv_nsec = 1000000}; (void)nanosleep(&delay, NULL);
    } while (oa_now_ms() < until);
}
static int ui_ready(scene_probe *s)
{
    const char *path = "/tmp/oa-scene-helper-20261101/ui.ready";
    uint64_t deadline = oa_now_ms()+5000;
    while (access(path, F_OK)) {
        if (oa_now_ms() >= deadline) { errno = ETIMEDOUT; return -1; }
        run_for(s, 1);
    }
    FILE *file = fopen(path, "r"); long pid; int width, height;
    if (!file) return -1;
    int good = fscanf(file, "%ld %dx%d", &pid, &width, &height) == 3;
    if (fclose(file)) good = 0;
    if (!good || pid != (long)getpid() || width != 1920 || height != 1080) { errno = EPROTO; return -1; }
    oa_display_result result;
    if (s->asynchronous->latest(&result) != 1 || result.error) { errno = EPROTO; return -1; }
    printf("UI readiness after GPU completion serial=%llu revision=%llu pid=%ld\n",
        (unsigned long long)result.serial, (unsigned long long)result.revision, pid);
    return 0;
}
static void key(scene_probe *s, unsigned char id)
{ oa_key_event event = {.id = id, .kind = OA_KEY_PRESS}; s->host.api.emit(&s->host, OA_TOPIC_KEY, &event); }
static int dialog_complete(scene_probe *s, const oa_portal_iface *portal, uint64_t previous)
{
    uint64_t deadline = oa_now_ms()+5000, animated = 0;
    const char *lab = getenv("OA_SCENE_PROBE_LAUNCH_ANIM");
    int needs_animation = lab && !strcmp(lab, "1");
    do {
        run_for(s, 1);
        oa_display_result result;
        if (s->asynchronous->latest(&result) == 1 && result.serial > previous) {
            if (result.error) { errno = result.error; return -1; }
            if (result.type == 4) animated = result.serial;
            if ((!needs_animation || (animated && result.serial > animated)) &&
                result.type == 0 && portal->screen() == 1) {
                printf("UI selection mode=%s animation receipt=%llu completed screen=1 identity receipt=%llu\n",
                    needs_animation ? "lab" : "vendor-instant", (unsigned long long)animated,
                    (unsigned long long)result.serial);
                return 0;
            }
        }
    } while (oa_now_ms() < deadline);
    errno = ETIMEDOUT; return -1;
}
int probe_ui(scene_probe *s)
{
    const char *enabled = getenv("OA_SCENE_PROBE_UI");
    if (!enabled || strcmp(enabled, "1")) return 0;
    if (!s || !s->asynchronous) { errno = EINVAL; return -1; }
    const char *ready = "/tmp/oa-scene-helper-20261101/ui.ready";
    if (!access(ready, F_OK)) { errno = EEXIST; return -1; }
    if (load_ui_module(s, "ticker") || load_ui_module(s, "ui_clock") ||
        load_ui_module(s, "ui_anim") || load_ui_module(s, "ui_engine") ||
        load_ui_module(s, "ui_vendor")) goto failure;
    if (!access(ready, F_OK)) { errno = EPROTO; goto failure; } /* queue cannot acknowledge */
    if (ui_ready(s)) goto failure;
    const oa_portal_iface *portal = s->host.api.lookup(&s->host, OA_SVC_PORTAL, 1);
    if (!portal || portal->screen() != 0) { errno = EPROTO; goto failure; }
    /* With real rail assets, require multiple completed image revisions. This
     * observes actual GPU work, not just the 30 ms repaint timer firing. */
    const char *rail = getenv("OA_SCENE_PROBE_RAIL");
    oa_display_result result;
    if (rail && !strcmp(rail, "1")) {
        uint64_t until = oa_now_ms()+2000, revision = 0;
        unsigned changes = 0;
        do {
            run_for(s, 1);
            if (s->asynchronous->latest(&result) == 1) {
                if (result.error) { errno = result.error; goto failure; }
                if (result.revision != revision) { revision = result.revision; changes++; }
            }
        } while (oa_now_ms() < until);
        if (changes < 3) { errno = EPROTO; goto failure; }
        printf("UI rail completed image revisions=%u over2000ms (completion-paced; not a frame-order measurement)\n", changes);
        run_for(s, 600); /* hold the last frame so the panel can see it before a focus key */
    }
    /* Step-hold: freeze the actual dialog content at p=0.5 and p=1 for panel viewing. No pump
     * during the hold, so the last presented frame stays; the vendor UI is left running. */
    const char *hold = getenv("OA_SCENE_PROBE_HOLD");
    if (hold && !strcmp(hold, "1")) {
        key(s, 53);
        run_for(s, 150);                         /* first progress callback draws the dialog */
        const oa_ui_anim_iface *anim = s->host.api.lookup(&s->host, OA_SVC_UI_ANIM, 1);
        if (!anim || anim->cancel(1) || !s->display || !s->asynchronous) {
            errno = EPROTO; goto failure;
        }
        /* The vendor Aurora repaints fb1, so keep re-presenting our dialog frame with a fixed
         * transform. The animation entry is gone, so nothing advances progress. */
        printf("HOLD dialog p=0.5 (type4) for 3s\n"); fflush(stdout);
        for (int frame = 0; frame < 60;) {
            uint64_t serial;
            if (s->display->set_scene_transform(4, 0.5f)) { errno = EPROTO; goto failure; }
            if (s->asynchronous->request(&serial) == 0) frame++;
            run_for(s, 50);
        }
        printf("HOLD dialog p=1 for 3s\n"); fflush(stdout);
        for (int frame = 0; frame < 60;) {
            uint64_t serial;
            if (s->display->set_scene_transform(4, 1.0f)) { errno = EPROTO; goto failure; }
            if (s->asynchronous->request(&serial) == 0) frame++;
            run_for(s, 50);
        }
        if (s->display->set_scene_transform(0, 0.0f)) { errno = EPROTO; goto failure; }
        key(s, 10); run_for(s, 200);
        printf("HOLD done\n"); fflush(stdout);
    }
    key(s, 19); run_for(s, 200);
    if (s->asynchronous->latest(&result) != 1) { errno = EPROTO; goto failure; }
    uint64_t before_dialog = result.serial;
    key(s, 53);
    if (dialog_complete(s, portal, before_dialog)) goto failure;
    key(s, 10); run_for(s, 200);
    if (portal->screen() != 0 || unlink(ready)) { errno = EPROTO; goto failure; }
    portal = NULL; /* drop before DSO reload */
    if (oa_module_reload(&s->host, "ui_vendor", 0)) goto failure;
    if (!access(ready, F_OK)) { errno = EPROTO; goto failure; }
    if (ui_ready(s)) goto failure;
    printf("UI reload id=%u completed with fresh receipt\n", oa_host_find_module(&s->host, "ui_vendor")->id);
    if (s->asynchronous->latest(&result) != 1) goto failure;
    uint64_t previous = result.serial, deadline = oa_now_ms()+5000;
    while (oa_module_suspend_all(&s->host)) {
        if (oa_now_ms() >= deadline) { errno = ETIMEDOUT; goto failure; }
        run_for(s, 1);
    }
    if (oa_module_resume_all(&s->host)) goto failure;
    deadline = oa_now_ms()+5000;
    do {
        run_for(s, 1);
        if (s->asynchronous->latest(&result) == 1 && result.serial > previous && !result.error) break;
        if (oa_now_ms() >= deadline) { errno = ETIMEDOUT; goto failure; }
    } while (1);
    printf("UI host-only suspend/resume completed receipt=%llu\n", (unsigned long long)result.serial);
    return 1;
failure:
    s->error = errno ? errno : EIO; errno = s->error; return -1;
}
void probe_error(const scene_probe *s, char *out, size_t size)
{ if (out && size) snprintf(out, size, "graphics module error=%d", s ? s->error : errno); }
