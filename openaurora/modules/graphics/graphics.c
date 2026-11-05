/* Reloadable parent only. Blocking EGL/driver code lives in the executable. */
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/scene_client.h"
struct ctx {
    const oa_host_api *api;
    oa_scene_client *client;
    uint32_t id, sequence, watch;
    uint64_t timer, ticket, internal_ticket;
    int published, active, closing, fault, opened, pending, ready, canceled, cleanup_unproven;
    oa_scene_reply reply;
};
static struct ctx *g_ctx;
static void unwatch(struct ctx *c)
{ if (c->watch) c->api->watch_del(c->api->priv, c->watch); c->watch = 0; }
static void collect(struct ctx *c)
{
    if (!c->client || !c->pending) return;
    oa_scene_reply reply;
    if (oa_scene_client_take(c->client, c->internal_ticket, &reply) != 1) return;
    c->pending = 0;
    if (!reply.error && reply.operation == OA_SCENE_OPEN) c->opened = 1;
    if (reply.error && reply.error != ECANCELED) c->fault = reply.error;
    if (!c->canceled) { reply.ticket = c->ticket; c->reply = reply; c->ready = 1; }
    c->canceled = 0;
}
static int drain(struct ctx *c)
{
    unwatch(c);
    if (c->cleanup_unproven) { errno = EIO; return -1; }
    if (!c->client) return 0;
    int done = oa_scene_client_shutdown(c->client);
    collect(c);
    if (done <= 0) { if (done < 0) c->fault = errno; errno = EBUSY; return -1; }
    int status;
    if (oa_scene_client_status(c->client, &status) || !WIFEXITED(status)) {
        c->fault = EIO; c->cleanup_unproven = 1; errno = EIO; return -1;
    }
    if (WEXITSTATUS(status)) c->fault = EIO;
    return oa_scene_client_destroy(&c->client);
}
static void tick(void *ud)
{
    struct ctx *c = ud;
    if (!c->client) return;
    if (c->closing) { (void)drain(c); return; }
    if (oa_scene_client_pump(c->client)) c->fault = errno;
    collect(c);
    if (oa_scene_client_fd(c->client) < 0) unwatch(c);
    int status;
    if (!oa_scene_client_status(c->client, &status)) {
        if (!WIFEXITED(status)) { c->fault = EIO; c->cleanup_unproven = 1; }
        else if (WEXITSTATUS(status)) c->fault = EIO;
        else if (!c->fault) c->fault = ECONNRESET;
        c->opened = 0;
    }
}
static void on_reply(void *ud, int fd, unsigned events)
{ (void)fd; (void)events; tick(ud); }
static int submit(const oa_scene_request *request, int image, uint64_t *ticket)
{
    struct ctx *c = g_ctx;
    if (!c || !ticket) { errno = EINVAL; return -1; }
    if (!c->active || c->closing) { errno = ESHUTDOWN; return -1; }
    if (c->fault || !c->client) { errno = c->fault ? c->fault : ENOTCONN; return -1; }
    if (c->pending || c->ready) { errno = EBUSY; return -1; }
    if (c->sequence == UINT32_MAX) { errno = EOVERFLOW; return -1; }
    uint64_t internal;
    if (oa_scene_client_submit(c->client, request, image, &internal)) return -1;
    c->ticket = (uint64_t)c->id << 32 | (uint64_t)++c->sequence;
    c->internal_ticket = internal; c->pending = 1; c->canceled = 0;
    *ticket = c->ticket; return 0;
}
static int open_scene(uint64_t *ticket)
{
    const oa_scene_request r = {.operation = OA_SCENE_OPEN, .width = OA_SCENE_WIDTH, .height = OA_SCENE_HEIGHT};
    return submit(&r, -1, ticket);
}
static int frame_rect(uint64_t revision, unsigned type, float progress, int image,
                      unsigned x, unsigned y, unsigned w, unsigned h, uint64_t *ticket)
{
    if (image >= 0) {
        if (!w || !h || x >= OA_SCENE_WIDTH || y >= OA_SCENE_HEIGHT ||
            w > OA_SCENE_WIDTH - x || h > OA_SCENE_HEIGHT - y) {
            errno = EINVAL; return -1;
        }
    } else if (x || y || w || h) {
        errno = EINVAL; return -1;
    }
    const oa_scene_request r = {.operation = OA_SCENE_FRAME, .revision = revision,
        .type = type, .progress = progress, .upload = image >= 0,
        .x = x, .y = y, .w = w, .h = h};
    return submit(&r, image, ticket);
}
static int frame(uint64_t revision, unsigned type, float progress, int image, uint64_t *ticket)
{
    return frame_rect(revision, type, progress, image, 0, 0,
                      image >= 0 ? OA_SCENE_WIDTH : 0u,
                      image >= 0 ? OA_SCENE_HEIGHT : 0u, ticket);
}
static int take(uint64_t ticket, oa_scene_reply *out)
{
    struct ctx *c = g_ctx;
    if (!c || !out || !ticket) { errno = EINVAL; return -1; }
    if (ticket != c->ticket || (!c->pending && !c->ready)) { errno = ENOENT; return -1; }
    if (c->canceled) { errno = ECANCELED; return -1; }
    if (!c->ready) return 0;
    *out = c->reply; c->ready = 0; return 1;
}
static int cancel(uint64_t ticket)
{
    struct ctx *c = g_ctx;
    if (!c || !ticket) { errno = EINVAL; return -1; }
    if (ticket != c->ticket || (!c->pending && !c->ready)) { errno = ENOENT; return -1; }
    c->ready = 0; c->canceled = c->pending; return 0;
}
static int is_open(void)
{
    struct ctx *c = g_ctx;
    if (!c) { errno = ENODEV; return -1; }
    if (c->fault) { errno = c->fault; return -1; }
    return c->active && !c->closing && c->opened;
}
static const oa_display_gpu_iface service = {open_scene, frame, take, cancel, is_open, frame_rect};
static int init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof *api) return -1;
    struct ctx *c = calloc(1, sizeof *c); if (!c) return -1;
    c->api = api; c->id = api->owner_id(api->priv); g_ctx = c; *out = c; return 0;
}
static int start(void *ud)
{
    struct ctx *c = ud;
    if (c->client) { errno = EBUSY; return -1; }
    const char *helper = c->api->config_get(c->api->priv, "graphics.broker", NULL);
    const char *renderer = c->api->config_get(c->api->priv, "graphics.renderer", NULL);
    if (!helper || !renderer) { errno = EINVAL; return -1; }
    if (!c->published) {
        if (c->api->publish(c->api->priv, OA_SVC_DISPLAY_GPU, 2, &service)) return -1;
        c->published = 1;
    }
    /* Timer remains owned/alive on watch-registration and partial-start failure. */
    if (!c->timer) c->timer = c->api->timer_add(c->api->priv, 20, 20, tick, c);
    if (!c->timer) return -1;
    c->closing = c->fault = c->opened = 0;
    if (oa_scene_client_spawn(&c->client, helper, renderer)) return -1;
    c->watch = c->api->watch_add(c->api->priv, oa_scene_client_fd(c->client), OA_WATCH_READ, on_reply, c);
    if (!c->watch) { c->closing = 1; (void)drain(c); return -1; }
    c->active = 1; return 0;
}
static int quiesce(void *ud)
{
    struct ctx *c = ud; if (!c) return 0;
    c->active = c->opened = 0; c->closing = 1;
    return drain(c);
}
static void stop(void *ud)
{
    struct ctx *c = ud; if (!c) return;
    unwatch(c);
    if (c->timer) c->api->timer_del(c->api->priv, c->timer);
    c->timer = 0;
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_DISPLAY_GPU);
    c->published = c->active = c->pending = c->ready = c->canceled = 0;
}
static void destroy(void *ud) { if (g_ctx == ud) g_ctx = NULL; free(ud); }
static int resume(void *ud)
{
    struct ctx *c = ud;
    int result = start(c);
    if (result) { c->active = 0; c->closing = 1; (void)drain(c); }
    return result;
}
static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "graphics", .version = "1", .init = init, .start = start,
    .quiesce = quiesce, .stop = stop, .destroy = destroy, .suspend = quiesce, .resume = resume
};
OA_MODULE_EXPORT(descriptor)
