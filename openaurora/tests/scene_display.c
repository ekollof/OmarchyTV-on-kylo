#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "host.h"
#include "scene_gpu_trace.h"
#include "openaurora/canvas.h"
#define require(expression) do { if (!(expression)) { \
    fprintf(stderr, "%s:%d: %s errno=%d\n", __FILE__, __LINE__, #expression, errno); exit(1); } } while (0)
static void pump(oa_host *h)
{
    for (unsigned i = 0; i < MAX_TIMERS; i++) if (h->timers[i].active) {
        oa_timer t = h->timers[i]; h->cur_module = t.owner; t.cb(t.ud); h->cur_module = 0;
    }
}
static void done(oa_host *h)
{
    h->stop_requested = 1; require(!oa_host_run(h));
    close(h->selfpipe[0]); close(h->selfpipe[1]); pthread_mutex_destroy(&h->post_mu);
}
static oa_mod *load(oa_host *h, const char *name)
{ oa_mod *m = oa_module_load(h, name); require(m && !oa_module_start(h, m)); return m; }
static void canvas(void)
{
    uint8_t pixels[4*4*4] = {0}; oa_canvas c = {pixels, 4, 4};
    require(!oa_canvas_fill(&c, 0, 0, 4, 4, 10, 20, 30));
    uint8_t source[] = {110, 120, 130, 128, 200, 210, 220, 0};
    require(!oa_canvas_blit(&c, -1, 1, 4, 2, 2, 1, source));
    require(pixels[(1*4+0)*4] == 60 && pixels[(1*4+0)*4+1] == 70 && pixels[(1*4+0)*4+2] == 80);
    require(pixels[(1*4+1)*4] == 10 && pixels[(1*4+1)*4+3] == 255);
    uint8_t before[sizeof pixels]; memcpy(before, pixels, sizeof pixels);
    require(!oa_canvas_fill(&c, INT_MAX, INT_MIN, INT_MAX, INT_MAX, 0, 0, 0));
    require(!oa_canvas_blit(&c, INT_MIN, INT_MAX, INT_MAX, INT_MAX, 2, 1, source));
    require(!memcmp(before, pixels, sizeof pixels));
    require(oa_canvas_fill(&c, 0, 0, 1, 1, -1, 0, 0) == -1);
    require(!oa_canvas_clear(&c, 200, 100, 50, 0));
    require(pixels[0] == 0 && pixels[3] == 0);
    require(!oa_canvas_blit(&c, 0, 0, 1, 1, 2, 1, source));
    require(pixels[0] == 110 && pixels[1] == 120 && pixels[3] == 128);
    require(!oa_canvas_clear(&c, 10, 20, 30, 128));
    require(!oa_canvas_blit(&c, 0, 0, 1, 1, 2, 1, source));
    require(pixels[0] == 76 && pixels[1] == 86 && pixels[2] == 96 && pixels[3] == 191);
    /* Vendor mipmapped sampler: 2x2 -> 1x1 box average, identity, alpha, and
     * the same extreme-coordinate contract as oa_canvas_blit. */
    uint8_t square[] = {
        10, 0, 100, 255, 20, 4, 100, 255,
        30, 8, 200, 255, 40, 12, 200, 255
    };
    uint8_t one[4] = {0, 0, 0, 0};
    oa_canvas small = {one, 1, 1};
    require(!oa_canvas_blit_mipmap(&small, 0, 0, 1, 1, 2, 2, square));
    require(one[0] == 25 && one[1] == 6 && one[2] == 150 && one[3] == 255);
    uint8_t ident[sizeof square] = {0};
    oa_canvas copy = {ident, 2, 2};
    require(!oa_canvas_blit_mipmap(&copy, 0, 0, 2, 2, 2, 2, square));
    require(!memcmp(ident, square, sizeof square));
    uint8_t red[] = {255, 0, 0, 128};
    uint8_t blend[4] = {0, 0, 255, 255};
    oa_canvas dest = {blend, 1, 1};
    require(!oa_canvas_blit_mipmap(&dest, 0, 0, 1, 1, 1, 1, red));
    require(blend[0] == 128 && blend[1] == 0 && blend[2] == 127 && blend[3] == 255);
    uint8_t keep[4]; memcpy(keep, blend, sizeof keep);
    require(!oa_canvas_blit_mipmap(&dest, INT_MIN, INT_MAX, INT_MAX, INT_MAX, 1, 1, red));
    require(!memcmp(keep, blend, sizeof keep));
    require(oa_canvas_blit_mipmap(&dest, 0, 0, 0, 1, 1, 1, red) == -1);
    puts("  ok   RGBA canvas clipping/scaling/alpha/extreme bounds and vendor mipmap sampler");
}
static void sessions(const char *directory)
{
    oa_host h; require(!oa_host_init(&h, directory, NULL)); h.log_level = OA_LOG_ERROR;
    (void)load(&h, "scene_gpu_fixture"); (void)load(&h, "scene_display");
    const scene_gpu_trace *t = h.api.lookup(&h, SCENE_GPU_TRACE, 1);
    const oa_display_iface *d = h.api.lookup(&h, OA_SVC_DISPLAY, 2);
    const oa_display_async_iface *a = h.api.lookup(&h, OA_SVC_DISPLAY_ASYNC, 1);
    require(t && d && a && d->present() == -1 && errno == ENOTSUP);
    require(oa_module_unload(&h, "scene_gpu_fixture", 0) == -1); /* provider pinned */
    oa_display_result result, before = {.serial = 99}; result = before;
    require(a->latest(&result) == 0 && !memcmp(&result, &before, sizeof result));
    require(!d->clear(42, 3, 4)); uint64_t initial, middle, final;
    require(!a->request(&initial)); require(a->latest(&result) == 0);
    require(!t->settle(0)); pump(&h); /* OPEN completes; first image submitted */
    unsigned uploads, frames; oa_display_result last;
    require(!t->stats(&last, &uploads, &frames) && uploads == 1 && frames == 1);
    uint64_t original_revision = last.revision;
    unsigned char pixel[4]; require(!t->pixel(0, 0, pixel) && pixel[0] == 42 && pixel[1] == 3 && pixel[3] == 255);
    require(!d->set_scene_transform(4, 0.2f) && !a->request(&middle));
    require(!d->set_scene_transform(0, 0) && !a->request(&final));
    require(initial < middle && middle < final && a->latest(&result) == 0);
    require(!t->settle(0)); pump(&h);
    require(a->latest(&result) == 1 && result.serial == initial && !result.error);
    require(!t->stats(&last, &uploads, &frames) && uploads == 1 && frames == 2 && last.type == 0 && last.revision == original_revision);
    require(!t->settle(0)); pump(&h);
    require(a->latest(&result) == 1 && result.serial == final && !result.error);
    /* Final frame was drained with no new request or animation tick. */
    require(!d->clear(80, 90, 100) && !a->request(&initial));
    require(!d->clear(120, 130, 140)); /* no request: must not mutate immutable inflight */
    require(!t->pixel(0, 0, pixel) && pixel[0] == 80 && pixel[1] == 90);
    require(!a->request(&middle));
    require(!d->clear(160, 170, 180) && !a->request(&final)); /* replaces queued snapshot */
    require(!t->settle(0)); pump(&h);
    require(!t->pixel(0, 0, pixel) && pixel[0] == 160 && pixel[1] == 170);
    require(!t->stats(&last, &uploads, &frames) && uploads == 3 && frames == 4);
    require(!t->settle(0)); pump(&h); require(a->latest(&result) == 1 && result.serial == final);
    uint64_t old_revision = last.revision, old_serial = final;
    require(!oa_module_reload(&h, "scene_display", 0));
    d = h.api.lookup(&h, OA_SVC_DISPLAY, 2); a = h.api.lookup(&h, OA_SVC_DISPLAY_ASYNC, 1); require(d && a);
    require(!d->clear(200, 210, 220) && !a->request(&initial));
    require(!t->stats(&last, &uploads, &frames) && last.revision > old_revision && (initial>>32) != (old_serial>>32));
    require(!t->settle(0)); pump(&h); require(a->latest(&result) == 1 && result.serial == initial);
    require(!oa_module_suspend_all(&h) && a->request(&final) == -1 && errno == ESHUTDOWN);
    require(!oa_module_resume_all(&h));
    require(!a->request(&final) && !t->settle(0)); pump(&h); /* fresh OPEN */
    require(!t->settle(0)); pump(&h); require(a->latest(&result) == 1 && result.serial == final);
    require(!a->request(&initial)); /* accepted cached frame outlives consumer */
    require(!d->clear(11, 22, 33) && !a->request(&middle)); /* owned queued file */
    require(!oa_module_unload(&h, "scene_display", 0));
    require(!t->settle(0) && !t->pixel(0, 0, pixel)); /* provider owns immutable cache */
    (void)load(&h, "scene_display");
    d = h.api.lookup(&h, OA_SVC_DISPLAY, 2); a = h.api.lookup(&h, OA_SVC_DISPLAY_ASYNC, 1);
    require(d && a && !d->clear(44, 55, 66) && !a->request(&initial));
    require(!d->set_scene_transform(4, 1) && !a->request(&final));
    require(!t->settle(EIO)); pump(&h);
    require(a->latest(&result) == 1 && result.error == EIO && result.serial == final);
    uint64_t unchanged = 999;
    require(a->request(&unchanged) == -1 && errno == EIO && unchanged == 999);
    done(&h);
    puts("  ok   coalescing retains final frame, exact committed receipts, cached revisions and immutable replaced snapshots");
    puts("  ok   reload gets new revision/receipt namespace; suspend/resume reopens and uploads retained canvas");
    puts("  ok   consumer unload releases queued image while provider retains inflight data; GPU error never acknowledges a frame");
}
static void partial(const char *directory)
{
    oa_host h; require(!oa_host_init(&h, directory, NULL)); h.log_level = OA_LOG_ERROR;
    (void)load(&h, "scene_gpu_fixture"); (void)load(&h, "scene_display");
    const scene_gpu_trace *t = h.api.lookup(&h, SCENE_GPU_TRACE, 1);
    const oa_display_iface *d = h.api.lookup(&h, OA_SVC_DISPLAY, 2);
    const oa_display_async_iface *a = h.api.lookup(&h, OA_SVC_DISPLAY_ASYNC, 1);
    const oa_display_patch_iface *p = h.api.lookup(&h, OA_SVC_DISPLAY_PATCH, 1);
    require(t && d && a && p);
    require(!d->clear(10, 20, 30));
    uint64_t serial;
    require(!a->request(&serial) && !t->settle(0)); pump(&h);
    require(!t->settle(0)); pump(&h);
    unsigned uploads = 0, frames = 0;
    oa_display_result last;
    require(!t->stats(&last, &uploads, &frames) && uploads == 1);
    unsigned char px[4];
    require(!t->pixel(0, 0, px) && px[0] == 10 && px[1] == 20 && px[2] == 30 && px[3] == 255);
    require(!p->fill_rgba(1, 1, 1, 1, 200, 100, 50, 255));
    require(!a->request(&serial) && !t->settle(0)); pump(&h);
    require(!t->stats(&last, &uploads, &frames) && uploads == 2);
    require(!t->pixel(1, 1, px) && px[0] == 200 && px[1] == 100 && px[2] == 50 && px[3] == 255);
    require(!t->pixel(0, 0, px) && px[0] == 10 && px[1] == 20 && px[2] == 30);
    require(!t->pixel(2, 2, px) && px[0] == 10 && px[1] == 20 && px[2] == 30);
    done(&h);
    puts("  ok   scene_display uploads only the patch rectangle; the GPU fixture keeps the surrounding scene");
}

static void timeout_handler(int sig) { (void)sig; _exit(124); }
int main(int argc, char **argv)
{
    require(argc == 2 && signal(SIGALRM, timeout_handler) != SIG_ERR); alarm(20);
    canvas(); sessions(argv[1]); partial(argv[1]); return 0;
}
