/* Host-only fake /dev/fb1 for the real display_fb module.
 *
 * LD_PRELOAD this DSO into openaurora. It intercepts the fb1 path and the
 * recovered fbdev/region/MAU ioctls. It writes initial/current state snapshots
 * to OA_FAKE_FB_STATE (on close) and OA_FAKE_FB_CHANGED (on region/pan writes)
 * so a socket test can prove that display_fb's snapshot restore actually
 * restored the fake layer. No real device or TV is touched. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "openaurora/fb_snapshot.h"

#define FAKE_FD 123
#define WIDTH 1920u
#define HEIGHT 1080u
#define VHEIGHT 2160u
static struct fb_var_screeninfo var;
static struct fb_fix_screeninfo fix;
static uint32_t region[8], mau = 1;
static uint32_t initial_region[8];
static uint32_t initial_yoffset, initial_mau = 1;
static unsigned char *pixels;
static uint64_t initial_hash;

static int (*real_open)(const char *, int, ...);
static int (*real_close)(int);
static int (*real_ioctl)(int, unsigned long, ...);
static int (*real_access)(const char *, int);
static void *(*real_mmap)(void *, size_t, int, int, int, off_t);
static int (*real_munmap)(void *, size_t);
static int (*real_msync)(void *, size_t, int);

static void load_real(void)
{
    void *p;
#define LOAD(field, name, type) do { \
    p = dlsym(RTLD_NEXT, name); if (p) memcpy(&field, &p, sizeof field); \
    if (!field) _exit(120); } while (0)
    LOAD(real_open, "open", void *);
    LOAD(real_close, "close", void *);
    LOAD(real_ioctl, "ioctl", void *);
    LOAD(real_access, "access", void *);
    LOAD(real_mmap, "mmap", void *);
    LOAD(real_munmap, "munmap", void *);
    LOAD(real_msync, "msync", void *);
#undef LOAD
}

static uint64_t fnv(const unsigned char *p, size_t n)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void write_state(const char *path, int include_pixels)
{
    if (!path || !*path) return;
    char text[1024];
    int n = snprintf(text, sizeof text,
        "initial_yoffset=%u current_yoffset=%u initial_mau=%u current_mau=%u "
        "initial_region=", initial_yoffset, var.yoffset, initial_mau, mau);
    for (int i = 0; i < 8; i++) n += snprintf(text + n, sizeof text - (size_t)n,
        "%s%u", i ? "," : "", initial_region[i]);
    n += snprintf(text + n, sizeof text - (size_t)n, " current_region=");
    for (int i = 0; i < 8; i++) n += snprintf(text + n, sizeof text - (size_t)n,
        "%s%u", i ? "," : "", region[i]);
    n += snprintf(text + n, sizeof text - (size_t)n, " initial_pixels=%016llx",
        (unsigned long long)initial_hash);
    if (include_pixels)
        n += snprintf(text + n, sizeof text - (size_t)n, " current_pixels=%016llx",
            (unsigned long long)fnv(pixels, fix.smem_len));
    n += snprintf(text + n, sizeof text - (size_t)n, "\n");
    int fd = real_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    (void)write(fd, text, (size_t)n);
    real_close(fd);
}

__attribute__((constructor)) static void fake_init(void)
{
    load_real();
    memset(&var, 0, sizeof var);
    memset(&fix, 0, sizeof fix);
    var.xres = var.xres_virtual = WIDTH;
    var.yres = HEIGHT;
    var.yres_virtual = VHEIGHT;
    var.bits_per_pixel = 32;
    var.yoffset = 0;
    fix.line_length = WIDTH * 4;
    fix.smem_len = fix.line_length * VHEIGHT;
    memset(fix.id, 0, sizeof fix.id);
    memcpy(fix.id, "fakefb", 6);
    for (int i = 0; i < 8; i++) initial_region[i] = region[i] = 0x100u + (uint32_t)i;
    initial_yoffset = var.yoffset;
    initial_mau = mau;
    pixels = malloc(fix.smem_len);
    if (!pixels) _exit(121);
    memset(pixels, 0xaa, fix.smem_len);
    initial_hash = fnv(pixels, fix.smem_len);
}

__attribute__((destructor)) static void fake_fini(void)
{
    write_state(getenv("OA_FAKE_FB_STATE"), 1);
}

int open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap; va_start(ap, flags); mode = (mode_t)va_arg(ap, int); va_end(ap);
    }
    if (!strcmp(path, "/dev/fb1")) return FAKE_FD;
    if (!real_open) load_real();
    return real_open(path, flags, mode);
}

int close(int fd)
{
    if (fd == FAKE_FD) { write_state(getenv("OA_FAKE_FB_STATE"), 1); return 0; }
    return real_close(fd);
}

int access(const char *path, int mode)
{
    if (!strcmp(path, "/dev/fb1")) return 0;
    return real_access(path, mode);
}

int ioctl(int fd, unsigned long cmd, ...)
{
    if (fd != FAKE_FD) {
        va_list ap; va_start(ap, cmd); void *a = va_arg(ap, void *); va_end(ap);
        return real_ioctl(fd, cmd, a);
    }
    va_list ap; va_start(ap, cmd); void *arg = va_arg(ap, void *); va_end(ap);
    switch (cmd) {
    case FBIOGET_VSCREENINFO: memcpy(arg, &var, sizeof var); return 0;
    case FBIOGET_FSCREENINFO: memcpy(arg, &fix, sizeof fix); return 0;
    case FBIOPAN_DISPLAY: {
        struct fb_var_screeninfo *v = arg;
        var.yoffset = v->yoffset;
        write_state(getenv("OA_FAKE_FB_CHANGED"), 1);
        return 0;
    }
    case OA_FB_REGION_RD: memcpy(arg, region, sizeof region); return 0;
    case OA_FB_REGION_WR:
        memcpy(region, arg, sizeof region);
        write_state(getenv("OA_FAKE_FB_CHANGED"), 0);
        return 0;
    case OA_FB_MAU_RD: *(uint32_t *)arg = mau; return 0;
    default: errno = EINVAL; return -1;
    }
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
    if (fd == FAKE_FD) {
        if (off != 0 || len > fix.smem_len) { errno = EINVAL; return MAP_FAILED; }
        return pixels;
    }
    return real_mmap(addr, len, prot, flags, fd, off);
}

int munmap(void *addr, size_t len)
{
    if (addr == pixels) return 0;
    return real_munmap(addr, len);
}

int msync(void *addr, size_t len, int flags)
{
    const unsigned char *p = addr;
    if (p >= pixels && len <= fix.smem_len &&
        (size_t)(p - pixels) <= fix.smem_len - len)
        return 0;
    return real_msync(addr, len, flags);
}
