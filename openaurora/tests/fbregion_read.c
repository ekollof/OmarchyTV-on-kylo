/* Only GET requests are accepted by this fake device. No real device is opened. */
#include <assert.h>
#include <stdarg.h>
#define main fbregion_main
#include "../tools/mcu/fbregion.c"
#undef main

static unsigned reads;
int __wrap_open(const char *path, int flags, ...)
{ assert(!strcmp(path, "/dev/fb1") && flags == O_RDONLY); return 123; }
int __wrap_close(int fd) { assert(fd == 123); return 0; }
int __wrap_ioctl(int fd, unsigned long cmd, ...)
{
    assert(fd == 123);
    va_list args;
    va_start(args, cmd);
    void *arg = va_arg(args, void *);
    va_end(args);
    reads++;
    switch (cmd) {
    case FBIOGET_VSCREENINFO: memset(arg, 0, sizeof(struct fb_var_screeninfo)); return 0;
    case FBIOGET_FSCREENINFO: memset(arg, 0, sizeof(struct fb_fix_screeninfo)); return 0;
    case OSD_CURRENT_MAU: *(uint32_t *)arg = 1; return 0;
    case OSD_REG_RD: memset(arg, 0, 32); return 0;
    default: assert(!"unexpected or state-changing ioctl"); return -1;
    }
}
int main(void)
{
    char *args[] = { "fbregion", NULL };
    assert(fbregion_main(1, args) == 0 && reads == 4);
    puts("  ok   fbregion opens read-only and submits only screeninfo/MAU/region GET requests");
    return 0;
}
