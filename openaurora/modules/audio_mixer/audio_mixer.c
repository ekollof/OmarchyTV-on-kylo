/* Reloadable, read-only IPC adapter. No NDAL link/init in the host process. */
#include <errno.h>
#include <pthread.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "openaurora/module.h"
#include "openaurora/services.h"
extern char **environ;
struct ctx {
    const oa_host_api *api;
    uint32_t id, sequence, watch;
    uint64_t timer, ticket;
    pid_t child;
    int fd, published, active, closing, broken, pending, ready, canceled;
    unsigned device;
    oa_audio_broker_reply reply;
    /* ABI 3 quiesce: the channel fd is detached and closed on a worker. The
     * reap timer stays active across suspend and closes the child later. */
    pthread_t cleanup_thread;
    int cleanup_running, cleanup_fd;
    unsigned cleanup_delay_ms;
};
static struct ctx *g_ctx;
static void on_reply(void *ud, int fd, unsigned events);
static int detach_channel(struct ctx *c)
{
    if (c->watch) c->api->watch_del(c->api->priv, c->watch);
    c->watch = 0;
    int fd = c->fd; c->fd = -1;
    return fd;
}

static void close_channel(struct ctx *c)
{
    int fd = detach_channel(c);
    if (fd >= 0) (void)close(fd); /* consume fd, never replay close */
}

static void *cleanup_worker(void *ud)
{
    struct ctx *c = ud;
    if (c->cleanup_delay_ms) {
        const struct timespec pause = {
            .tv_sec = c->cleanup_delay_ms / 1000u,
            .tv_nsec = (long)(c->cleanup_delay_ms % 1000u) * 1000000L
        };
        (void)nanosleep(&pause, NULL);
    }
    int fd = c->cleanup_fd;
    c->cleanup_fd = -1;
    if (fd >= 0) (void)close(fd);
    return NULL;
}
static void failed_channel(struct ctx *c, int error)
{
    c->broken = 1;
    if (c->pending) {
        c->reply = (oa_audio_broker_reply){ .ticket = c->ticket, .error = error,
                                          .gain = { .device = c->device } };
        c->pending = 0;
        c->ready = !c->canceled;
        c->canceled = 0;
    }
    close_channel(c);
}
static void reap(void *ud)
{
    struct ctx *c = ud;
    if (c->child <= 0) return;
    int status;
    pid_t result = waitpid(c->child, &status, WNOHANG);
    if (result == c->child || (result < 0 && errno == ECHILD)) {
        c->child = 0;
        if (!c->closing) {
            /* A terminated peer may have left its last valid reply buffered. */
            if (c->fd >= 0) on_reply(c, c->fd, OA_WATCH_READ);
            failed_channel(c, ECONNRESET);
        }
    }
}
static void on_reply(void *ud, int fd, unsigned events)
{
    (void)events;
    struct ctx *c = ud;
    uint8_t bytes[OA_AUDIO_BROKER_PACKET_SIZE];
    ssize_t n = recv(fd, bytes, sizeof bytes, MSG_DONTWAIT | MSG_TRUNC);
    if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) return;
    if (n <= 0) { failed_channel(c, n == 0 ? ECONNRESET : errno); return; }
    oa_audio_broker_reply reply;
    if (!c->pending || oa_audio_broker_decode_reply(&reply, bytes, (size_t)n) ||
        reply.ticket != c->ticket || reply.gain.device != c->device) {
        failed_channel(c, EPROTO);
        return;
    }
    c->pending = 0;
    if (!c->canceled) { c->reply = reply; c->ready = 1; }
    c->canceled = 0;
}
static int gain(unsigned device, uint64_t *ticket)
{
    struct ctx *c = g_ctx;
    if (!c || !ticket || device > 5) { errno = EINVAL; return -1; }
    if (!c->active || c->closing) { errno = ESHUTDOWN; return -1; }
    if (c->broken || c->fd < 0 || !c->child) { errno = ENOTCONN; return -1; }
    if (c->pending || c->ready) { errno = EBUSY; return -1; }
    if (c->sequence == UINT32_MAX) { errno = EOVERFLOW; return -1; }
    uint64_t next = (uint64_t)c->id << 32 | (uint64_t)(c->sequence + 1);
    oa_audio_broker_request request = { next, device, OA_AUDIO_BROKER_MAX_TIMEOUT_MS };
    uint8_t bytes[OA_AUDIO_BROKER_PACKET_SIZE];
    if (oa_audio_broker_encode_request(bytes, sizeof bytes, &request)) { errno = EINVAL; return -1; }
    ssize_t n = send(c->fd, bytes, sizeof bytes, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (n != (ssize_t)sizeof bytes) {
        int error = n < 0 ? errno : EIO;
        if (error != EAGAIN && error != EWOULDBLOCK) failed_channel(c, error);
        errno = error;
        return -1;
    }
    c->sequence++;
    c->ticket = next; c->device = device;
    c->pending = 1; c->canceled = 0;
    *ticket = next;
    return 0;
}
static int take(uint64_t ticket, oa_audio_broker_reply *out)
{
    struct ctx *c = g_ctx;
    if (!c || !out || !ticket) { errno = EINVAL; return -1; }
    if (ticket != c->ticket || (!c->pending && !c->ready)) { errno = ENOENT; return -1; }
    if (c->canceled) { errno = ECANCELED; return -1; }
    if (!c->ready) return 0;
    *out = c->reply; c->ready = 0;
    return 1;
}
static int cancel(uint64_t ticket)
{
    struct ctx *c = g_ctx;
    if (!c || !ticket) { errno = EINVAL; return -1; }
    if (ticket != c->ticket || (!c->pending && !c->ready)) { errno = ENOENT; return -1; }
    c->ready = 0;
    c->canceled = c->pending;
    return 0;
}
static const oa_audio_mixer_iface service = { gain, take, cancel };
static int init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof *api) return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api; c->id = api->owner_id(api->priv);
    c->fd = -1; c->cleanup_fd = -1;
    g_ctx = c; *out = c;
    return 0;
}
static int start(void *ud)
{
    struct ctx *c = ud;
    if (c->child > 0 || c->cleanup_running || c->cleanup_fd >= 0) {
        errno = EBUSY;
        return -1;
    }
    c->cleanup_running = 0;
    c->cleanup_fd = -1;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "audio_mixer.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    const char *path = c->api->config_get(c->api->priv, "audio_mixer.broker", NULL);
    if (!path || !*path) { errno = EINVAL; return -1; }
    if (access(path, X_OK)) return -1; /* not an exec/startup-health guarantee */
    c->closing = c->broken = c->pending = c->ready = c->canceled = 0;
    if (!c->published) {
        if (c->api->publish(c->api->priv, OA_SVC_AUDIO_MIXER, 1, &service)) return -1;
        c->published = 1;
    }
    /* Reserve completion machinery before spawning any child. */
    if (!c->timer) c->timer = c->api->timer_add(c->api->priv, 50, 50, reap, c);
    if (!c->timer) return -1;
    int pair[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, pair)) return -1;
    c->fd = pair[0];
    c->watch = c->api->watch_add(c->api->priv, c->fd, OA_WATCH_READ, on_reply, c);
    if (!c->watch) { close(pair[1]); return -1; }
    posix_spawn_file_actions_t actions;
    int rc = posix_spawn_file_actions_init(&actions);
    if (!rc) {
        rc = posix_spawn_file_actions_addclose(&actions, pair[0]);
        if (!rc) rc = posix_spawn_file_actions_adddup2(&actions, pair[1], STDIN_FILENO);
        if (!rc && pair[1] != STDIN_FILENO) rc = posix_spawn_file_actions_addclose(&actions, pair[1]);
        char *const args[] = { (char *)path, "--serve", NULL };
        if (!rc) rc = posix_spawn(&c->child, path, &actions, NULL, args, environ);
        posix_spawn_file_actions_destroy(&actions);
    }
    close(pair[1]);
    if (rc) { c->child = 0; errno = rc; return -1; }
    c->active = 1;
    return 0;
}
/* unpublish=1 is the stop/reload gate; unpublish=0 is suspend, which must
 * preserve the service and its holds across the stop/start cycle. */
static int quiesce_impl(struct ctx *c, int unpublish)
{
    if (!c) return 0;
    if (c->cleanup_running) {
        for (int i = 0; i < 10; i++) {
            int rc = pthread_tryjoin_np(c->cleanup_thread, NULL);
            if (rc == 0) {
                c->cleanup_running = 0;
                break;
            }
            if (rc != EBUSY) {
                c->cleanup_running = 0;
                return -1;
            }
            const struct timespec pause = { .tv_nsec = 5000000 };
            (void)nanosleep(&pause, NULL);
        }
        if (c->cleanup_running) return -1;
    }
    c->active = 0;
    c->closing = 1;
    c->pending = c->ready = c->canceled = 0;
    if (unpublish && c->published) {
        c->api->unpublish(c->api->priv, OA_SVC_AUDIO_MIXER);
        c->published = 0;
    }
    int fd = detach_channel(c);
    if (fd >= 0) {
        if (!c->cleanup_delay_ms) {
            (void)close(fd);
        } else {
            c->cleanup_fd = fd;
            if (pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c)) {
                (void)close(fd);
                c->cleanup_fd = -1;
            } else {
                c->cleanup_running = 1;
            }
        }
    }
    reap(c);
    if (c->cleanup_running) return -1;
    return c->child > 0 ? -1 : 0;
}

static int quiesce(void *ud) { return quiesce_impl(ud, 1); }
static void stop(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    if (c->timer) c->api->timer_del(c->api->priv, c->timer);
    c->timer = 0;
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->cleanup_fd >= 0) {
        (void)close(c->cleanup_fd);
        c->cleanup_fd = -1;
    }
    close_channel(c);
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_AUDIO_MIXER);
    c->published = c->active = 0;
}

static void destroy(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->cleanup_fd >= 0) (void)close(c->cleanup_fd);
    close_channel(c);
    if (g_ctx == ud) g_ctx = NULL;
    free(ud);
}

static int suspend(void *ud) { return quiesce_impl(ud, 0); }
static int resume(void *ud)
{
    struct ctx *c = ud;
    /* Preserve service holds and the retry/reap timer across suspend. */
    if (c->child > 0) { errno = EBUSY; return -1; }
    int rc = start(c);
    /* Unlike failed start, a failed resume is not followed by host stop(). */
    if (rc) { c->active = 0; c->closing = 1; close_channel(c); }
    return rc;
}
static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "audio_mixer", .version = "1",
    .init = init, .start = start, .quiesce = quiesce, .stop = stop, .destroy = destroy,
    .suspend = suspend, .resume = resume
};
OA_MODULE_EXPORT(descriptor)
