/* CEC owner module. start() creates the worker and does not open a device.
 * hardware_open_supported() is 0: no mirror map, STBC IPC or ntkcec node is
 * installed. Acquire stays on the injected-I/O owner API used by tests. */
#include <errno.h>
#include <stdlib.h>
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
};
static struct ctx *g_ctx;

static void poke(void *ud) { (void)ud; }
static void settled(void *ud)
{
    struct ctx *c = ud;
    if (c && c->api) (void)c->api->post(c->api->priv, c->id, poke, c);
}
static int supported(void) { return 0; }
static const oa_cec_service service = { supported };
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
static int start(void *ud)
{
    struct ctx *c = ud;
    if (oa_cec_owner_create(&c->owner, settled, c) != 0) return -1;
    if (c->api->publish(c->api->priv, OA_SVC_CEC_OWNER, 1, &service) != 0) return -1;
    c->owner_published = 1;
    if (c->api->publish(c->api->priv, OA_SVC_CEC_AUDIO, 1, &audio_service) != 0) return -1;
    c->audio_published = 1;
    return 0;
}
static int quiesce(void *ud)
{
    struct ctx *c = ud;
    if (!c || !c->owner) return 0;
    return oa_cec_owner_quiesce(c->owner) == 0 ? 0 : 1;
}
/* Idle workers exit within one generation. A failed hardware close keeps the
 * session: this returns -1 and leaves the owner in place. */
static int finish_owner(struct ctx *c)
{
    if (!c || !c->owner) return 0;
    if (oa_cec_owner_quiesce(c->owner) != 0) return -1;
    if (oa_cec_owner_join(c->owner) != 0) return -1;
    oa_cec_owner_destroy(c->owner);
    c->owner = NULL;
    return 0;
}
static int suspend(void *ud) { return finish_owner(ud); }
static int resume(void *ud)
{
    struct ctx *c = ud;
    if (!c) return -1;
    if (c->owner) return 0;
    return oa_cec_owner_create(&c->owner, settled, c) == 0 ? 0 : -1;
}
static void stop(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    (void)finish_owner(c);
    if (c->api && c->owner_published) c->api->unpublish(c->api->priv, OA_SVC_CEC_OWNER);
    if (c->api && c->audio_published) c->api->unpublish(c->api->priv, OA_SVC_CEC_AUDIO);
    c->owner_published = 0;
    c->audio_published = 0;
}
static void destroy(void *ud)
{
    struct ctx *c = ud;
    if (g_ctx == c) g_ctx = NULL;
    if (c) oa_cec_policy_destroy(c->policy);
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
