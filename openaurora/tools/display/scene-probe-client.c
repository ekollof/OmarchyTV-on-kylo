#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include "openaurora/scene_broker.h"
#include "scene-probe-client.h"
extern char **environ;
struct scene_probe { int fd; pid_t child; uint64_t ticket, revision; int error, cleanup_unproven; };
static int exchange(scene_probe *s, oa_scene_request request, int image)
{
    uint8_t bytes[64]; request.ticket = ++s->ticket;
    int primary = 0;
    if (oa_scene_encode_request(bytes, sizeof bytes, &request)) primary = EINVAL;
    else if (oa_scene_packet_send(s->fd, bytes, image, 1)) primary = errno;
    if (image >= 0) (void)close(image);
    if (!primary) {
        struct pollfd watch = {.fd = s->fd, .events = POLLIN};
        int polled = poll(&watch, 1, 5000);
        if (polled != 1) primary = polled < 0 ? errno : ETIMEDOUT;
        else {
            int received;
            int count = oa_scene_packet_receive(s->fd, bytes, &received, 1);
            if (count <= 0) primary = count < 0 ? errno : EPIPE;
            else {
                oa_scene_reply reply;
                if (received >= 0) { (void)close(received); primary = EPROTO; }
                else if (oa_scene_decode_reply(&reply, bytes, sizeof bytes) ||
                    reply.ticket != request.ticket || reply.operation != request.operation ||
                    reply.revision != request.revision) primary = EPROTO;
                else primary = reply.error;
            }
        }
    }
    s->error = primary;
    if (primary) { errno = primary; return -1; }
    return 0;
}
int probe_open(scene_probe **out, unsigned width, unsigned height, const char *helper, const char *renderer)
{
    if (!out || *out || !helper || !renderer || helper[0] != '/' || renderer[0] != '/') { errno = EINVAL; return -1; }
    scene_probe *s = calloc(1, sizeof *s); if (!s) return -1;
    s->fd = -1; *out = s;
    int fds[2]; if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds)) return -1;
    s->fd = fds[0];
    posix_spawn_file_actions_t actions;
    int rc = posix_spawn_file_actions_init(&actions);
    if (rc) { close(fds[1]); errno = rc; return -1; }
    if (!(rc = posix_spawn_file_actions_addclose(&actions, fds[0])) &&
        !(rc = posix_spawn_file_actions_adddup2(&actions, fds[1], STDIN_FILENO)) &&
        !(rc = posix_spawn_file_actions_addclose(&actions, fds[1])) &&
        !(rc = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0)) &&
        !(rc = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0))) {
        char *args[] = {(char *)helper, "--serve", (char *)renderer, NULL};
        pid_t child;
        rc = posix_spawn(&child, helper, &actions, NULL, args, environ);
        if (!rc) s->child = child;
    }
    (void)posix_spawn_file_actions_destroy(&actions); (void)close(fds[1]);
    if (rc) { s->error = rc; errno = rc; return -1; }
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
    /* EOF drains accepted work and fallible EGL cleanup in the child. A stuck
     * child intentionally keeps this diagnostic waiting; never restore while
     * a graphics owner can still write. The fsynced backup remains available. */
    if (s->fd >= 0) { (void)close(s->fd); s->fd = -1; }
    if (s->child > 0) {
        int status; pid_t reaped;
        do reaped = waitpid(s->child, &status, 0); while (reaped < 0 && errno == EINTR);
        if (reaped < 0) return -1;
        printf("broker child=%ld reaped status=%d\n", (long)s->child, status);
        /* An abnormal death cannot certify completed GPU cleanup. */
        if (!WIFEXITED(status)) { s->child = 0; s->cleanup_unproven = 1; errno = EIO; return -1; }
    }
    free(s); *out = NULL; return 0;
}
void probe_error(const scene_probe *s, char *out, size_t size)
{ if (out && size) snprintf(out, size, "scene IPC child=%ld error=%d", s ? (long)s->child : 0, s ? s->error : errno); }
