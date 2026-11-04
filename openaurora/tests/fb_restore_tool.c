/* End-to-end fb-restore save/restore file test with a fake framebuffer. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define main fb_restore_main
#include "../tools/display/fb-restore.c"
#undef main

static struct fb_var_screeninfo var;
static struct fb_fix_screeninfo fix;
static uint32_t region[8], mau;
static unsigned char fb[4096];
static unsigned opens, writes, pans;

static void init_fake(void)
{
    memset(&var, 0, sizeof var);
    memset(&fix, 0, sizeof fix);
    memset(fb, 0xaa, sizeof fb);
    for (int i = 0; i < 8; i++) region[i] = 0x200u + (uint32_t)i;
    var.xres = 8;
    var.yres = 2;
    var.yres_virtual = 4;
    var.bits_per_pixel = 32;
    var.yoffset = 2;
    fix.line_length = 32;
    fix.smem_len = 128;
    mau = 1;
    opens = writes = pans = 0;
}

int __wrap_open(const char *path, int flags, ...)
{
    assert(!strcmp(path, "/dev/fb1"));
    assert(flags == O_RDONLY || flags == O_RDWR);
    opens++;
    return 123;
}
int __wrap_close(int fd) { assert(fd == 123); return 0; }
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
    case OA_FB_REGION_WR: memcpy(region, arg, sizeof region); writes++; return 0;
    case OA_FB_MAU_RD: *(uint32_t *)arg = mau; return 0;
    case FBIOPAN_DISPLAY:
        var.yoffset = ((struct fb_var_screeninfo *)arg)->yoffset;
        pans++;
        return 0;
    default: assert(!"unexpected ioctl"); return -1;
    }
}

extern int __real_close(int fd);

int main(void)
{
    char path[] = "/tmp/oa-fb-restore-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    assert(!__real_close(fd));
    init_fake();
    char *save_args[] = { "fb-restore", "save", "/dev/fb1", path };
    assert(!fb_restore_main(4, save_args));
    assert(opens == 1);

    memset(fb, 0, sizeof fb);
    memset(region, 0, sizeof region);
    var.yoffset = 0;
    char *restore_args[] = { "fb-restore", "restore", "/dev/fb1", path };
    assert(!fb_restore_main(4, restore_args));
    assert(fb[0] == 0xaa && fb[127] == 0xaa);
    assert(region[0] == 0x200 && region[7] == 0x207);
    assert(var.yoffset == 2 && writes == 1 && pans == 1);
    assert(!unlink(path));
    puts("  ok   fb-restore tool saves/restores pixels, region and pan through the v2 file");
    return 0;
}
