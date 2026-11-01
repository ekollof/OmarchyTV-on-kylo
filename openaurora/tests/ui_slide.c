/* Text-slide caller test: drives the shipped mod_ui_fb with a fake display recorder, a settable
 * ui.clock and the shipped mod_ui_anim, and checks the vendor sequence:
 *
 *   overflow label (isTextSlidingNeeded: distance = text_width - box_width)
 *     -> slide starts, margin 0 through the 800 ms pause
 *     -> margin follows AnimationHelper::calculateSlidingMargin
 *     -> the single 2000 ms repeat is armed once progress passes 0.99
 *     -> the pass ends at distance, the label restarts at margin 0 when the repeat fires
 *
 * The recorder keeps the first source pixel of each blit; the synthetic title encodes its column
 * in the red channel, so the test can see the clipped source window rather than just a dest x.
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

/* Mirror fire_due_timers: one-shots are deactivated before the callback, repeating timers stay
 * active, and cur_module is the owner for the duration of the call. */
static int timer_fire(oa_host *h, uint32_t owner, uint32_t repeat_ms)
{
    for (int i = 0; i < MAX_TIMERS; i++) {
        oa_timer *t = &h->timers[i];
        if (!t->active || t->owner != owner || t->repeat_ms != repeat_ms)
            continue;
        void (*cb)(void *) = t->cb;
        void *ud = t->ud;
        if (!t->repeat_ms)
            t->active = 0;
        uint32_t prev = h->cur_module;
        h->cur_module = t->owner;
        cb(ud);
        h->cur_module = prev;
        return 1;
    }
    return 0;
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

/* The title blob is the only one drawn at y == 80. Its source column is in the red channel. */
static int title_x(const oa_nav_trace *t)
{
    int n = t->blit_count();
    for (int i = 0; i < n; i++) {
        int x = 0, y = 0, w = 0, h = 0;
        if (t->blit_at(i, &x, &y, &w, &h) == 0 && y == 80)
            return x;
    }
    return -999;
}

static int title_w(const oa_nav_trace *t)
{
    int n = t->blit_count();
    for (int i = 0; i < n; i++) {
        int x = 0, y = 0, w = 0, h = 0;
        if (t->blit_at(i, &x, &y, &w, &h) == 0 && y == 80)
            return w;
    }
    return -999;
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
    assert(argc == 4);   /* module dir, config file (ui.slide_box set), ui asset dir */
    oa_host host;
    char out[512];

    /* Synthetic title: 263 px wide (the parity asset's width), red channel = column. */
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

    assert(!command(&host, "load fake_clock", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start fake_clock", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load nav_display", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start nav_display", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_anim", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_anim", out, sizeof out) && !strcmp(out, "ok\n"));

    /* The vendor starts the effect only when animation is enabled; ui.anim starts disabled. */
    const oa_ui_anim_iface *anim = host.api.lookup(&host, OA_SVC_UI_ANIM, 1);
    assert(anim && anim->set_enabled(1) == 0);

    assert(!command(&host, "load ui_engine", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_engine", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_fb", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_fb", out, sizeof out) && !strcmp(out, "ok\n"));

    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    const oa_fake_clock_iface *fake = host.api.lookup(&host, OA_SVC_FAKE_CLOCK, 1);
    oa_mod *fb = oa_host_find_module(&host, "ui_fb");
    assert(trace && fake && fb);

    /* box 100 px against the 263 px title => distance 163, duration 1600 + 3*1000 = 4600 ms */
    fake->set(0);
    assert(title_x(trace) == 29);                        /* box origin */
    assert(title_w(trace) == 100);                       /* clipped to the box, not the label */
    assert(title_shift(trace) == 0);                     /* t=0: no margin yet */

    assert(timer_count(&host, fb->id, 10) == 1);         /* 10 ms sampler is running */
    assert(timer_fire(&host, fb->id, 10));

    fake->set(800);                                      /* still inside the 800 ms pause */
    assert(timer_fire(&host, fb->id, 10));
    assert(title_shift(trace) == 0);
    assert(timer_count(&host, fb->id, 0) == 0);          /* no repeat yet */

    fake->set(2300);                                     /* half of the 3000 ms movement span */
    assert(timer_fire(&host, fb->id, 10));
    assert(title_x(trace) == 29 && title_w(trace) == 100);
    assert(title_shift(trace) == 81);                    /* (int)(163 * 0.5) */

    fake->set(4555);                                     /* progress > 0.99, pass still running */
    assert(timer_fire(&host, fb->id, 10));
    assert(timer_count(&host, fb->id, 10) == 1);         /* sampler continues */
    assert(timer_count(&host, fb->id, 0) == 1);          /* the single repeat was armed here */

    fake->set(4600);                                     /* duration reached */
    assert(timer_fire(&host, fb->id, 10));
    assert(title_shift(trace) == 163);                   /* vendor ends at distance */
    assert(timer_count(&host, fb->id, 10) == 0);         /* sampler stopped */
    assert(timer_count(&host, fb->id, 0) == 1);          /* no second repeat */

    fake->set(4600 + OA_ANIM_SLIDE_REPEAT_MS);
    assert(timer_fire(&host, fb->id, 0));                /* repeat fires: slide restarts */
    assert(title_shift(trace) == 0);                     /* repainted at margin 0 immediately */
    assert(timer_count(&host, fb->id, 10) == 1);         /* sampler running again */

    /* Second full pass. */
    fake->set(6600 + 2300);
    assert(timer_fire(&host, fb->id, 10));
    assert(title_shift(trace) == 81);
    fake->set(6600 + 4600);
    assert(timer_fire(&host, fb->id, 10));
    assert(title_shift(trace) == 163);
    assert(timer_count(&host, fb->id, 10) == 0);
    assert(timer_count(&host, fb->id, 0) == 1);          /* a fresh repeat is armed for pass 3 */

    /* Disabling animation stops the repeat from starting another pass. */
    assert(anim->set_enabled(0) == 0);
    fake->set(6600 + 4600 + OA_ANIM_SLIDE_REPEAT_MS);
    assert(timer_fire(&host, fb->id, 0));
    assert(timer_count(&host, fb->id, 10) == 0);         /* no new sampler */
    assert(title_shift(trace) == 163);                   /* label keeps its end margin */

    /* Suspend drops both slide timers (nothing may fire against the released display) and resume
     * starts a fresh pass. */
    assert(anim->set_enabled(1) == 0);
    assert(!command(&host, "suspend ui_fb", out, sizeof out) && strstr(out, "ok"));
    assert(timer_count(&host, fb->id, 10) == 0);
    assert(timer_count(&host, fb->id, 0) == 0);
    assert(!command(&host, "resume ui_fb", out, sizeof out) && strstr(out, "ok"));
    assert(timer_count(&host, fb->id, 10) == 1);
    assert(timer_fire(&host, fb->id, 10));
    assert(title_shift(trace) == 0);                     /* resumed pass starts at margin 0 */

    /* Stop clears the stale slide fields; a following start must arm a sampler and (later) its
     * own repeat rather than being suppressed by an old nonzero slide_repeat. */
    assert(!command(&host, "stop ui_fb", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(timer_count(&host, fb->id, 10) == 0 && timer_count(&host, fb->id, 0) == 0);
    assert(!command(&host, "start ui_fb", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(timer_count(&host, fb->id, 10) == 1);
    assert(timer_fire(&host, fb->id, 10));
    assert(title_shift(trace) == 0);
    fake->set(fake->get() + 4555);
    assert(timer_fire(&host, fb->id, 10));
    assert(timer_count(&host, fb->id, 0) == 1);          /* repeat re-armed after a restart */

    host.stop_requested = 1;
    assert(oa_host_run(&host) == 0);
    close(host.selfpipe[0]); close(host.selfpipe[1]); pthread_mutex_destroy(&host.post_mu);
    printf("  ok   text slide clips to the box, follows AnimationHelper margins and repeats\n");
    return 0;
}
