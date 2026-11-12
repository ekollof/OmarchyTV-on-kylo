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
#include "openaurora/broadcast.h"
#include "openaurora/cec.h"
#include "openaurora/cec_authority_mem.h"
#include "openaurora/cec_module_transport.h"
#include "openaurora/cec_owner.h"
#include "openaurora/cec_policy.h"
#include "openaurora/channels.h"
#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/tuner_view.h"

struct ctx {
    const oa_host_api *api;
    oa_cec_owner *owner;
    oa_cec_policy *policy;
    uint32_t id;
    int owner_published;
    int audio_published;
    int inactive_enabled; /* cec.inactive_fallback */
    int active_source_enabled; /* cec.active_source */
    int standby_enabled; /* cec.standby */
    int identity_enabled; /* cec.identity */
    int power_status_enabled; /* cec.power_status */
    int rcp_enabled; /* cec.rcp: active-source volume/mute */
    int rcp_muted;   /* local CEC mute toggle state */
    int active_logical;  /* last valid 0x82 initiator, -1 unknown */
    uint16_t active_pa;  /* last valid 0x82 physical address, 0 unknown */
    uint16_t device_pa[16];   /* 0x84 PA per logical address */
    uint8_t device_type[16];  /* 0x84 device type per logical address */
    char osd_name[15];   /* cec.osd_name, 1..14 printable ASCII */

    /* Optional hardware transport (cec.enable=1). */
    int enabled;
    const oa_cec_module_transport *transport;
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
    /* User sends are queued when the single owner slot is occupied by the
     * receive poll; the poll drains this ring before replies/receive. */
    oa_cec_message tx_q[4];
    unsigned tx_head;
    unsigned tx_tail;

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
    if (c->closing) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "cec send rejected: transport closing");
        return -EBUSY;
    }
    if (!message || message->initiator != 0) return -EINVAL;
    uint8_t wire[OA_CEC_MAX_FRAME];
    size_t length = 0;
    if (oa_cec_encode(message, wire, sizeof wire, &length) != 0)
        return -EINVAL;
    unsigned next = (c->tx_tail + 1u) & 3u;
    if (next == c->tx_head) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "cec send rejected: tx queue full (head %u tail %u)",
                    c->tx_head, c->tx_tail);
        return -EBUSY;
    }
    c->tx_q[c->tx_tail] = *message;
    c->tx_tail = next;
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
    if (c->job_pending || c->tx_head != c->tx_tail) return -EBUSY;
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

static void drop_lookup(struct ctx *c, const char *name, const void *iface)
{
    if (iface && c->api->release)
        c->api->release(c->api->priv, name);
}

/* Recovered stock route: SvgAppCECEventsListener handles broadcast 0x9D
 * <Inactive Source> by posting UIKeyPressMessage(597); the UI selects DTV and
 * the saved tuner service. This module reproduces the source/tuner effect,
 * not the UI key. Gate on cec.inactive_fallback, require an external source
 * to be displayed, and resolve the saved service handle back to its active
 * list position. No saved/known service means no guessed fallback. */
static void handle_inactive_source(struct ctx *c, const oa_cec_message *m)
{
    if (!c || !c->inactive_enabled || !m || m->opcode != 0x9d) return;
    /* CEC 1.3a: Inactive Source is broadcast by the device that was active.
     * Ignore our own LA and directed frames. */
    if (m->initiator == 0 || m->destination != 15) return;

    const oa_source_iface *source =
        c->api->lookup(c->api->priv, OA_SVC_SOURCE, 3);
    if (!source || !source->inactive_source || !source->current_type) {
        drop_lookup(c, OA_SVC_SOURCE, source);
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "cec 0x9d ignored: source service v3 unavailable");
        return;
    }
    int current = source->current_type();
    /* 0 = TV, 25 = disconnect VPLT, -1 = service down. The worker must only
     * run while an external input is actually displayed. */
    if (current <= 0 || current >= 25) {
        drop_lookup(c, OA_SVC_SOURCE, source);
        return;
    }

    const oa_channel_iface *channels =
        c->api->lookup(c->api->priv, OA_SVC_CHANNELS, 1);
    const oa_service_list_iface *services =
        c->api->lookup(c->api->priv, OA_SVC_SERVICE_LIST, 1);
    const oa_tuner_iface *tuner =
        c->api->lookup(c->api->priv, OA_SVC_TUNER, 1);
    uint32_t handle = 0;
    int position = -1;
    const struct oa_channel_snapshot *snapshot = NULL;
    const struct oa_service_list *service_list = NULL;
    if (channels && channels->snapshot) snapshot = channels->snapshot();
    if (services && services->snapshot) service_list = services->snapshot();
    if (tuner && tuner->selected) handle = tuner->selected();

    int rc = -ENOENT;
    if (handle) {
        if (snapshot &&
            oa_tuner_position_of_handle(snapshot, handle, &position) == 0)
            rc = source->inactive_source(position);
        else if (service_list &&
                 oa_tuner_position_of_service_handle(service_list, NULL,
                                                     service_list->count,
                                                     handle, &position) == 0)
            rc = source->inactive_source(position);
    }

    if (rc == 0)
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x9d inactive source: external %d handle %u "
                    "position %d selected", current, handle, position);
    else
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x9d inactive source skipped (%d): external %d "
                    "handle %u position %d", rc, current, handle, position);

    drop_lookup(c, OA_SVC_TUNER, tuner);
    drop_lookup(c, OA_SVC_SERVICE_LIST, services);
    drop_lookup(c, OA_SVC_CHANNELS, channels);
    drop_lookup(c, OA_SVC_SOURCE, source);
}

/* CEC 1.3a <Active Source> carries the source's physical address. The HDMI
 * input is the top PA nibble: 1..4 -> HDMI1..HDMI4. The vendor
 * NovoCECController::selectSource maps the CEC logical address instead; using
 * the PA is the specification-correct behavior. Gate on cec.active_source. */
static int active_source_type(uint16_t pa)
{
    switch ((pa >> 12) & 15u) {
    case 1: return 17; /* HDMI1 */
    case 2: return 18; /* HDMI2 */
    case 3: return 19; /* HDMI3 */
    case 4: return 22; /* HDMI4 */
    default: return -1;
    }
}

static void handle_active_source(struct ctx *c, const oa_cec_message *m)
{
    if (!c || !c->active_source_enabled || !m || m->opcode != 0x82) return;
    if (m->initiator == 0 || m->destination != 15 || m->count != 2) return;

    uint16_t pa = (uint16_t)((m->operands[0] << 8) | m->operands[1]);
    if (!oa_cec_physical_address_valid(pa)) {
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x82 active source ignored: invalid pa 0x%04x", pa);
        return;
    }
    int type = active_source_type(pa);
    if (type < 0) {
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x82 active source ignored: pa 0x%04x has no "
                    "mapped HDMI input", pa);
        return;
    }
    c->active_logical = m->initiator;
    c->active_pa = pa;

    const oa_source_iface *source =
        c->api->lookup(c->api->priv, OA_SVC_SOURCE, 1);
    if (!source || !source->select_type) {
        drop_lookup(c, OA_SVC_SOURCE, source);
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "cec 0x82 ignored: source service unavailable");
        return;
    }
    int rc = source->select_type(type);
    if (rc == 0)
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x82 active source: pa 0x%04x type %d selected",
                    pa, type);
    else
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x82 active source: pa 0x%04x type %d failed (%d)",
                    pa, type, rc);
    drop_lookup(c, OA_SVC_SOURCE, source);
}

/* CEC 1.3a <Routing Change> carries the old and new physical addresses.
 * Follow it only when the old PA is the source recorded from 0x82, so a
 * stale routing message cannot steal the screen. */
static void handle_routing_change(struct ctx *c, const oa_cec_message *m)
{
    if (!c || !c->active_source_enabled || !m || m->opcode != 0x80) return;
    if (m->destination != 15 || m->initiator == 0 || m->count != 4) return;

    uint16_t old_pa = (uint16_t)((m->operands[0] << 8) | m->operands[1]);
    uint16_t new_pa = (uint16_t)((m->operands[2] << 8) | m->operands[3]);
    if (!oa_cec_physical_address_valid(old_pa) ||
        !oa_cec_physical_address_valid(new_pa)) {
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x80 routing ignored: invalid pa 0x%04x -> 0x%04x",
                    old_pa, new_pa);
        return;
    }
    if (c->active_pa == 0 || old_pa != c->active_pa) {
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x80 routing ignored: old pa 0x%04x is not active",
                    old_pa);
        return;
    }
    int type = active_source_type(new_pa);
    if (type < 0) {
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x80 routing ignored: new pa 0x%04x has no "
                    "mapped HDMI input", new_pa);
        return;
    }

    const oa_source_iface *source =
        c->api->lookup(c->api->priv, OA_SVC_SOURCE, 1);
    if (!source || !source->select_type) {
        drop_lookup(c, OA_SVC_SOURCE, source);
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "cec 0x80 ignored: source service unavailable");
        return;
    }
    int rc = source->select_type(type);
    if (rc == 0) {
        c->active_pa = new_pa;
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x80 routing: old 0x%04x new 0x%04x type %d selected",
                    old_pa, new_pa, type);
    } else {
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x80 routing: old 0x%04x new 0x%04x type %d failed (%d)",
                    old_pa, new_pa, type, rc);
    }
    drop_lookup(c, OA_SVC_SOURCE, source);
}

/* CEC one-touch play: Image View On / Text View On should switch to the
 * requesting device's HDMI input when its PA was reported by 0x84. The module
 * only runs while the TV is on, so this is the source-selection half; waking
 * from standby remains a power-path task. */
static void handle_image_view(struct ctx *c, const oa_cec_message *m)
{
    if (!c || !c->active_source_enabled || !m) return;
    if (m->opcode != 0x04 && m->opcode != 0x0d) return;
    if (m->destination != 0 && m->destination != 15) return;
    if (m->initiator == 0 || m->initiator == 15 || m->count != 0) return;

    uint16_t pa = c->device_pa[m->initiator];
    if (pa == 0) return;
    int type = active_source_type(pa);
    if (type < 0) {
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x%02x view ignored: pa 0x%04x has no mapped "
                    "HDMI input", m->opcode, pa);
        return;
    }
    const oa_source_iface *source =
        c->api->lookup(c->api->priv, OA_SVC_SOURCE, 1);
    if (!source || !source->select_type) {
        drop_lookup(c, OA_SVC_SOURCE, source);
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "cec 0x%02x view ignored: source service unavailable",
                    m->opcode);
        return;
    }
    int rc = source->select_type(type);
    if (rc == 0) {
        c->active_pa = pa;
        c->active_logical = m->initiator;
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x%02x %s view: la %u pa 0x%04x type %d selected",
                    m->opcode, m->opcode == 0x04 ? "image" : "text",
                    m->initiator, pa, type);
    } else {
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x%02x %s view: la %u pa 0x%04x type %d failed (%d)",
                    m->opcode, m->opcode == 0x04 ? "image" : "text",
                    m->initiator, pa, type, rc);
    }
    drop_lookup(c, OA_SVC_SOURCE, source);
}

/* CEC 1.3a <Standby> is broadcast and is accepted only from the device that
 * announced itself as the active source. The vendor controller checks
 * getActiveLogicalDevice/CEC-enabled and notifies listeners; the replacement
 * applies the spec policy directly through OA_SVC_POWER behind cec.standby. */
static void handle_standby(struct ctx *c, const oa_cec_message *m)
{
    if (!c || !c->standby_enabled || !m || m->opcode != 0x36) return;
    if (m->destination != 15 || m->initiator == 0 || m->count != 0) return;
    if (c->active_logical < 0 || m->initiator != c->active_logical) return;

    const oa_power_iface *power =
        c->api->lookup(c->api->priv, OA_SVC_POWER, 1);
    if (!power || !power->standby || !power->is_available ||
        !power->is_available()) {
        drop_lookup(c, OA_SVC_POWER, power);
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "cec 0x36 standby ignored: power unavailable");
        return;
    }
    int rc = power->standby();
    if (rc == 0)
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x36 standby: la %u accepted", m->initiator);
    else
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "cec 0x36 standby: la %u failed (%d)", m->initiator, rc);
    drop_lookup(c, OA_SVC_POWER, power);
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

/* Standard CEC identity queries. 0x84 is broadcast (PA + device type TV);
 * 0x85 asks who owns the screen, so the TV answers 0x82 0x0000 only while the
 * tuner is displayed. 0x84 reports are logged for the device table. */
static void handle_identity(struct ctx *c, const oa_cec_message *m)
{
    if (!c || !m) return;
    if (m->initiator == 0 || m->initiator == 15) return;

    if (m->opcode == 0x84) {
        if (m->count != 3) return;
        uint16_t pa = (uint16_t)((m->operands[0] << 8) | m->operands[1]);
        if (!oa_cec_physical_address_valid(pa)) return;
        c->device_pa[m->initiator] = pa;
        c->device_type[m->initiator] = m->operands[2];
        if (!c->identity_enabled) return;
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x84 la %u pa 0x%04x type %u",
                    m->initiator, pa, (unsigned)m->operands[2]);
        return;
    }
    if (!c->identity_enabled) return;

    if (m->opcode == 0x83) {
        if (m->count != 0) return;
        if (m->destination != 0 && m->destination != 15) return;
        oa_cec_message reply = {
            .initiator = 0, .destination = 15, .opcode = 0x84,
            .count = 3, .operands = { 0x00, 0x00, 0x00 }
        };
        queue_replies(c, &reply, 1);
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x83 -> 0x84 pa 0x0000 type 0");
        return;
    }
    if (m->opcode == 0x85) {
        if (m->destination != 15 || m->count != 0) return;
        const oa_source_iface *source =
            c->api->lookup(c->api->priv, OA_SVC_SOURCE, 1);
        int current = source && source->current_type
            ? source->current_type() : -1;
        drop_lookup(c, OA_SVC_SOURCE, source);
        if (current != 0) return; /* an external source owns the screen */
        oa_cec_message reply = {
            .initiator = 0, .destination = 15, .opcode = 0x82,
            .count = 2, .operands = { 0x00, 0x00 }
        };
        queue_replies(c, &reply, 1);
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x85 -> 0x82 pa 0x0000");
        return;
    }
}

/* 0x8F Give Device Power Status is directed to the TV; reply 0x90 with the
 * running state ("on"). The module only receives while the TV is powered.
 * 0x90 reports from other devices are logged for the device table. */
static void handle_power_status(struct ctx *c, const oa_cec_message *m)
{
    if (!c || !c->power_status_enabled || !m) return;
    if (m->opcode == 0x8f) {
        if (m->destination != 0 || m->initiator == 0 || m->initiator == 15 ||
            m->count != 0) return;
        oa_cec_message reply = {
            .initiator = 0, .destination = m->initiator, .opcode = 0x90,
            .count = 1, .operands = { 0x00 } /* on */
        };
        queue_replies(c, &reply, 1);
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x8f -> 0x90 status 0");
        return;
    }
    if (m->opcode == 0x90) {
        if (m->count != 1 || m->initiator == 0 || m->initiator == 15) return;
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x90 la %u status %u",
                    m->initiator, (unsigned)m->operands[0]);
    }
}

/* CEC OSD-name exchange. The vendor stores incoming 0x47 names; the
 * replacement also answers 0x46 Give OSD Name with a directed 0x47 carrying
 * the configured TV name (1..14 printable ASCII). Gate with cec.identity. */
static void handle_osd_name(struct ctx *c, const oa_cec_message *m)
{
    if (!c || !c->identity_enabled || !m) return;
    if (m->initiator == 0 || m->initiator == 15) return;

    if (m->opcode == 0x46) {
        if (m->count != 0) return;
        if (m->destination != 0 && m->destination != 15) return;
        size_t n = strlen(c->osd_name);
        if (n > OA_CEC_MAX_OPERANDS) n = OA_CEC_MAX_OPERANDS;
        oa_cec_message reply = {
            .initiator = 0, .destination = m->initiator, .opcode = 0x47,
            .count = (uint8_t)n
        };
        memcpy(reply.operands, c->osd_name, n);
        queue_replies(c, &reply, 1);
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x46 -> 0x47 name %s", c->osd_name);
        return;
    }
    if (m->opcode == 0x47) {
        if (m->destination != 0 || m->count < 1 || m->count > 14) return;
        for (size_t i = 0; i < m->count; i++)
            if (m->operands[i] < 0x20 || m->operands[i] > 0x7e) return;
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec 0x47 la %u name %.*s", m->initiator,
                    (int)m->count, (const char *)m->operands);
    }
}

/* CEC user-control codes -> the RC ids the UI/IR path already consumes:
 * Select 53, Up 20, Down 19, Left 21, Right 22, Exit 10. */
static int cec_key_id(unsigned code)
{
    switch (code) {
    case 0x00: return 53; /* Select */
    case 0x01: return 20; /* Up */
    case 0x02: return 19; /* Down */
    case 0x03: return 21; /* Left */
    case 0x04: return 22; /* Right */
    case 0x0d: return 10; /* Exit */
    default: return -1;
    }
}

/* CEC 0x44/0x45 user control: the active source's volume/mute and navigation
 * keys. The vendor maps 0x41..0x43 through its key table; this module sends
 * volume/mute to OA_SVC_AUDIO and the standard navigation codes to the UI as
 * the same RC key events mod_ir emits. */
static void handle_rcp(struct ctx *c, const oa_cec_message *m)
{
    if (!c || !c->rcp_enabled || !m) return;
    if (m->opcode != 0x44 && m->opcode != 0x45) return;
    if (m->destination != 0 && m->destination != 15) return;
    if (m->initiator == 0 || m->initiator == 15) return;
    if (c->active_logical < 0 || m->initiator != c->active_logical) return;
    if (m->count != 1) return;

    if (m->opcode == 0x45) {
        int key = cec_key_id(m->operands[0]);
        if (key >= 0 && c->api->emit) {
            oa_key_event ev = {
                .id = (uint8_t)key, .kind = OA_KEY_RELEASE, .ts_ms = 0
            };
            c->api->emit(c->api->priv, OA_TOPIC_KEY, &ev);
            c->api->log(c->api->priv, OA_LOG_INFO,
                        "cec 0x45 release key %d la %u",
                        key, m->initiator);
        } else {
            c->api->log(c->api->priv, OA_LOG_INFO,
                        "cec 0x45 release code 0x%02x la %u",
                        m->operands[0], m->initiator);
        }
        return;
    }

    unsigned code = m->operands[0];
    if (code != 0x41 && code != 0x42 && code != 0x43) {
        int key = cec_key_id(code);
        if (key >= 0 && c->api->emit) {
            oa_key_event ev = {
                .id = (uint8_t)key, .kind = OA_KEY_PRESS, .ts_ms = 0
            };
            c->api->emit(c->api->priv, OA_TOPIC_KEY, &ev);
            c->api->log(c->api->priv, OA_LOG_INFO,
                        "cec 0x44 key %d la %u", key, m->initiator);
        } else {
            c->api->log(c->api->priv, OA_LOG_INFO,
                        "cec 0x44 code 0x%02x la %u ignored",
                        code, m->initiator);
        }
        return;
    }

    const oa_audio_iface *audio =
        c->api->lookup(c->api->priv, OA_SVC_AUDIO, 1);
    if (!audio || !audio->is_available || !audio->is_available() ||
        !audio->set_volume || !audio->set_mute) {
        drop_lookup(c, OA_SVC_AUDIO, audio);
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "cec 0x44 ignored: audio service unavailable");
        return;
    }

    if (code == 0x43) {
        int next = !c->rcp_muted;
        int rc = audio->set_mute(next);
        if (rc == 0) {
            c->rcp_muted = next;
            c->api->log(c->api->priv, OA_LOG_INFO,
                        "cec 0x44 mute -> %d", next);
        } else {
            c->api->log(c->api->priv, OA_LOG_INFO,
                        "cec 0x44 mute failed (%d)", rc);
        }
    } else {
        int volume = audio->get_volume ? audio->get_volume() : -1;
        if (volume < 0 || volume > 100) {
            c->api->log(c->api->priv, OA_LOG_INFO,
                        "cec 0x44 volume ignored: current volume unknown");
        } else {
            int next = code == 0x41 ? volume + 1 : volume - 1;
            if (next < 0) next = 0;
            if (next > 100) next = 100;
            int rc = audio->set_volume(next);
            if (rc == 0)
                c->api->log(c->api->priv, OA_LOG_INFO,
                            "cec 0x44 volume %s -> %d",
                            code == 0x41 ? "up" : "down", next);
            else
                c->api->log(c->api->priv, OA_LOG_INFO,
                            "cec 0x44 volume %s failed (%d)",
                            code == 0x41 ? "up" : "down", rc);
        }
    }
    drop_lookup(c, OA_SVC_AUDIO, audio);
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
            handle_inactive_source(c, &comp->message);
            handle_active_source(c, &comp->message);
            handle_routing_change(c, &comp->message);
            handle_image_view(c, &comp->message);
            handle_standby(c, &comp->message);
            handle_identity(c, &comp->message);
            handle_power_status(c, &comp->message);
            handle_osd_name(c, &comp->message);
            handle_rcp(c, &comp->message);
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
            c->reply_head = c->reply_tail = 0;
            c->tx_head = c->tx_tail = 0;
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
    if (c->session_open && !c->closing && c->tx_head != c->tx_tail) {
        oa_cec_message message = c->tx_q[c->tx_head];
        uint64_t ticket = 0;
        if (oa_cec_owner_send_async(c->owner, &message, &ticket) == 0) {
            c->tx_head = (c->tx_head + 1u) & 3u;
            c->job_pending = 1;
        } else {
            c->last_error = errno ? errno : EIO;
            /* A busy owner is a race with the completion slot; retry next
             * poll. A permanent encode/address error drops only this frame. */
            if (errno != EBUSY)
                c->tx_head = (c->tx_head + 1u) & 3u;
        }
        return;
    }
    if (c->session_open && c->reply_head != c->reply_tail) {
        oa_cec_message message = c->reply_q[c->reply_head];
        uint64_t ticket = 0;
        if (oa_cec_owner_send_async(c->owner, &message, &ticket) == 0) {
            c->reply_head = (c->reply_head + 1u) & 3u;
            c->job_pending = 1;
        } else {
            c->last_error = errno ? errno : EIO;
            /* Drop only the failed frame; later replies keep their order and
             * are attempted on the next poll. Never send replies after close. */
            c->reply_head = (c->reply_head + 1u) & 3u;
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
    const oa_cec_device_io *device =
        c->transport && c->transport->device ? c->transport->device
                                             : &native_device_io;
    oa_cec_acquire request = {
        .authority = &c->authority.authority,
        .device = device,
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

static void stop_poll(struct ctx *c)
{
    if (c->poll_timer) {
        c->api->timer_del(c->api->priv, c->poll_timer);
        c->poll_timer = 0;
    }
}

static int start_poll(struct ctx *c)
{
    if (!c || !c->api) return -1;
    if (c->poll_timer) return 0;
    long poll = atol(c->api->config_get(c->api->priv, "cec.poll_ms", "100"));
    if (poll < 20) poll = 20;
    if (poll > 1000) poll = 1000;
    c->poll_timer = c->api->timer_add(c->api->priv, 0, (unsigned)poll,
                                      cec_poll, c);
    return c->poll_timer ? 0 : -1;
}

static void clear_session_state(struct ctx *c)
{
    c->opening = 0;
    c->session_open = 0;
    c->closing = 0;
    c->job_pending = 0;
    c->reply_head = c->reply_tail = 0;
    c->tx_head = c->tx_tail = 0;
}

/* Reap a cleanup generation without blocking the loop. Returns 0 when the
 * worker is joined, -EBUSY while it still runs, and the generation's error
 * otherwise. A retained owner remains for an explicit later retry. */
static int reap_cleanup(struct ctx *c)
{
    if (!c->cleanup_running) return 0;
    int rc = pthread_tryjoin_np(c->cleanup_thread, NULL);
    if (rc == EBUSY) return -EBUSY;
    if (rc) {
        c->cleanup_running = 0;
        return -EIO;
    }
    c->cleanup_running = 0;
    int result = c->cleanup_result;
    c->cleanup_result = 0;
    return result;
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
    /* The authority callbacks borrow this mapping through acquire. A retained
     * owner (failed/blocked close) may still be inside authority_ensure, so
     * unmap only after a successful join. A later generation retries. */
    if (c->cleanup_authority && rc == 0) {
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
    c->active_logical = -1;
    c->active_pa = 0;
    c->rcp_muted = 0;
    memset(c->device_pa, 0, sizeof c->device_pa);
    memset(c->device_type, 0, sizeof c->device_type);
    c->cleanup_result = 0;
    c->cleanup_authority = 0;
    c->transport = NULL;
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
    c->tx_head = c->tx_tail = 0;
    memset(&c->last_rx, 0, sizeof c->last_rx);
    clear_session_state(c);
    long delay = atol(c->api->config_get(c->api->priv,
                                         "cec.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;

    const char *enable = c->api->config_get(c->api->priv, "cec.enable", "0");
    c->enabled = enable && (!strcmp(enable, "1") || !strcmp(enable, "true"));
    const char *inactive = c->api->config_get(c->api->priv,
                                              "cec.inactive_fallback", "0");
    c->inactive_enabled = inactive &&
        (!strcmp(inactive, "1") || !strcmp(inactive, "true"));
    if (c->inactive_enabled)
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec: inactive-source fallback enabled (0x9d)");
    const char *active = c->api->config_get(c->api->priv,
                                            "cec.active_source", "0");
    c->active_source_enabled = active &&
        (!strcmp(active, "1") || !strcmp(active, "true"));
    if (c->active_source_enabled)
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec: active-source routing enabled (0x82)");
    const char *standby = c->api->config_get(c->api->priv,
                                             "cec.standby", "0");
    c->standby_enabled = standby &&
        (!strcmp(standby, "1") || !strcmp(standby, "true"));
    if (c->standby_enabled)
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec: standby routing enabled (0x36)");
    const char *identity = c->api->config_get(c->api->priv,
                                              "cec.identity", "0");
    c->identity_enabled = identity &&
        (!strcmp(identity, "1") || !strcmp(identity, "true"));
    if (c->identity_enabled)
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec: identity-query replies enabled (0x83/0x85)");
    const char *power_status = c->api->config_get(c->api->priv,
                                                  "cec.power_status", "0");
    c->power_status_enabled = power_status &&
        (!strcmp(power_status, "1") || !strcmp(power_status, "true"));
    if (c->power_status_enabled)
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec: power-status replies enabled (0x8f/0x90)");
    const char *rcp = c->api->config_get(c->api->priv, "cec.rcp", "0");
    c->rcp_enabled = rcp && (!strcmp(rcp, "1") || !strcmp(rcp, "true"));
    if (c->rcp_enabled)
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec: active-source volume/mute enabled (0x44/0x45)");
    const char *osd = c->api->config_get(c->api->priv, "cec.osd_name", "TV");
    size_t osd_len = 0;
    if (osd) {
        while (osd_len < 14 && osd[osd_len] >= 0x20 && osd[osd_len] <= 0x7e)
            osd_len++;
    }
    if (!osd || osd_len == 0 || osd[osd_len] != '\0') {
        memcpy(c->osd_name, "TV", 3);
    } else {
        memcpy(c->osd_name, osd, osd_len);
        c->osd_name[osd_len] = '\0';
    }
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
        c->transport = oa_cec_module_transport_get();
        const oa_cec_authority_mem_io *am =
            c->transport ? c->transport->authority_mem : NULL;
        if (oa_cec_authority_mem_open(&c->authority, am) != 0) {
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
        if (start_poll(c) != 0) {
            c->api->log(c->api->priv, OA_LOG_ERROR,
                        "cec: poll timer unavailable");
            return -1;
        }
        if (begin_acquire(c) != 0)
            c->api->log(c->api->priv, OA_LOG_ERROR,
                        "cec: acquire submission failed (%d)",
                        c->last_error);
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "cec: hardware enabled (%s)", c->device_path);
    }
    return 0;
}
static int quiesce(void *ud)
{
    struct ctx *c = ud;
    if (!c) return 0;
    stop_poll(c);
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
    c->inactive_enabled = 0;
    c->active_source_enabled = 0;
    c->standby_enabled = 0;
    c->identity_enabled = 0;
    c->power_status_enabled = 0;
    c->rcp_enabled = 0;
    c->active_logical = -1;
    c->active_pa = 0;
    c->rcp_muted = 0;
    memset(c->device_pa, 0, sizeof c->device_pa);
    memset(c->device_type, 0, sizeof c->device_type);
    c->quiescing = 1;
    clear_session_state(c);
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

static int suspend(void *ud)
{
    struct ctx *c = ud;
    if (!c) return -1;
    /* Reap a generation left by an earlier suspend attempt that timed out
     * waiting for the worker, so a retry does not stay stuck on
     * cleanup_running. A retained owner (failed close) needs quiesce/stop. */
    int prior = reap_cleanup(c);
    if (prior == -EBUSY || c->cleanup_running) return -EBUSY;
    if (prior != 0 || c->cleanup_owner) return -1;
    stop_poll(c);
    clear_session_state(c);
    c->active_logical = -1;
    c->active_pa = 0;
    c->rcp_muted = 0;
    memset(c->device_pa, 0, sizeof c->device_pa);
    memset(c->device_type, 0, sizeof c->device_type);
    if (c->owner) {
        c->quiescing = 1;
        c->cleanup_owner = c->owner;
        c->owner = NULL;
        c->cleanup_authority = c->authority_open ? 1 : 0;
        c->cleanup_result = 0;
        int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
        if (rc) {
            /* Rare fallback: finish synchronously rather than leaking. A
             * failed close (_EAGAIN) retains the owner for a later
             * generation, so keep the pointer and the mapping too. */
            int result = finish_owner_ptr(c->cleanup_owner);
            if (result != 0)
                return -1;
            c->cleanup_owner = NULL;
            if (c->authority_open) {
                oa_cec_authority_mem_close(&c->authority);
                c->authority_open = 0;
            }
            c->cleanup_authority = 0;
            return 0;
        }
        c->cleanup_running = 1;
        /* suspend() is synchronous in this host: no retry is provided for a
         * module left SUSPENDED. Let the cleanup worker own the blocking
         * join/close, but do not return success until it has finished. */
        for (int i = 0; i < 40; i++) {
            int rc = reap_cleanup(c);
            if (rc == 0) break;
            if (rc != -EBUSY) return -1;
            const struct timespec pause = { .tv_nsec = 5000000 };
            (void)nanosleep(&pause, NULL);
        }
        if (c->cleanup_running || c->cleanup_owner) return -EBUSY;
        return 0;
    }
    if (c->authority_open) {
        oa_cec_authority_mem_close(&c->authority);
        c->authority_open = 0;
    }
    c->cleanup_authority = 0;
    return 0;
}

static int resume(void *ud)
{
    struct ctx *c = ud;
    if (!c) return -1;
    int rc = reap_cleanup(c);
    if (rc != 0) return rc == -EBUSY ? -EBUSY : -EIO;
    if (c->cleanup_owner) return -EBUSY; /* retained generation needs a retry */
    c->quiescing = 0;
    if (!c->owner && oa_cec_owner_create(&c->owner, settled, c) != 0)
        return -1;
    if (!c->enabled) {
        clear_session_state(c);
        return 0;
    }
    if (!c->authority_open) {
        c->authority = (oa_cec_authority_mem)OA_CEC_AUTHORITY_MEM_INIT;
        if (oa_cec_authority_mem_open(&c->authority, NULL) != 0)
            return -1;
        c->authority_open = 1;
    }
    if (start_poll(c) != 0) return -1;
    clear_session_state(c);
    if (begin_acquire(c) != 0) return -1;
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
    /* Keep the authority page mapped while any owner generation is retained. */
    if (c->authority_open && !c->owner && !c->cleanup_owner) {
        oa_cec_authority_mem_close(&c->authority);
        c->authority_open = 0;
    }
    c->enabled = 0;
    c->inactive_enabled = 0;
    c->active_source_enabled = 0;
    c->standby_enabled = 0;
    c->identity_enabled = 0;
    c->power_status_enabled = 0;
    c->rcp_enabled = 0;
    c->active_logical = -1;
    c->active_pa = 0;
    c->rcp_muted = 0;
    memset(c->device_pa, 0, sizeof c->device_pa);
    memset(c->device_type, 0, sizeof c->device_type);
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
    if (g_ctx == c) g_ctx = NULL;
    if (c->owner || c->cleanup_owner) {
        /* A retained hardware close must not be freed or unloaded underneath
         * its worker. Keep the authority mapping alive too; a later quiesce
         * generation owns the close. */
        return;
    }
    if (c->authority_open) {
        oa_cec_authority_mem_close(&c->authority);
        c->authority_open = 0;
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
