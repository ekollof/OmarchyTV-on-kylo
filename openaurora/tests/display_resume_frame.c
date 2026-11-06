/* Resume first-frame contract: after a suspend closed the layer, reopening a
 * driver that clears its mapping must re-render the retained scene and commit
 * it through FBIOPAN_DISPLAY (the recovered first-pan show path). The vendor
 * idtvResume re-enables the OSD plane and restores its STR frame; this is the
 * fbdev userspace equivalent and does not claim kernel PM parity. */
#include <assert.h>
#include <errno.h>
#include <linux/fb.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include <time.h>
#include "openaurora/fb_snapshot.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

const oa_module *oa_module_entry(void);
static const oa_display_iface *display;
static struct fb_var_screeninfo var;
static unsigned char pixels[128];
static uint32_t region[8], mau;
static unsigned opens, closes, commits, region_writes, last_yoffset;
static unsigned scanout_ok, scanout_bad;
static int fail_commit, kernel_suspended, bad_verify, arm_bad_get;
static char last_log[256];

int __wrap_open(const char *path, int flags, ...)
{
    (void)flags;
    assert(!strcmp(path, "/dev/fb1"));
    opens++;
    /* A reopen is not guaranteed to preserve userspace-visible pixels; the
     * module must re-render its retained scene, not trust stale storage. */
    memset(pixels, 0xa5, sizeof pixels);
    return 123;
}
int __wrap_close(int fd) { assert(fd == 123); closes++; return 0; }
int __wrap_system(const char *command) { (void)command; assert(!"unexpected system call"); return -1; }
void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    assert(!addr && len == sizeof pixels && fd == 123 && !offset);
    assert(prot == PROT_READ || prot == (PROT_READ | PROT_WRITE));
    assert(flags == MAP_SHARED);
    return pixels;
}
int __wrap_munmap(void *addr, size_t len) { assert(addr == pixels && len == sizeof pixels); return 0; }
int __wrap_msync(void *addr, size_t len, int flags)
{
    const unsigned char *p = addr;
    assert(flags == MS_SYNC && len > 0);
    assert(p >= pixels && p + len <= pixels + sizeof pixels);
    return 0;
}
int __wrap_ioctl(int fd, unsigned long request, ...)
{
    assert(fd == 123);
    va_list args;
    va_start(args, request);
    void *arg = va_arg(args, void *);
    va_end(args);
    switch (request) {
    case FBIOGET_VSCREENINFO:
        memcpy(arg, &var, sizeof var);
        if (arm_bad_get) {
            /* One-shot mismatch for the post-pan verification read; the next
             * reopen/retry reads the real state again. */
            ((struct fb_var_screeninfo *)arg)->yoffset =
                var.yoffset ? 0 : var.yres;
            arm_bad_get = 0;
        }
        return 0;
    case FBIOGET_FSCREENINFO: {
        struct fb_fix_screeninfo *fix = arg;
        memset(fix, 0, sizeof *fix);
        fix->line_length = var.xres * 4;
        fix->smem_len = fix->line_length * var.yres_virtual;
        return 0;
    }
    case OA_FB_REGION_RD: memcpy(arg, region, sizeof region); return 0;
    case OA_FB_REGION_WR: memcpy(region, arg, sizeof region); region_writes++; return 0;
    case OA_FB_MAU_RD: *(uint32_t *)arg = mau; return 0;
    case FBIOPAN_DISPLAY: {
        const struct fb_var_screeninfo *commit = arg;
        if (kernel_suspended) { errno = EFAULT; return -1; }
        assert(commit->yoffset + var.yres <= var.yres_virtual);
        commits++;
        if (fail_commit) { fail_commit = 0; errno = EIO; return -1; }
        last_yoffset = commit->yoffset;
        var.yoffset = commit->yoffset;
        if (bad_verify) { arm_bad_get = 1; bad_verify = 0; }
        return 0;
    }
    default: assert(!"unexpected framebuffer ioctl"); return -1;
    }
}
static void log_message(void *priv, int level, const char *fmt, ...)
{
    (void)priv; (void)level;
    va_list ap; va_start(ap, fmt);
    vsnprintf(last_log, sizeof last_log, fmt, ap);
    va_end(ap);
    if (strstr(last_log, "scanout verified")) scanout_ok++;
    if (strstr(last_log, "scanout mismatch")) scanout_bad++;
}
static const char *config(void *priv, const char *key, const char *def)
{
    (void)priv;
    if (!strcmp(key, "display.fb")) return "/dev/fb1";
    if (!strcmp(key, "display.restore_snapshot")) return "1";
    if (!strcmp(key, "display.mirror")) return "0";
    if (!strcmp(key, "display.verify_scanout")) return "1";
    return def;
}
static int publish(void *priv, const char *name, uint32_t version, const void *iface)
{
    (void)priv;
    if (!strcmp(name, OA_SVC_DISPLAY)) {
        assert(version == 2);
        display = iface;
        return 0;
    }
    return 0;
}
static int unpublish(void *priv, const char *name)
{
    (void)priv;
    if (!strcmp(name, OA_SVC_DISPLAY)) display = NULL;
    return 0;
}
static void release(void *priv, const char *name) { (void)priv; (void)name; }
static const oa_host_api api = { .size = sizeof(oa_host_api), .abi = OA_ABI_VERSION,
    .log = log_message, .config_get = config, .publish = publish,
    .unpublish = unpublish, .release = release };

int main(void)
{
    var = (struct fb_var_screeninfo){0};
    var.xres = var.xres_virtual = 8;
    var.yres = 2;
    var.yres_virtual = 4;
    var.bits_per_pixel = 32;
    var.yoffset = 2;
    mau = 0;
    void *ctx = NULL;
    assert(oa_module_entry()->init(&api, &ctx) == 0);
    assert(oa_module_entry()->start(ctx) == 0 && display);

    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 8; x++)
            assert(!display->fill(x, y, 1, 1, x * 16, y * 16, 0));
    assert(!display->present());
    assert(commits == 1 && last_yoffset == 2);
    /* display.pan defaults off (set_region on): the current frame is offset 2. */
    assert(pixels[64] == 0 && pixels[65] == 0 && pixels[66] == 0 && pixels[67] == 255);
    assert(pixels[64 + (1 * 8 + 7) * 4 + 1] == 16 &&
           pixels[64 + (1 * 8 + 7) * 4 + 2] == 112);
    unsigned frame_start = commits, writes = region_writes;

    assert(oa_module_entry()->suspend(ctx) == 0);
    assert(opens == 1 && closes == 1 && pixels[0] == 0xa5);
    /* Snapshot restore also writes the region and pans, so count after it. */
    frame_start = commits;
    writes = region_writes;
    assert(oa_module_entry()->resume(ctx) == 0);
    assert(opens == 2 && closes == 1);
    assert(region_writes == writes + 1);
    assert(commits == frame_start + 1 && last_yoffset == 2);
    assert(scanout_ok == 1);
    assert(memchr(last_log, 0, sizeof last_log) && strstr(last_log, "first frame committed"));
    /* The mapping was cleared on open; the retained scene was re-rendered. */
    assert(pixels[64] == 0 && pixels[65] == 0 && pixels[66] == 0 && pixels[67] == 255);
    assert(pixels[64 + (1 * 8 + 7) * 4 + 1] == 16 &&
           pixels[64 + (1 * 8 + 7) * 4 + 2] == 112);
    assert(!display->clear(1, 2, 3));
    assert(!display->present() && last_yoffset == 2);

    /* First-frame commit failure: restore the pre-resume snapshot, close, keep
     * the scene, and make a later resume retry the whole path. */
    assert(oa_module_entry()->suspend(ctx) == 0);
    assert(opens == 2 && closes == 2);
    fail_commit = 1;
    assert(oa_module_entry()->resume(ctx) == -1);
    assert(opens == 3 && closes == 3);
    assert(oa_module_entry()->resume(ctx) == 0);
    assert(opens == 4 && closes == 3);
    assert(commits >= frame_start + 4);
    assert(pixels[64] == 3 && pixels[65] == 2 && pixels[66] == 1 && pixels[67] == 255);

    /* The pan succeeds but the driver reports the wrong active yoffset: the
     * first frame must not be accepted, restore/close, then retry cleanly. */
    assert(oa_module_entry()->suspend(ctx) == 0);
    assert(opens == 4 && closes == 4);
    bad_verify = 1;
    assert(oa_module_entry()->resume(ctx) == -1);
    assert(scanout_bad == 1);
    assert(opens == 5 && closes == 5);
    assert(oa_module_entry()->resume(ctx) == 0);
    assert(opens == 6 && closes == 5);
    assert(scanout_ok == 3);
    assert(pixels[64] == 3 && pixels[65] == 2 && pixels[66] == 1 && pixels[67] == 255);

    for (unsigned i = 0;; ++i) {
        int rc = oa_module_entry()->quiesce(ctx);
        if (!rc) break;
        assert(i < 10000);
        const struct timespec pause = {.tv_nsec = 1000000};
        nanosleep(&pause, NULL);
    }
    oa_module_entry()->stop(ctx);
    oa_module_entry()->destroy(ctx);
    assert(!display && opens == closes);
    puts("ok display resume frame: reopen clears the map, retained scene recommits, region reapplied, failure retries");
    return 0;
}
