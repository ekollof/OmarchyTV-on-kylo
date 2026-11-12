/* Host fixture for the CEC 0x9D route: publishes source v3, channels and
 * tuner services. The fake source records the position selected by mod_cec;
 * it does not touch hardware, DTV or a real tuner. */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/broadcast.h"
#include "openaurora/channels.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    int source_published;
    int channels_published;
    int tuner_published;
    int external_type;
    uint32_t selected_handle;
    struct oa_channel_entry entries[3];
    struct oa_channel_snapshot snapshot;
};

static struct ctx *g_ctx;

static int fake_select_type(int type)
{
    (void)type;
    return 0;
}
static int fake_current_type(void)
{
    return g_ctx ? g_ctx->external_type : -1;
}
static int fake_available(void)
{
    return g_ctx ? 1 : 0;
}
static int fake_diag(int *drm_dev, int *plane, unsigned *info4,
                     unsigned *fd9)
{
    (void)drm_dev; (void)plane; (void)info4; (void)fd9;
    return -1;
}
static int fake_video_status(void)
{
    return -1;
}
static int fake_inactive_source(int position)
{
    if (!g_ctx) return -ENODEV;
    g_ctx->api->log(g_ctx->api->priv, OA_LOG_INFO,
                    "fake source inactive position %d", position);
    return 0;
}
static const oa_source_iface source_iface = {
    fake_select_type, fake_current_type, fake_available, fake_diag,
    fake_video_status, fake_inactive_source
};

static const struct oa_channel_snapshot *fake_snapshot(void)
{
    return g_ctx ? &g_ctx->snapshot : NULL;
}
static int fake_ingest(const char *xml, size_t len)
{
    (void)xml; (void)len;
    return -1;
}
static void fake_clear(void)
{
}
static const oa_channel_iface channels_iface = {
    fake_snapshot, fake_ingest, fake_clear
};

static int fake_status(struct oa_tuner_status *out)
{
    (void)out;
    return -1;
}
static int fake_scan_start(uint8_t network, int clear_existing)
{
    (void)network; (void)clear_existing;
    return -1;
}
static int fake_scan_cancel(void)
{
    return -1;
}
static int fake_select(uint32_t service_handle)
{
    (void)service_handle;
    return 0;
}
static uint32_t fake_selected(void)
{
    return g_ctx ? g_ctx->selected_handle : 0;
}
static const oa_tuner_iface tuner_iface = {
    fake_status, fake_scan_start, fake_scan_cancel, fake_select, fake_selected
};

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof *api || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api;
    c->external_type = atoi(api->config_get(api->priv,
                                            "fake.source_external", "17"));
    c->selected_handle = (uint32_t)strtoul(api->config_get(
        api->priv, "fake.tuner_selected", "200"), NULL, 10);
    for (size_t i = 0; i < 3; i++)
        c->entries[i].handle = (uint32_t)(100 * (i + 1));
    c->snapshot = (struct oa_channel_snapshot){
        .entries = c->entries, .count = 3, .source = 1
    };
    g_ctx = c;
    *out = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->api->publish(c->api->priv, OA_SVC_SOURCE, 3, &source_iface) != 0)
        return -1;
    c->source_published = 1;
    if (c->api->publish(c->api->priv, OA_SVC_CHANNELS, 1,
                        &channels_iface) != 0) {
        c->api->unpublish(c->api->priv, OA_SVC_SOURCE);
        c->source_published = 0;
        return -1;
    }
    c->channels_published = 1;
    if (c->api->publish(c->api->priv, OA_SVC_TUNER, 1, &tuner_iface) != 0) {
        c->api->unpublish(c->api->priv, OA_SVC_CHANNELS);
        c->api->unpublish(c->api->priv, OA_SVC_SOURCE);
        c->channels_published = 0;
        c->source_published = 0;
        return -1;
    }
    c->tuner_published = 1;
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->tuner_published)
        c->api->unpublish(c->api->priv, OA_SVC_TUNER);
    if (c->channels_published)
        c->api->unpublish(c->api->priv, OA_SVC_CHANNELS);
    if (c->source_published)
        c->api->unpublish(c->api->priv, OA_SVC_SOURCE);
    c->tuner_published = c->channels_published = c->source_published = 0;
}

static int m_suspend(void *v) { (void)v; return 0; }
static int m_resume(void *v) { (void)v; return 0; }
static int m_quiesce(void *v) { (void)v; return 0; }

static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (g_ctx == c) g_ctx = NULL;
    free(c);
}

static const oa_module descriptor = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof descriptor,
    .name = "cec_services_fake", .version = "1",
    .requires = { "", 0 }, .init = m_init, .start = m_start,
    .stop = m_stop, .destroy = m_destroy, .suspend = m_suspend,
    .resume = m_resume, .quiesce = m_quiesce
};
OA_MODULE_EXPORT(descriptor)
