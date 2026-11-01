#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "host.h"
#include "openaurora/events.h"
#include "openaurora/ui_engine.h"
#include "scene_gpu_trace.h"
#define require(expression) do { if (!(expression)) { \
    fprintf(stderr, "%s:%d: %s errno=%d\n", __FILE__, __LINE__, #expression, errno); exit(1); } } while (0)
static char directory[128], ready_path[160];
static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "ui.dir")) return directory; /* missing assets exercise ordinary fallback drawing */
    if (!strcmp(key, "ui.ok_file")) return ready_path;
    if (!strcmp(key, "ui.fps")) return "0";
    if (!strcmp(key, "ui.launch_anim")) return "1"; /* lab path: vendor launcher is instant */
    return def;
}
static void pump(oa_host *h)
{
    for (unsigned i = 0; i < MAX_TIMERS; i++) if (h->timers[i].active) {
        oa_timer t = h->timers[i];
        if (!t.repeat_ms) h->timers[i].active = 0;
        h->cur_module = t.owner; t.cb(t.ud); h->cur_module = 0;
    }
}
static oa_mod *load(oa_host *h, const char *name)
{ oa_mod *m = oa_module_load(h, name); require(m && !oa_module_start(h, m)); return m; }
static int timers(oa_host *h, uint32_t owner)
{
    int n = 0;
    for (unsigned i = 0; i < MAX_TIMERS; i++) n += h->timers[i].active && h->timers[i].owner == owner && h->timers[i].repeat_ms == 10;
    return n;
}
static int held(oa_host *h, uint32_t owner, const char *name)
{
    for (unsigned i = 0; i < MAX_HOLDS; i++)
        if (h->holds[i].used && h->holds[i].holder == owner && !strcmp(h->holds[i].name, name)) return 1;
    return 0;
}
static uint64_t (*real_timer_add)(void *, uint32_t, uint32_t, void (*)(void *), void *);
static uint32_t failed_owner;
static uint64_t timer_add(void *ud, uint32_t delay, uint32_t repeat, void (*cb)(void *), void *ctx)
{ oa_host *h = ud; return h->cur_module == failed_owner ? 0 : real_timer_add(ud, delay, repeat, cb, ctx); }
static void timeout_handler(int sig) { (void)sig; _exit(124); }
static void cached_dialog(const char *modules, const char *fake_clock)
{
    oa_host h; require(!oa_host_init(&h, modules, NULL)); h.log_level = OA_LOG_ERROR; h.api.config_get = config;
    (void)load(&h, "scene_gpu_fixture"); (void)load(&h, "scene_display");
    (void)load(&h, "ui_engine");
    (void)load(&h, fake_clock); (void)load(&h, "ui_anim"); (void)load(&h, "ui_vendor");
    const scene_gpu_trace *t = h.api.lookup(&h, SCENE_GPU_TRACE, 1);
    const oa_ui_anim_iface *anim = h.api.lookup(&h, OA_SVC_UI_ANIM, 2);
    typedef struct { void (*set)(uint32_t); uint32_t (*get)(void); } clock_control;
    const clock_control *clock = h.api.lookup(&h, "fake.clock", 1);
    require(t && anim && clock && !t->settle(0)); pump(&h); require(!t->settle(0)); pump(&h);
    oa_display_result last; unsigned uploads, frames;
    require(!t->stats(&last, &uploads, &frames) && uploads == 1);
    uint64_t revision = last.revision;
    require(!anim->set_enabled(1));
    oa_key_event key = {.id = 53, .kind = OA_KEY_PRESS}; h.api.emit(&h, OA_TOPIC_KEY, &key);
    oa_host_dispatch_posts(&h);
    uint32_t started;
    require(!anim->get(1, NULL, NULL, NULL, &started, NULL));
    clock->set(started+500); require(anim->sample(started+500) == 1);
    require(!t->settle(0)); pump(&h);
    require(!t->stats(&last, &uploads, &frames) && uploads == 1 && last.revision == revision && last.type == 4 && last.progress == 0.875f);
    clock->set(started+1000); require(anim->sample(started+1000) == 0);
    require(!t->settle(0)); pump(&h);
    require(!t->stats(&last, &uploads, &frames) && uploads == 2 && last.revision > revision && last.type == 0);
    require(!t->settle(0)); pump(&h);
    h.stop_requested = 1; require(!oa_host_run(&h));
    close(h.selfpipe[0]); close(h.selfpipe[1]); pthread_mutex_destroy(&h.post_mu);
    (void)unlink(ready_path);
    puts("  ok   dialog progress reuses immutable pixels; cubic midpoint and final identity/new screen are completed");
}
int main(int argc, char **argv)
{
    require(argc == 3 && signal(SIGALRM, timeout_handler) != SIG_ERR); alarm(30);
    snprintf(directory, sizeof directory, "/tmp/oa-ui-async-XXXXXX"); require(mkdtemp(directory));
    snprintf(ready_path, sizeof ready_path, "%s/ready", directory);
    oa_host h; require(!oa_host_init(&h, argv[1], NULL)); h.log_level = OA_LOG_ERROR; h.api.config_get = config;
    (void)load(&h, "scene_gpu_fixture"); (void)load(&h, "scene_display"); (void)load(&h, "ui_engine"); oa_mod *ui = load(&h, "ui_vendor");
    oa_mod *eng = oa_host_find_module(&h, "ui_engine"); require(eng);
    const scene_gpu_trace *t = h.api.lookup(&h, SCENE_GPU_TRACE, 1); require(t);
    require(timers(&h, eng->id) == 1 && held(&h, eng->id, OA_SVC_DISPLAY) && held(&h, eng->id, OA_SVC_DISPLAY_ASYNC));
    require(held(&h, eng->id, OA_SVC_DISPLAY_ALPHA) && held(&h, ui->id, OA_SVC_UI_ENGINE));
    require(access(ready_path, F_OK) == -1);
    require(!t->settle(0)); pump(&h); require(access(ready_path, F_OK) == -1); /* OPEN isn't a frame */
    unsigned char backdrop[4];
    require(!t->pixel(1919, 0, backdrop));
    require(backdrop[0] == 36 && backdrop[1] == 38 && backdrop[2] == 40 && backdrop[3] == 153);
    require(!t->settle(EIO)); pump(&h); require(access(ready_path, F_OK) == -1);
    require(!oa_module_unload(&h, "ui_vendor", 0) && !oa_module_unload(&h, "scene_display", 1));
    (void)load(&h, "scene_display"); ui = load(&h, "ui_vendor");
    pump(&h);                         /* let the engine rebind the new display provider */
    require(access(ready_path, F_OK) == -1);
    /* A newer paint is queued while the first is in flight: readiness may use
     * that first actual UI completion and must not chase the newest serial. */
    oa_key_event key = {.id = 19, .kind = OA_KEY_PRESS}; h.api.emit(&h, OA_TOPIC_KEY, &key);
    oa_host_dispatch_posts(&h);
    require(!t->settle(0)); pump(&h); require(!access(ready_path, F_OK));
    FILE *file = fopen(ready_path, "r"); long pid; int w, height;
    require(file && fscanf(file, "%ld %dx%d", &pid, &w, &height) == 3 && !fclose(file));
    require(pid == (long)getpid() && w == 1920 && height == 1080);
    puts("  ok   queue/OPEN/error never acknowledge; actual coalesced UI completion writes the PID/dimensions receipt");
    require(!unlink(ready_path));
    require(!oa_module_reload(&h, "ui_vendor", 0)); ui = oa_host_find_module(&h, "ui_vendor"); require(ui);
    pump(&h);
    /* The engine keeps the receipt timer across a skin reload, so the new
     * skin's first completed frame may acknowledge immediately. Any receipt
     * here must belong to this process; pre-reset completions are excluded by
     * the reset_ready serial epoch. */
    if (access(ready_path, F_OK) == 0) {
        FILE *f = fopen(ready_path, "r"); long pid; int w, height;
        require(f && fscanf(f, "%ld %dx%d", &pid, &w, &height) == 3 && !fclose(f));
        require(pid == (long)getpid() && w == 1920 && height == 1080);
        require(!unlink(ready_path));
    }
    (void)t->settle(0); pump(&h);
    (void)t->settle(0); pump(&h);
    (void)unlink(ready_path);
    require(!oa_module_suspend_all(&h));
    require(!timers(&h, eng->id) && !held(&h, eng->id, OA_SVC_DISPLAY_ASYNC) && !held(&h, eng->id, OA_SVC_DISPLAY));
    require(!held(&h, eng->id, OA_SVC_DISPLAY_ALPHA));
    require(!held(&h, ui->id, OA_SVC_UI_ENGINE));
    real_timer_add = h.api.timer_add; failed_owner = eng->id; h.api.timer_add = timer_add;
    require(oa_module_resume_all(&h) == -1 && eng->state == OA_ST_SUSPENDED);
    require(!held(&h, eng->id, OA_SVC_DISPLAY_ASYNC) && !held(&h, eng->id, OA_SVC_DISPLAY));
    require(!held(&h, eng->id, OA_SVC_DISPLAY_ALPHA));
    h.api.timer_add = real_timer_add;
    require(!oa_module_resume_all(&h) && timers(&h, eng->id) == 1);
    require(held(&h, eng->id, OA_SVC_DISPLAY_ALPHA));
    /* A resumed already-acknowledged UI need not rewrite a process-start receipt. */
    require(!t->settle(0)); pump(&h); require(!t->settle(0)); pump(&h);
    puts("  ok   old receipts cannot acknowledge a reloaded UI; suspend/failed resume release both holds and timer");
    require(oa_module_unload(&h, "scene_display", 0) == -1);
    require(!oa_module_unload(&h, "scene_display", 1)); pump(&h);
    require(!timers(&h, eng->id));
    (void)load(&h, "scene_display"); pump(&h);
    require(timers(&h, eng->id) == 1 && held(&h, eng->id, OA_SVC_DISPLAY_ASYNC));
    require(!t->settle(0)); pump(&h);
    h.stop_requested = 1; require(!oa_host_run(&h));
    close(h.selfpipe[0]); close(h.selfpipe[1]); pthread_mutex_destroy(&h.post_mu);
    (void)unlink(ready_path);
    puts("  ok   forced provider removal clears aliases/callbacks; returning provider rebinds without stale service use");
    cached_dialog(argv[1], argv[2]); require(!rmdir(directory));
    return 0;
}
