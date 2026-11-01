/* Blocking diagnostic adapter around the maintained nonblocking parent client.
 * Never link this polling/wait adapter into the host loop or a module gate. */
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "openaurora/scene_client.h"
#include "scene-probe-client.h"
struct scene_probe { oa_scene_client *client; uint64_t revision; int error, cleanup_unproven; };
static int exchange(scene_probe *s, oa_scene_request request, int image)
{
    uint64_t ticket;
    int submitted = oa_scene_client_submit(s->client, &request, image, &ticket);
    int primary = submitted ? errno : 0;
    if (image >= 0) (void)close(image);
    if (!primary) for (;;) {
        int pumped = oa_scene_client_pump(s->client), pump_error = errno;
        oa_scene_reply reply;
        int taken = oa_scene_client_take(s->client, ticket, &reply);
        if (taken == 1) { primary = reply.error; break; }
        if (taken < 0 || pumped < 0) { primary = taken < 0 ? errno : pump_error; break; }
        struct pollfd watch = {.fd = oa_scene_client_fd(s->client), .events = POLLIN};
        int ready = poll(&watch, 1, 5000);
        if (ready != 1) { primary = ready < 0 ? errno : ETIMEDOUT; break; }
    }
    s->error = primary;
    if (primary) { errno = primary; return -1; }
    return 0;
}
int probe_open(scene_probe **out, unsigned width, unsigned height, const char *helper, const char *renderer)
{
    if (!out || *out) { errno = EINVAL; return -1; }
    scene_probe *s = calloc(1, sizeof *s); if (!s) return -1;
    *out = s;
    if (oa_scene_client_spawn(&s->client, helper, renderer)) { s->error = errno; return -1; }
    return exchange(s, (oa_scene_request){.operation = OA_SCENE_OPEN, .width = width, .height = height}, -1);
}
int probe_render(scene_probe *s, const uint8_t *rgba, size_t size, int type, float progress)
{
    if (!s || s->error) { errno = s ? s->error : ENODEV; return -1; }
    int image = -1;
    if (rgba) {
        image = oa_scene_image_create(rgba, size); if (image < 0) return -1;
        s->revision++;
    } else if (size) { errno = EINVAL; return -1; }
    return exchange(s, (oa_scene_request){.operation = OA_SCENE_FRAME, .revision = s->revision,
        .type = (unsigned)type, .progress = progress, .upload = rgba != NULL}, image);
}
int probe_close(scene_probe **out)
{
    if (!out) { errno = EINVAL; return -1; }
    scene_probe *s = *out; if (!s) return 0;
    if (s->cleanup_unproven) { errno = EIO; return -1; }
    if (s->client) {
        for (;;) {
            int drained = oa_scene_client_shutdown(s->client);
            if (drained < 0) return -1;
            if (drained) break;
            /* Only the diagnostic waits. Production integrates a reap timer. */
            struct timespec delay = {.tv_nsec = 1000000}; (void)nanosleep(&delay, NULL);
        }
        int status;
        if (oa_scene_client_status(s->client, &status)) return -1;
        printf("broker reaped status=%d\n", status);
        if (!WIFEXITED(status)) { s->cleanup_unproven = 1; errno = EIO; return -1; }
        if (oa_scene_client_destroy(&s->client)) return -1;
    }
    free(s); *out = NULL; return 0;
}
int probe_coalesce(scene_probe *scene) { (void)scene; return 0; }
void probe_error(const scene_probe *s, char *out, size_t size)
{ if (out && size) snprintf(out, size, "scene IPC error=%d", s ? s->error : errno); }
