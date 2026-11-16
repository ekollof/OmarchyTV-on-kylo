/* Host fake for oa_source_hal. Records every call in order and mirrors the
 * useful names to FAKE_VIN_LOG for the control-socket test. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/source_hal.h"
#include "openaurora/dev_gpio.h"
#include "openaurora/source_vin.h"
#include "openaurora/video_mute.h"
#include "source_fake.h"

static struct source_fake_event g_ev[256];
static int g_drm_dev = 7;
static int g_n;
static char g_log[16384];
static size_t g_log_n;
static uint32_t g_vin_words[OA_SOURCE_VIN_WORDS] = { 1, 1, 1, 0, 0 };
static int g_vin_opens, g_vin_reads, g_vin_closes;

static void rec(int kind, const long long *v)
{
    if (g_n >= (int)(sizeof g_ev / sizeof g_ev[0])) return;
    g_ev[g_n++] = (struct source_fake_event){
        kind, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8]
    };
}
#define REC(kind, ...) rec(kind, (const long long[9]){__VA_ARGS__})

static void append_log(const char *fmt, ...)
{
    if (g_log_n >= sizeof g_log) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(g_log + g_log_n, sizeof g_log - g_log_n, fmt, ap);
    va_end(ap);
    if (n > 0) g_log_n += (size_t)n < sizeof g_log - g_log_n
                              ? (size_t)n : sizeof g_log - g_log_n - 1;
    const char *path = getenv("FAKE_VIN_LOG");
    if (path && *path) {
        FILE *f = fopen(path, "a");
        if (f) {
            va_start(ap, fmt);
            vfprintf(f, fmt, ap);
            va_end(ap);
            fputc('\n', f);
            fclose(f);
        }
    }
}

void source_fake_set_drm_dev(int dev) { g_drm_dev = dev; }
void source_fake_vin_set(const uint32_t words[OA_SOURCE_VIN_WORDS])
{
    if (words) memcpy(g_vin_words, words, sizeof g_vin_words);
}
int source_fake_vin_opens(void) { return g_vin_opens; }
int source_fake_vin_reads(void) { return g_vin_reads; }
int source_fake_vin_closes(void) { return g_vin_closes; }
void source_fake_reset(void)
{
    g_n = 0;
    g_log_n = 0;
    g_log[0] = 0;
    g_vin_opens = g_vin_reads = g_vin_closes = 0;
}
int source_fake_count(void) { return g_n; }
const struct source_fake_event *source_fake_at(int index)
{
    return index >= 0 && index < g_n ? &g_ev[index] : NULL;
}
const char *source_fake_log_text(void) { return g_log; }

static int f_open(int index)
{
    REC(SF_OPEN, index, 100 + index);
    append_log("open idx=%d fd=%d", index, 100 + index);
    return 100 + index;
}
static void f_close(int fd)
{
    REC(SF_CLOSE, fd);
    append_log("close fd=%d", fd);
}
static void f_select(int fd, int id)
{
    REC(SF_SELECT, fd, id);
    append_log("select fd=%d id=%d", fd, id);
}
static void f_start(int fd)
{
    REC(SF_START, fd);
    append_log("start fd=%d", fd);
}
static void f_audio(int path, int id, const void *records)
{
    const unsigned char *p = records;
    REC(SF_AUDIO, path, id, p ? p[0] : -1);
    append_log("audio path=%d id=%d first=%d", path, id, p ? p[0] : -1);
}
static int f_vplt(int cmd, int a, void *buf, int b, int c, int d,
                  int e, int f, int g)
{
    (void)b; (void)c; (void)d; (void)e; (void)f; (void)g;
    int *pair = buf;
    REC(SF_VPLT, cmd, a, pair ? pair[0] : -1, pair ? pair[1] : -1,
        b, c, d);
    append_log("vplt cmd=%d a=%d x=%d y=%d", cmd, a,
               pair ? pair[0] : -1, pair ? pair[1] : -1);
    return 0;
}
static void f_info_get(int index, void *out)
{
    uint32_t *w = out;
    w[0] = (uint32_t)index;
    w[1] = 0x200u + (uint32_t)index;
    w[2] = 0x11;
    w[3] = 0x22;
    REC(SF_INFO_GET, index, w[0], w[1], w[2], w[3]);
}
static void f_info_set(int index, int w0, int w1, int w2, int w3)
{
    REC(SF_INFO_SET, index, w0, w1, w2, w3);
    append_log("info set idx=%d w=%d,%d,%d,%d", index, w0, w1, w2, w3);
}
static void f_info_reset(int index)
{
    REC(SF_INFO_RESET, index);
    append_log("info reset idx=%d", index);
}
static void f_drm_get(int index, void *out)
{
    memset(out, 0, 0x1a4);
    ((uint32_t *)out)[0] = 0x44;
    REC(SF_DRM_GET, index);
}
static void f_fd_get(void *out)
{
    uint32_t *w = out;
    for (int i = 0; i < 9; i++) w[i] = (uint32_t)(i + 1);
    REC(SF_FD_GET, 0);
}
static void f_fd_set(int w0, int w1, int w2, int w3, int w4,
                     int w5, int w6, int w7, int w8)
{
    REC(SF_FD_SET, w0, w1, w2, w3, w4, w5, w6, w7, w8);
    append_log("fd set w=%d,%d,%d,%d,%d,%d,%d,%d,%d",
               w0, w1, w2, w3, w4, w5, w6, w7, w8);
}
static int f_device(int index)
{
    REC(SF_DRM_DEV, index);
    return g_drm_dev;
}
static int f_plane(int path)
{
    REC(SF_PLANE, path);
    return 5;
}
static void f_plane_info(int plane, void *out)
{
    uint32_t *w = out;
    for (int i = 0; i < 12; i++) w[i] = 0xa0u + (uint32_t)i;
    REC(SF_PLANE_INFO, plane);
}
static void f_dp(int dev, int w0, int w1, int w2)
{
    REC(SF_DP, dev, w0, w1, w2);
    append_log("dp dev=%d w=%d,%d,%d", dev, w0, w1, w2);
}
static void f_ext(int flag, unsigned type)
{
    REC(SF_EXT, flag, (int)type);
    append_log("videoext flag=%d type=%u", flag, type);
}
static void f_dec_stop(int path)
{
    REC(SF_DEC_STOP, path);
    append_log("dec stop path=%d", path);
}
static void f_dec_close(int path)
{
    REC(SF_DEC_CLOSE, path);
    append_log("dec close path=%d", path);
}
static void f_sd(void)
{
    REC(SF_SD, 0);
    append_log("sd connect");
}
static int f_ioctl(int fd, unsigned long request, void *arg)
{
    struct ext_controls {
        uint32_t which, count, error_idx, request_fd, reserved;
        void *controls;
    } *ctls = arg;
    struct ext_control {
        uint32_t id, size, reserved2;
        void *ptr;
    } *ctl = ctls ? ctls->controls : NULL;
    REC(SF_IOCTL, fd, (long long)request,
        ctls ? (long long)ctls->which : -1,
        ctls ? (long long)ctls->count : -1,
        ctl ? (long long)ctl->id : -1,
        ctl ? (long long)ctl->size : -1);
    append_log("ioctl fd=%d req=%lx id=%x size=%x", fd, request,
               ctl ? ctl->id : 0, ctl ? ctl->size : 0);
    if (ctl && ctl->ptr && ctl->size >= 12) {
        uint32_t *w = ctl->ptr;
        w[0] = 0x33;
        w[1] = 0x44;
        w[2] = 0x55;
    }
    return 0;
}

static int f_color(int fd, uint32_t v0, uint32_t v1)
{
    REC(SF_COLOR, fd, v0, v1);
    append_log("color fd=%d v0=%x v1=%x", fd, v0, v1);
    return 0;
}

static const struct oa_source_hal g_fake = {
    f_open, f_close, f_select, f_start, f_audio, f_vplt,
    f_info_get, f_info_set, f_info_reset, f_drm_get, f_fd_get, f_fd_set,
    f_device, f_plane, f_plane_info, f_dp, f_ext,
    f_dec_stop, f_dec_close, f_sd, f_ioctl, f_color
};

const struct oa_source_hal *oa_source_hal(void)
{
    return &g_fake;
}

static int f_vin_open(void *context)
{
    (void)context;
    g_vin_opens++;
    append_log("vin open fd=105");
    return 105;
}

static int f_vin_read(void *context, int fd,
                      uint32_t words[OA_SOURCE_VIN_WORDS])
{
    (void)context;
    if (fd != 105 || !words) return -1;
    g_vin_reads++;
    memcpy(words, g_vin_words, sizeof g_vin_words);
    append_log("vin read w0=%u", words[0]);
    return 0;
}

static int f_vin_close(void *context, int fd)
{
    (void)context;
    if (fd != 105) return -1;
    g_vin_closes++;
    append_log("vin close fd=%d", fd);
    return 0;
}

static const struct oa_source_vin_io g_fake_vin = {
    NULL, f_vin_open, f_vin_read, f_vin_close
};

const struct oa_source_vin_io *oa_source_vin_io(void)
{
    return &g_fake_vin;
}

/* Fake DEV GPIO transport for the gated hotplug pulse and earphone detector.
 * The descriptor buffer is the recovered 0x88-byte record with the DTS name
 * pointer at +0x80; only EARPHONE and HDMI_HOTPLUG_* names are described. */
static int fake_earphone_level;
static int fake_gpio_open(void *ud) { (void)ud; return 200; }
static int fake_gpio_close(void *ud, int fd)
{ (void)ud; (void)fd; return 0; }
static int fake_gpio_ioctl_desc(void *ud, int fd, unsigned long cmd, void *buf)
{
    (void)ud; (void)fd; (void)cmd;
    unsigned char *b = buf;
    uintptr_t np = 0;
    if (!b) return -1;
    memcpy(&np, b + 0x80, sizeof np);
    const char *name = (const char *)(uintptr_t)np;
    if (!name) return -1;
    uint32_t pin;
    if (!strcmp(name, "EARPHONE")) pin = 117;
    else if (!strcmp(name, "HDMI_HOTPLUG_1")) pin = 274;
    else if (!strcmp(name, "HDMI_HOTPLUG_2")) pin = 275;
    else if (!strcmp(name, "HDMI_HOTPLUG_3")) pin = 276;
    else if (!strcmp(name, "HDMI_HOTPLUG_4")) pin = 277;
    else { append_log("gpio_desc_missing name=%s", name); return -1; }
    memset(b, 0, 0x88);
    memcpy(b, &pin, sizeof pin);
    memcpy(b + 0x08, "input", 5);
    memcpy(b + 0x20, "low", 3);
    memcpy(b + 0x38, "high", 4);
    memcpy(b + 0x50, "default", 7);
    memcpy(b + 0x68, "no_int", 6);
    append_log("gpio_desc name=%s pin=%u", name, pin);
    return 0;
}
static int fake_gpio_read_level(void *ud, int pin, int *level)
{
    (void)ud;
    if (!level) return -EINVAL;
    if (pin != 117) return -EINVAL;
    *level = fake_earphone_level;
    append_log("earphone level=%d", fake_earphone_level);
    return 0;
}
static int fake_gpio_write_level(void *ud, int pin, int level)
{
    (void)ud;
    if (pin != 117) return -EINVAL;
    fake_earphone_level = level;
    return 0;
}
static const struct oa_dev_gpio_io g_fake_dev_gpio = {
    NULL, fake_gpio_open, fake_gpio_close, fake_gpio_ioctl_desc,
    fake_gpio_read_level, fake_gpio_write_level
};
const struct oa_dev_gpio_io *oa_dev_gpio_vendor_io(void)
{
    return &g_fake_dev_gpio;
}
void source_fake_set_earphone_level(int level)
{
    fake_earphone_level = level ? 1 : 0;
}

/* Fake libNDAL3 video-mute chain. Mirrors plat_drm_set_mute's call order and
 * arguments into the shared log for the source_path fake-service test. */
static int f_vm_dev(void *ud, int index)
{
    (void)ud;
    append_log("vm dev idx=%d", index);
    return 17;
}
static int f_vm_path(void *ud, int path)
{
    (void)ud;
    append_log("vm path=%d", path);
    return 3;
}
static int f_vm_info(void *ud, int plane_type,
                     uint32_t info[OA_VIDEO_MUTE_INFO_WORDS])
{
    (void)ud;
    append_log("vm info pt=%d", plane_type);
    info[0] = 42;
    return 0;
}
static int f_vm_mute(void *ud, int fd, unsigned plane, int enable, int on)
{
    (void)ud;
    append_log("vm mute fd=%d plane=%u en=%d on=%d", fd, plane, enable, on);
    return 0;
}
static const oa_video_mute_io g_fake_vm = {
    NULL, f_vm_dev, f_vm_path, f_vm_info, f_vm_mute
};
const oa_video_mute_io *oa_video_mute_vendor_io(void)
{
    return &g_fake_vm;
}
