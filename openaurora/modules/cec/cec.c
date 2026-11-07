/* CEC owner module. start() creates the worker and publishes policy. With
 * cec.enable=1 it also maps the read-only authority mirror page and submits an
 * exclusive raw-device acquire; the blocking open/send/receive/close calls run
 * on the owner worker. Receive is polled on the host loop and never blocks it. */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include "openaurora/cec_authority_mem.h"
#include "openaurora/cec_owner.h"
#include "openaurora/cec_policy.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    oa_cec_owner *owner;
    oa_cec_policy *policy;
    uint32_t id;
    int owner_published;
    int audio_published;

    /* Optional hardware transport (cec.enable=1). */
    int enabled;
    char device_path[OA_CEC_OWNER_PATH_MAX];
    oa_cec_authority_mem authority;
    int authority_open;
    uint32_t poll_timer;
    int job_pending;
    int opening;
    int session_open;
    int closing;
    int last_error;
    int rx_error_logged;
    uint64_t rx_frames;
    uint64_t tx_frames;
    int have_last_rx;
    oa_cec_message last_rx;
    oa_cec_message reply_q[4];
    unsigned reply_head;
    unsigned reply_tail;

    /* ABI 3 quiesce: the owner is detached and finished on a worker. A failed
     * hardware close retains the owner for a later quiesce generation. */
    pthread_t cleanup_thread;
    int cleanup_running;
    oa_cec_owner *cleanup_owner;
    int cleanup_authority;
    int cleanup_result;
    unsigned cleanup_delay_ms;
    int quiescing;
};
static struct ctx *g_ctx;

static int dev_open(void *context, const char *path, int flags)
{
    (void)context;
    return open(path, flags);
}
static int dev_ioctl(void *context, int fd, unsigned long request, void *arg)
{
    (void)context;
    return ioctl(fd, request, arg);
}
static int dev_close(void *context, int fd)
{
    (void)context;
    return close(fd);
}
static const oa_cec_device_io native_device_io = {
    NULL, dev_open, dev_ioctl, dev_close
};

static void poke(void *ud) { (void)ud; }
static void settled(void *ud)
{
    struct ctx *c = ud;
    if (c && !c->quiescing && c->api)
        (void)c->api->post(c->api->priv, c->id, poke, c);
}
static int supported(void) { return g_ctx ? g_ctx->enabled : 0; }

static int begin_acquire(struct ctx *c);

static int transport_status(oa_cec_transport_status *out)
{
    struct ctx *c = g_ctx;
    if (!c || !out) return -EINVAL;
    memset(out, 0, sizeof *out);
    out->enabled = c->enabled;
    out->opening = c->opening;
    out->open = c->session_open;
    out->last_error = c->last_error;
    out->rx_frames = c->rx_frames;
    out->tx_frames = c->tx_frames;
    out->have_last_rx = c->have_last_rx;
    if (c->have_last_rx) out->last_rx = c->last_rx;
    return 0;
}

static int transport_send(const oa_cec_message *message)
{
    struct ctx *c = g_ctx;
    if (!c || !c->enabled || !c->session_open || !c->owner) return -ENODEV;
    if (!message || message->initiator != 0) return -EINVAL;
    uint8_t wire[OA_CEC_MAX_FRAME];
    size_t length = 0;
    if (oa_cec_encode(message, wire, sizeof wire, &length) != 0)
        return -EINVAL;
    if (c->job_pending) return -EBUSY;
    uint64_t ticket = 0;
    if (oa_cec_owner_send_async(c->owner, message, &ticket) != 0)
        return -(errno ? errno : EIO);
    c->job_pending = 1;
    return 0;
}

static int transport_open(void)
{
    struct ctx *c = g_ctx;
    if (!c || !c->enabled) return -ENODEV;
    if (c->session_open || c->opening || c->job_pending) return -EBUSY;
    return begin_acquire(c);
}

static int transport_close(void)
{
    struct ctx *c = g_ctx;
    if (!c || !c->enabled) return -ENODEV;
    if (!c->session_open || !c->owner) return -ENODEV;
    if (c->job_pending) return -EBUSY;
    uint64_t ticket = 0;
    if (oa_cec_owner_release_async(c->owner, &ticket) != 0)
        return -(errno ? errno : EIO);
    c->job_pending = 1;
    c->closing = 1;
    return 0;
}

static const oa_cec_service service = {
    supported, transport_status, transport_send, transport_open,
    transport_close
};
static int audio_handle(const oa_cec_message *incoming, oa_cec_message *replies, int capacity)
{
    return g_ctx ? oa_cec_policy_handle(g_ctx->policy, incoming, replies, capacity) : -1;
}
static void audio_route(oa_cec_audio_route *out)
{
    oa_cec_policy_route(g_ctx ? g_ctx->policy : NULL, out);
}
static int audio_local(void)
{
    return g_ctx ? oa_cec_policy_local_volume_permitted(g_ctx->policy) : 1;
}
static int audio_request(int on, uint16_t source_pa, oa_cec_message *out)
{
    return g_ctx ? oa_cec_policy_request_system_audio(g_ctx->policy, on, source_pa, out) : -1;
}
static int audio_arc(int initiate, oa_cec_message *out)
{
    return g_ctx ? oa_cec_policy_request_arc(g_ctx->policy, initiate, out) : -1;
}
static int audio_query(int opcode, oa_cec_message *out)
{
    return g_ctx ? oa_cec_policy_query(g_ctx->policy, opcode, out) : -1;
}
static int audio_user(int action, oa_cec_message *out)
{
    return g_ctx ? oa_cec_policy_user_volume(g_ctx->policy, action, out) : -1;
}
static int audio_link(int adjacent, int can_transmit)
{
    return g_ctx ? oa_cec_policy_set_arc_link(g_ctx->policy, adjacent, can_transmit) : -1;
}
static const oa_cec_audio_iface audio_service = {
    audio_handle, audio_route, audio_local, audio_request, audio_arc, audio_query, audio_user, audio_link
};

static void log_frame(struct ctx *c, const char *tag,
                      const oa_cec_message *m)
{
    char operands[3 * OA_CEC_MAX_OPERANDS + 1];
    size_t at = 0;
    operands[0] = '\0';
    for (size_t i = 0; i < m->count && at + 3 < sizeof operands; i++)
        at += (size_t)snprintf(operands + at, sizeof operands - at, "%02x ",
                               m->operands[i]);
    if (m->opcode < 0)
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec %s %u->%u poll", tag, m->initiator, m->destination);
    else
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec %s %u->%u op %02x len %u [%s]", tag, m->initiator,
                    m->destination, (unsigned)m->opcode, (unsigned)m->count,
                    operands);
}

static void queue_replies(struct ctx *c, const oa_cec_message *replies, int n)
{
    for (int i = 0; i < n; i++) {
        unsigned next = (c->reply_tail + 1u) & 3u;
        if (next == c->reply_head) {
            c->api->log(c->api->priv, OA_LOG_WARN,
                        "cec reply queue full; dropping op %02x",
                        (unsigned)replies[i].opcode);
            break;
        }
        c->reply_q[c->reply_tail] = replies[i];
        c->reply_tail = next;
    }
}

static void handle_completion(struct ctx *c, const oa_cec_completion *comp)
{
    switch (comp->operation) {
    case OA_CEC_ASYNC_ACQUIRE:
        c->opening = 0;
        if (comp->rc == 0) {
            c->session_open = 1;
            c->last_error = 0;
            c->api->log(c->api->priv, OA_LOG_INFO,
                        "cec raw transport open (%s)", c->device_path);
        } else {
            c->last_error = comp->error ? comp->error : EIO;
            c->api->log(c->api->priv, OA_LOG_WARN,
                        "cec acquire failed (%d)", c->last_error);
        }
        break;
    case OA_CEC_ASYNC_SEND:
        if (comp->rc == 0) {
            c->tx_frames++;
            c->last_error = 0;
            c->api->log(c->api->priv, OA_LOG_INFO, "cec tx %llu",
                        (unsigned long long)c->tx_frames);
        } else {
            c->last_error = comp->error ? comp->error : EIO;
            c->api->log(c->api->priv, OA_LOG_WARN, "cec tx failed (%d)",
                        c->last_error);
        }
        break;
    case OA_CEC_ASYNC_RECEIVE:
        if (comp->rc == 1) {
            c->rx_frames++;
            c->rx_error_logged = 0;
            c->last_rx = comp->message;
            c->have_last_rx = 1;
            log_frame(c, "rx", &comp->message);
            oa_cec_message replies[4];
            int count = oa_cec_policy_handle(c->policy, &comp->message,
                                             replies, 4);
            if (count < 0)
                c->api->log(c->api->priv, OA_LOG_WARN,
                            "cec audio policy rejected frame");
            else if (count > 0)
                queue_replies(c, replies, count);
        } else if (comp->rc < 0) {
            c->last_error = comp->error ? comp->error : EIO;
            if (!c->rx_error_logged) {
                c->api->log(c->api->priv, OA_LOG_WARN,
                            "cec rx failed (%d)", c->last_error);
                c->rx_error_logged = 1;
            }
        } else {
            c->rx_error_logged = 0;
        }
        break;
    case OA_CEC_ASYNC_RELEASE:
        c->closing = 0;
        if (comp->rc == 0) {
            c->session_open = 0;
            c->last_error = 0;
            c->api->log(c->api->priv, OA_LOG_INFO, "cec raw transport closed");
        } else {
            c->last_error = comp->error ? comp->error : EIO;
            c->api->log(c->api->priv, OA_LOG_WARN,
                        "cec release failed (%d)", c->last_error);
        }
        break;
    default:
        break;
    }
}

static void cec_poll(void *ud)
{
    struct ctx *c = ud;
    if (!c || !c->owner || c->quiescing)
        return;
    if (c->job_pending) {
        oa_cec_completion comp;
        int rc = oa_cec_owner_take(c->owner, &comp);
        if (rc == 1) {
            c->job_pending = 0;
            handle_completion(c, &comp);
        } else if (rc < 0) {
            c->job_pending = 0;
            c->opening = 0;
            c->closing = 0;
            c->last_error = errno ? errno : EIO;
            c->api->log(c->api->priv, OA_LOG_WARN,
                        "cec completion take failed (%d)", c->last_error);
        }
        return;
    }
    if (c->reply_head != c->reply_tail) {
        oa_cec_message message = c->reply_q[c->reply_head];
        uint64_t ticket = 0;
        if (oa_cec_owner_send_async(c->owner, &message, &ticket) == 0) {
            c->reply_head = (c->reply_head + 1u) & 3u;
            c->job_pending = 1;
        } else {
            c->last_error = errno ? errno : EIO;
            c->reply_head = c->reply_tail; /* drop on submission failure */
        }
        return;
    }
    if (c->session_open) {
        uint64_t ticket = 0;
        if (oa_cec_owner_receive_async(c->owner, &ticket) == 0)
            c->job_pending = 1;
        else
            c->last_error = errno ? errno : EIO;
    }
}

static int begin_acquire(struct ctx *c)
{
    if (!c || !c->owner) return -ENODEV;
    oa_cec_acquire request = {
        .authority = &c->authority.authority,
        .device = &native_device_io,
        .path = c->device_path
    };
    uint64_t ticket = 0;
    if (oa_cec_owner_acquire_async(c->owner, &request, &ticket) != 0) {
        c->last_error = errno ? errno : EIO;
        return -1;
    }
    c->opening = 1;
    c->job_pending = 1;
    c->last_error = 0;
    return 0;
}

static int init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof *api || api->abi != OA_ABI_VERSION) return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->policy = oa_cec_policy_create();
    if (!c->policy) { free(c); return -1; }
    c->api = api;
    c->id = api->owner_id(api->priv);
    g_ctx = c;
    *out = c;
    return 0;
}
/* 0 = joined and destroyed, -EAGAIN = owner retained for a later generation,
 * -EIO = fatal join error. Each quiesce call is a new retry generation for a
 * retained close, so the poll intentionally re-issues it. */
static int finish_owner_ptr(oa_cec_owner *owner)
{
    if (!owner) return 0;
    if (oa_cec_owner_quiesce(owner) != 0) {
        for (int i = 0; i < 40; i++) {
            const struct timespec pause = { .tv_nsec = 5000000 };
            (void)nanosleep(&pause, NULL);
            if (oa_cec_owner_quiesce(owner) == 0) break;
        }
        if (oa_cec_owner_quiesce(owner) != 0) return -EAGAIN;
    }
    if (oa_cec_owner_join(owner) != 0)
        return errno == EBUSY ? -EAGAIN : -EIO;
    oa_cec_owner_destroy(owner);
    return 0;
}

static void *cleanup_worker(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_delay_ms) {
        const struct timespec pause = {
            .tv_sec = c->cleanup_delay_ms / 1000u,
            .tv_nsec = (long)(c->cleanup_delay_ms % 1000u) * 1000000L
        };
        (void)nanosleep(&pause, NULL);
    }
    oa_cec_owner *owner = c->cleanup_owner;
    c->cleanup_owner = NULL;
    int rc = finish_owner_ptr(owner);
    if (rc == -EAGAIN) c->cleanup_owner = owner;
    if (c->cleanup_authority) {
        oa_cec_authority_mem_close(&c->authority);
        c->authority_open = 0;
        c->cleanup_authority = 0;
    }
    c->cleanup_result = rc;
    return NULL;
}

static int start(void *ud)
{
    struct ctx *c = ud;
    if (c->owner || c->cleanup_running || c->cleanup_owner) return -EBUSY;
    c->quiescing = 0;
    c->cleanup_result = 0;
    c->cleanup_authority = 0;
    c->job_pending = 0;
    c->opening = 0;
    c->session_open = 0;
    c->closing = 0;
    c->last_error = 0;
    c->rx_error_logged = 0;
    c->rx_frames = 0;
    c->tx_frames = 0;
    c->have_last_rx = 0;
    c->reply_head = c->reply_tail = 0;
    memset(&c->last_rx, 0, sizeof c->last_rx);
    long delay = atol(c->api->config_get(c->api->priv,
                                         "cec.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;

    const char *enable = c->api->config_get(c->api->priv, "cec.enable", "0");
    c->enabled = enable && (!strcmp(enable, "1") || !strcmp(enable, "true"));
    if (c->enabled) {
        const char *path = c->api->config_get(c->api->priv, "cec.device",
                                              "/tmp/ntkcec");
        if (!path || !*path || strlen(path) >= sizeof c->device_path) {
            c->api->log(c->api->priv, OA_LOG_ERROR,
                        "cec: invalid cec.device");
            return -EINVAL;
        }
        snprintf(c->device_path, sizeof c->device_path, "%s", path);
        c->authority = (oa_cec_authority_mem)OA_CEC_AUTHORITY_MEM_INIT;
        if (oa_cec_authority_mem_open(&c->authority, NULL) != 0) {
            c->api->log(c->api->priv, OA_LOG_ERROR,
                        "cec: authority mirror map failed (%d)",
                        errno ? errno : EIO);
            c->enabled = 0;
            return -1;
        }
        c->authority_open = 1;
    }

    if (oa_cec_owner_create(&c->owner, settled, c) != 0) {
        oa_cec_authority_mem_close(&c->authority);
        c->authority_open = 0;
        return -1;
    }
    if (c->api->publish(c->api->priv, OA_SVC_CEC_OWNER, 1, &service) != 0) {
        oa_cec_authority_mem_close(&c->authority);
        c->authority_open = 0;
        return -1;
    }
    c->owner_published = 1;
    if (c->api->publish(c->api->priv, OA_SVC_CEC_AUDIO, 1, &audio_service) != 0) {
        oa_cec_authority_mem_close(&c->authority);
        c->authority_open = 0;
        return -1;
    }
    c->audio_published = 1;

    if (c->enabled) {
        long poll = atol(c->api->config_get(c->api->priv,
                                            "cec.poll_ms", "100"));
        if (poll < 20) poll = 20;
        if (poll > 1000) poll = 1000;
        c->poll_timer = c->api->timer_add(c->api->priv, 0, (unsigned)poll,
                                          cec_poll, c);
        if (!c->poll_timer) {
            c->api->log(c->api->priv, OA_LOG_ERROR,
                        "cec: poll timer unavailable");
            return -1;
        }
        if (begin_acquire(c) != 0)
            c->api->log(c->api->priv, OA_LOG_ERROR,
                        "cec: acquire submission failed (%d)",
                        c->last_error);
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec: hardware enabled (%s, poll %ld ms)",
                    c->device_path, poll);
    }
    return 0;
}
static int quiesce(void *ud)
{
    struct ctx *c = ud;
    if (!c) return 0;
    if (c->poll_timer) {
        c->api->timer_del(c->api->priv, c->poll_timer);
        c->poll_timer = 0;
    }
    if (c->cleanup_running) {
        for (int i = 0; i < 10; i++) {
            int rc = pthread_tryjoin_np(c->cleanup_thread, NULL);
            if (rc == 0) {
                c->cleanup_running = 0;
                int result = c->cleanup_result;
                c->cleanup_result = 0;
                if (result == 0) return 0;
                if (result == -EAGAIN) return -EAGAIN;
                c->api->log(c->api->priv, OA_LOG_ERROR,
                            "cec: owner cleanup failed (%d)", result);
                return -EIO;
            }
            if (rc != EBUSY) {
                c->cleanup_running = 0;
                return -EIO;
            }
            const struct timespec pause = { .tv_nsec = 5000000 };
            (void)nanosleep(&pause, NULL);
        }
        return -EAGAIN;
    }

    int owner_published = c->owner_published;
    int audio_published = c->audio_published;
    c->owner_published = 0;
    c->audio_published = 0;
    if (owner_published)
        c->api->unpublish(c->api->priv, OA_SVC_CEC_OWNER);
    if (audio_published)
        c->api->unpublish(c->api->priv, OA_SVC_CEC_AUDIO);
    c->enabled = 0;
    c->quiescing = 1;
    c->cleanup_authority = c->authority_open ? 1 : 0;
    if (c->owner) {
        c->cleanup_owner = c->owner;
        c->owner = NULL;
    }
    if (!c->cleanup_owner) {
        if (c->cleanup_authority) {
            oa_cec_authority_mem_close(&c->authority);
            c->authority_open = 0;
            c->cleanup_authority = 0;
        }
        return 0;
    }
    c->cleanup_result = 0;
    int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
    if (rc) {
        oa_cec_owner *owner = c->cleanup_owner;
        c->cleanup_owner = NULL;
        int result = finish_owner_ptr(owner);
        if (result == -EAGAIN) {
            c->cleanup_owner = owner;
            return -EAGAIN;
        }
        return result ? -EIO : 0;
    }
    c->cleanup_running = 1;
    return -EAGAIN;
}

/* Idle workers exit within one generation. A failed hardware close keeps the
 * session: finish_owner_ptr returns -EAGAIN and leaves the owner for a later
 * quiesce generation. */
static int finish_owner(struct ctx *c)
{
    if (!c) return 0;
    if (c->cleanup_running || c->cleanup_owner) return -1;
    oa_cec_owner *owner = c->owner;
    c->owner = NULL;
    int rc = finish_owner_ptr(owner);
    if (rc == -EAGAIN) c->owner = owner;
    return rc == 0 ? 0 : -1;
}

static int suspend(void *ud)
{
    struct ctx *c = ud;
    if (!c) return -1;
    if (c->poll_timer) {
        c->api->timer_del(c->api->priv, c->poll_timer);
        c->poll_timer = 0;
    }
    int rc = finish_owner(c);
    if (rc == 0 && c->authority_open) {
        oa_cec_authority_mem_close(&c->authority);
        c->authority_open = 0;
    }
    return rc;
}
static int resume(void *ud)
{
    struct ctx *c = ud;
    if (!c || c->cleanup_running || c->cleanup_owner) return -1;
    c->quiescing = 0;
    if (!c->owner && oa_cec_owner_create(&c->owner, settled, c) != 0)
        return -1;
    if (!c->enabled) return 0;
    if (!c->authority_open) {
        c->authority = (oa_cec_authority_mem)OA_CEC_AUTHORITY_MEM_INIT;
        if (oa_cec_authority_mem_open(&c->authority, NULL) != 0)
            return -1;
        c->authority_open = 1;
    }
    if (!c->poll_timer) {
        long poll = atol(c->api->config_get(c->api->priv,
                                            "cec.poll_ms", "100"));
        if (poll < 20) poll = 20;
        if (poll > 1000) poll = 1000;
        c->poll_timer = c->api->timer_add(c->api->priv, 0, (unsigned)poll,
                                          cec_poll, c);
        if (!c->poll_timer) return -1;
    }
    if (!c->session_open && !c->opening && !c->job_pending)
        (void)begin_acquire(c);
    return 0;
}

static void stop(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    if (c->poll_timer) {
        c->api->timer_del(c->api->priv, c->poll_timer);
        c->poll_timer = 0;
    }
    c->quiescing = 1;
    int owner_published = c->owner_published;
    int audio_published = c->audio_published;
    c->owner_published = 0;
    c->audio_published = 0;
    if (c->api && owner_published)
        c->api->unpublish(c->api->priv, OA_SVC_CEC_OWNER);
    if (c->api && audio_published)
        c->api->unpublish(c->api->priv, OA_SVC_CEC_AUDIO);
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->cleanup_owner) {
        int rc = finish_owner_ptr(c->cleanup_owner);
        if (rc == 0) c->cleanup_owner = NULL;
    }
    if (c->owner) {
        oa_cec_owner *owner = c->owner;
        c->owner = NULL;
        int rc = finish_owner_ptr(owner);
        if (rc == -EAGAIN) c->owner = owner;
    }
    if (c->authority_open) {
        oa_cec_authority_mem_close(&c->authority);
        c->authority_open = 0;
    }
    c->enabled = 0;
    c->cleanup_authority = 0;
}

static void destroy(void *ud)
{
    struct ctx *c = ud;
    if (c->poll_timer && c->api) {
        c->api->timer_del(c->api->priv, c->poll_timer);
        c->poll_timer = 0;
    }
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->cleanup_owner) {
        int rc = finish_owner_ptr(c->cleanup_owner);
        if (rc == 0) c->cleanup_owner = NULL;
    }
    if (c->owner) {
        int rc = finish_owner_ptr(c->owner);
        if (rc == 0) c->owner = NULL;
    }
    if (c->authority_open) {
        oa_cec_authority_mem_close(&c->authority);
        c->authority_open = 0;
    }
    if (g_ctx == c) g_ctx = NULL;
    if (c->owner || c->cleanup_owner) {
        /* A retained hardware close must not be freed or unloaded underneath
         * its worker. The normal stop path waits for quiesce to return 0; a
         * force unload with a retained owner is outside the safe contract. */
        return;
    }
    oa_cec_policy_destroy(c->policy);
    free(c);
}

static size_t save(void *ud, void *buf, size_t cap)
{
    struct ctx *c = ud;
    return c ? oa_cec_policy_save(c->policy, buf, cap) : 0;
}
static int restore(void *ud, const void *buf, size_t len)
{
    struct ctx *c = ud;
    return c ? oa_cec_policy_restore(c->policy, buf, len) : -1;
}
static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "cec", .version = "1",
    .init = init, .start = start, .stop = stop, .destroy = destroy,
    .suspend = suspend, .resume = resume, .quiesce = quiesce,
    .save = save, .restore = restore
};
OA_MODULE_EXPORT(descriptor)
