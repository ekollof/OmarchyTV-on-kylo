/* Call-order/argument test for the recovered source switch. Fake HAL only;
 * no TV, /dev, vendor library or display. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/broadcast.h"
#include "openaurora/channels.h"
#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/source_vin.h"
#include "source_fake.h"

extern const oa_module *oa_module_entry(void);
static const oa_source_iface *iface;
static int source_events;
static uint8_t source_event_type;
static int enabled = 1;
static int color_gate;
static int vin_gate;
static int inactive_gate;
static int pos;

static struct oa_channel_entry channel_entries[3];
static struct oa_channel_snapshot channel_snapshot = {
    .entries = channel_entries, .count = 3, .source = 1
};
static uint32_t tuner_selected;
static int tuner_select_calls;

static const struct oa_channel_snapshot *fake_channel_snapshot(void)
{
    return &channel_snapshot;
}
static const oa_channel_iface fake_channels = {
    fake_channel_snapshot, NULL, NULL
};
static int fake_tuner_select(uint32_t handle)
{
    tuner_selected = handle;
    tuner_select_calls++;
    return 0;
}
static const oa_tuner_iface fake_tuner = {
    NULL, NULL, NULL, fake_tuner_select, NULL
};

static struct oa_service_row service_rows[3] = {
    { .handle = 100 }, { .handle = 200 }, { .handle = 300 }
};
static struct oa_service_list service_list = {
    .rows = service_rows, .count = 3, .capacity = 3
};
static const struct oa_service_list *fake_service_snapshot(void)
{
    return &service_list;
}
static int fake_service_format(const struct oa_service_list *list, size_t index,
                               char *buf, size_t cap)
{
    (void)list; (void)index; (void)buf; (void)cap;
    return -1;
}
static const oa_service_list_iface fake_services = {
    fake_service_snapshot, fake_service_format
};

#define SKIP INT64_MIN
static void expect(int kind, int64_t a, int64_t b, int64_t c, int64_t d,
                   int64_t e)
{
    assert(pos < source_fake_count());
    int at = pos++;
    const struct source_fake_event *ev = source_fake_at(at);
    if (!ev || ev->kind != kind) {
        fprintf(stderr, "expect pos=%d want=%d got=%d a=%lld b=%lld c=%lld d=%lld e=%lld\n",
                at, kind, ev ? ev->kind : -1,
                ev ? (long long)ev->a : -1, ev ? (long long)ev->b : -1,
                ev ? (long long)ev->c : -1, ev ? (long long)ev->d : -1,
                ev ? (long long)ev->e : -1);
        assert(0);
    }
    if (a != SKIP) assert(ev->a == a);
    if (b != SKIP) assert(ev->b == b);
    if (c != SKIP) assert(ev->c == c);
    if (d != SKIP) assert(ev->d == d);
    if (e != SKIP) assert(ev->e == e);
}
static void expect_fd(int a, int b, int c, int d, int e, int f, int g,
                      int h, int i)
{
    assert(pos < source_fake_count());
    const struct source_fake_event *ev = source_fake_at(pos++);
    assert(ev && ev->kind == SF_FD_SET);
    assert(ev->a == a && ev->b == b && ev->c == c && ev->d == d &&
           ev->e == e && ev->f == f && ev->g == g && ev->h == h &&
           ev->i == i);
}
static void expect_ioctl(int fd, int64_t req, int64_t which, int64_t count,
                         int64_t id, int64_t size)
{
    assert(pos < source_fake_count());
    const struct source_fake_event *ev = source_fake_at(pos++);
    assert(ev && ev->kind == SF_IOCTL);
    assert(ev->a == fd && ev->b == req && ev->c == which &&
           ev->d == count && ev->e == id && ev->f == size);
}
static void reset(void)
{
    pos = 0;
    source_fake_reset();
}
static void log_message(void *p, int level, const char *fmt, ...)
{
    (void)p; (void)level; (void)fmt;
}
static const char *config(void *p, const char *key, const char *def)
{
    (void)p;
    if (!strcmp(key, "source.enable")) return enabled ? "1" : "0";
    if (!strcmp(key, "source.auto_color_system")) return color_gate ? "1" : "0";
    if (!strcmp(key, "source.vin_status")) return vin_gate ? "1" : "0";
    if (!strcmp(key, "source.inactive_fallback")) return inactive_gate ? "1" : "0";
    if (!strcmp(key, "source.cleanup_delay_ms")) return "0";
    return def;
}
static int publish(void *p, const char *name, uint32_t version, const void *ifc)
{
    (void)p;
    assert(!strcmp(name, OA_SVC_SOURCE) && version == 3);
    iface = ifc;
    return 0;
}
static int unpublish(void *p, const char *name)
{
    (void)p;
    assert(!strcmp(name, OA_SVC_SOURCE));
    iface = NULL;
    return 0;
}
static void release(void *p, const char *name) { (void)p; (void)name; }
static void emit(void *p, const char *topic, const void *ev)
{
    (void)p;
    if (!strcmp(topic, OA_TOPIC_SOURCE) && ev) {
        const oa_source_event *e = ev;
        source_events++;
        source_event_type = e->type;
    }
}
static const void *lookup(void *p, const char *name, uint32_t min_version)
{
    (void)p;
    if (min_version > 1) return NULL;
    if (!strcmp(name, OA_SVC_CHANNELS)) return &fake_channels;
    if (!strcmp(name, OA_SVC_TUNER)) return &fake_tuner;
    if (!strcmp(name, OA_SVC_SERVICE_LIST)) return &fake_services;
    return NULL;
}
static const oa_host_api api = {
    .size = sizeof(oa_host_api), .abi = OA_ABI_VERSION,
    .log = log_message, .config_get = config, .publish = publish,
    .unpublish = unpublish, .lookup = lookup, .release = release,
    .emit = emit
};

static void *start(int on)
{
    enabled = on;
    void *ctx = NULL;
    assert(oa_module_entry()->init(&api, &ctx) == 0 && ctx);
    assert(oa_module_entry()->start(ctx) == 0 && iface);
    return ctx;
}
static void done(void *ctx)
{
    oa_module_entry()->stop(ctx);
    oa_module_entry()->destroy(ctx);
    assert(!iface);
}

int main(void)
{
    void *ctx = start(1);
    assert(iface->current_type() == -1 && iface->is_available() == 1);
    assert(source_fake_count() == 0); /* start must not open a VIN fd */
    /* The diagnostic reads the vendor tables but opens/switches nothing. */
    int drm_dev = -1, plane = -1;
    unsigned info[4] = { 0 }, fd[9] = { 0 };
    assert(iface->diag(&drm_dev, &plane, info, fd) == 0);
    assert(drm_dev == 7 && plane == 5);
    assert(info[0] == 0 && info[1] == 0x200 && info[2] == 0x11 &&
           info[3] == 0x22);
    assert(fd[0] == 1 && fd[8] == 9);
    assert(source_fake_count() == 4); /* DEV, PLANE, INFO_GET, FD_GET */
    source_fake_reset();

    /* Type 0: single VIN(0), VPLT 0x4c4 {1,0}, select id1, start, 4-word
     * V4L2Info (word2 preserved), 9-word FdCommon (fd word0), DRM and NDAL. */
    reset();
    assert(iface->select_type(0) == 0);
    expect(SF_DRM_DEV, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_OPEN, 0, 100, SKIP, SKIP, SKIP);
    expect(SF_VPLT, 0x4c4, 0, 1, 0, SKIP);
    expect(SF_SELECT, 100, 1, SKIP, SKIP, SKIP);
    expect(SF_START, 100, SKIP, SKIP, SKIP, SKIP);
    expect(SF_INFO_GET, 0, 0, 0x200, 0x11, 0x22);
    expect(SF_INFO_SET, 0, 0, 100, 0x11, 0);
    expect(SF_FD_GET, SKIP, SKIP, SKIP, SKIP, SKIP);
    expect_fd(100, 2, 3, 4, 5, 6, 7, 8, 9);
    expect(SF_DRM_DEV, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_PLANE, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_PLANE_INFO, 5, SKIP, SKIP, SKIP, SKIP);
    expect(SF_INFO_GET, 0, 0, 0x200, 0x11, 0x22);
    expect_ioctl(0x200, 0xc0185647, 0xa00000, 1, 0xa00968, 0x1a4);
    expect(SF_DP, 7, 7, 0x44, 0x55, SKIP);
    expect(SF_EXT, 0, 0, SKIP, SKIP, SKIP);
    assert(pos == source_fake_count());

    /* Type 17 (HDMI1): dual Open(3)+Open(2), VPLT {3,0}, fd1 select id3,
     * fd2 SelectInput 0 then SetStreamStart, V4L2Info(slot,slot,fd2,fd1,17),
     * FdCommon word3=fd2 word4=fd1. */
    reset();
    assert(iface->select_type(17) == 0);
    /* Disconnect old type 0: DRM code 0xe + plane 0xa0, VPLT type25,
     * close the old fd, reset V4L2Info, FdCommon word0=-1. */
    expect(SF_DRM_DEV, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_PLANE, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_PLANE_INFO, 5, SKIP, SKIP, SKIP, SKIP);
    expect(SF_INFO_GET, 0, 0, 0x200, 0x11, 0x22);
    expect_ioctl(0x200, 0xc0185647, 0xa00000, 1, 0xa00968, 0x1a4);
    expect(SF_DP, 7, 0x0e, 0xa0, 0x55, SKIP);
    expect(SF_VPLT, 0x4c4, 0, 255, 0, SKIP);
    expect(SF_SELECT, 100, 255, SKIP, SKIP, SKIP);
    expect(SF_CLOSE, 100, SKIP, SKIP, SKIP, SKIP);
    expect(SF_INFO_RESET, 0, SKIP, SKIP, SKIP, SKIP);
    expect_fd(-1, 2, 3, 4, 5, 6, 7, 8, 9);
    expect(SF_DRM_DEV, 0, SKIP, SKIP, SKIP, SKIP); /* fail-fast check */
    expect(SF_AUDIO, 0, 1, 1, SKIP, SKIP);
    expect(SF_OPEN, 3, 103, SKIP, SKIP, SKIP);
    expect(SF_VPLT, 0x4c4, 0, 3, 0, SKIP);
    expect(SF_SELECT, 103, 3, SKIP, SKIP, SKIP);
    expect(SF_INFO_GET, 0, 0, 0x200, 0x11, 0x22);
    expect(SF_FD_GET, SKIP, SKIP, SKIP, SKIP, SKIP);
    expect(SF_OPEN, 2, 102, SKIP, SKIP, SKIP);
    expect(SF_SELECT, 102, 0, SKIP, SKIP, SKIP);
    expect(SF_START, 102, SKIP, SKIP, SKIP, SKIP);
    expect(SF_INFO_SET, 0, 0, 102, 103, 17);
    expect_fd(1, 2, 3, 102, 103, 6, 7, 8, 9);
    expect(SF_DRM_DEV, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_PLANE, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_PLANE_INFO, 5, SKIP, SKIP, SKIP, SKIP);
    expect(SF_INFO_GET, 0, 0, 0x200, 0x11, 0x22);
    expect_ioctl(0x200, 0xc0185647, 0xa00000, 1, 0xa00968, 0x1a4);
    expect(SF_DP, 7, 7, 0x44, 0x55, SKIP);
    expect(SF_EXT, 0, 17, SKIP, SKIP, SKIP);
    assert(pos == source_fake_count());

    /* Same source: PLAT_VIU_ChangeInput's early return, no HAL traffic. */
    reset();
    assert(iface->select_type(17) == 0);
    assert(source_fake_count() == 0);
    done(ctx);
    ctx = start(1);

    /* Type 19 (HDMI3): single Open(4), VPLT {66,0}, fd word2 of FdCommon. */
    reset();
    assert(iface->select_type(19) == 0);
    expect(SF_DRM_DEV, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_AUDIO, 0, 1, 1, SKIP, SKIP);
    expect(SF_OPEN, 4, 104, SKIP, SKIP, SKIP);
    expect(SF_VPLT, 0x4c4, 0, 66, 0, SKIP);
    expect(SF_SELECT, 104, 66, SKIP, SKIP, SKIP);
    expect(SF_START, 104, SKIP, SKIP, SKIP, SKIP);
    expect(SF_INFO_GET, 0, 0, 0x200, 0x11, 0x22);
    expect(SF_INFO_SET, 0, 0, 104, 0x11, 19);
    expect(SF_FD_GET, SKIP, SKIP, SKIP, SKIP, SKIP);
    expect_fd(1, 2, 104, 4, 5, 6, 7, 8, 9);
    expect(SF_DRM_DEV, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_PLANE, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_PLANE_INFO, 5, SKIP, SKIP, SKIP, SKIP);
    expect(SF_INFO_GET, 0, 0, 0x200, 0x11, 0x22);
    expect_ioctl(0x200, 0xc0185647, 0xa00000, 1, 0xa00968, 0x1a4);
    expect(SF_DP, 7, 7, 0x44, 0x55, SKIP);
    expect(SF_EXT, 0, 19, SKIP, SKIP, SKIP);
    assert(pos == source_fake_count());
    done(ctx);
    ctx = start(1);

    /* Type 1: SD connect + DRM + NDAL, no VIN slot open or audio select. */
    reset();
    assert(iface->select_type(1) == 0);
    expect(SF_DRM_DEV, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_SD, SKIP, SKIP, SKIP, SKIP, SKIP);
    expect(SF_DRM_DEV, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_PLANE, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_PLANE_INFO, 5, SKIP, SKIP, SKIP, SKIP);
    expect(SF_INFO_GET, 0, 0, 0x200, 0x11, 0x22);
    expect(SF_DRM_GET, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_DP, 7, 0, 0, 0, SKIP);
    expect(SF_EXT, 0, 1, SKIP, SKIP, SKIP);
    assert(pos == source_fake_count());
    done(ctx);
    ctx = start(1);

    /* Type 2/3 skip the slot/DRM connect and only open the video-extension
     * device. */
    reset();
    assert(iface->select_type(2) == 0);
    expect(SF_EXT, 0, 2, SKIP, SKIP, SKIP);
    assert(pos == source_fake_count());
    done(ctx);
    ctx = start(1);

    /* Unsupported slot types fail after the audio select and never open. */
    reset();
    assert(iface->select_type(6) == -1);
    expect(SF_DRM_DEV, 0, SKIP, SKIP, SKIP, SKIP);
    expect(SF_AUDIO, 0, 1, 1, SKIP, SKIP);
    assert(pos == source_fake_count());
    assert(iface->select_type(99) == -1);
    assert(source_fake_count() == 2);

    /* Disabled module: no HAL traffic at all. */
    done(ctx);
    reset();
    ctx = start(0);
    assert(iface->select_type(0) == -2);
    assert(source_fake_count() == 0);
    done(ctx);

    /* Uninitialized DRM (takeover profile): fail fast before any VIN/VPLT. */
    source_fake_set_drm_dev(-1);
    reset();
    ctx = start(1);
    assert(iface->select_type(17) == -1);
    assert(source_fake_count() == 1); /* the read-only DRM id check only */
    expect(SF_DRM_DEV, 0, SKIP, SKIP, SKIP, SKIP);
    source_fake_set_drm_dev(7);

    /* dev_src_init's plat_vin_slot_set_auto_color_system(2): open VIN(0),
     * set {0x00ff40ff, 0}, close. Explicit gate; the default start above
     * still opens nothing. */
    done(ctx);
    reset();
    color_gate = 1;
    ctx = start(1);
    expect(SF_OPEN, 0, 100, SKIP, SKIP, SKIP);
    expect(SF_COLOR, 100, 0x00ff40ff, 0, SKIP, SKIP);
    expect(SF_CLOSE, 100, SKIP, SKIP, SKIP, SKIP);
    assert(pos == source_fake_count());
    done(ctx);
    color_gate = 0;

    /* v2 source.video_status: source.vin_status gates the read-only HDMI RX
     * query. The fake returns the live ready/suspend word sets. */
    reset();
    vin_gate = 1;
    const uint32_t ready[OA_SOURCE_VIN_WORDS] = { 1, 1, 1, 0, 0 };
    const uint32_t sleep0[OA_SOURCE_VIN_WORDS] = { 0, 1, 1, 0, 0xffffffffu };
    const uint32_t sleep5[OA_SOURCE_VIN_WORDS] = { 5, 0, 0, 0, 0xffffffffu };
    source_fake_vin_set(ready);
    ctx = start(1);
    assert(source_fake_vin_opens() == 0 && source_fake_vin_reads() == 0);
    assert(iface->video_status() == 1);
    assert(source_fake_vin_opens() == 1 && source_fake_vin_reads() == 1);
    source_fake_vin_set(sleep0);
    assert(iface->video_status() == 0);
    source_fake_vin_set(sleep5);
    assert(iface->video_status() == 0);
    source_fake_vin_set(ready);
    assert(iface->video_status() == 1);
    assert(source_fake_vin_reads() == 4);
    done(ctx);
    assert(source_fake_vin_closes() == 1);
    vin_gate = 0;

    reset();
    source_fake_vin_set(ready);
    ctx = start(1);
    assert(iface->video_status() == -1);
    assert(source_fake_vin_opens() == 0);
    done(ctx);

    /* v3 inactive_source: CEC 0x9D worker selects DTV then the active-list
     * position's service handle. source.inactive_fallback gates it. */
    source_fake_set_drm_dev(7);
    channel_entries[0].handle = 100;
    channel_entries[1].handle = 200;
    channel_entries[2].handle = 300;
    channel_snapshot.count = 3;
    tuner_selected = 0;
    tuner_select_calls = 0;
    reset();
    source_events = 0;
    source_event_type = 0xff;
    inactive_gate = 1;
    ctx = start(1);
    assert(iface->inactive_source(-1) == -EINVAL);
    assert(iface->inactive_source(1) == 0);
    assert(tuner_selected == 200 && tuner_select_calls == 1);
    assert(iface->current_type() == 0);
    assert(source_events == 1 && source_event_type == 0);
    done(ctx);

    /* No runtime channel list: the stored service.list rows are the fallback. */
    reset();
    channel_snapshot.count = 0;
    service_list.count = 3;
    tuner_selected = 0;
    tuner_select_calls = 0;
    ctx = start(1);
    assert(iface->inactive_source(1) == 0);
    assert(tuner_selected == 200 && tuner_select_calls == 1);
    done(ctx);

    /* Neither source has a row: reject without guessing a service. */
    reset();
    service_list.count = 0;
    ctx = start(1);
    assert(iface->inactive_source(0) == -ENODEV);
    done(ctx);
    channel_snapshot.count = 3;
    service_list.count = 3;

    inactive_gate = 0;
    ctx = start(1);
    assert(iface->inactive_source(1) == -2);
    done(ctx);

    puts("ok source path: type gates, VIN opens/VPLT, V4L2Info/FdCommon, "
         "DRM/NDAL, audio, gated HDMI RX status and inactive-source order");
    return 0;
}
