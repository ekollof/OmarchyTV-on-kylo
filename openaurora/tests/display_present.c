/* Exercise the actual display module with a fake fbdev, without opening devices.
 * The commit expectations follow NVTGA_OSD_UpdateRegion/FlipRegion and
 * _ker_osd_fb_PanDisplay, not the old display.pan implementation. */
#include <assert.h>
#include <errno.h>
#include <linux/fb.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include "openaurora/module.h"
#include "openaurora/services.h"

const oa_module *oa_module_entry(void);
static const oa_display_iface *display;
static struct fb_var_screeninfo var;
static unsigned char pixels[256];
static const char *rotate;
static unsigned commits, last_yoffset, syncs, opens, closes;
static int fail_sync, fail_commit;

int __wrap_open(const char *path, int flags, ...)
{
    (void)flags;
    assert(!strcmp(path, "/dev/fb1"));
    opens++;
    return 123;
}
int __wrap_close(int fd) { assert(fd == 123); closes++; return 0; }
int __wrap_system(const char *command) { (void)command; assert(!"unexpected system call"); return -1; }
void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    assert(!addr && len <= sizeof pixels && prot == (PROT_READ | PROT_WRITE));
    assert(flags == MAP_SHARED && fd == 123 && offset == 0);
    return pixels;
}
int __wrap_munmap(void *addr, size_t len)
{ assert(addr == pixels && len <= sizeof pixels); return 0; }
int __wrap_msync(void *addr, size_t len, int flags)
{
    assert(addr == pixels && len <= sizeof pixels && flags == MS_SYNC);
    syncs++;
    if (fail_sync) { errno = EIO; return -1; }
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
    case FBIOGET_VSCREENINFO: memcpy(arg, &var, sizeof var); return 0;
    case FBIOGET_FSCREENINFO: {
        struct fb_fix_screeninfo *fix = arg;
        memset(fix, 0, sizeof *fix);
        fix->line_length = var.xres * 4;
        fix->smem_len = fix->line_length * var.yres_virtual;
        return 0;
    }
    case 0x8020c86cu: memset(arg, 0, 32); return 0;
    case 0x4020c86bu: return 0;
    case FBIOPAN_DISPLAY: {
        const struct fb_var_screeninfo *commit = arg;
        assert(syncs > commits); /* sync precedes every attempted commit */
        assert(commit->xres == var.xres && commit->yres == var.yres);
        assert(commit->yoffset + var.yres <= var.yres_virtual);
        last_yoffset = commit->yoffset;
        commits++;
        if (fail_commit) { errno = EIO; return -1; }
        var.yoffset = commit->yoffset;
        return 0;
    }
    default: assert(!"unexpected framebuffer ioctl"); return -1;
    }
}
static void log_message(void *priv, int level, const char *fmt, ...)
{ (void)priv; (void)level; (void)fmt; }
static const char *config(void *priv, const char *key, const char *def)
{
    (void)priv;
    if (!strcmp(key, "display.pan") && rotate) return rotate;
    if (!strcmp(key, "display.mirror")) return "0";
    return def;
}
static int publish(void *priv, const char *name, uint32_t version, const void *iface)
{
    (void)priv;
    assert(!strcmp(name, OA_SVC_DISPLAY) && version == 1);
    display = iface;
    return 0;
}
static int unpublish(void *priv, const char *name)
{ (void)priv; assert(!strcmp(name, OA_SVC_DISPLAY)); display = NULL; return 0; }
static void release(void *priv, const char *name) { (void)priv; (void)name; }
static const oa_host_api api = { .size = sizeof(oa_host_api), .abi = OA_ABI_VERSION,
    .log = log_message, .config_get = config, .publish = publish,
    .unpublish = unpublish, .release = release };

static void *start(unsigned frames, unsigned yoffset, const char *rotation)
{
    memset(&var, 0, sizeof var);
    memset(pixels, 0, sizeof pixels);
    var.xres = var.xres_virtual = 8;
    var.yres = 2;
    var.yres_virtual = frames * var.yres;
    var.bits_per_pixel = 32;
    var.yoffset = yoffset;
    rotate = rotation;
    commits = syncs = 0;
    fail_sync = fail_commit = 0;
    void *ctx = NULL;
    assert(oa_module_entry()->init(&api, &ctx) == 0);
    assert(oa_module_entry()->start(ctx) == 0 && display);
    return ctx;
}
static void finish(void *ctx)
{
    oa_module_entry()->stop(ctx);
    oa_module_entry()->destroy(ctx);
    assert(!display && opens == closes);
}
int main(void)
{
    void *ctx = start(2, 2, NULL); /* default rotation off, second frame current */
    assert(display->clear(0x12, 0x34, 0x56) == 0);
    assert(pixels[0] == 0 && pixels[64] == 0x56 && pixels[66] == 0x12);
    assert(display->present() == 0 && commits == 1 && last_yoffset == 2);
    assert(display->present() == 0 && commits == 2 && last_yoffset == 2);
    assert(oa_module_entry()->suspend(ctx) == 0);
    var.yoffset = 0;
    assert(oa_module_entry()->resume(ctx) == 0);
    assert(display->clear(1, 2, 3) == 0 && pixels[0] == 3);
    assert(display->present() == 0 && last_yoffset == 0);
    finish(ctx);
    puts("  ok   default current-frame presentation commits every time, including after resume");

    ctx = start(1, 0, "1");
    assert(display->present() == 0 && commits == 1 && last_yoffset == 0);
    finish(ctx);
    puts("  ok   single-buffer presentation also commits through FBIOPAN_DISPLAY");

    ctx = start(2, 0, "1");
    fail_sync = 1;
    assert(display->present() == -1 && commits == 0);
    fail_sync = 0;
    fail_commit = 1;
    assert(display->present() == -1 && commits == 1 && last_yoffset == 2);
    fail_commit = 0;
    assert(display->present() == 0 && commits == 2 && last_yoffset == 2);
    assert(display->present() == 0 && commits == 3 && last_yoffset == 0);
    finish(ctx);
    puts("  ok   sync/commit failures propagate and retry the same frame before rotation");
    return 0;
}
