#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include "openaurora/scene_client.h"
extern char **environ;
struct oa_scene_client {
    pid_t owner, child;
    int fd, broken, shutting, pending, ready, opened, closing, reaped, status;
    uint32_t instance, sequence;
    uint64_t revision;
    oa_scene_request request;
    oa_scene_reply reply;
};
static _Atomic uint32_t instances;
static int owned(const oa_scene_client *c)
{
    if (!c) { errno = EINVAL; return -1; }
    if (c->owner != getpid()) { errno = EPERM; return -1; }
    return 0;
}
static void close_channel(oa_scene_client *c)
{ int fd = c->fd; c->fd = -1; if (fd >= 0) (void)close(fd); }
static void broken(oa_scene_client *c, int error)
{
    if (!c->broken) c->broken = error > 0 && error <= 4095 ? error : EIO;
    close_channel(c);
    if (c->pending) {
        c->reply = (oa_scene_reply){.operation = c->request.operation, .ticket = c->request.ticket,
            .revision = c->request.revision, .error = c->broken};
        c->pending = 0; c->ready = 1;
    }
}
static void receive_one(oa_scene_client *c)
{
    if (c->fd < 0) return;
    uint8_t bytes[OA_SCENE_PACKET_SIZE]; int image;
    int result = oa_scene_packet_receive(c->fd, bytes, &image, 1);
    if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return;
    if (result <= 0) { broken(c, result ? errno : ECONNRESET); return; }
    if (image >= 0) { (void)close(image); broken(c, EPROTO); return; }
    oa_scene_reply reply;
    if (!c->pending || oa_scene_decode_reply(&reply, bytes, sizeof bytes) ||
        reply.operation != c->request.operation || reply.ticket != c->request.ticket ||
        reply.revision != c->request.revision) { broken(c, EPROTO); return; }
    c->pending = 0; c->ready = 1; c->reply = reply;
    if (!reply.error) {
        if (reply.operation == OA_SCENE_OPEN) c->opened = 1;
        else if (reply.operation == OA_SCENE_FRAME) c->revision = reply.revision;
        else { c->opened = 0; c->shutting = 1; close_channel(c); }
    } else if (reply.operation != OA_SCENE_CLOSE) broken(c, reply.error);
}
static int reap(oa_scene_client *c)
{
    if (c->child <= 0) return 1;
    int status;
    pid_t result = waitpid(c->child, &status, WNOHANG);
    if (!result || (result < 0 && errno == EINTR)) return 0;
    if (result < 0) return -1; /* ECHILD is an external ownership violation */
    c->child = 0; c->status = status; c->reaped = 1;
    if (!c->shutting) {
        receive_one(c); /* preserve a final completion buffered at exit */
        broken(c, ECONNRESET);
    }
    return 1;
}
int oa_scene_client_spawn(oa_scene_client **out, const char *helper, const char *renderer)
{
    if (!out || *out || !helper || !renderer || helper[0] != '/' || renderer[0] != '/') { errno = EINVAL; return -1; }
    if (access(helper, X_OK) || access(renderer, R_OK)) return -1;
    uint32_t instance = atomic_load(&instances);
    do { if (instance == UINT32_MAX) { errno = EOVERFLOW; return -1; } }
    while (!atomic_compare_exchange_weak(&instances, &instance, instance+1));
    oa_scene_client *c = calloc(1, sizeof *c); if (!c) return -1;
    c->owner = getpid(); c->fd = -1; c->instance = instance+1;
    int pair[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair)) { free(c); return -1; }
    posix_spawn_file_actions_t actions;
    int rc = posix_spawn_file_actions_init(&actions);
    if (!rc) {
        rc = posix_spawn_file_actions_addclose(&actions, pair[0]);
        if (!rc) rc = posix_spawn_file_actions_adddup2(&actions, pair[1], STDIN_FILENO);
        if (!rc && pair[1] != STDIN_FILENO) rc = posix_spawn_file_actions_addclose(&actions, pair[1]);
        if (!rc) rc = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        if (!rc) rc = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        char *args[] = {(char *)helper, "--serve", (char *)renderer, NULL};
        pid_t child;
        if (!rc) { rc = posix_spawn(&child, helper, &actions, NULL, args, environ); if (!rc) c->child = child; }
        (void)posix_spawn_file_actions_destroy(&actions);
    }
    (void)close(pair[1]);
    if (rc) { (void)close(pair[0]); free(c); errno = rc; return -1; }
    c->fd = pair[0]; *out = c; return 0;
}
int oa_scene_client_submit(oa_scene_client *c, const oa_scene_request *request, int image, uint64_t *ticket)
{
    if (owned(c)) return -1;
    if (!request || !ticket || image < -1) { errno = EINVAL; return -1; }
    if (c->shutting) { errno = ESHUTDOWN; return -1; }
    if (c->broken) { errno = c->broken; return -1; }
    if (c->pending || c->ready) { errno = EBUSY; return -1; }
    if (c->sequence == UINT32_MAX) { errno = EOVERFLOW; return -1; }
    oa_scene_request r = *request;
    r.ticket = (uint64_t)c->instance << 32 | (uint64_t)(c->sequence+1);
    uint8_t bytes[OA_SCENE_PACKET_SIZE];
    if (oa_scene_encode_request(bytes, sizeof bytes, &r) ||
        ((r.operation == OA_SCENE_FRAME && r.upload) != (image >= 0))) { errno = EINVAL; return -1; }
    if ((c->closing && r.operation != OA_SCENE_CLOSE) ||
        (r.operation == OA_SCENE_OPEN && c->opened) ||
        (r.operation == OA_SCENE_FRAME && (!c->opened ||
            (r.upload ? r.revision <= c->revision : r.revision != c->revision)))) { errno = EINVAL; return -1; }
    if (oa_scene_packet_send(c->fd, bytes, image, 1)) {
        int primary = errno;
        if (primary != EAGAIN && primary != EWOULDBLOCK) broken(c, primary);
        errno = primary; return -1;
    }
    c->sequence++; c->request = r; c->pending = 1;
    if (r.operation == OA_SCENE_CLOSE) c->closing = 1;
    *ticket = r.ticket; return 0;
}
int oa_scene_client_pump(oa_scene_client *c)
{
    if (owned(c)) return -1;
    receive_one(c);
    if (reap(c) < 0) return -1;
    if (c->broken) { errno = c->broken; return -1; }
    return 0;
}
int oa_scene_client_take(oa_scene_client *c, uint64_t ticket, oa_scene_reply *out)
{
    if (owned(c)) return -1;
    if (!ticket || !out) { errno = EINVAL; return -1; }
    if ((!c->pending && !c->ready) || ticket != c->request.ticket) { errno = ENOENT; return -1; }
    if (c->pending) return 0;
    *out = c->reply; c->ready = 0; return 1;
}
int oa_scene_client_fd(const oa_scene_client *c)
{ if (owned(c)) return -1; return c->fd; }
int oa_scene_client_shutdown(oa_scene_client *c)
{
    if (owned(c)) return -1;
    c->shutting = 1; close_channel(c);
    if (c->pending) {
        c->reply = (oa_scene_reply){.operation = c->request.operation, .ticket = c->request.ticket,
            .revision = c->request.revision, .error = ECANCELED};
        c->pending = 0; c->ready = 1;
    }
    return reap(c);
}
int oa_scene_client_status(const oa_scene_client *c, int *status)
{
    if (owned(c)) return -1;
    if (!status) { errno = EINVAL; return -1; }
    if (!c->reaped) { errno = EAGAIN; return -1; }
    *status = c->status; return 0;
}
int oa_scene_client_destroy(oa_scene_client **out)
{
    if (!out) { errno = EINVAL; return -1; }
    oa_scene_client *c = *out; if (!c) return 0;
    if (owned(c)) return -1;
    if (!c->shutting || c->child > 0 || c->fd >= 0) { errno = EBUSY; return -1; }
    free(c); *out = NULL; return 0;
}
