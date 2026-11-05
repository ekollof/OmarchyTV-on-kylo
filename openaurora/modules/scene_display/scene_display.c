/* Canvas/coalescing consumer; hardware lives in the held display.gpu provider.
 * Every accepted image is immutable before transfer. No device calls here. */
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/canvas.h"
struct desired { oa_display_result value; int image, valid; };
struct ctx {
    const oa_host_api *api;
    const oa_display_gpu_iface *gpu;
    oa_canvas canvas;
    uint32_t id, sequence;
    uint64_t revision, cached_revision, ticket, timer;
    unsigned type;
    float progress;
    int active, pending, opened, fault, result_valid;
    int display_published, async_published, alpha_published, patch_published;
    struct desired queued;
    oa_display_result inflight, result;
};
static struct ctx *g_ctx;
static void discard(struct ctx *c)
{ if (c->queued.image >= 0) (void)close(c->queued.image); c->queued = (struct desired){.image = -1}; }
static void failed(struct ctx *c, int error)
{
    c->fault = error > 0 ? error : EIO;
    if (c->queued.valid) { c->result = c->queued.value; c->result.error = c->fault; c->result_valid = 1; }
    discard(c);
}
static void drive(void *ud)
{
    struct ctx *c = ud;
    if (!c->active || !c->gpu || c->fault) return;
    if (c->pending) {
        oa_scene_reply reply;
        int ready = c->gpu->take(c->ticket, &reply);
        if (ready < 0) { c->pending = 0; failed(c, errno); return; }
        if (!ready) return;
        c->pending = 0;
        if (reply.error) {
            if (reply.operation == OA_SCENE_FRAME) {
                c->result = c->inflight; c->result.error = reply.error; c->result_valid = 1;
            }
            failed(c, reply.error); return;
        }
        if (reply.operation == OA_SCENE_OPEN) c->opened = 1;
        else {
            c->cached_revision = c->inflight.revision;
            c->result = c->inflight; c->result_valid = 1;
        }
    }
    if (!c->opened) {
        int state = c->gpu->is_open();
        if (state < 0) { failed(c, errno); return; }
        if (state) c->opened = 1; /* consumer reload can reuse the provider session */
        else {
            if (c->gpu->open(&c->ticket)) {
                if (errno != EBUSY && errno != EAGAIN && errno != EWOULDBLOCK) failed(c, errno);
            } else c->pending = 1;
            return;
        }
    }
    if (!c->queued.valid) return;
    const oa_display_result *q = &c->queued.value;
    if (c->gpu->frame(q->revision, q->type, q->progress, c->queued.image, &c->ticket)) {
        if (errno != EBUSY && errno != EAGAIN && errno != EWOULDBLOCK) failed(c, errno);
        return;
    }
    c->inflight = *q; c->pending = 1; discard(c);
}
static int request(uint64_t *serial)
{
    struct ctx *c = g_ctx;
    if (!c || !serial) { errno = EINVAL; return -1; }
    if (!c->active) { errno = ESHUTDOWN; return -1; }
    if (c->fault) { errno = c->fault; return -1; }
    if (c->sequence == UINT32_MAX) { errno = EOVERFLOW; return -1; }
    int image = -1;
    int can_reuse = c->queued.valid && c->queued.value.revision == c->revision;
    if (can_reuse) image = c->queued.image;
    else if (c->revision != c->cached_revision && (!c->pending || c->inflight.revision != c->revision)) {
        image = oa_scene_image_create(c->canvas.pixels, OA_SCENE_IMAGE_BYTES);
        if (image < 0) return -1;
    }
    if (can_reuse) c->queued.image = -1;
    discard(c);
    uint64_t next = (uint64_t)c->id << 32 | (uint64_t)++c->sequence;
    c->queued = (struct desired){.valid = 1, .image = image,
        .value = {.serial = next, .revision = c->revision, .type = c->type, .progress = c->progress}};
    *serial = next; drive(c); return 0;
}
static int latest(oa_display_result *out)
{
    struct ctx *c = g_ctx;
    if (!c || !out) { errno = EINVAL; return -1; }
    if (c->result_valid) { *out = c->result; return 1; }
    if (c->fault) { errno = c->fault; return -1; }
    return 0;
}
static int drawing(struct ctx *c)
{
    if (!c || !c->active || !c->canvas.pixels) { errno = ESHUTDOWN; return -1; }
    if ((uint32_t)c->revision == UINT32_MAX) { errno = EOVERFLOW; return -1; }
    return 0;
}
static int fill(int x, int y, int w, int h, int r, int g, int b)
{
    struct ctx *c = g_ctx; if (drawing(c)) return -1;
    if (oa_canvas_fill(&c->canvas, x, y, w, h, r, g, b)) return -1;
    c->revision++; return 0;
}
static int clear(int r, int g, int b) { return fill(0, 0, OA_SCENE_WIDTH, OA_SCENE_HEIGHT, r, g, b); }
static int clear_rgba(int r, int g, int b, int a)
{
    struct ctx *c = g_ctx; if (drawing(c)) return -1;
    if (oa_canvas_clear(&c->canvas, r, g, b, a)) return -1;
    c->revision++; return 0;
}
static int blit_scaled(int x, int y, int w, int h, int sw, int sh, const unsigned char *rgba)
{
    struct ctx *c = g_ctx; if (drawing(c)) return -1;
    /* Match the vendor card/rail sampler (GL_LINEAR_MIPMAP_LINEAR) used by
     * display_fb; nearest sampling loses the exact applications-grid and
     * portal card pixels. */
    if (oa_canvas_blit_mipmap(&c->canvas, x, y, w, h, sw, sh, rgba)) return -1;
    c->revision++; return 0;
}
static int blit(int x, int y, int w, int h, const unsigned char *rgba)
{ return blit_scaled(x, y, w, h, w, h, rgba); }

static int patch_fill_rgba(int x, int y, int w, int h, int r, int g, int b, int a)
{
    struct ctx *c = g_ctx;
    if (drawing(c) || w <= 0 || h <= 0 ||
        r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 ||
        a < 0 || a > 255)
        return -1;
    int64_t right = (int64_t)x + w, bottom = (int64_t)y + h;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = right > c->canvas.width ? c->canvas.width : (int)right;
    int y1 = bottom > c->canvas.height ? c->canvas.height : (int)bottom;
    if (x0 >= x1 || y0 >= y1) return 0;
    for (int yy = y0; yy < y1; yy++) {
        uint8_t *row = c->canvas.pixels +
                       (size_t)yy * (size_t)c->canvas.width * 4u;
        for (int xx = x0; xx < x1; xx++) {
            uint8_t *p = row + (size_t)xx * 4u;
            p[0] = (uint8_t)r; p[1] = (uint8_t)g;
            p[2] = (uint8_t)b; p[3] = (uint8_t)a;
        }
    }
    c->revision++;
    return 0;
}
static int present(void) { errno = ENOTSUP; return -1; }
static int info(int *w, int *h, int *bpp, int *frames)
{
    if (w) *w = OA_SCENE_WIDTH;
    if (h) *h = OA_SCENE_HEIGHT;
    if (bpp) *bpp = 32;
    if (frames) *frames = 2;
    return 0;
}
static int transform(int type, float progress)
{
    struct ctx *c = g_ctx; if (!c || type < 0 || type > 8) { errno = EINVAL; return -1; }
    if (!type || !isfinite(progress) || progress < 0 || progress > 1) { c->type = 0; c->progress = 1; }
    else { c->type = (unsigned)type; c->progress = progress; }
    return 0;
}
static const oa_display_iface display = {clear, fill, blit, blit_scaled, present, info, transform};
static const oa_display_async_iface asynchronous = {request, latest};
static const oa_display_alpha_iface alpha = {clear_rgba};
static const oa_display_patch_iface patch = {patch_fill_rgba};
static int init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof *api) return -1;
    struct ctx *c = calloc(1, sizeof *c); if (!c) return -1;
    c->api = api; c->id = api->owner_id(api->priv); c->queued.image = -1;
    c->revision = (uint64_t)c->id << 32 | UINT64_C(1);
    c->progress = 1; g_ctx = c; *out = c; return 0;
}
static int activate(struct ctx *c)
{
    /* A restart/resume can reuse a still-open GPU provider. Its upload cache
     * cannot accept an old revision for the new canvas/session snapshot. */
    if ((uint32_t)c->revision == UINT32_MAX) { errno = EOVERFLOW; return -1; }
    c->revision++;
    if (!c->timer) c->timer = c->api->timer_add(c->api->priv, 10, 10, drive, c);
    if (!c->timer) return -1;
    c->fault = c->opened = c->result_valid = 0; c->cached_revision = 0;
    c->active = 1; drive(c); return 0;
}
static int start(void *ud)
{
    struct ctx *c = ud;
    c->gpu = c->api->lookup(c->api->priv, OA_SVC_DISPLAY_GPU, 1); if (!c->gpu) return -1;
    c->canvas = (oa_canvas){.width = OA_SCENE_WIDTH, .height = OA_SCENE_HEIGHT,
        .pixels = calloc(1, OA_SCENE_IMAGE_BYTES)};
    if (!c->canvas.pixels || activate(c)) return -1;
    if (c->api->publish(c->api->priv, OA_SVC_DISPLAY, 2, &display)) return -1;
    c->display_published = 1;
    if (c->api->publish(c->api->priv, OA_SVC_DISPLAY_ALPHA, 1, &alpha)) return -1;
    c->alpha_published = 1;
    if (c->api->publish(c->api->priv, OA_SVC_DISPLAY_PATCH, 1, &patch)) return -1;
    c->patch_published = 1;
    if (c->api->publish(c->api->priv, OA_SVC_DISPLAY_ASYNC, 1, &asynchronous)) return -1;
    c->async_published = 1; return 0;
}
static int suspend(void *ud)
{
    struct ctx *c = ud; if (!c) return 0;
    c->active = 0;
    if (c->timer) c->api->timer_del(c->api->priv, c->timer);
    c->timer = 0;
    if (c->pending && c->gpu) (void)c->gpu->cancel(c->ticket);
    c->pending = c->opened = 0; c->inflight = (oa_display_result){0};
    discard(c); return 0;
}
static void stop(void *ud)
{
    struct ctx *c = ud; if (!c) return;
    (void)suspend(c);
    if (c->async_published) c->api->unpublish(c->api->priv, OA_SVC_DISPLAY_ASYNC);
    if (c->patch_published) c->api->unpublish(c->api->priv, OA_SVC_DISPLAY_PATCH);
    if (c->alpha_published) c->api->unpublish(c->api->priv, OA_SVC_DISPLAY_ALPHA);
    if (c->display_published) c->api->unpublish(c->api->priv, OA_SVC_DISPLAY);
    c->async_published = c->display_published = c->alpha_published = c->patch_published = 0;
    if (c->gpu) c->api->release(c->api->priv, OA_SVC_DISPLAY_GPU);
    c->gpu = NULL; free(c->canvas.pixels); c->canvas.pixels = NULL;
}
static void destroy(void *ud) { if (g_ctx == ud) g_ctx = NULL; free(ud); }
static int resume(void *ud)
{
    struct ctx *c = ud;
    if (activate(c)) { (void)suspend(c); return -1; }
    return 0;
}
static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "scene_display", .version = "1", .requires = {OA_SVC_DISPLAY_GPU, ""},
    .init = init, .start = start, .stop = stop, .destroy = destroy,
    .quiesce = suspend, .suspend = suspend, .resume = resume
};
OA_MODULE_EXPORT(descriptor)
