/* Full fb snapshot/restore with a fake device. No real framebuffer is opened. */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "openaurora/fb_snapshot.h"

static struct fb_var_screeninfo var;
static struct fb_fix_screeninfo fix;
static uint32_t region[8];
static uint32_t mau;
static unsigned char fb[4096];
static unsigned writes, pans;
static int fail_region, fail_pan, fail_sync;

static void init_fake(void)
{
    memset(&var, 0, sizeof var);
    memset(&fix, 0, sizeof fix);
    memset(fb, 0xaa, sizeof fb);
    for (int i = 0; i < 8; i++) region[i] = 0x100u + (uint32_t)i;
    var.xres = 8;
    var.yres = 2;
    var.yres_virtual = 4;
    var.bits_per_pixel = 32;
    var.xoffset = 0;
    var.yoffset = 0;
    fix.line_length = 32;
    fix.smem_len = 128;
    mau = 1;
    writes = pans = 0;
    fail_region = fail_pan = fail_sync = 0;
}

void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fd,
    off_t offset)
{
    assert(!addr && len == fix.smem_len && fd == 123 && offset == 0);
    assert(flags == MAP_SHARED);
    assert(prot == PROT_READ || prot == (PROT_READ | PROT_WRITE));
    return fb;
}
int __wrap_munmap(void *addr, size_t len)
{
    assert(addr == fb && len == fix.smem_len);
    return 0;
}
int __wrap_msync(void *addr, size_t len, int flags)
{
    assert(addr == fb && len == fix.smem_len && flags == MS_SYNC);
    if (fail_sync) { errno = EIO; return -1; }
    return 0;
}
int __wrap_ioctl(int fd, unsigned long cmd, ...)
{
    assert(fd == 123);
    va_list args;
    va_start(args, cmd);
    void *arg = va_arg(args, void *);
    va_end(args);
    switch (cmd) {
    case FBIOGET_VSCREENINFO: memcpy(arg, &var, sizeof var); return 0;
    case FBIOGET_FSCREENINFO: memcpy(arg, &fix, sizeof fix); return 0;
    case OA_FB_REGION_RD: memcpy(arg, region, sizeof region); return 0;
    case OA_FB_REGION_WR:
        if (fail_region) { errno = EIO; return -1; }
        memcpy(region, arg, sizeof region);
        writes++;
        return 0;
    case OA_FB_MAU_RD: *(uint32_t *)arg = mau; return 0;
    case FBIOPAN_DISPLAY:
        if (fail_pan) { errno = EIO; return -1; }
        var.yoffset = ((struct fb_var_screeninfo *)arg)->yoffset;
        pans++;
        return 0;
    default: assert(!"unexpected ioctl"); return -1;
    }
}

int main(void)
{
    struct oa_fb_snapshot snap;
    init_fake();
    assert(!oa_fb_snapshot_save(123, &snap));
    assert(snap.fix.smem_len == 128 && snap.var.yres == 2 &&
        snap.mau == 1 && snap.region[0] == 0x100 && snap.region[7] == 0x107);
    assert(snap.pixels && snap.pixels[0] == 0xaa && snap.pixels[127] == 0xaa);
    assert(!writes && !pans);

    memset(fb, 0, sizeof fb);
    memset(region, 0, sizeof region);
    var.yoffset = 2;
    assert(!oa_fb_snapshot_restore(123, &snap));
    assert(fb[0] == 0xaa && fb[127] == 0xaa);
    assert(region[0] == 0x100 && region[7] == 0x107);
    assert(var.yoffset == 0 && writes == 1 && pans == 1);
    oa_fb_snapshot_free(&snap);
    assert(!snap.pixels);

    init_fake();
    assert(!oa_fb_snapshot_save(123, &snap));
    mau = 0;
    memset(fb, 0, sizeof fb);
    assert(oa_fb_snapshot_restore(123, &snap) == -ESTALE);
    assert(fb[0] == 0 && !writes && !pans);
    oa_fb_snapshot_free(&snap);

    init_fake();
    assert(!oa_fb_snapshot_save(123, &snap));
    memset(fb, 0, sizeof fb);
    fail_region = 1;
    assert(oa_fb_snapshot_restore(123, &snap) < 0);
    assert(fb[0] == 0xaa && pans == 1);
    fail_region = 0;
    oa_fb_snapshot_free(&snap);

    init_fake();
    assert(!oa_fb_snapshot_save(123, &snap));
    memset(fb, 0, sizeof fb);
    fail_pan = 1;
    assert(oa_fb_snapshot_restore(123, &snap) < 0);
    assert(fb[0] == 0xaa && region[0] == 0x100);
    fail_pan = 0;
    oa_fb_snapshot_free(&snap);

    init_fake();
    fix.smem_len = 0;
    assert(oa_fb_snapshot_save(123, &snap) == -EINVAL);
    assert(oa_fb_snapshot_restore(123, NULL) == -EINVAL);
    puts("  ok   fb snapshot: pixels/var/region/MAU save; restore verifies MAU and failures");
    return 0;
}
