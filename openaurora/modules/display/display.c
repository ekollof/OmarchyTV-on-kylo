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
#include <math.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/canvas.h"
#include "openaurora/fb_snapshot.h"
#include "openaurora/ui_anim.h"

extern char **environ;

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
    struct oa_fb_snapshot snapshot;
    int snapshot_active;
    int closing, cleanup_running, cleanup_result;
    pthread_t cleanup_thread;
    uint64_t retry_after;
    unsigned cleanup_delay_ms; /* diagnostic: hold the close worker open */
    int mirror;       /* copy the painted frame into the other frames (fights DFB's flip) */
    int profile;      /* log per-stage present timings when display.profile=1 */

    /* Offscreen RGBA scene: every draw renders here, present() copies it to the visible frame or
     * maps it through the recovered full-screen transition quad. */
    uint8_t *scene;
    int scene_w, scene_h;
    int scene_opaque; /* a full opaque fill; subsequent draws cannot reduce alpha */
    int fs_type;      /* 0 = no transition */
    float fs_p;

    /* Changed scene rectangle since the last successful present. */
    int dirty;
    int dirty_x0, dirty_y0, dirty_x1, dirty_y1;
};

static struct ctx *g_ctx;

/* ---------------------------------------------------------------- drawing */

static void put_px(struct ctx *c, uint8_t *row, unsigned x, int r, int g, int b, int a)
{
    if (c->var.bits_per_pixel == 32) {
        /* measured on the set: /dev/fb1 is BGRA (a test pattern with byte0=255 showed as blue),
         * so the red channel goes in byte 2, not byte 0. */
        uint8_t *p = row + (size_t)x * 4;
        p[0] = (uint8_t)b;
        p[1] = (uint8_t)g;
        p[2] = (uint8_t)r;
        p[3] = (uint8_t)a;
    } else if (c->var.bits_per_pixel == 16) {
        ((uint16_t *)row)[x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
}

static void mark_dirty(struct ctx *c, int x, int y, int w, int h)
{
    if (!c || !c->scene || w <= 0 || h <= 0)
        return;
    int64_t right = (int64_t)x + w, bottom = (int64_t)y + h;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = right > c->scene_w ? c->scene_w : (int)right;
    int y1 = bottom > c->scene_h ? c->scene_h : (int)bottom;
    if (x0 >= x1 || y0 >= y1)
        return;
    if (!c->dirty) {
        c->dirty = 1;
        c->dirty_x0 = x0; c->dirty_y0 = y0;
        c->dirty_x1 = x1; c->dirty_y1 = y1;
        return;
    }
    if (x0 < c->dirty_x0) c->dirty_x0 = x0;
    if (y0 < c->dirty_y0) c->dirty_y0 = y0;
    if (x1 > c->dirty_x1) c->dirty_x1 = x1;
    if (y1 > c->dirty_y1) c->dirty_y1 = y1;
}

static int fill_rect(struct ctx *c, int x, int y, int w, int h, int r, int g, int b)
{
    if (c->closing || !c->scene)
        return -1;
    int fx = c->scene_w, fy = c->scene_h;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > fx ? fx : x + w;
    int y1 = (y + h) > fy ? fy : y + h;
    if (x0 >= x1 || y0 >= y1)
        return 0;
    if (x0 == 0 && y0 == 0 && x1 == fx && y1 == fy) c->scene_opaque = 1;
    mark_dirty(c, x0, y0, x1 - x0, y1 - y0);

    for (int yy = y0; yy < y1; yy++) {
        uint8_t *row = c->scene + (size_t)yy * (size_t)fx * 4;
        for (int xx = x0; xx < x1; xx++) {
            uint8_t *p = row + (size_t)xx * 4;
            p[0] = (uint8_t)r;
            p[1] = (uint8_t)g;
            p[2] = (uint8_t)b;
            p[3] = 0xff;
        }
    }
    return 0;
}

static int clear(int r, int g, int b)
{
    if (!g_ctx)
        return -1;
    return fill_rect(g_ctx, 0, 0, (int)g_ctx->var.xres, (int)g_ctx->var.yres, r, g, b);
}

static int clear_rgba(int r, int g, int b, int a)
{
    struct ctx *c = g_ctx;
    if (!c || c->closing || !c->scene) return -1;
    oa_canvas canvas = {c->scene, c->scene_w, c->scene_h};
    if (oa_canvas_clear(&canvas, r, g, b, a)) return -1;
    c->scene_opaque = a == 255;
    mark_dirty(c, 0, 0, c->scene_w, c->scene_h);
    return 0;
}

static int fill(int x, int y, int w, int h, int r, int g, int b)
{
    return g_ctx ? fill_rect(g_ctx, x, y, w, h, r, g, b) : -1;
}

static int blit(int x, int y, int w, int h, const unsigned char *rgba)
{
    struct ctx *c = g_ctx;
    if (!c || c->closing || !c->scene || !rgba)
        return -1;

    mark_dirty(c, x, y, w, h);
    int W = c->scene_w, H = c->scene_h;
    for (int sy = 0; sy < h; sy++) {
        int dy = y + sy;
        if (dy < 0 || dy >= H)
            continue;
        uint8_t *row = c->scene + (size_t)dy * (size_t)W * 4;
        for (int sx = 0; sx < w; sx++) {
            int dx = x + sx;
            if (dx < 0 || dx >= W)
                continue;
            const unsigned char *p = rgba + ((size_t)sy * (size_t)w + (size_t)sx) * 4;
            uint8_t *d = row + (size_t)dx * 4;
            oa_canvas_over(d, p);
        }
    }
    return 0;
}

static int blit_scaled(int x, int y, int w, int h, int sw, int sh, const unsigned char *rgba)
{
    struct ctx *c = g_ctx;
    if (!c || c->closing || !c->scene || !rgba || sw <= 0 || sh <= 0 || w <= 0 || h <= 0)
        return -1;
    oa_canvas canvas = {c->scene, c->scene_w, c->scene_h};
    int rc = oa_canvas_blit_mipmap(&canvas, x, y, w, h, sw, sh, rgba);
    if (!rc) mark_dirty(c, x, y, w, h);
    return rc;
}

static int patch_fill_rgba(int x, int y, int w, int h, int r, int g, int b, int a)
{
    struct ctx *c = g_ctx;
    if (!c || c->closing || !c->scene || w <= 0 || h <= 0 ||
        r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 ||
        a < 0 || a > 255)
        return -1;
    int64_t right = (int64_t)x + w, bottom = (int64_t)y + h;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = right > c->scene_w ? c->scene_w : (int)right;
    int y1 = bottom > c->scene_h ? c->scene_h : (int)bottom;
    if (x0 >= x1 || y0 >= y1)
        return 0;
    for (int yy = y0; yy < y1; yy++) {
        uint8_t *row = c->scene + (size_t)yy * (size_t)c->scene_w * 4;
        for (int xx = x0; xx < x1; xx++) {
            uint8_t *p = row + (size_t)xx * 4;
            p[0] = (uint8_t)r; p[1] = (uint8_t)g;
            p[2] = (uint8_t)b; p[3] = (uint8_t)a;
        }
    }
    mark_dirty(c, x0, y0, x1 - x0, y1 - y0);
    return 0;
}

/* ------------------------------------------------------- scene presentation */

static void scene_to_fb_rect(struct ctx *c, int x0, int y0, int x1, int y1)
{
    unsigned w = c->var.xres, h = c->var.yres;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)w) x1 = (int)w;
    if (y1 > (int)h) y1 = (int)h;
    if (x0 >= x1 || y0 >= y1) return;
    if (c->var.bits_per_pixel == 32) {
        /* The common opaque case needs no per-channel multiply. A 32-bit store
         * is also materially cheaper than four byte stores on the TV's ARM. */
        for (int y = y0; y < y1; y++) {
            uint8_t *row = c->map + (size_t)(c->back * h + (unsigned)y) * c->fix.line_length;
            uint32_t *dst = (uint32_t *)row;
            const uint8_t *srow = c->scene + (size_t)y * (size_t)w * 4;
            for (int x = x0; x < x1; x++) {
                const uint8_t *p = srow + (size_t)x * 4;
                unsigned a = p[3];
                if (a == 255) {
                    dst[x] = (uint32_t)p[2] | ((uint32_t)p[1] << 8) |
                             ((uint32_t)p[0] << 16) | 0xff000000u;
                } else {
                    unsigned r = (p[0]*a+127)/255, g = (p[1]*a+127)/255;
                    unsigned b = (p[2]*a+127)/255;
                    dst[x] = b | (g << 8) | (r << 16) | (a << 24);
                }
            }
        }
        return;
    }
    for (int y = y0; y < y1; y++) {
        uint8_t *row = c->map + (size_t)(c->back * h + (unsigned)y) * c->fix.line_length;
        const uint8_t *srow = c->scene + (size_t)y * (size_t)w * 4;
        for (int x = x0; x < x1; x++) {
            const uint8_t *p = srow + (size_t)x * 4;
            put_px(c, row, (unsigned)x, (p[0]*p[3]+127)/255, (p[1]*p[3]+127)/255,
                   (p[2]*p[3]+127)/255, p[3]);
        }
    }
}

/* Map the offscreen scene through the recovered four-vertex clip-space quad:
 *   BL(-1,-1) BR(1,-1) TR(1,1) TL(-1,1), UV (0,0) (1,0) (1,1) (0,1)
 * Type 1 scales only the two right vertices (keystone); types 2/3 translate x,
 * 4/5 y, 7/8 scale uniformly. The vendor draws GL_TRIANGLE_FAN (0,1,2),(0,2,3),
 * with w=1: interpolate UV linearly within each triangle, not bilinearly across
 * the quad. For the keystone these differ on either side of diagonal 0->2.
 */
/* Opaque slide/fade scenes have one constant fractional texture offset. Build
 * the exact two-texel/filter/fade result once per byte pair rather than doing
 * float channel interpolation for every 1080p pixel. This is the same linear
 * filter as the general rasterizer, not a nearest-neighbour approximation. */
static void slide_scene(struct ctx *c)
{
    oa_fs_scene transform;
    oa_fs_scene_transform(c->fs_type, c->fs_p, &transform);
    int vertical = transform.dx == 0.0f;
    float shift = vertical ? transform.dy * (float)c->scene_h * 0.5f :
                             -transform.dx * (float)c->scene_w * 0.5f;
    int base = (int)floorf(shift);
    float fraction = shift - (float)base;
    uint8_t blend[65536];
    for (int a = 0; a < 256; a++)
        for (int b = 0; b < 256; b++)
            blend[a * 256 + b] = (uint8_t)(((float)a + fraction * (float)(b-a)) * c->fs_p + 0.5f);
    uint8_t alpha = (uint8_t)(255.0f * c->fs_p + 0.5f);
    for (int y = 0; y < c->scene_h; y++) {
        uint8_t *row = c->map + (size_t)(c->back * c->var.yres + (unsigned)y) * c->fix.line_length;
        float position = (float)y + 0.5f + shift;
        if (vertical && (position < 0.0f || position > (float)c->scene_h)) {
            memset(row, 0, (size_t)c->scene_w * 4);
            continue;
        }
        int sy = vertical ? y + base : y;
        int ny = sy + (vertical != 0);
        if (sy < 0) sy = 0;
        if (ny < 0) ny = 0;
        if (sy >= c->scene_h) sy = c->scene_h - 1;
        if (ny >= c->scene_h) ny = c->scene_h - 1;
        const uint8_t *top = c->scene + (size_t)sy * (size_t)c->scene_w * 4;
        const uint8_t *bottom = c->scene + (size_t)ny * (size_t)c->scene_w * 4;
        for (int x = 0; x < c->scene_w; x++) {
            uint8_t *out = row + (size_t)x * 4;
            position = (float)x + 0.5f + shift;
            if (!vertical && (position < 0.0f || position > (float)c->scene_w)) {
                memset(out, 0, 4);
                continue;
            }
            int sx = vertical ? x : x + base;
            int nx = sx + !vertical;
            if (sx < 0) sx = 0;
            if (nx < 0) nx = 0;
            if (sx >= c->scene_w) sx = c->scene_w - 1;
            if (nx >= c->scene_w) nx = c->scene_w - 1;
            const uint8_t *a = top + (size_t)sx * 4;
            const uint8_t *b = bottom + (size_t)nx * 4;
            out[0] = blend[a[2] * 256 + b[2]];
            out[1] = blend[a[1] * 256 + b[1]];
            out[2] = blend[a[0] * 256 + b[0]];
            out[3] = alpha;
        }
    }
}

static void warp_scene(struct ctx *c)
{
    const float W = (float)c->scene_w, H = (float)c->scene_h;
    oa_fs_scene sc;
    float xl, xr, ybl, ytl, ybr, ytr;

    if (c->scene_opaque && c->var.bits_per_pixel == 32 && c->fs_type >= OA_FS_SLIDE_NEG &&
        c->fs_type <= OA_FS_UNIFORM_ONLY) {
        slide_scene(c);
        return;
    }

    oa_fs_scene_transform(c->fs_type, c->fs_p, &sc);
    if (c->fs_type == OA_FS_SCALE_AXES) {
        xl = -1.0f;
        xr = sc.sx;
        ybl = -1.0f;
        ytl = 1.0f;
        ybr = -sc.sy;
        ytr = sc.sy;
    } else {
        xl = sc.dx - sc.sx;
        xr = sc.dx + sc.sx;
        ybl = sc.dy - sc.sy;
        ytl = sc.dy + sc.sy;
        ybr = ybl;
        ytr = ytl;
    }

    const float inv_width = 1.0f / (xr - xl);
    const float inv_left_height = 1.0f / (ytl - ybl);
    const float inv_right_height = 1.0f / (ytr - ybr);
    for (unsigned Y = 0; Y < c->var.yres; Y++) {
        float yc = 1.0f - 2.0f * ((float)Y + 0.5f) / H;
        uint8_t *row = c->map + (size_t)(c->back * c->var.yres + Y) * c->fix.line_length;
        for (unsigned X = 0; X < c->var.xres; X++) {
            float xc = 2.0f * ((float)X + 0.5f) / W - 1.0f;
            float u = (xc - xl) * inv_width;
            float v = -1.0f;
            if (u >= 0.0f && u <= 1.0f) {
                float bottom = ybl + u * (ybr - ybl);
                float diagonal = ybl + u * (ytr - ybl);
                v = yc <= diagonal ? (yc - bottom) * inv_right_height :
                                     u + (yc - diagonal) * inv_left_height;
            }
            if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
                put_px(c, row, X, 0, 0, 0, 0);
                continue;
            }
            /* createFrameBufferObject selects GL_LINEAR and CLAMP_TO_EDGE.
             * Normalized UVs address texel centers at (i+0.5)/size. */
            float tx = u * W - 0.5f, ty = (1.0f - v) * H - 0.5f;
            if (tx < 0.0f) tx = 0.0f;
            if (ty < 0.0f) ty = 0.0f;
            if (tx > W - 1.0f) tx = W - 1.0f;
            if (ty > H - 1.0f) ty = H - 1.0f;
            int sx = (int)tx, sy = (int)ty;
            int nx = sx + (sx + 1 < c->scene_w), ny = sy + (sy + 1 < c->scene_h);
            float fx = tx - (float)sx, fy = ty - (float)sy, sample[4];
            const uint8_t *p00 = c->scene + ((size_t)sy * (size_t)c->scene_w + (size_t)sx) * 4;
            const uint8_t *p10 = c->scene + ((size_t)sy * (size_t)c->scene_w + (size_t)nx) * 4;
            const uint8_t *p01 = c->scene + ((size_t)ny * (size_t)c->scene_w + (size_t)sx) * 4;
            const uint8_t *p11 = c->scene + ((size_t)ny * (size_t)c->scene_w + (size_t)nx) * 4;
            for (int channel = 0; channel < 4; channel++) {
                float top = (float)p00[channel] + fx * ((float)p10[channel] - (float)p00[channel]);
                float bottom = (float)p01[channel] + fx * ((float)p11[channel] - (float)p01[channel]);
                sample[channel] = top + fy * (bottom - top);
            }
            /* The vendor texture shader multiplies alpha by progress (uAlpha),
             * then blends SRC_ALPHA/ONE_MINUS_SRC_ALPHA onto transparent black.
             * Alpha uses ONE/ONE. Preserve that premultiplied output in BGRA;
             * type6 is a fade even though its vertices do not move. */
            float alpha = sample[3] * c->fs_p;
            float coverage = alpha / 255.0f;
            put_px(c, row, X, (int)(sample[0] * coverage + 0.5f),
                   (int)(sample[1] * coverage + 0.5f),
                   (int)(sample[2] * coverage + 0.5f), (int)(alpha + 0.5f));
        }
    }
}

static int set_scene_transform(int type, float progress)
{
    struct ctx *c = g_ctx;
    if (!c || c->closing)
        return -1;
    if (type < 0 || type > 8)
        return -1;
    if (type == 0 || progress < 0.0f || progress > 1.0f || progress != progress) {
        c->fs_type = 0;
        c->fs_p = 0.0f;
        mark_dirty(c, 0, 0, c->scene_w, c->scene_h);
        return 0;
    }
    c->fs_type = type;
    c->fs_p = progress;
    mark_dirty(c, 0, 0, c->scene_w, c->scene_h);
    return 0;
}

static uint64_t monotonic_ms(void);

static int present(void){
    struct ctx *c = g_ctx;
    if (!c || c->closing || !c->map || !c->scene)
        return -1;

    int full = c->fs_type > 0;
    /* Rotating frames without mirroring would show stale pixels outside a
     * partial dirty rectangle, so force a full render in that mode. */
    if (c->pan && c->frames > 1 && !c->mirror)
        full = 1;
    int noop = !full && !c->dirty;
    int x0 = full ? 0 : noop ? 0 : c->dirty_x0;
    int y0 = full ? 0 : noop ? 0 : c->dirty_y0;
    int x1 = full ? (int)c->var.xres : noop ? (int)c->var.xres : c->dirty_x1;
    int y1 = full ? (int)c->var.yres : noop ? (int)c->var.yres : c->dirty_y1;
    size_t frame_bytes = (size_t)c->fix.line_length * c->var.yres;

    uint64_t t0 = c->profile ? monotonic_ms() : 0;
    if (!noop) {
        if (c->fs_type > 0 && c->fs_p == 0.0f)
            memset(c->map + (size_t)c->back * c->var.yres * c->fix.line_length,
                   0, (size_t)c->var.yres * c->fix.line_length);
        else if (c->fs_type > 0 && c->fs_p > 0.0f && c->fs_p < 1.0f)
            warp_scene(c);
        else
            scene_to_fb_rect(c, x0, y0, x1, y1);
    }
    uint64_t t1 = c->profile ? monotonic_ms() : 0;

    /* Mirror the rendered rectangle into every frame of the layer.
     *
     * The vendor's DirectFB also owns this layer and flips its own two buffers, so a single
     * frame of ours alternates with a frame of theirs — visible as a one-frame flash. Filling
     * both halves means whichever frame gets scanned out is ours, and the image stays put
     * until aurora repaints. A takeover with stock stopped sets display.mirror=0; the active
     * frame and its dirty rows are then the only work.
     */
    if (!noop && c->mirror && c->frames > 1) {
        size_t span = (size_t)(x1 - x0) * 4u;
        for (int y = y0; y < y1; y++) {
            uint8_t *src = c->map + (size_t)c->back * frame_bytes +
                           (size_t)y * c->fix.line_length + (size_t)x0 * 4u;
            for (unsigned f = 0; f < c->frames; f++)
                if (f != c->back)
                    memcpy(c->map + (size_t)f * frame_bytes +
                           (size_t)y * c->fix.line_length + (size_t)x0 * 4u, src, span);
        }
    }
    uint64_t t2 = c->profile ? monotonic_ms() : 0;

    /* Push the rendered rows out before flipping. Sync only page-rounded dirty
     * ranges (and every mirrored frame), not the whole 16.5 MB mapping. */
    long ps = sysconf(_SC_PAGESIZE);
    size_t page = ps > 0 ? (size_t)ps : 4096u;
    unsigned first_frame = noop ? c->back : c->mirror ? 0 : c->back;
    unsigned last_frame = noop ? c->back : c->mirror ? c->frames - 1 : c->back;
    for (unsigned f = first_frame; f <= last_frame; f++) {
        size_t start = (size_t)f * frame_bytes + (size_t)y0 * c->fix.line_length +
                       (size_t)x0 * 4u;
        size_t end = (size_t)f * frame_bytes + (size_t)(y1 - 1) * c->fix.line_length +
                     (size_t)x1 * 4u;
        start &= ~(page - 1u);
        end = (end + page - 1u) & ~(page - 1u);
        if (end > c->maplen) end = c->maplen;
        if (msync(c->map + start, end - start, MS_SYNC) != 0)
            return -1;
    }
    uint64_t t3 = c->profile ? monotonic_ms() : 0;

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
    c->dirty = 0;
    if (c->profile) {
        uint64_t t4 = monotonic_ms();
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "present: scene=%llums mirror=%llums sync=%llums pan=%llums mirror=%d rect=%d,%d %dx%d",
                    (unsigned long long)(t1-t0), (unsigned long long)(t2-t1),
                    (unsigned long long)(t3-t2), (unsigned long long)(t4-t3),
                    c->mirror, x0, y0, x1 - x0, y1 - y0);
    }
    return 0;
}

static int info(int *w, int *h, int *bpp, int *frames)
{
    if (!g_ctx || g_ctx->closing)
        return -1;
    if (w) *w = (int)g_ctx->var.xres;
    if (h) *h = (int)g_ctx->var.yres;
    if (bpp) *bpp = g_ctx->var.bits_per_pixel;
    if (frames) *frames = (int)g_ctx->frames;
    return 0;
}

static const oa_display_iface g_iface = {
    .clear = clear, .fill = fill, .blit = blit, .blit_scaled = blit_scaled,
    .present = present, .info = info, .set_scene_transform = set_scene_transform,
};
static const oa_display_alpha_iface alpha_iface = {clear_rgba};
static const oa_display_patch_iface patch_iface = {patch_fill_rgba};

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
#if defined(OA_NO_VENDOR) || defined(OA_DISPLAY_NO_PLAT)
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

#ifndef OA_DISPLAY_NO_PLAT
static int run_mdev_scan(void)
{
    char *const argv[] = { (char *)"/sbin/mdev", (char *)"-s", NULL };
    pid_t pid = -1;
    int rc = posix_spawn(&pid, "/sbin/mdev", NULL, NULL, argv, environ);
    if (rc) {
        errno = rc;
        return -1;
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}
#endif

static int fb_make_node(struct ctx *c)
{
#ifdef OA_DISPLAY_NO_PLAT
    /* An already-initialized binding must not run global device discovery. */
    (void)c;errno=ENOENT;return -1;
#else
    /* rc.local sets hotplug to /sbin/mdev but leaves `mdev -s` commented out, so a framebuffer
     * registered before the hotplug helper was installed ends up with sysfs entries and no /dev
     * node - exactly the state seen in a takeover boot. mdev is the vendor's own mechanism for
     * populating /dev from sysfs, so use it instead of hand-rolling mknod. */
    if (run_mdev_scan() != 0)
        return -1;
    if (access(c->path, F_OK) != 0)
        return -1;
    c->api->log(c->api->priv, OA_LOG_INFO, "%s: created by mdev -s", c->path);
    return 0;
#endif
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
    c->scene_w = (int)c->var.xres;
    c->scene_h = (int)c->var.yres;
    c->scene = calloc((size_t)c->scene_w * (size_t)c->scene_h, 4);
    c->scene_opaque = 0;
    c->dirty = 0;
    if (!c->scene) {
        munmap(c->map, c->maplen);
        c->map = NULL;
        close(c->fd);
        c->fd = -1;
        return -1;
    }
    return 0;
}

static int restore_snapshot(struct ctx *c)
{
    if (!c->snapshot_active) return 0;
    int rc = oa_fb_snapshot_restore(c->fd, &c->snapshot);
    if (rc) return rc; /* retain snapshot and fd for a later attempt */
    oa_fb_snapshot_free(&c->snapshot);
    c->snapshot_active = 0;
    return 0;
}

static int close_layer(struct ctx *c)
{
    free(c->scene);
    c->scene = NULL;
    c->scene_w = c->scene_h = 0;
    if (c->map) {
        if (munmap(c->map, c->maplen)) return -(errno ? errno : EIO);
        c->map = NULL;
    }
    if (c->fd >= 0) {
        int fd = c->fd;
        c->fd = -1;
        /* Linux close consumes the descriptor even on a final-close error. */
        if (close(fd)) return -(errno ? errno : EIO);
    }
    return 0;
}

/* Snapshot restore/retry is openaurora's temporary-binding/reload policy,
 * not the vendor TDGA RemoveRegion (a no-op), pool unmap or kernel PMI resume.
 * Only this worker accesses mutable layer/snapshot state while closing.
 * The loop reads it again only after actual join (including TLS destruction).
 * No host API is called from the worker. */
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
    c->cleanup_result = restore_snapshot(c);
    if (!c->cleanup_result) c->cleanup_result = close_layer(c);
    return NULL;
}

static uint64_t monotonic_ms(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}

static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
    c->closing = 1;
    /* Every draw/info entry point checks closing while the worker restores
     * and closes the layer; m_stop unpublishes after the worker joins. */
    /* Check running before inspecting fields that the worker may modify. */
    if (c->cleanup_running) {
        int rc = pthread_tryjoin_np(c->cleanup_thread, NULL);
        if (rc) return -rc; /* retain code, ctx, fd and snapshot on join error */
        c->cleanup_running = 0;
        if (c->cleanup_result) {
            c->api->log(c->api->priv, OA_LOG_ERROR, "%s: cleanup pending: %s",
                c->path, strerror(-c->cleanup_result));
            c->retry_after = monotonic_ms() + 250;
            return c->cleanup_result;
        }
    }
    if (c->fd < 0 && !c->map && !c->scene && !c->snapshot_active) return 0;
    if (monotonic_ms() < c->retry_after) return -EAGAIN;
    int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
    if (rc) return -rc;
    c->cleanup_running = 1;
    return -EAGAIN;
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
    if (c->cleanup_running || (c->closing && c->fd >= 0)) return -EBUSY;
    c->closing = 0;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "display.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
#ifdef OA_DISPLAY_NO_PLAT
    if (c->api->config_get(c->api->priv,"display.drm_init","0")[0]=='1') {
        c->api->log(c->api->priv,OA_LOG_ERROR,"display_fb cannot initialize platform HAL");
        errno=ENOTSUP;return -1;
    }
#endif
    const char *dev = c->api->config_get(c->api->priv, "display.fb", "/dev/fb1");
    snprintf(c->path, sizeof c->path, "%s", dev);

    int want_region = c->api->config_get(c->api->priv, "display.set_region", "1")[0] == '1';
    c->mirror = c->api->config_get(c->api->priv, "display.mirror", "1")[0] == '1';
    c->profile = c->api->config_get(c->api->priv, "display.profile", "0")[0] == '1';
    c->pan = c->api->config_get(c->api->priv, "display.pan", want_region ? "0" : "1")[0] == '1';

    drm_bringup(c);

    if (open_layer(c) != 0) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "%s: cannot open layer", c->path);
        return -1;
    }
    c->api->log(c->api->priv, OA_LOG_INFO, "%s: %ux%u %ubpp, %u frame(s), back=%u", c->path,
                c->var.xres, c->var.yres, c->var.bits_per_pixel, c->frames, c->back);

    if (c->api->config_get(c->api->priv, "display.restore_snapshot", "0")[0] == '1') {
        int rc = oa_fb_snapshot_save(c->fd, &c->snapshot);
        if (rc) {
            c->api->log(c->api->priv, OA_LOG_ERROR,
                "%s: full snapshot failed: %s", c->path, strerror(-rc));
            close_layer(c);
            return -1;
        }
        c->snapshot_active = 1;
        c->api->log(c->api->priv, OA_LOG_INFO,
            "%s: full snapshot saved (%u bytes, mau %u)", c->path,
            c->snapshot.fix.smem_len, c->snapshot.mau);
    }

    /* Rotation is configurable; present() still commits with the vendor ioctl
     * when rotation is disabled. Region words 0/1 retain the current source origin. */
    if (want_region) {
        int rc = set_region(c, c->var.xres, c->var.yres);
        c->api->log(c->api->priv, rc == 0 ? OA_LOG_INFO : OA_LOG_WARN,
                    "region -> %ux%u (%s)", c->var.xres, c->var.yres, rc == 0 ? "ok" : "refused");
    }

    if (c->api->publish(c->api->priv, OA_SVC_DISPLAY, 2, &g_iface)) {
        return -1;
    }
    if (c->api->publish(c->api->priv, OA_SVC_DISPLAY_ALPHA, 1, &alpha_iface)) {
        c->api->unpublish(c->api->priv, OA_SVC_DISPLAY);
        return -1;
    }
    if (c->api->publish(c->api->priv, OA_SVC_DISPLAY_PATCH, 1, &patch_iface)) {
        c->api->unpublish(c->api->priv, OA_SVC_DISPLAY_ALPHA);
        c->api->unpublish(c->api->priv, OA_SVC_DISPLAY);
        return -1;
    }

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
    c->api->unpublish(c->api->priv, OA_SVC_DISPLAY_ALPHA);
    c->api->unpublish(c->api->priv, OA_SVC_DISPLAY_PATCH);
    /* ABI3 quiesce completed restoration and close on its owning thread. */
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (c->snapshot_active) {
        oa_fb_snapshot_free(&c->snapshot);
        c->snapshot_active = 0;
    }
    if (g_ctx == c)
        g_ctx = NULL;
    free(c);
}

static int m_suspend(void *v)
{
    struct ctx *c = v;
    if (c->closing || c->cleanup_running) return -EBUSY;
    /* The host's suspend transaction is synchronous; unlike quiesce it has
     * no asynchronous retry protocol. Preserve the existing sync path but
     * propagate failure and retain the snapshot/fd instead of discarding it. */
    int rc = restore_snapshot(c);
    if (rc) return rc;
    return close_layer(c);
}

static int m_resume(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running || c->fd >= 0 || c->snapshot_active) return -EBUSY;
    if (open_layer(c) != 0) return -1;
    if (c->api->config_get(c->api->priv, "display.restore_snapshot", "0")[0] == '1') {
        int rc = oa_fb_snapshot_save(c->fd, &c->snapshot);
        if (rc) {
            c->api->log(c->api->priv, OA_LOG_ERROR,
                "%s: resume snapshot failed: %s", c->path, strerror(-rc));
            close_layer(c);
            return -1;
        }
        c->snapshot_active = 1;
    }
    c->closing = 0;
    return 0;
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
#ifdef OA_DISPLAY_NO_PLAT
    .name = "display_fb",
#else
    .name = "display",
#endif
    .version = "0.1",
#ifndef OA_DISPLAY_NO_PLAT
    .requires = { OA_SVC_PLAT, "" },
#else
    /* Supervised binding to the vendor-initialized fb1: no platform bring-up.
     * Only valid while the vendor has already initialized the OSD layer. */
    .requires = { "", 0 },
#endif
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
    .suspend = m_suspend,
    .resume = m_resume,
    .quiesce = m_quiesce,
};

OA_MODULE_EXPORT(g_module)
