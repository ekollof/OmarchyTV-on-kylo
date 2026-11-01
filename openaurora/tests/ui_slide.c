/* Text-slide caller test: drives the shipped mod_ui_fb with a fake display recorder, a settable
 * ui.clock and the shipped mod_ui_anim, and checks the vendor sequence:
 *
 *   overflow label (isTextSlidingNeeded: distance = text_width - box_width)
 *     -> slide starts, margin 0 through the 800 ms pause
 *     -> margin follows AnimationHelper::calculateSlidingMargin
 *     -> at completion the 2000 ms one-shot repeat is armed (onAnimationProgress 0x10c8e48)
 *     -> the repeat restarts the slide from 0
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

static const oa_timer *find_timer(const oa_host *h, uint32_t owner, uint32_t repeat_ms)
{
    for (int i = 0; i < MAX_TIMERS; i++)
        if (h->timers[i].active && h->timers[i].owner == owner &&
            h->timers[i].repeat_ms == repeat_ms)
            return &h->timers[i];
    return NULL;
}

static int title_x(const oa_nav_trace *t)
{
    /* the title blob is the only one drawn at y == 80 */
    int n = t->blit_count();
    for (int i = 0; i < n; i++) {
        int x = 0, y = 0, w = 0, h = 0;
        if (t->blit_at(i, &x, &y, &w, &h) == 0 && y == 80)
            return x;
    }
    return -999;
}

int main(int argc, char **argv)
{
    assert(argc == 4);   /* module dir, config file (ui.slide_box set), ui asset dir */
    oa_host host;
    char out[512];

    /* synthetic title blob: 263 px wide (the parity asset's width), transparent */
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
        for (unsigned i = 0; i < w * h; i++) {
            unsigned char px[4] = { 0, 0, 0, 0 };
            assert(fwrite(px, 1, sizeof px, f) == sizeof px);
        }
        assert(fclose(f) == 0);
    }

    assert(!oa_host_init(&host, argv[1], argv[2]));
    host.log_level = OA_LOG_ERROR;

    assert(!command(&host, "load fake_clock", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start fake_clock", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load nav_display", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start nav_display", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_anim", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_anim", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_fb", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_fb", out, sizeof out) && !strcmp(out, "ok\n"));

    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    const oa_fake_clock_iface *fake = host.api.lookup(&host, OA_SVC_FAKE_CLOCK, 1);
    oa_mod *fb = oa_host_find_module(&host, "ui_fb");
    assert(trace && fake && fb);

    /* box 100 px against the 263 px title => distance 163, duration 1600 + 3*1000 = 4600 ms */
    fake->set(0);
    assert(title_x(trace) == 29);                       /* t=0: no margin yet */

    const oa_timer *tick = find_timer(&host, fb->id, 10);
    assert(tick);                                        /* 10 ms sampler is running */
    /* the host records registrations against cur_module; call callbacks as the module */
    host.cur_module = fb->id;
    tick->cb(tick->ud);

    fake->set(800);                                      /* still inside the 800 ms pause */
    tick->cb(tick->ud);
    assert(title_x(trace) == 29);

    fake->set(2300);                                     /* half of the 3000 ms movement span */
    tick->cb(tick->ud);
    assert(title_x(trace) == 29 - 81);                   /* (int)(163 * 0.5) */

    fake->set(4600);                                     /* duration reached */
    tick->cb(tick->ud);
    assert(title_x(trace) == 29 - 163);                  /* vendor ends at distance */
    assert(!find_timer(&host, fb->id, 10));              /* sampler stopped */

    const oa_timer *repeat = find_timer(&host, fb->id, 0);
    assert(repeat);                                      /* the 2000 ms one-shot is armed */
    /* due uses the host clock, so compare against it rather than the fake test clock */
    uint64_t due_in = repeat->due > oa_now_ms() ? repeat->due - oa_now_ms() : 0;
    assert(repeat->repeat_ms == 0);
    assert(due_in <= OA_ANIM_SLIDE_REPEAT_MS && due_in + 50 >= OA_ANIM_SLIDE_REPEAT_MS);

    fake->set(4600 + OA_ANIM_SLIDE_REPEAT_MS);
    repeat->cb(repeat->ud);                              /* repeat fires: slide restarts */
    const oa_timer *tick2 = find_timer(&host, fb->id, 10);
    assert(tick2);                                       /* sampler running again */
    tick2->cb(tick2->ud);                                /* first sample of the new pass */
    assert(title_x(trace) == 29);
    host.cur_module = 0;

    host.stop_requested = 1;
    assert(oa_host_run(&host) == 0);
    close(host.selfpipe[0]); close(host.selfpipe[1]); pthread_mutex_destroy(&host.post_mu);
    printf("  ok   text slide follows AnimationHelper margins and repeats after %d ms\n",
           OA_ANIM_SLIDE_REPEAT_MS);
    return 0;
}
