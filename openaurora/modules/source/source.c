/*
 * mod_source — input switching through the vendor VIN/VPLT/V4L2/DRM path.
 *
 * Recovered from aurora's PLAT_VIU_ChangeInput / plat_vin_slot_connect and
 * libNDAL3/libNDAL_EX. The hardware calls go through oa_source_hal so the
 * exact order and arguments can be host-tested without a TV.
 *
 * Key contracts:
 *   Hal_Vin_Open(index)          0..4 -> /dev/video20/24/21/28/66
 *   Hal_Vin_SelectInput(fd,id)   ioctl(fd,0xc0045627,&id)
 *   Hal_Vin_SetStreamStart(fd)   ioctl(fd,0x40045612,&{1})
 *   Hal_Video_Vpltio_SendCmd(0x4c4,0,&{tableA,tableB},0...)
 *   Hal_Video_Get/Set_V4l2Info   4 words at slot*16
 *   Hal_Video_Get/Set_V4l2FdCommon  9 words
 *   NDAL_VideoExt_OpenDevice(flag,type)
 *   Hal_Audio_EasySelect(0,1,&{1,"adc on"})
 *
 * source.enable remains the policy gate. A disabled module returns -2 and
 * makes no HAL call. Unsupported types fail without touching the display.
 */
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/dev_gpio.h"
#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/source_hal.h"
#include "openaurora/src_hotplug.h"

/* table A: source type -> Hal_Vin input id (aurora .data 0x013b9e78) */
static const int g_input_id[26] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    2, 0, 0, 0, 0, 1, 2, 3, 3, 66,
    3, 3, 0, 0, 0, 255
};
/* table B: only types 5 and 21 are 1 */
static const int g_input_aux[26] = {
    0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 0, 0
};

struct audio_adc_record {
    int id;
    char name[0x28];
};
static const struct audio_adc_record g_adc_on = { 1, "adc on" };
static const struct audio_adc_record g_adc_off = { 0, "adc off" };

struct ext_control {
    uint32_t id, size, reserved2;
    void *ptr;
};
struct ext_controls {
    uint32_t which, count, error_idx, request_fd, reserved;
    struct ext_control *controls;
};

#define MAX_FDS 2

struct ctx {
    const oa_host_api *api;
    const struct oa_source_hal *h;
    int enabled;
    int current;
    int slot_connected;
    int slot_type;
    int audio_refs;
    int fds[MAX_FDS];
    int nfds;
    uint32_t v4l2_info[4];
    uint32_t fdcommon[9];
    /* ABI 3 quiesce: fds are detached and closed on a worker. */
    pthread_t cleanup_thread;
    int cleanup_running;
    int close_fds[MAX_FDS];
    int close_n;
    unsigned cleanup_delay_ms;
    /* dev_src_init's one-shot DEV_SRC_HDMIHotPlug task, started only behind
     * source.enable=1 and source.hotplug_pulse=1. */
    pthread_t pulse_thread;
    int pulse_running;
};

static struct ctx *g_ctx;

static void close_fds(struct ctx *c)
{
    for (int i = 0; i < c->nfds; i++)
        if (c->fds[i] >= 0 && c->h && c->h->vin_close)
            c->h->vin_close(c->fds[i]);
    c->nfds = 0;
    c->slot_connected = 0;
}

static int open_index(int type)
{
    switch (type) {
    case 0: case 4: case 5: case 20: case 21: return 0;
    case 10: return 1;
    case 14: case 15: case 16: case 17: case 18: return 3;
    case 19: return 4;
    default: return -1; /* 1,2,3,6..9,11..13 and >21 are not VIN slots */
    }
}

/* 0x92e8ac: VPLT 0x4c4 with {tableA,tableB}, then SelectInput(fd,tableA). */
static void vplt_select(struct ctx *c, int fd, int type)
{
    int pair[2] = { g_input_id[type], g_input_aux[type] };
    if (c->h->vpltio_send)
        (void)c->h->vpltio_send(0x4c4, 0, pair, 0, 0, 0, 0, 0, 0);
    if (c->h->vin_select)
        c->h->vin_select(fd, pair[0]);
}

static int vin_slot_connect(struct ctx *c, int type)
{
    if (type < 0 || type > 21)
        return -1;
    /* plat_vin_slot_connect calls EasySelect for types 4..21 before the
     * connected short-circuit, even when the slot is already connected. */
    if (type >= 4 && c->h->audio_easy_select)
        c->h->audio_easy_select(0, 1, &g_adc_on);

    if (c->slot_connected && c->slot_type == type)
        return 0;
    if (c->slot_connected) {
        /* The vendor PLAT_VIU_ChangeInput disconnects the previous source
         * before connecting a new one. We do the fd/slot part natively; the
         * platform DRM/VDEC disconnect body is not available as a library. */
        close_fds(c);
    }

    int idx = open_index(type);
    if (idx < 0)
        return -1;

    if (idx == 3) {
        int fd1 = c->h->vin_open ? c->h->vin_open(3) : -1;
        if (fd1 < 0)
            return -1;
        vplt_select(c, fd1, type);

        uint32_t info[4];
        memset(info, 0, sizeof info);
        if (c->h->v4l2_info_get) c->h->v4l2_info_get(0, info);
        uint32_t fdinfo[9];
        memset(fdinfo, 0, sizeof fdinfo);
        if (c->h->v4l2_fd_get) c->h->v4l2_fd_get(fdinfo);

        int fd2 = c->h->vin_open ? c->h->vin_open(2) : -1;
        if (fd2 < 0) {
            if (c->h->vin_close) c->h->vin_close(fd1);
            return -1;
        }
        if (c->h->vin_select) c->h->vin_select(fd2, 0);
        if (c->h->vin_start) c->h->vin_start(fd2);

        info[0] = 0;
        info[1] = (uint32_t)fd2;
        info[2] = (uint32_t)fd1;
        info[3] = (uint32_t)type;
        if (c->h->v4l2_info_set)
            c->h->v4l2_info_set(0, (int)info[0], (int)info[1],
                                (int)info[2], (int)info[3]);
        fdinfo[3] = (uint32_t)fd2;
        fdinfo[4] = (uint32_t)fd1;
        if (c->h->v4l2_fd_set)
            c->h->v4l2_fd_set((int)fdinfo[0], (int)fdinfo[1], (int)fdinfo[2],
                              (int)fdinfo[3], (int)fdinfo[4], (int)fdinfo[5],
                              (int)fdinfo[6], (int)fdinfo[7], (int)fdinfo[8]);
        c->fds[0] = fd1;
        c->fds[1] = fd2;
        c->nfds = 2;
        memcpy(c->v4l2_info, info, sizeof info);
        memcpy(c->fdcommon, fdinfo, sizeof fdinfo);
    } else {
        int fd = c->h->vin_open ? c->h->vin_open(idx) : -1;
        if (fd < 0)
            return -1;
        vplt_select(c, fd, type);
        if (c->h->vin_start) c->h->vin_start(fd);

        uint32_t info[4];
        memset(info, 0, sizeof info);
        if (c->h->v4l2_info_get) c->h->v4l2_info_get(0, info);
        info[0] = 0;
        info[1] = (uint32_t)fd;
        info[3] = (uint32_t)type;
        if (c->h->v4l2_info_set)
            c->h->v4l2_info_set(0, (int)info[0], (int)info[1],
                                (int)info[2], (int)info[3]);

        uint32_t fdinfo[9];
        memset(fdinfo, 0, sizeof fdinfo);
        if (c->h->v4l2_fd_get) c->h->v4l2_fd_get(fdinfo);
        int fd_index = idx == 0 ? 0 : idx == 1 ? 1 : 2;
        fdinfo[fd_index] = (uint32_t)fd;
        if (c->h->v4l2_fd_set)
            c->h->v4l2_fd_set((int)fdinfo[0], (int)fdinfo[1], (int)fdinfo[2],
                              (int)fdinfo[3], (int)fdinfo[4], (int)fdinfo[5],
                              (int)fdinfo[6], (int)fdinfo[7], (int)fdinfo[8]);
        c->fds[0] = fd;
        c->nfds = 1;
        memcpy(c->v4l2_info, info, sizeof info);
        memcpy(c->fdcommon, fdinfo, sizeof fdinfo);
    }
    c->slot_connected = 1;
    c->slot_type = type;
    if (type >= 4)
        c->audio_refs++;
    return 0;
}

/* plat_vin_slot_disconnect @0x92f108. The V4L2 table carries the old type in
 * word3 and the fd in word1 (single or dual fd2); dual fd1 is word2 of Info.
 * The disconnect VPLT uses type 25 (tableA 255). */
static void vin_slot_disconnect(struct ctx *c)
{
    if (!c->slot_connected)
        return;
    int type = c->slot_type;
    int fd2 = c->nfds > 0 ? c->fds[c->nfds - 1] : -1;
    int fd1 = c->nfds > 1 ? c->fds[0] : -1;
    uint32_t fdinfo[9];
    memcpy(fdinfo, c->fdcommon, sizeof fdinfo);
    switch (type) {
    case 0: case 4: case 5: case 20: case 21:
        fdinfo[0] = 0xffffffffu;
        break;
    case 10:
        fdinfo[1] = 0xffffffffu;
        break;
    case 14: case 15: case 16: case 17: case 18:
        if (fd1 >= 0 && c->h->vin_close) c->h->vin_close(fd1);
        fdinfo[3] = 0xffffffffu;
        fdinfo[4] = 0xffffffffu;
        break;
    case 19:
        fdinfo[2] = 0xffffffffu;
        break;
    default:
        break;
    }
    vplt_select(c, fd2, 25);
    if (fd2 >= 0 && c->h->vin_close) c->h->vin_close(fd2);
    if (c->h->v4l2_info_reset) c->h->v4l2_info_reset(0);
    c->v4l2_info[0] = 7;
    c->v4l2_info[1] = c->v4l2_info[2] = 0xffffffffu;
    c->v4l2_info[3] = 25;
    if (c->h->v4l2_fd_set)
        c->h->v4l2_fd_set((int)fdinfo[0], (int)fdinfo[1], (int)fdinfo[2],
                          (int)fdinfo[3], (int)fdinfo[4], (int)fdinfo[5],
                          (int)fdinfo[6], (int)fdinfo[7], (int)fdinfo[8]);
    memcpy(c->fdcommon, fdinfo, sizeof fdinfo);
    c->nfds = 0;
    c->fds[0] = c->fds[1] = -1;
    c->slot_connected = 0;
    if (c->audio_refs > 0) {
        c->audio_refs--;
        if (c->audio_refs == 0 && c->h->audio_easy_select)
            c->h->audio_easy_select(0, 0, &g_adc_off);
    }
}

/* plat_drm_get_dpsource @0x92cac8: V4L2Info word1 is the fd, word2 is the
 * format selector. Types 1..3 use Get_V4l2Drm; other types ask
 * VIDIOC_G_EXT_CTRLS id 0xa00968 (size 0x1a4) into the output buffer. */
static int drm_get_dpsource(struct ctx *c, int dev, int type, uint32_t *out)
{
    uint32_t info[4];
    memset(info, 0, sizeof info);
    if (c->h->v4l2_info_get) c->h->v4l2_info_get(dev, info);

    if (type >= 1 && type <= 3) {
        if (c->h->v4l2_drm_get) c->h->v4l2_drm_get(0, out);
        if (type == 1) out[0] = 0;
        else if (type == 2) out[0] = 1;
        /* type 3 keeps the Get_V4l2Drm result and falls through to the map */
        if (type != 3) {
            out[0x67] = 0;
            return 0;
        }
    } else {
        struct ext_control ctl;
        struct ext_controls ctls;
        memset(&ctl, 0, sizeof ctl);
        memset(&ctls, 0, sizeof ctls);
        ctl.id = 0xa00968;
        ctl.size = 0x1a4;
        ctl.ptr = out;
        ctls.which = 0xa00000;
        ctls.count = 1;
        ctls.controls = &ctl;
        if (!c->h->ioctl ||
            c->h->ioctl((int)info[1], 0xc0185647UL, &ctls) != 0)
            return -1;
    }

    switch (info[2]) {
    case 0: out[0] = 2; break;
    case 4: case 5: out[0] = 3; break;
    case 0x0e: case 0x0f: case 0x10: case 0x11: case 0x12:
        out[0] = 7; break;
    case 0x13: out[0] = 5; break;
    case 0x14: case 0x15: out[0] = 0x0b; break;
    default: out[0] = 4; break;
    }
    out[0x67] = 0;
    return 0;
}

/* plat_drm_connect_dpsource @0x92cbe0. */
static int drm_connect(struct ctx *c, int dev, int type)
{
    int drm_dev = c->h->drm_device_id ? c->h->drm_device_id(0) : -1;
    int plane = c->h->drm_plane_type ? c->h->drm_plane_type(dev) : -1;
    if (drm_dev < 0 || plane < 0)
        return -1;
    uint32_t pinfo[12];
    memset(pinfo, 0, sizeof pinfo);
    if (c->h->drm_plane_info) c->h->drm_plane_info(plane, pinfo);

    uint32_t src[0x69];
    memset(src, 0, sizeof src);
    if (drm_get_dpsource(c, dev, type, src) != 0)
        return -1;
    if (c->h->drm_set_dpsource)
        c->h->drm_set_dpsource(drm_dev, (int)src[0], (int)src[1], (int)src[2]);
    return 0;
}

/* plat_drm_disconnect_dpsource @0x92cce0: same plane lookup, then code 0xe
 * and the plane ID in words 0/1 of the Set_DpSource payload. */
static void drm_disconnect(struct ctx *c, int type)
{
    int drm_dev = c->h->drm_device_id ? c->h->drm_device_id(0) : -1;
    int plane = c->h->drm_plane_type ? c->h->drm_plane_type(0) : -1;
    if (drm_dev < 0 || plane < 0)
        return;
    uint32_t pinfo[12];
    memset(pinfo, 0, sizeof pinfo);
    if (c->h->drm_plane_info) c->h->drm_plane_info(plane, pinfo);
    uint32_t src[0x69];
    memset(src, 0, sizeof src);
    if (drm_get_dpsource(c, 0, type, src) != 0)
        return;
    src[0] = 0x0e;
    src[1] = pinfo[0];
    if (c->h->drm_set_dpsource)
        c->h->drm_set_dpsource(drm_dev, (int)src[0], (int)src[1], (int)src[2]);
}

/* PLAT_VIU_ChangeInput connect half. Path 0 is the main display path used by
 * this set's Bronlijst. The vendor also disconnects the old source and updates
 * aurora-internal NdVideoSetting/VinParam tables; those are not library calls
 * and are deliberately not guessed here. */
static int change_input(struct ctx *c, int type)
{
    if (!c->enabled)
        return -2;
    if (type < 0 || type > 25)
        return -1;
    /* PLAT_VIU_ChangeInput's early same-source return. */
    if (c->current == type && c->slot_connected)
        return 0;
    /* PLAT_VIU_ChangeInput disconnects the old path before the type gates. */
    if (c->current >= 0) {
        drm_disconnect(c, c->current);
        if (c->slot_connected) {
            vin_slot_disconnect(c);
        } else if (c->current == 2 || c->current == 3) {
            if (c->h->video_dec_stop) c->h->video_dec_stop(0);
            if (c->h->video_dec_close) c->h->video_dec_close(0);
        }
        c->current = -1;
    }
    if (type == 25)
        return 0;
    /* A source switch cannot complete without the vendor DRM device. The
     * takeover profile skips Hal_Video_DrmInit(1), which leaves the device id
     * at -1; fail before opening VIN or sending VPLT so no half-switch occurs. */
    if (type != 2 && type != 3) {
        int drm_dev = c->h->drm_device_id ? c->h->drm_device_id(0) : -1;
        if (drm_dev < 0) {
            c->api->log(c->api->priv, OA_LOG_ERROR,
                        "source: DRM not initialized (plat.drm_init=1 required); "
                        "type %d not connected", type);
            return -1;
        }
    }

    if (type == 1) {
        if (c->h->sd_video_connect) c->h->sd_video_connect();
        if (drm_connect(c, 0, 1) != 0) return -1;
        if (c->h->video_ext_open) c->h->video_ext_open(0, 1);
    } else if (type == 2 || type == 3) {
        if (c->h->video_ext_open) c->h->video_ext_open(0, (unsigned)type);
    } else {
        if (vin_slot_connect(c, type) != 0)
            return -1;
        if (drm_connect(c, 0, type) != 0)
            return -1;
        if (c->h->video_ext_open)
            c->h->video_ext_open(0, (unsigned)type);
    }
    c->current = type;
    c->api->log(c->api->priv, OA_LOG_INFO,
                "source -> type %d (vin id %d, %d fd)", type,
                g_input_id[type], c->nfds);
    return 0;
}

static int select_type(int type)
{
    return g_ctx ? change_input(g_ctx, type) : -1;
}
static int iface_current(void)
{
    return g_ctx ? g_ctx->current : -1;
}
static int iface_available(void)
{
    return g_ctx && g_ctx->h ? 1 : 0;
}
static int iface_diag(int *drm_dev, int *plane, unsigned *info4, unsigned *fd9)
{
    struct ctx *c = g_ctx;
    if (!c || !c->h)
        return -1;
    if (drm_dev)
        *drm_dev = c->h->drm_device_id ? c->h->drm_device_id(0) : -1;
    if (plane)
        *plane = c->h->drm_plane_type ? c->h->drm_plane_type(0) : -1;
    if (info4) {
        memset(info4, 0, 4 * sizeof *info4);
        if (c->h->v4l2_info_get) c->h->v4l2_info_get(0, info4);
    }
    if (fd9) {
        memset(fd9, 0, 9 * sizeof *fd9);
        if (c->h->v4l2_fd_get) c->h->v4l2_fd_get(fd9);
    }
    return 0;
}

static const oa_source_iface g_iface = { select_type, iface_current,
                                         iface_available, iface_diag };

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    c->h = oa_source_hal();
    c->fds[0] = c->fds[1] = -1;
    c->close_fds[0] = c->close_fds[1] = -1;
    c->current = -1;
    *out = c;
    g_ctx = c;
    return 0;
}

/* --------------------------------------------------- HDMI hotplug pulse */
/* dev_src_init creates DEV_SRC_HDMIHotPlug before source selection. The task
 * is one-shot: descriptors -> inactive -> 500 ms -> active. Run it on a
 * worker because the recovered delay is blocking; the host loop and the ABI 3
 * quiesce gate must not wait for it. */
extern const struct oa_dev_gpio_io *oa_dev_gpio_vendor_io(void);

struct pulse_job {
    struct ctx *mod;
    struct oa_dev_gpio gpio;
};

static int pulse_desc(void *ud, int gpio_id, int *pin, int *active)
{
    struct pulse_job *j = ud;
    struct oa_gpio_desc d;
    if (oa_dev_gpio_describe(&j->gpio, gpio_id, &d) != 1)
        return 0;
    *pin = d.pin;
    *active = d.active;
    return 1;
}

static int pulse_get(void *ud, int pin, int *level)
{
    struct pulse_job *j = ud;
    return oa_dev_gpio_get_level(&j->gpio, pin, level);
}

static void pulse_set(void *ud, int pin, int level)
{
    struct pulse_job *j = ud;
    (void)oa_dev_gpio_set_level(&j->gpio, pin, level);
}

static void pulse_delay(void *ud, unsigned ms)
{
    (void)ud;
    struct timespec pause = {
        .tv_sec = ms / 1000u,
        .tv_nsec = (long)(ms % 1000u) * 1000000L
    };
    (void)nanosleep(&pause, NULL);
}

static void pulse_log(void *ud, const char *fmt, ...)
{
    struct pulse_job *j = ud;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    j->mod->api->log(j->mod->api->priv, OA_LOG_INFO, "source: %s", buf);
}

static void *pulse_worker(void *v)
{
    struct ctx *c = v;
    struct pulse_job job = { .mod = c };
    const struct oa_dev_gpio_io *io = oa_dev_gpio_vendor_io();
    if (!io) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "source: hotplug pulse requested without a vendor GPIO transport");
        return NULL;
    }
    int rc = oa_dev_gpio_open(&job.gpio, io);
    if (rc != 0) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "source: hotplug /dev/ntgpio open failed (%d)", rc);
        return NULL;
    }
    const oa_src_hotplug_ops ops = {
        .ud = &job,
        .descriptor = pulse_desc,
        .get_level = pulse_get,
        .set_level = pulse_set,
        .delay_ms = pulse_delay,
        .log = pulse_log,
    };
    int n = oa_src_hdmi_hotplug_pulse(&ops);
    c->api->log(c->api->priv, OA_LOG_INFO,
                "source: HDMI hotplug pulse finished (%d descriptor(s))", n);
    oa_dev_gpio_close(&job.gpio);
    return NULL;
}

static void apply_auto_color_system(struct ctx *c)
{
    const char *en = c->api->config_get(c->api->priv,
                                        "source.auto_color_system", "0");
    if (!c->enabled || !en || atoi(en) == 0)
        return;
    if (!c->h->vin_open || !c->h->vin_close || !c->h->vin_set_color_system) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "source: auto color system unavailable");
        return;
    }
    /* dev_src_init calls plat_vin_slot_set_auto_color_system(2) after
     * creating the hotplug task: Hal_Vin_Open(0), ioctl 0x40085618 with
     * {0x00ff40ff, 0}, Hal_Vin_Close(fd). */
    int fd = c->h->vin_open(0);
    if (fd < 0) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "source: auto color VIN(0) open failed (%d)", fd);
        return;
    }
    int rc = c->h->vin_set_color_system(fd, 0x00ff40ffu, 0u);
    c->h->vin_close(fd);
    c->api->log(c->api->priv, OA_LOG_INFO,
                "source: auto color system 2 -> 0x00ff40ff (%d)", rc);
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (!c->h) return -1;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "source.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    c->cleanup_running = 0;
    c->current = -1;
    c->slot_connected = 0;
    c->nfds = 0;
    c->close_n = 0;
    const char *en = c->api->config_get(c->api->priv, "source.enable", "0");
    c->enabled = atoi(en) != 0;
    c->api->log(c->api->priv, OA_LOG_INFO,
                "source: per-type VIN switching %s",
                c->enabled ? "enabled" : "disabled (source.enable=1 to allow)");
    if (c->api->publish(c->api->priv, OA_SVC_SOURCE, 1, &g_iface) != 0)
        c->api->log(c->api->priv, OA_LOG_WARN, "source service already published");

    const char *pulse = c->api->config_get(c->api->priv,
                                           "source.hotplug_pulse", "0");
    if (c->enabled && atoi(pulse) != 0) {
        int rc = pthread_create(&c->pulse_thread, NULL, pulse_worker, c);
        if (rc == 0) {
            c->pulse_running = 1;
            c->api->log(c->api->priv, OA_LOG_INFO,
                        "source: HDMI hotplug pulse started");
        } else {
            c->api->log(c->api->priv, OA_LOG_WARN,
                        "source: HDMI hotplug pulse thread failed (%d)", rc);
        }
    }
    apply_auto_color_system(c);
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
    for (int i = 0; i < c->close_n; i++) {
        if (c->close_fds[i] >= 0 && c->h && c->h->vin_close)
            c->h->vin_close(c->close_fds[i]);
        c->close_fds[i] = -1;
    }
    c->close_n = 0;
    return NULL;
}

static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
    if (c->pulse_running) {
        int rc = pthread_tryjoin_np(c->pulse_thread, NULL);
        if (rc == EBUSY)
            return -EAGAIN;
        c->pulse_running = 0;
    }
    if (c->cleanup_running) {
        for (int i = 0; i < 10; i++) {
            int rc = pthread_tryjoin_np(c->cleanup_thread, NULL);
            if (rc == 0) { c->cleanup_running = 0; return 0; }
            if (rc != EBUSY) { c->cleanup_running = 0; return -EIO; }
            const struct timespec pause = { .tv_nsec = 5000000 };
            (void)nanosleep(&pause, NULL);
        }
        return -EAGAIN;
    }
    c->enabled = 0;
    c->api->release(c->api->priv, OA_SVC_SOURCE);
    c->api->unpublish(c->api->priv, OA_SVC_SOURCE);
    if (c->nfds == 0) return 0;

    c->close_n = c->nfds;
    for (int i = 0; i < c->nfds; i++) {
        c->close_fds[i] = c->fds[i];
        c->fds[i] = -1;
    }
    c->nfds = 0;
    c->slot_connected = 0;
    c->current = -1;
    if (!c->cleanup_delay_ms) {
        for (int i = 0; i < c->close_n; i++) {
            if (c->close_fds[i] >= 0 && c->h && c->h->vin_close)
                c->h->vin_close(c->close_fds[i]);
            c->close_fds[i] = -1;
        }
        c->close_n = 0;
        return 0;
    }
    int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
    if (rc) {
        for (int i = 0; i < c->close_n; i++) {
            if (c->close_fds[i] >= 0 && c->h && c->h->vin_close)
                c->h->vin_close(c->close_fds[i]);
            c->close_fds[i] = -1;
        }
        c->close_n = 0;
        return -EIO;
    }
    c->cleanup_running = 1;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->pulse_running) {
        (void)pthread_join(c->pulse_thread, NULL);
        c->pulse_running = 0;
    }
    c->enabled = 0;
    c->api->release(c->api->priv, OA_SVC_SOURCE);
    c->api->unpublish(c->api->priv, OA_SVC_SOURCE);
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    close_fds(c);
    c->current = -1;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (c->pulse_running) {
        (void)pthread_join(c->pulse_thread, NULL);
        c->pulse_running = 0;
    }
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    close_fds(c);
    if (g_ctx == v)
        g_ctx = NULL;
    free(v);
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "source",
    .version = "0.2",
    .requires = { OA_SVC_PLAT, "" },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
    .quiesce = m_quiesce,
};

OA_MODULE_EXPORT(g_module)
