/*
 * mod_display — the OSD layer, driven through fbdev.
 *
 * Measured on the set (see artifacts/reports/OSD-INIT-RE-2026-10-30.md):
 *
 *  - /dev/fb1 (OSD_NORMAL2, 1920x1080, 2 frames, BGRA) holds the vendor UI. Painting a
 *    vendor-initialised, enabled layer was visible; this does not establish cold scanout.
 *  - /dev/fb2 painting was not visible in the recorded experiment.
 *  - A historical region read showed source 1920x1080 and destination 3840x2160;
 *    quarter-screen boot paint did not establish a complete geometry/init cause.
 *    Use current OSD contract reports before adding cold setup calls.
 *
 * This module renders into an existing fbdev mapping and pans it. Independent OSD
 * allocation/binding/enable is incomplete. Offline decompilation proves the vendor
 * uses DRM ADDFB2/SETPLANE plus a private enable ioctl. Its CREATE_DUMB path allocates
 * only 4 KiB, while actual OSD allocation uses a private create command; the earlier
 * dumb-buffer failures do not rule DRM out. See OSD-BUFFER-CONTRACT-2026-10-31.md.
 */
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

#define OSD_REG_RD 0x8020c86cu /* IOR(0xc8,108,32) */
#define OSD_REG_WR 0x4020c86bu /* IOW(0xc8,107,32) */

struct ctx {
    const oa_host_api *api;
    int fd;
    char path[32];

    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    uint8_t *map;
    size_t maplen;

    unsigned frames;
    unsigned back;    /* frame we render into; 1 - displayed */
    int pan;          /* rotate frames after commit; off = commit the current frame */
    int mirror;       /* copy the painted frame into the other frames (fights DFB's flip) */
};

static struct ctx *g_ctx;

/* ---------------------------------------------------------------- drawing */

static void put_px(struct ctx *c, uint8_t *row, unsigned x, int r, int g, int b)
{
    if (c->var.bits_per_pixel == 32) {
        /* measured on the set: /dev/fb1 is BGRA (a test pattern with byte0=255 showed as blue),
         * so the red channel goes in byte 2, not byte 0. */
        uint8_t *p = row + (size_t)x * 4;
        p[0] = (uint8_t)b;
        p[1] = (uint8_t)g;
        p[2] = (uint8_t)r;
        p[3] = 0xff;
    } else if (c->var.bits_per_pixel == 16) {
        ((uint16_t *)row)[x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
}

static int fill_rect(struct ctx *c, int x, int y, int w, int h, int r, int g, int b)
{
    if (!c->map)
        return -1;
    unsigned fx = c->var.xres, fy = c->var.yres;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > (int)fx ? (int)fx : x + w;
    int y1 = (y + h) > (int)fy ? (int)fy : y + h;

    for (int yy = y0; yy < y1; yy++) {
        uint8_t *row = c->map + (size_t)(c->back * fy + (unsigned)yy) * c->fix.line_length;
        for (int xx = x0; xx < x1; xx++)
            put_px(c, row, (unsigned)xx, r, g, b);
    }
    return 0;
}

static int clear(int r, int g, int b)
{
    if (!g_ctx)
        return -1;
    return fill_rect(g_ctx, 0, 0, (int)g_ctx->var.xres, (int)g_ctx->var.yres, r, g, b);
}

static int fill(int x, int y, int w, int h, int r, int g, int b)
{
    return g_ctx ? fill_rect(g_ctx, x, y, w, h, r, g, b) : -1;
}

static int blit(int x, int y, int w, int h, const unsigned char *rgba)
{
    struct ctx *c = g_ctx;
    if (!c || !c->map || !rgba)
        return -1;

    unsigned fy = c->var.yres;
    for (int sy = 0; sy < h; sy++) {
        int dy = y + sy;
        if (dy < 0 || dy >= (int)fy)
            continue;
        uint8_t *row = c->map + (size_t)(c->back * fy + (unsigned)dy) * c->fix.line_length;
        for (int sx = 0; sx < w; sx++) {
            int dx = x + sx;
            if (dx < 0 || dx >= (int)c->var.xres)
                continue;
            const unsigned char *p = rgba + ((size_t)sy * (size_t)w + (size_t)sx) * 4;
            unsigned a = p[3];
            if (a == 0)
                continue;
            unsigned sr = p[0], sg = p[1], sb = p[2]; /* source blobs are RGBA */
            if (a == 255) {
                put_px(c, row, (unsigned)dx, (int)sr, (int)sg, (int)sb);
            } else {
                /* simple blend against what is already there (32bpp only) */
                if (c->var.bits_per_pixel == 32) {
                    uint8_t *d = row + (size_t)dx * 4;
                    d[0] = (uint8_t)((sb * a + d[0] * (255 - a)) / 255);
                    d[1] = (uint8_t)((sg * a + d[1] * (255 - a)) / 255);
                    d[2] = (uint8_t)((sr * a + d[2] * (255 - a)) / 255);
                    d[3] = 0xff;
                } else {
                    put_px(c, row, (unsigned)dx, (int)sr, (int)sg, (int)sb);
                }
            }
        }
    }
    return 0;
}

static int blit_scaled(int x, int y, int w, int h, int sw, int sh, const unsigned char *rgba)
{
    struct ctx *c = g_ctx;
    if (!c || !c->map || !rgba || sw <= 0 || sh <= 0 || w <= 0 || h <= 0)
        return -1;

    unsigned fy = c->var.yres;
    for (int dy = 0; dy < h; dy++) {
        int py = y + dy;
        if (py < 0 || py >= (int)fy)
            continue;
        int sy = (int)((int64_t)dy * sh / h);
        uint8_t *row = c->map + (size_t)(c->back * fy + (unsigned)py) * c->fix.line_length;
        for (int dx = 0; dx < w; dx++) {
            int px = x + dx;
            if (px < 0 || px >= (int)c->var.xres)
                continue;
            int sx = (int)((int64_t)dx * sw / w);
            const unsigned char *p = rgba + ((size_t)sy * (size_t)sw + (size_t)sx) * 4;
            unsigned a = p[3];
            if (a == 0)
                continue;
            if (a == 255) {
                put_px(c, row, (unsigned)px, p[0], p[1], p[2]);
            } else if (c->var.bits_per_pixel == 32) {
                uint8_t *d = row + (size_t)px * 4;
                d[0] = (uint8_t)((p[2] * a + d[0] * (255 - a)) / 255);
                d[1] = (uint8_t)((p[1] * a + d[1] * (255 - a)) / 255);
                d[2] = (uint8_t)((p[0] * a + d[2] * (255 - a)) / 255);
                d[3] = 0xff;
            } else {
                put_px(c, row, (unsigned)px, p[0], p[1], p[2]);
            }
        }
    }
    return 0;
}

static int present(void){
    struct ctx *c = g_ctx;
    if (!c || !c->map)
        return -1;

    /* Mirror the rendered frame into every frame of the layer.
     *
     * The vendor's DirectFB also owns this layer and flips its own two buffers, so a single
     * frame of ours alternates with a frame of theirs — visible as a one-frame flash. Filling
     * both halves means whichever frame gets scanned out is ours, and the image stays put
     * until aurora repaints.
     */
    if (c->mirror && c->frames > 1) {
        size_t bytes = (size_t)c->fix.line_length * c->var.yres;
        uint8_t *src = c->map + (size_t)c->back * bytes;
        for (unsigned f = 0; f < c->frames; f++)
            if (f != c->back)
                memcpy(c->map + (size_t)f * bytes, src, bytes);
    }

    /* push the rendered frame out before flipping */
    if (msync(c->map, c->maplen, MS_SYNC) != 0)
        return -1;

    /* NVTGA_OSD_UpdateRegion/FlipRegion always commit through FBIOPAN_DISPLAY.
     * The kernel's _ker_osd_fb_PanDisplay binds the physical buffer and calls
     * pfUpdateEnable. This is needed even without buffer rotation (or with one
     * frame); msync alone does not follow the vendor presentation path. */
    struct fb_var_screeninfo v = c->var;
    v.yoffset = c->back * c->var.yres;
    if (ioctl(c->fd, FBIOPAN_DISPLAY, &v) != 0)
        return -1;
    c->var.yoffset = v.yoffset;
    if (c->pan && c->frames > 1)
        c->back = (c->back + 1) % c->frames;
    return 0;
}

static int info(int *w, int *h, int *bpp, int *frames)
{
    if (!g_ctx)
        return -1;
    if (w) *w = (int)g_ctx->var.xres;
    if (h) *h = (int)g_ctx->var.yres;
    if (bpp) *bpp = g_ctx->var.bits_per_pixel;
    if (frames) *frames = (int)g_ctx->frames;
    return 0;
}

static const oa_display_iface g_iface = { clear, fill, blit, blit_scaled, present, info };

/* ------------------------------------------------------------------- layer */

/* Set source dimensions and destination origin while preserving the driver's
 * source origin and destination dimensions. Historical quarter-screen paint
 * did not prove a complete cold geometry/initialization diagnosis. */
static int set_region(struct ctx *c, unsigned w, unsigned h)
{
    /* Read-modify-write, as NVTGA_OSD_SetRegion does (libdirectfb_tdga, decompiled):
     *   ioctl(0x8020c86c) reads the 32-byte region; words 2/3 are the window size and words 4/5
     *   are its position on the OSD canvas. Words 0/1 are left exactly as the driver reports them.
     *
     * Both halves were confirmed the hard way on the set: writing the size into words 4/5 put the
     * window at canvas (1920,1080) - visible, but as a quadrant - and keeping a stale offset in
     * words 4/5 then pushed it off-screen entirely. So the offset has to be driven to 0 as well.
     */
    uint32_t reg[8];
    memset(reg, 0, sizeof reg);
    if (ioctl(c->fd, OSD_REG_RD, reg) != 0)
        return -1;
    reg[2] = w;
    reg[3] = h;
    reg[4] = 0;
    reg[5] = 0;
    return ioctl(c->fd, OSD_REG_WR, reg) == 0 ? 0 : -1;
}

/*
 * The vendor UI brings the display up before it touches /dev/fb*: Hal_Video_DrmInit(1) opens
 * /dev/dri/card0 (its own retry is only 5 s, so we wait for the node first), then does
 * info/framebuffer/KMS init, and only afterwards does it open fb1/fb0/fb2 (boot trace:
 * artifacts/protocol/oatrace-boot-2026-10-31.txt).
 *
 * Linked, not dlopen'd: aurora has libNDAL3.so and libNDAL_EX.so as NEEDED entries and the loader
 * resolves gs32Ndp_PQ_fd from EX by that order - the display module now links both the same way the
 * panel module links the PWM HAL (see VENDOR_MODULES in the Makefile). dlopen'ing NDAL3 without EX in
 * scope is what produced the earlier "undefined symbol" error.
 */
#ifdef OA_NO_VENDOR
/* Host builds have no vendor HAL: keep the module loadable so the lifecycle tests still run. */
static void Hal_Video_Vpltio_Init(void) {}
static void Hal_Video_V4l2InfoInit(void) {}
static void Hal_Video_DrmShareMemCreate(void) {}
static void Hal_Video_DrmInit(int enable)
{
    (void)enable;
}
static int Hal_Video_Vpltio_SendCmd(int cmd, int a, void *buf, int b, int c, int d, int e, int f,
                                    int g)
{
    (void)cmd; (void)a; (void)buf; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g;
    return 0;
}
#else
extern void Hal_Video_Vpltio_Init(void);
extern void Hal_Video_V4l2InfoInit(void);
extern void Hal_Video_DrmShareMemCreate(void);
extern void Hal_Video_DrmInit(int enable);
extern int Hal_Video_Vpltio_SendCmd(int cmd, int a, void *buf, int b, int c, int d, int e, int f,
                                    int g);
#endif

static void drm_bringup(struct ctx *c)
{
    /* The vendor bring-up now lives in mod_plat (same order as PLAT_DRV_INI_Init) and display
     * requires that service, so this is off by default: running plat_common_init twice is not
     * something the vendor does. Kept behind a flag for isolating display problems. */
    if (c->api->config_get(c->api->priv, "display.drm_init", "0")[0] != '1')
        return;

    /* Vendor order, from aurora's plat_common_init("DVIF") @009247f8 - called from
     * PLAT_DRV_INI_Init after the audio Protect/Mixer/Proxy inits, and idempotent via a module-level
     * flag. Doing only Hal_Video_DrmInit(1) (what this module used to do) leaves vpltio/v4l2 and the
     * shared-memory pool uninitialised. These calls do not reproduce the separate
     * OSD surface allocation, buffer binding and enable sequence. */
    c->api->log(c->api->priv, OA_LOG_INFO, "drm_init: vpltio/v4l2/drm bring-up (DVIF)");
    Hal_Video_Vpltio_Init();
    Hal_Video_V4l2InfoInit();
    Hal_Video_DrmShareMemCreate();
    Hal_Video_DrmInit(1);

    int src = 1;                                 /* DVIF (SIF would be 0) */
    Hal_Video_Vpltio_SendCmd(0x302, 0, &src, 0, 0, 0, 0, 0, 0);
    c->api->log(c->api->priv, OA_LOG_INFO, "drm_init: done");
}

/* sysfs is the authority on whether a framebuffer exists: /proc/fb lists it and
 * /sys/class/graphics/fbN/dev holds its major:minor even when the /dev node has not been created
 * (devtmpfs/hotplug timing - mdev is only wired up by rc.local's active_usb()). So drive readiness
 * from sysfs and create the node ourselves if it is missing. Returns 1 when the fb is registered. */
static int fb_registered(const char *path)
{
    char name[96];
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    snprintf(name, sizeof name, "/sys/class/graphics/%s/dev", base);
    return access(name, R_OK) == 0;
}

static int fb_make_node(struct ctx *c)
{
    /* rc.local sets hotplug to /sbin/mdev but leaves `mdev -s` commented out, so a framebuffer
     * registered before the hotplug helper was installed ends up with sysfs entries and no /dev
     * node - exactly the state seen in a takeover boot. mdev is the vendor's own mechanism for
     * populating /dev from sysfs, so use it instead of hand-rolling mknod. */
    if (system("/sbin/mdev -s") != 0)
        return -1;
    if (access(c->path, F_OK) != 0)
        return -1;
    c->api->log(c->api->priv, OA_LOG_INFO, "%s: created by mdev -s", c->path);
    return 0;
}

static int open_layer(struct ctx *c)
{
    /* The fb devices are not necessarily there when the UI process starts (the boot hook runs
     * before the display driver has published them), so wait for the node instead of failing:
     * measured on the set, /dev/fb1 appears a moment after the hook starts. */
    int timeout_ms = atoi(c->api->config_get(c->api->priv, "display.open_timeout_ms", "60000"));
    int waited = 0;
    int made = 0;
    for (;;) {
        c->fd = open(c->path, O_RDWR);
        if (c->fd >= 0)
            break;
        int err = errno;
        struct stat st;
        int have = stat(c->path, &st) == 0;
        /* node missing but the framebuffer is registered -> create the node from sysfs */
        if (!made && !have && fb_registered(c->path)) {
            made = 1;
            if (fb_make_node(c) == 0)
                continue;
        }
        /* one line per second: is the node there, is the fb registered, what does open say, and -
         * because there are five OSD layers - which of the others exist right now */
        if (waited % 1000 == 0)
            c->api->log(c->api->priv, OA_LOG_WARN,
                        "%s: open %s, node %s, fb %s, nodes present: %d%d%d%d%d (waited %d ms)",
                        c->path, strerror(err), have ? "ok" : "missing",
                        fb_registered(c->path) ? "registered" : "absent",
                        access("/dev/fb0", F_OK) == 0, access("/dev/fb1", F_OK) == 0,
                        access("/dev/fb2", F_OK) == 0, access("/dev/fb3", F_OK) == 0,
                        access("/dev/fb4", F_OK) == 0, waited);
        if (waited >= timeout_ms)
            return -1;
        usleep(200000);
        waited += 200;
    }
    if (waited)
        c->api->log(c->api->priv, OA_LOG_INFO, "%s: appeared after %d ms", c->path, waited);
    if (ioctl(c->fd, FBIOGET_VSCREENINFO, &c->var) != 0 ||
        ioctl(c->fd, FBIOGET_FSCREENINFO, &c->fix) != 0) {
        close(c->fd);
        c->fd = -1;
        return -1;
    }
    c->frames = c->var.yres ? c->var.yres_virtual / c->var.yres : 1;
    if (c->frames == 0)
        c->frames = 1;
    c->back = c->var.yres ? c->var.yoffset / c->var.yres : 0;
    if (c->back >= c->frames)
        c->back = 0;
    if (c->pan && c->frames > 1)
        c->back = (c->back + 1) % c->frames;
    c->maplen = (size_t)c->fix.line_length * c->var.yres_virtual;
    c->map = mmap(NULL, c->maplen, PROT_READ | PROT_WRITE, MAP_SHARED, c->fd, 0);
    if (c->map == MAP_FAILED) {
        c->map = NULL;
        close(c->fd);
        c->fd = -1;
        return -1;
    }
    return 0;
}

static void close_layer(struct ctx *c)
{
    if (c->map) {
        munmap(c->map, c->maplen);
        c->map = NULL;
    }
    if (c->fd >= 0) {
        close(c->fd);
        c->fd = -1;
    }
}

/* ---------------------------------------------------------------- module */

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    c->fd = -1;
    snprintf(c->path, sizeof c->path, "%s", "/dev/fb1");
    *out = c;
    g_ctx = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    const char *dev = c->api->config_get(c->api->priv, "display.fb", "/dev/fb1");
    snprintf(c->path, sizeof c->path, "%s", dev);

    int want_region = c->api->config_get(c->api->priv, "display.set_region", "1")[0] == '1';
    c->mirror = c->api->config_get(c->api->priv, "display.mirror", "1")[0] == '1';
    c->pan = c->api->config_get(c->api->priv, "display.pan", want_region ? "0" : "1")[0] == '1';

    drm_bringup(c);

    if (open_layer(c) != 0) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "%s: cannot open layer", c->path);
        return -1;
    }
    c->api->log(c->api->priv, OA_LOG_INFO, "%s: %ux%u %ubpp, %u frame(s), back=%u", c->path,
                c->var.xres, c->var.yres, c->var.bits_per_pixel, c->frames, c->back);

    /* Rotation is configurable; present() still commits with the vendor ioctl
     * when rotation is disabled. Region words 0/1 retain the current source origin. */
    if (want_region) {
        int rc = set_region(c, c->var.xres, c->var.yres);
        c->api->log(c->api->priv, rc == 0 ? OA_LOG_INFO : OA_LOG_WARN,
                    "region -> %ux%u (%s)", c->var.xres, c->var.yres, rc == 0 ? "ok" : "refused");
    }

    c->api->publish(c->api->priv, OA_SVC_DISPLAY, 1, &g_iface);

    /* optional self-test: a colour band, so "does the layer actually show" is one command away */
    const char *test = c->api->config_get(c->api->priv, "display.test", "0");
    if (test[0] == '1') {
        fill_rect(c, 0, 0, (int)c->var.xres, (int)c->var.yres / 8, 255, 0, 255);
        present();
        c->api->log(c->api->priv, OA_LOG_INFO, "self-test: magenta band painted");
    }
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    c->api->release(c->api->priv, OA_SVC_DISPLAY);
    c->api->unpublish(c->api->priv, OA_SVC_DISPLAY);
    if (c->map)
        close_layer(c);
}

static void m_destroy(void *v)
{
    if (g_ctx == v)
        g_ctx = NULL;
    free(v);
}

static int m_suspend(void *v)
{
    close_layer(v);
    return 0;
}

static int m_resume(void *v)
{
    struct ctx *c = v;
    return open_layer(c) == 0 ? 0 : -1;
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "display",
    .version = "0.1",
    .requires = { OA_SVC_PLAT, "" },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
    .suspend = m_suspend,
    .resume = m_resume,
};

OA_MODULE_EXPORT(g_module)
