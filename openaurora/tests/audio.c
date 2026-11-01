/* Real module callbacks with a fake HAL: no devices or vendor library loaded. */
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/vendor_audio.h"

extern const oa_module *oa_module_entry(void);
static int calls, fail, ready = 1, errors, enabled = 1, held;
static int last_op;
static uint32_t last_params[OA_AUDIO_PARAM_WORDS];
static const void *published;
static int is_ready(void) { return ready; }
static int stage_errors(void) { return errors; }
static const oa_plat_iface plat = { is_ready, stage_errors };
int Hal_Audio_Ctrl(int opcode, uint32_t *params)
{
    calls++; last_op = opcode; memcpy(last_params, params, sizeof last_params);
    return fail;
}
static void log_msg(void *p, int level, const char *fmt, ...) { (void)p; (void)level; (void)fmt; }
static const void *lookup(void *p, const char *name, uint32_t version)
{
    (void)p;
    assert(version == 1);
    if (!strcmp(name, OA_SVC_PLAT)) { held = 1; return &plat; }
    if (!strcmp(name, OA_SVC_CEC_AUDIO)) return NULL;
    assert(0);
    return NULL;
}
static void release(void *p, const char *name)
{
    (void)p;
    if (!strcmp(name, OA_SVC_PLAT)) { held = 0; return; }
    if (!strcmp(name, OA_SVC_CEC_AUDIO)) return;
    assert(0);
}
static int publish(void *p, const char *name, uint32_t version, const void *iface)
{
    (void)p; assert(!strcmp(name, OA_SVC_AUDIO) && version == 1);
    if (published) return -1;
    published = iface; return 0;
}
static int unpublish(void *p, const char *name) { (void)p; assert(!strcmp(name, OA_SVC_AUDIO)); published = NULL; return 0; }
static const char *config(void *p, const char *key, const char *def)
{
    (void)p;
    return !strcmp(key, "audio.enable") ? (enabled ? "1" : "0") : def;
}
static const oa_host_api api = {
    .size = sizeof(oa_host_api), .abi = OA_ABI_VERSION, .log = log_msg,
    .lookup = lookup, .release = release, .publish = publish, .unpublish = unpublish,
    .config_get = config
};
static int threads(void)
{
    FILE *file = fopen("/proc/self/status", "r");
    assert(file);
    char line[80];
    int count = -1;
    while (fgets(line, sizeof line, file))
        if (sscanf(line, "Threads: %d", &count) == 1) break;
    fclose(file);
    return count;
}
static void wait_threads(int want)
{
    for (int i = 0; i < 100; i++) {
        if (threads() == want) return;
        sched_yield();
    }
    assert(threads() == want);
}
static int descriptors(void)
{
    DIR *dir = opendir("/proc/self/fd");
    assert(dir);
    int count = 0;
    while (readdir(dir)) count++;
    closedir(dir);
    return count;
}
static void check_request(int op, uint32_t a, uint32_t b)
{
    assert(last_op == op && last_params[0] == a && last_params[1] == b);
    for (unsigned i = 2; i < OA_AUDIO_PARAM_WORDS; i++) assert(last_params[i] == 0);
}
static void stop(const oa_module *m, void *c)
{
    int done = 0;
    for (int i = 0; i < 1000; i++) {
        if (!m->quiesce || m->quiesce(c) == 0) { done = 1; break; }
        struct timespec pause = { .tv_nsec = 1000000 };
        nanosleep(&pause, NULL);
    }
    assert(done);
    m->stop(c);
}
int main(void)
{
    const oa_module *m = oa_module_entry(); void *c = NULL;
    assert(!m->init(&api, &c) && !m->start(c) && held && calls == 0);
    const oa_audio_iface *audio = published; assert(audio && audio->is_available());
    int open_fds = descriptors();
    errno = 0;
    assert(audio->bind_session(NULL) == -1 && errno == ENOTSUP && descriptors() == open_fds);
    assert(m->suspend(c) == -1);
    assert(audio->get_volume() == -1);
    unsigned char unknown[2]; assert(m->save(c, unknown, 2) == 2 && unknown[0] == 255 && unknown[1] == 255);
    assert(!audio->set_volume(53) && audio->get_volume() == 53); check_request(0x33, 53, 0);
    assert(!audio->set_volume(500) && audio->get_volume() == 100); check_request(0x33, 100, 0);
    assert(!audio->set_volume(-1) && audio->get_volume() == 0); check_request(0x33, 0, 0);
    assert(!audio->set_mute(1)); check_request(0x35, 0, 1);
    fail = -7;
    assert(audio->set_volume(40) == -7 && audio->get_volume() == 0);
    assert(audio->set_mute(0) == -7);
    unsigned char state[2]; assert(m->save(c, state, sizeof state) == 2 && state[0] == 0 && state[1] == 1);
    fail = 0;
    stop(m, c); assert(!published && !held && !audio->is_available()); m->destroy(c);
    puts("  ok   verified HAL request blocks, clamping, scalar failures and unchanged cached state on failure");

    assert(!m->init(&api, &c)); state[0] = 35;
    assert(!m->restore(c, state, sizeof state));
    int n = calls; assert(!m->start(c) && calls == n + 2);
    audio = published; assert(audio->get_volume() == 35); check_request(0x35, 0, 1);
    const unsigned char invalid[][2] = { {101,0}, {1,2} };
    for (unsigned i = 0; i < 2; i++) assert(m->restore(c, invalid[i], 2) == -1);
    assert(m->restore(c, state, 1) == -1 && m->restore(c, NULL, 2) == -1);
    unsigned char after[2]; assert(m->save(c, after, 2) == 2 && !memcmp(state, after, 2));
    stop(m, c); m->destroy(c);
    puts("  ok   state handoff reapplies volume/mute; invalid snapshots reject atomically");

    assert(!m->init(&api, &c) && !m->restore(c, unknown, 2)); n = calls;
    assert(!m->start(c) && calls == n && ((const oa_audio_iface *)published)->get_volume() == -1);
    stop(m, c); m->destroy(c);
    puts("  ok   unknown volume/mute snapshot never applies guessed zero defaults");

    int peer = 0; published = &peer; n = calls;
    assert(!m->init(&api, &c) && m->start(c) == -1);
    stop(m, c); assert(published == &peer && calls == n && !held); m->destroy(c); published = NULL;
    for (unsigned i = 0; i < 2; i++) {
        ready = i == 0 ? 0 : 1; errors = i == 1 ? 1 : 0;
        assert(!m->init(&api, &c) && m->start(c) == -1);
        stop(m, c); m->destroy(c); assert(!published && calls == n && !held);
    }
    ready = 1; errors = 0; enabled = 0;
    int before = threads();
    assert(!m->init(&api, &c) && !m->start(c)); audio = published;
    assert(!audio->is_available() && audio->set_volume(12) == -1 && calls == n);
    wait_threads(before + 1);
    open_fds = descriptors();
    errno = 0;
    assert(audio->bind_session(NULL) == -1 && errno == ENOTSUP && descriptors() == open_fds);
    int suspended = -1;
    for (int i = 0; i < 1000 && suspended; i++) {
        suspended = m->suspend(c);
        if (suspended) {
            struct timespec pause = { .tv_nsec = 1000000 };
            nanosleep(&pause, NULL);
        }
    }
    assert(!suspended);
    wait_threads(before);
    assert(!m->resume(c));
    wait_threads(before + 1);
    stop(m, c);
    wait_threads(before);
    m->destroy(c);
    puts("  ok   publication collision preserves peer; failed platform and disabled audio never call HAL");
    puts("  ok   missing session I/O fails before a socket; enabled HAL refuses suspend; idle owner joins");
    return 0;
}
