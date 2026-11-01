/* Completion/FD ownership fixture; no display or GPU emulation. */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "openaurora/module.h"
#include "scene_gpu_trace.h"
struct ctx {
    const oa_host_api *api;
    uint64_t sequence;
    int opened, pending, ready, canceled, image;
    unsigned uploads, frames;
    oa_scene_reply reply;
    oa_display_result last;
};
static struct ctx *g_ctx;
static int accept(unsigned op, uint64_t revision, uint64_t *ticket)
{
    struct ctx *c = g_ctx;
    if (!ticket || c->pending || c->ready) { errno = EBUSY; return -1; }
    c->reply = (oa_scene_reply){.operation = op, .revision = revision, .ticket = ++c->sequence};
    c->pending = 1; c->canceled = 0; *ticket = c->reply.ticket; return 0;
}
static int opening(uint64_t *ticket) { return accept(OA_SCENE_OPEN, 0, ticket); }
static int frame(uint64_t revision, unsigned type, float p, int image, uint64_t *ticket)
{
    struct ctx *c = g_ctx;
    if (!c->opened) { errno = ENODEV; return -1; }
    if (accept(OA_SCENE_FRAME, revision, ticket)) return -1;
    if (image >= 0) {
        int owned = (int)syscall(SYS_fcntl, image, F_DUPFD_CLOEXEC, 3);
        if (owned < 0) { c->pending = 0; return -1; }
        if (c->image >= 0) (void)close(c->image);
        c->image = owned; c->uploads++;
    }
    c->frames++; c->last = (oa_display_result){.revision = revision, .type = type, .progress = p};
    return 0;
}
static int take(uint64_t ticket, oa_scene_reply *out)
{
    struct ctx *c = g_ctx;
    if (!out || ticket != c->reply.ticket || (!c->pending && !c->ready)) { errno = ENOENT; return -1; }
    if (c->pending) return 0;
    *out = c->reply; c->ready = 0; return 1;
}
static int cancel(uint64_t ticket)
{
    struct ctx *c = g_ctx;
    if (ticket != c->reply.ticket || (!c->pending && !c->ready)) { errno = ENOENT; return -1; }
    c->ready = 0; c->canceled = c->pending; return 0;
}
static int is_open(void) { return g_ctx->opened; }
static int settle(int error)
{
    struct ctx *c = g_ctx;
    if (!c->pending) { errno = ENOENT; return -1; }
    c->reply.error = error;
    if (!error && c->reply.operation == OA_SCENE_OPEN) c->opened = 1;
    c->pending = 0; c->ready = !c->canceled; c->canceled = 0; return 0;
}
static int stats(oa_display_result *last, unsigned *uploads, unsigned *frames)
{ if (last) *last = g_ctx->last; if (uploads) *uploads = g_ctx->uploads; if (frames) *frames = g_ctx->frames; return 0; }
static int pixel(unsigned x, unsigned y, unsigned char out[4])
{
    if (!out || x >= OA_SCENE_WIDTH || y >= OA_SCENE_HEIGHT || g_ctx->image < 0) return -1;
    off_t offset = (off_t)((y*OA_SCENE_WIDTH+x)*4u);
    return pread(g_ctx->image, out, 4, offset) == 4 ? 0 : -1;
}
static const oa_display_gpu_iface gpu = {opening, frame, take, cancel, is_open};
static const scene_gpu_trace trace = {settle, stats, pixel};
static int init(const oa_host_api *api, void **out)
{
    struct ctx *c = calloc(1, sizeof *c); if (!c) return -1;
    c->api = api; c->image = -1; c->sequence = (uint64_t)api->owner_id(api->priv)<<32;
    g_ctx = c; *out = c; return 0;
}
static int start(void *ud)
{
    struct ctx *c = ud;
    if (c->api->publish(c->api->priv, OA_SVC_DISPLAY_GPU, 1, &gpu)) return -1;
    return c->api->publish(c->api->priv, SCENE_GPU_TRACE, 1, &trace);
}
static int suspend(void *ud)
{ struct ctx *c = ud; c->opened = c->pending = c->ready = c->canceled = 0; return 0; }
static int resume(void *ud) { (void)ud; return 0; }
static void stop(void *ud)
{
    struct ctx *c = ud;
    c->api->unpublish(c->api->priv, SCENE_GPU_TRACE);
    c->api->unpublish(c->api->priv, OA_SVC_DISPLAY_GPU);
    if (c->image >= 0) (void)close(c->image);
}
static void destroy(void *ud) { if (g_ctx == ud) g_ctx = NULL; free(ud); }
static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "scene_gpu_fixture", .version = "test", .init = init, .start = start,
    .stop = stop, .destroy = destroy, .suspend = suspend, .resume = resume
};
OA_MODULE_EXPORT(descriptor)
