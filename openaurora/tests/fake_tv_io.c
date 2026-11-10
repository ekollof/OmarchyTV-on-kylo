/* Permissive qemu device-boundary model for the TV HAL.
 *
 * LD_PRELOAD this ARM DSO into qemu-arm-static. It intercepts the TV device
 * nodes used by libNDAL3 (GPIO/PWM/IIC/VPLT/dvif/SPI/CEC/led/sys) and returns
 * success while logging each open/ioctl. It is a tracing/mock boundary, not a
 * kernel or hardware emulator: it does not prove register effects or scanout.
 *
 * Recovered HAL calls used by the panel slice:
 *   Hal_Pwm_Open            open64(...), ioctl 0x403c4c4c
 *   Hal_Pwm_Set_ByChannel   ioctl 0xc03c4c53
 *   Hal_Gpio_Write          ioctl 0x670a, 0x670e, 0x670c
 * See artifacts/reports/... and the Ghidra NDAL3 project for the source.
 *
 *   OA_TV_IO_LOG=/path      append open/ioctl trace lines
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FAKE_FD 200

static int (*real_open)(const char *, int, ...);
static int (*real_open64)(const char *, int, ...);
static int (*real_close)(int);
static int (*real_ioctl)(int, unsigned long, ...);
static int (*real_access)(const char *, int);

static void load_real(void)
{
    void *p;
#define LOAD(field, name) do { \
    p = dlsym(RTLD_NEXT, name); if (p) memcpy(&field, &p, sizeof field); \
    if (!field) _exit(120); } while (0)
    LOAD(real_open, "open");
    LOAD(real_open64, "open64");
    LOAD(real_close, "close");
    LOAD(real_ioctl, "ioctl");
    LOAD(real_access, "access");
#undef LOAD
}

static void io_log(const char *what, const char *path, unsigned long cmd)
{
    const char *out = getenv("OA_TV_IO_LOG");
    if (!out || !*out) return;
    FILE *f = fopen(out, "a");
    if (!f) return;
    if (path) fprintf(f, "%s %s\n", what, path);
    else fprintf(f, "%s cmd=%#lx\n", what, cmd);
    fclose(f);
}

static void io_log_words(const char *what, unsigned long cmd, const uint32_t *p,
                         int n)
{
    const char *out = getenv("OA_TV_IO_LOG");
    if (!out || !*out || !p) return;
    FILE *f = fopen(out, "a");
    if (!f) return;
    fprintf(f, "%s cmd=%#lx", what, cmd);
    for (int i = 0; i < n; i++) fprintf(f, " w%d=%u", i, p[i]);
    fputc('\n', f);
    fclose(f);
}

static int is_tv_path(const char *p)
{
    if (!p) return 0;
    return !strncmp(p, "/dev/nt", 7) ||
           !strncmp(p, "/dev/nvt/", 9) ||
           !strncmp(p, "/dev/i2c-", 9) ||
           !strncmp(p, "/dev/video", 10) ||
           !strncmp(p, "/dev/dvb/", 9) ||
           !strncmp(p, "/dev/ndp_hwmem", 14);
}

static int fake_open(const char *path)
{
    io_log("open", path, 0);
    return FAKE_FD;
}

__attribute__((constructor)) static void tvio_init(void) { load_real(); }

int open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap; va_start(ap, flags); mode = (mode_t)va_arg(ap, int); va_end(ap);
    }
    if (is_tv_path(path)) return fake_open(path);
    if (!real_open) load_real();
    return real_open(path, flags, mode);
}

int open64(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap; va_start(ap, flags); mode = (mode_t)va_arg(ap, int); va_end(ap);
    }
    if (is_tv_path(path)) return fake_open(path);
    if (!real_open64) load_real();
    return real_open64(path, flags, mode);
}

int close(int fd)
{
    if (fd == FAKE_FD) { io_log("close", NULL, 0); return 0; }
    return real_close(fd);
}

int access(const char *path, int mode)
{
    if (is_tv_path(path)) return 0;
    if (!real_access) load_real();
    return real_access(path, mode);
}

int ioctl(int fd, unsigned long cmd, ...)
{
    va_list ap;
    va_start(ap, cmd);
    void *arg = va_arg(ap, void *);
    va_end(ap);
    if (fd != FAKE_FD) {
        if (!real_ioctl) load_real();
        return real_ioctl(fd, cmd, arg);
    }
    if ((cmd == 0x670au || cmd == 0x670eu || cmd == 0x670cu) && arg) {
        const uint32_t *p = arg;
        /* Hal_Gpio_Write: word0=pin, byte4=value (0x670a/0x670e preset 0). */
        io_log_words("gpio", cmd, p, 2);
        if (getenv("OA_TV_IO_LOG")) {
            FILE *f = fopen(getenv("OA_TV_IO_LOG"), "a");
            if (f) { fprintf(f, "gpio_value cmd=%#lx pin=%u value=%u\n",
                             cmd, p[0], ((const unsigned char *)arg)[4]); fclose(f); }
        }
    } else if (cmd == 0x403c4c4cu && arg) {
        io_log_words("pwm_open", cmd, arg, 6);
    } else if (cmd == 0xc03c4c53u && arg) {
        io_log_words("pwm_set", cmd, arg, 6);
    } else {
        io_log("ioctl", NULL, cmd);
    }
    (void)arg;
    return 0;
}
