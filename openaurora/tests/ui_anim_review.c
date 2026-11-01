#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "host.h"
#include "openaurora/ui_anim.h"
#include "openaurora/ui_time.h"

static uint32_t clock_now;
static uint32_t now_ms(void) { return clock_now; }
static uint32_t elapsed(uint32_t now, uint32_t before) { return now - before; }
static const oa_ui_clock_iface clock_service = { now_ms, elapsed };
static int completions;
static void restart(int id, int kind, const oa_anim_rect *rect, float eased, int done, void *ud)
{
    (void)kind; (void)eased;
    oa_ui_anim *anim = ud;
    if (!done) return;
    completions++;
    oa_anim_rect before = *rect;
    oa_anim_rect origin = {0};
    oa_anim_rect target = { 30, 40, 50, 60 };
    assert(oa_ui_anim_start(anim, id, OA_ANIM_LIST, &origin, &target, 10, 10) == 0);
    assert(rect->x == before.x && rect->y == before.y); /* callback event is stable */
}
static int timers(const oa_host *h, uint32_t owner)
{
    int count = 0;
    for (int i = 0; i < MAX_TIMERS; i++)
        count += h->timers[i].active && h->timers[i].owner == owner;
    return count;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    oa_ui_anim *anim = oa_ui_anim_create();
    assert(anim && oa_ui_anim_set_enabled(anim, 1) == 0);
    oa_ui_anim_bind(anim, restart, anim);
    oa_anim_rect from = {0}, to = {10, 20, 30, 40};
    assert(oa_ui_anim_start(anim, 1, OA_ANIM_LIST, &from, &to, 10, 0) == 0);
    assert(oa_ui_anim_sample(anim, 10) == 1 && completions == 1);
    assert(oa_ui_anim_get(anim, 1, NULL, NULL, NULL, NULL, NULL) == 0);
    assert(oa_ui_anim_finalise(anim, 1) == 0 && completions == 2);
    assert(oa_ui_anim_get(anim, 1, NULL, NULL, NULL, NULL, NULL) == 0);
    oa_ui_anim_bind(anim, NULL, NULL);
    oa_anim_rect extreme = { INT_MAX, INT_MIN, INT_MAX, INT_MIN }, result;
    assert(oa_ui_anim_start(anim, 1, OA_ANIM_LIST, &extreme, &extreme, 10, 0) == 0);
    assert(oa_ui_anim_sample(anim, 5) == 1);
    assert(oa_ui_anim_get(anim, 1, NULL, &result, NULL, NULL, NULL) == 0);
    assert(result.x == INT_MAX && result.y == INT_MIN);
    oa_ui_anim_destroy(anim);
    puts("  ok   animation completion/finalise callbacks can replace their own slot");

    oa_host h;
    assert(oa_host_init(&h, argv[1], NULL) == 0);
    assert(h.api.publish(&h, OA_SVC_UI_CLOCK, 1, &clock_service) == 0);
    oa_mod *m = oa_module_load(&h, "ui_anim");
    assert(m && oa_module_start(&h, m) == 0);
    const oa_ui_anim_iface *service = h.api.lookup(&h, OA_SVC_UI_ANIM, 1);
    assert(service && service->set_enabled(1) == 0);
    assert(service->start(7, OA_ANIM_LIST, &from, &to, 100, NULL) == 0);
    assert(h.nposts == 1 && h.posts[0].owner == m->id);
    oa_host_dispatch_posts(&h);
    assert(timers(&h, m->id) == 1);
    assert(oa_module_suspend_all(&h) == 0 && timers(&h, m->id) == 0);
    for (int i = 0; i < MAX_TIMERS; i++) h.timers[i].active = 1;
    assert(oa_module_resume_all(&h) == -1 && m->state == OA_ST_SUSPENDED);
    for (int i = 0; i < MAX_TIMERS; i++) h.timers[i].active = 0;
    assert(oa_module_resume_all(&h) == 0 && timers(&h, m->id) == 1);
    clock_now = 100;
    for (int i = 0; i < MAX_TIMERS; i++) {
        oa_timer *t = &h.timers[i];
        if (!t->active || t->owner != m->id) continue;
        h.cur_module = m->id;
        t->cb(t->ud);
        h.cur_module = 0;
        break;
    }
    assert(service->get(7, NULL, NULL, NULL, NULL, NULL) == -1);
    assert(timers(&h, m->id) == 0);

    /* Text slide, from AnimationHelper 0xef0210 / 0xef0264 (pause 800 ms, speed 50, x1000):
     * duration = 1600 + (distance/50)*1000, integer division first. */
    {
        struct { int distance; float duration; } dur[] = {
            { 0, 1600.0f }, { 49, 1600.0f }, { 50, 2600.0f }, { 100, 3600.0f }, { 400, 9600.0f },
        };
        for (unsigned i = 0; i < sizeof dur / sizeof dur[0]; i++)
            assert(oa_anim_slide_duration(dur[i].distance) == dur[i].duration);

        /* distance 400 -> duration 9600 -> movement span 8000 ms */
        const float d400 = oa_anim_slide_duration(400);
        assert(oa_anim_slide_margin(0, d400, 400, 0) == 0);        /* before/at the pause */
        assert(oa_anim_slide_margin(800, d400, 400, 0) == 0);
        assert(oa_anim_slide_margin(801, d400, 400, 0) == 0);      /* trunc(400/8000) */
        assert(oa_anim_slide_margin(4800, d400, 400, 0) == 200);   /* half way */
        assert(oa_anim_slide_margin(9600, d400, 400, 0) == 400);   /* vendor ends at distance */
        assert(oa_anim_slide_margin(90000, d400, 400, 0) == 400);  /* clamped, not reset */
        assert(oa_anim_slide_margin(0, d400, 400, 1) == 400);      /* reverse starts at distance */
        assert(oa_anim_slide_margin(4800, d400, 400, 1) == 200);
        assert(oa_anim_slide_margin(9600, d400, 400, 1) == 0);

        /* distance < speed gives a zero movement span; the vendor divides by zero, gets +inf,
         * clamps to 1, so the move is immediate after the pause. */
        const float d49 = oa_anim_slide_duration(49);
        assert(oa_anim_slide_margin(800, d49, 49, 0) == 0);
        assert(oa_anim_slide_margin(801, d49, 49, 0) == 49);
        assert(oa_anim_slide_margin(801, d49, 49, 1) == 0);
    }

    /* oa_slide is the caller-side runner around that math: it holds the start time and derives the
     * margin, so the shipped module never re-implements the vendor timing. Deterministic, no sleep;
     * the unsigned subtraction means a wrapping clock stays correct. */
    {
        oa_slide s;
        memset(&s, 0, sizeof s);
        assert(oa_slide_offset(&s, 0) == 0 && oa_slide_running(&s, 0) == 0); /* not started */

        oa_slide_start(&s, 400, 0, 1000);
        assert(s.distance == 400 && s.reverse == 0 && s.active == 1);
        assert(s.duration == 9600.0f && s.started == 1000);
        assert(oa_slide_offset(&s, 1000) == 0);                 /* t=0 of the pause */
        assert(oa_slide_running(&s, 1000) == 1);
        assert(oa_slide_offset(&s, 1800) == 0);                 /* still paused */
        assert(oa_slide_offset(&s, 5800) == 200);               /* half of the movement */
        assert(oa_slide_running(&s, 10599) == 1);               /* last running ms */
        assert(oa_slide_running(&s, 10600) == 0);               /* duration elapsed */
        assert(oa_slide_offset(&s, 10600) == 400);              /* holds the end position */
        assert(oa_slide_offset(&s, 10000000) == 400);           /* no wrap reset */

        oa_slide_start(&s, 400, 1, 0);                          /* reverse (RTL / vendor flag) */
        assert(oa_slide_offset(&s, 0) == 400);
        assert(oa_slide_offset(&s, 4800) == 200);
        assert(oa_slide_offset(&s, 9600) == 0);

        oa_slide_start(&s, -17, 0, 0);                          /* negative distance clamps to 0 */
        assert(s.distance == 0 && oa_slide_offset(&s, 0) == 0 && oa_slide_running(&s, 0) == 1);

        /* ui.clock is not a plain uint32 clock: it rolls over at 90,000,000 ms. The runner must
         * use the recovered period arithmetic, not raw subtraction. */
        oa_slide_start(&s, 400, 0, OA_UI_TIME_PERIOD - 500);
        assert(oa_slide_offset(&s, OA_UI_TIME_PERIOD - 500) == 0);
        assert(oa_slide_offset(&s, 500) == 10);              /* elapsed 1000 -> 400*200/8000 */
        assert(oa_slide_running(&s, 500) == 1);
        assert(oa_slide_progress(&s, 500) > 0.0f && oa_slide_progress(&s, 500) < 0.2f);
        assert(oa_slide_running(&s, 9100) == 0);             /* elapsed 9600 across the wrap */
        assert(oa_slide_offset(&s, 9100) == 400);
        assert(oa_slide_progress(&s, 9100) == 1.0f);

        /* progress follows the vendor 0..1 callback value */
        oa_slide_start(&s, 400, 0, 0);
        assert(oa_slide_progress(&s, 0) == 0.0f);
        assert(oa_slide_progress(&s, 4800) == 0.5f);         /* 4800 / 9600 */
        assert(oa_slide_progress(&s, 9504) == 0.99f);        /* the repeat threshold */
        assert(oa_slide_progress(&s, 9600) == 1.0f);

        /* NULL runners are tolerated: the module can call the helpers unconditionally. */
        oa_slide_start(NULL, 1, 0, 0);
        assert(oa_slide_offset(NULL, 0) == 0 && oa_slide_running(NULL, 0) == 0);
    }

    /* Full-screen animation geometry (hal::OpenGLGraphicsContext::iterateFullScreenAnimation
     * @0xb32910). Each type's vertex set and formula comes from the instructions; the constants
     * pi/2-3/1-3/1-6 are the pool at 0xb32aa8. */
    {
        float vx[8], vy[8];

        for (int i = 0; i < 8; i++) { vx[i] = 10.0f; vy[i] = 30.0f; }
        oa_fs_apply(OA_FS_SCALE_AXES, 0.0f, vx, vy, 8);
        assert(vx[0] == 10.0f && vy[0] == 30.0f);      /* vertex 0 untouched */
        assert(fabsf(vx[1] - 5.0f) < 1e-6f);           /* 10 * 0.5 */
        assert(fabsf(vy[1] - 20.0f) < 1e-5f);          /* 30 * 2/3 */
        assert(fabsf(vx[2] - 5.0f) < 1e-6f && fabsf(vy[2] - 20.0f) < 1e-5f);
        assert(vx[3] == 10.0f && vy[3] == 30.0f);      /* vertex 3 untouched */
        assert(vx[4] == 10.0f && vy[7] == 30.0f);      /* second quad untouched */

        for (int i = 0; i < 8; i++) { vx[i] = 10.0f; vy[i] = 30.0f; }
        oa_fs_apply(OA_FS_SCALE_AXES, 1.0f, vx, vy, 8); /* p = 1 -> identity */
        for (int i = 0; i < 8; i++) assert(vx[i] == 10.0f && vy[i] == 30.0f);

        /* p = 0.5 for the types whose report row is not just an endpoint */
        for (int i = 0; i < 8; i++) { vx[i] = 8.0f; vy[i] = 8.0f; }
        oa_fs_apply(OA_FS_SCALE_AXES, 0.5f, vx, vy, 8);
        assert(fabsf(vx[1] - 6.0f) < 1e-6f);           /* 8 * 0.75 */
        assert(fabsf(vy[1] - 8.0f * 5.0f / 6.0f) < 1e-5f); /* 8 * (2/3 + 1/6) */
        assert(vx[0] == 8.0f && vx[3] == 8.0f);

        for (int i = 0; i < 8; i++) vx[i] = 0.0f;
        oa_fs_apply(OA_FS_SLIDE_POS, 0.5f, vx, vy, 8);
        for (int i = 0; i < 4; i++) assert(fabsf(vx[i] - 1.0f / 12.0f) < 1e-6f);
        for (int i = 4; i < 8; i++) assert(vx[i] == 0.0f);

        for (int i = 0; i < 8; i++) vx[i] = 0.0f;
        oa_fs_apply(OA_FS_SLIDE_POS_SECOND, 0.5f, vx, vy, 8);
        for (int i = 0; i < 4; i++) assert(vx[i] == 0.0f);
        for (int i = 4; i < 8; i++) assert(fabsf(vx[i] - 1.0f / 12.0f) < 1e-6f);

        for (int i = 0; i < 8; i++) { vx[i] = 6.0f; vy[i] = 3.0f; }
        oa_fs_apply(OA_FS_SCALE, 0.5f, vx, vy, 8);
        assert(fabsf(vx[0] - 5.0f) < 1e-5f);           /* 6 * 5/6 */
        assert(fabsf(vy[0] - 2.5f) < 1e-5f);           /* 3 * 5/6 */
        assert(vx[4] == 6.0f && vy[4] == 3.0f);

        /* slides: (1-p)/6; negative for 2/4, positive for 3/5; second quad only for 4/5 */
        for (int i = 0; i < 8; i++) { vx[i] = 0.0f; vy[i] = 0.0f; }
        oa_fs_apply(OA_FS_SLIDE_NEG, 0.0f, vx, vy, 8);
        for (int i = 0; i < 4; i++) assert(fabsf(vx[i] + 1.0f / 6.0f) < 1e-6f);
        for (int i = 4; i < 8; i++) assert(vx[i] == 0.0f);

        for (int i = 0; i < 8; i++) vx[i] = 0.0f;
        oa_fs_apply(OA_FS_SLIDE_POS, 1.0f, vx, vy, 8);  /* p = 1 -> no shift */
        for (int i = 0; i < 8; i++) assert(vx[i] == 0.0f);

        for (int i = 0; i < 8; i++) vx[i] = 0.0f;
        oa_fs_apply(OA_FS_SLIDE_NEG_SECOND, 0.0f, vx, vy, 8);
        for (int i = 0; i < 4; i++) assert(vx[i] == 0.0f);
        for (int i = 4; i < 8; i++) assert(fabsf(vx[i] + 1.0f / 6.0f) < 1e-6f);

        for (int i = 0; i < 8; i++) vx[i] = 0.0f;
        oa_fs_apply(OA_FS_SLIDE_POS_SECOND, 0.0f, vx, vy, 8);
        for (int i = 0; i < 4; i++) assert(vx[i] == 0.0f);
        for (int i = 4; i < 8; i++) assert(fabsf(vx[i] - 1.0f / 6.0f) < 1e-6f);

        /* type 6 changes no vertex */
        for (int i = 0; i < 8; i++) { vx[i] = 7.0f; vy[i] = 9.0f; }
        oa_fs_apply(OA_FS_UNIFORM_ONLY, 0.25f, vx, vy, 8);
        for (int i = 0; i < 8; i++) assert(vx[i] == 7.0f && vy[i] == 9.0f);

        /* type 7: 2/3 + p/3 on both axes of vertices 0..3 */
        for (int i = 0; i < 8; i++) { vx[i] = 3.0f; vy[i] = 6.0f; }
        oa_fs_apply(OA_FS_SCALE, 0.0f, vx, vy, 8);
        for (int i = 0; i < 4; i++) {
            assert(fabsf(vx[i] - 2.0f) < 1e-5f);       /* 3 * 2/3 */
            assert(fabsf(vy[i] - 4.0f) < 1e-5f);       /* 6 * 2/3 */
        }
        for (int i = 4; i < 8; i++) assert(vx[i] == 3.0f && vy[i] == 6.0f);

        for (int i = 0; i < 8; i++) { vx[i] = 3.0f; vy[i] = 6.0f; }
        oa_fs_apply(OA_FS_SCALE, 1.0f, vx, vy, 8);
        for (int i = 0; i < 8; i++) assert(vx[i] == 3.0f && vy[i] == 6.0f);

        /* type 8: cosmetically a wobble; p=0 and p=1 are identity, p=1/4 gives sin(1.5*pi) = -1
         * so the factor is 1 - 0.75/3 = 0.75. */
        for (int i = 0; i < 8; i++) { vx[i] = 4.0f; vy[i] = 8.0f; }
        oa_fs_apply(OA_FS_WOBBLE, 0.0f, vx, vy, 8);
        for (int i = 0; i < 4; i++) assert(fabsf(vx[i] - 4.0f) < 1e-5f);
        oa_fs_apply(OA_FS_WOBBLE, 0.25f, vx, vy, 8);
        for (int i = 0; i < 4; i++) {
            assert(fabsf(vx[i] - 3.0f) < 1e-4f);       /* 4 * 0.75 */
            assert(fabsf(vy[i] - 6.0f) < 1e-4f);       /* 8 * 0.75 */
        }
        assert(vx[4] == 4.0f && vy[4] == 8.0f);        /* second quad untouched */
        for (int i = 0; i < 8; i++) { vx[i] = 4.0f; vy[i] = 8.0f; }
        oa_fs_apply(OA_FS_WOBBLE, 1.0f, vx, vy, 8);
        for (int i = 0; i < 8; i++) assert(vx[i] == 4.0f && vy[i] == 8.0f);

        /* progress is clamped to 0..1 exactly as the HAL does */
        for (int i = 0; i < 8; i++) { vx[i] = 10.0f; vy[i] = 30.0f; }
        oa_fs_apply(OA_FS_SCALE_AXES, -1.0f, vx, vy, 8);
        assert(fabsf(vx[1] - 5.0f) < 1e-6f);
        for (int i = 0; i < 8; i++) { vx[i] = 10.0f; vy[i] = 30.0f; }
        oa_fs_apply(OA_FS_SCALE_AXES, 2.0f, vx, vy, 8);
        assert(vx[1] == 10.0f && vy[1] == 30.0f);

        /* unknown type and a short vertex count are safe no-ops for the untouched entries */
        for (int i = 0; i < 8; i++) { vx[i] = 1.0f; vy[i] = 1.0f; }
        oa_fs_apply(99, 0.5f, vx, vy, 8);
        for (int i = 0; i < 8; i++) assert(vx[i] == 1.0f && vy[i] == 1.0f);
        for (int i = 0; i < 8; i++) { vx[i] = 0.0f; vy[i] = 0.0f; }
        oa_fs_apply(OA_FS_SLIDE_NEG_SECOND, 0.0f, vx, vy, 5); /* only vertex 4 exists */
        assert(fabsf(vx[4] + 1.0f / 6.0f) < 1e-6f);
        oa_fs_apply(OA_FS_SCALE_AXES, 0.0f, NULL, vy, 8);     /* NULL is tolerated */
        oa_fs_apply(OA_FS_SCALE_AXES, 0.0f, vx, vy, 0);
    }

    h.stop_requested = 1;
    assert(oa_host_run(&h) == 0);
    close(h.selfpipe[0]); close(h.selfpipe[1]); pthread_mutex_destroy(&h.post_mu);
    puts("  ok   real animation DSO restores its timer after suspend; timer allocation failure is retryable");
    puts("  ok   text-slide duration/margin match AnimationHelper (pause 800, speed 50, x1000)");
    puts("  ok   full-screen animation types 1..8 match the HAL vertex rules and clamp");
    return 0;
}
