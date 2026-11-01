/* Provider lifecycle test: ui_fb must discover services that are published after it starts, drop
 * cached pointers when a provider is force-unloaded, and rebind when the provider returns.
 *
 *   nav_display, ui_fb            -> no clock/anim: no slide
 *   fake_clock (ui.clock)         -> clock found, anim still missing
 *   ui_anim (ui.anim v2)          -> provider present but disabled: no slide
 *   enable animation, ticker start-> rebind: slide starts
 *   unload ui_anim --force        -> service/removed: pointers dropped, sampler stopped
 *   load/start ui_anim again      -> rebind: slide starts again
 */
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "host.h"
#include "openaurora/services.h"
#include "openaurora/ui_anim.h"
#include "ui_nav_trace.h"

#define OA_SVC_FAKE_CLOCK "fake.clock"
typedef struct {
    void (*set)(uint32_t now_ms);
    uint32_t (*get)(void);
} oa_fake_clock_iface;

static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[160];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}

static int timer_count(const oa_host *h, uint32_t owner, uint32_t repeat_ms)
{
    int n = 0;
    for (int i = 0; i < MAX_TIMERS; i++)
        if (h->timers[i].active && h->timers[i].owner == owner &&
            h->timers[i].repeat_ms == repeat_ms)
            n++;
    return n;
}

/* Fire one repeating timer (the 10 ms sampler) as the host loop would. */
static int timer_fire_repeat(oa_host *h, uint32_t owner, uint32_t repeat_ms)
{
    for (int i = 0; i < MAX_TIMERS; i++) {
        oa_timer *t = &h->timers[i];
        if (!t->active || t->owner != owner || t->repeat_ms != repeat_ms)
            continue;
        void (*cb)(void *) = t->cb;
        void *ud = t->ud;
        uint32_t prev = h->cur_module;
        h->cur_module = t->owner;
        cb(ud);
        h->cur_module = prev;
        return 1;
    }
    return 0;
}

/* Fire the earliest pending one-shot (repeat_ms == 0) for the module, deactivating it first.
 * Until a slide completes, the only such timer ui_fb owns is the deferred rebind. */
static int timer_fire_oneshot(oa_host *h, uint32_t owner)
{
    oa_timer *best = NULL;
    for (int i = 0; i < MAX_TIMERS; i++) {
        oa_timer *t = &h->timers[i];
        if (!t->active || t->owner != owner || t->repeat_ms != 0)
            continue;
        if (!best || t->due < best->due)
            best = t;
    }
    if (!best) return 0;
    void (*cb)(void *) = best->cb;
    void *ud = best->ud;
    best->active = 0;
    uint32_t prev = h->cur_module;
    h->cur_module = best->owner;
    cb(ud);
    h->cur_module = prev;
    return 1;
}

static int title_shift(const oa_nav_trace *t)
{
    int n = t->blit_count();
    for (int i = 0; i < n; i++) {
        int x = 0, y = 0, w = 0, h = 0, r = -1, g = 0, b = 0, a = 0;
        if (t->blit_at(i, &x, &y, &w, &h) == 0 && y == 80 &&
            t->blit_first(i, &r, &g, &b, &a) == 0)
            return r;
    }
    return -999;
}

int main(int argc, char **argv)
{
    assert(argc == 4);   /* module dir, config file, ui asset dir */
    oa_host host;
    char out[512];

    {
        char path[512];
        snprintf(path, sizeof path, "%s/title.rgba", argv[3]);
        FILE *f = fopen(path, "wb");
        assert(f);
        unsigned char hdr[12] = { 'O', 'A', 'I', '1' };
        unsigned w = 263, h = 52;
        memcpy(hdr + 4, &w, 4);
        memcpy(hdr + 8, &h, 4);
        assert(fwrite(hdr, 1, sizeof hdr, f) == sizeof hdr);
        for (unsigned y = 0; y < h; y++)
            for (unsigned x = 0; x < w; x++) {
                unsigned char px[4] = { (unsigned char)(x & 0xffu), (unsigned char)y, 0, 255 };
                assert(fwrite(px, 1, sizeof px, f) == sizeof px);
            }
        assert(fclose(f) == 0);
    }

    assert(!oa_host_init(&host, argv[1], argv[2]));
    host.log_level = OA_LOG_ERROR;

    assert(!command(&host, "load nav_display", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start nav_display", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_fb", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_fb", out, sizeof out) && !strcmp(out, "ok\n"));

    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    const oa_fake_clock_iface *fake = host.api.lookup(&host, OA_SVC_FAKE_CLOCK, 1);
    oa_mod *fb = oa_host_find_module(&host, "ui_fb");
    assert(trace && fb);

    /* No clock and no anim: nothing may slide. */
    assert(timer_count(&host, fb->id, 10) == 0);
    assert(title_shift(trace) == 0);

    /* Late clock publication is discovered and rebound. */
    assert(!command(&host, "load fake_clock", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start fake_clock", out, sizeof out) && !strcmp(out, "ok\n"));
    fake = host.api.lookup(&host, OA_SVC_FAKE_CLOCK, 1);
    assert(fake);
    assert(timer_fire_oneshot(&host, fb->id));           /* deferred rebind */
    assert(timer_count(&host, fb->id, 10) == 0);         /* still no provider */

    /* Late ui.anim publication: bound, but the controller starts disabled. */
    assert(!command(&host, "load ui_anim", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_anim", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(timer_fire_oneshot(&host, fb->id));
    assert(timer_count(&host, fb->id, 10) == 0);

    const oa_ui_anim_iface *anim = host.api.lookup(&host, OA_SVC_UI_ANIM, 1);
    assert(anim && anim->set_enabled(1) == 0);

    /* Any later lifecycle event re-evaluates the providers and starts the slide. */
    assert(!command(&host, "load ticker", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ticker", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(timer_fire_oneshot(&host, fb->id));
    assert(timer_count(&host, fb->id, 10) == 1);
    fake->set(2300);
    assert(timer_fire_repeat(&host, fb->id, 10));
    assert(title_shift(trace) == 81);

    uint32_t started = 0;
    assert(!anim->start(1, OA_ANIM_DIALOG, NULL, NULL, 1000, &started));
    assert(anim->sample(started + 100) == 1);
    int transform = 0;
    assert(!trace->transform(&transform, NULL, NULL) && transform == 4);

    /* Forced removal of the provider must drop the cached pointer and stop the sampling. */
    assert(!command(&host, "unload ui_anim --force", out, sizeof out) && strstr(out, "ok"));
    assert(timer_fire_oneshot(&host, fb->id));           /* deferred rebind from service/removed */
    assert(!trace->transform(&transform, NULL, NULL) && transform == 0);
    assert(timer_count(&host, fb->id, 10) == 0);         /* no sampler without the provider */
    assert(timer_count(&host, fb->id, 0) == 0);
    assert(title_shift(trace) == 0);                     /* runner cleared, label redrawn at origin */

    /* Provider returns: rebinding starts a fresh pass. */
    assert(!command(&host, "load ui_anim", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_anim", out, sizeof out) && !strcmp(out, "ok\n"));
    anim = host.api.lookup(&host, OA_SVC_UI_ANIM, 1);
    assert(anim && anim->set_enabled(1) == 0);
    assert(!command(&host, "reload ticker", out, sizeof out) && strstr(out, "ok"));
    assert(timer_fire_oneshot(&host, fb->id));
    assert(timer_count(&host, fb->id, 10) == 1);
    assert(timer_fire_repeat(&host, fb->id, 10));
    assert(title_shift(trace) == 0);

    /* Both v1 and v2 aliases must disappear synchronously before dlclose.
     * Suspend calls the v2 transform reset, so it must be safe even before rebind. */
    assert(!command(&host, "unload nav_display --force", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "suspend ui_fb", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(command(&host, "resume ui_fb", out, sizeof out) != 0);
    assert(!command(&host, "load nav_display", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start nav_display", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "resume ui_fb", out, sizeof out) && !strcmp(out, "ok\n"));
    trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    assert(trace && timer_count(&host, fb->id, 10) == 1);

    host.stop_requested = 1;
    assert(oa_host_run(&host) == 0);
    close(host.selfpipe[0]); close(host.selfpipe[1]); pthread_mutex_destroy(&host.post_mu);
    printf("  ok   ui_fb rebinds late providers, drops forced-unloaded ones and resumes\n");
    return 0;
}
