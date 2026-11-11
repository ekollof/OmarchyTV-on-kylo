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

static void io_log_text(const char *fmt, ...)
{
    const char *out = getenv("OA_TV_IO_LOG");
    if (!out || !*out) return;
    FILE *f = fopen(out, "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
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

#define GPIO_DESC_IOCTL 0xc088671fUL
#define STBC_V4L2_CMD 0xc0185648UL

struct fake_ext_control {
    uint32_t id, size, reserved2;
    uintptr_t ptr;
};
struct fake_ext_controls {
    uint32_t which, count, error_idx, request_fd, reserved;
    uintptr_t controls;
};

/* Answer DEV_GPIO_GetDescription with the live TV mapping: HDMI1..4 ->
 * pins 274..277, output, normal low, active low. MHL_CDSENSE and
 * HDMI_HOTPLUG_5 are absent on this set (the kernel returns -ERFKILL/524). */
static int fill_gpio_desc(void *arg)
{
    unsigned char *b = arg;
    uintptr_t np;
    memcpy(&np, b + 0x80, sizeof np);
    const char *name = (const char *)(uintptr_t)np;
    if (!name) { errno = EINVAL; return -1; }
    uint32_t pin;
    if (!strcmp(name, "HDMI_HOTPLUG_1")) pin = 274;
    else if (!strcmp(name, "HDMI_HOTPLUG_2")) pin = 275;
    else if (!strcmp(name, "HDMI_HOTPLUG_3")) pin = 276;
    else if (!strcmp(name, "HDMI_HOTPLUG_4")) pin = 277;
    else {
        io_log_text("gpio_desc_missing name=%s errno=524", name);
        errno = 524;
        return -1;
    }
    memset(b, 0, 0x88);
    memcpy(b, &pin, sizeof pin);
    memcpy(b + 0x08, "output", 6);
    memcpy(b + 0x20, "low", 3);
    memcpy(b + 0x38, "low", 3);
    memcpy(b + 0x50, "default", 7);
    memcpy(b + 0x68, "no_int", 6);
    io_log_text("gpio_desc name=%s pin=%u active=low", name, pin);
    return 0;
}

static void log_ext_controls(unsigned long cmd, const void *arg)
{
    const struct fake_ext_controls *c = arg;
    io_log_text("ext cmd=%#lx which=%#x count=%u", cmd, c->which, c->count);
    if (!c->controls || c->count > 16)
        return;
    /* struct v4l2_ext_control is 20 bytes here: id, size, reserved2 and an
     * 8-byte union (pointer at +12 on ARM32). */
    const unsigned char *base = (const unsigned char *)(uintptr_t)c->controls;
    for (uint32_t i = 0; i < c->count; i++) {
        const unsigned char *e = base + (size_t)i * 20u;
        uint32_t id, size, ptr;
        memcpy(&id, e + 0, 4);
        memcpy(&size, e + 4, 4);
        memcpy(&ptr, e + 12, 4);
        unsigned v = 0;
        if (ptr && size)
            v = *(const unsigned char *)(uintptr_t)ptr;
        io_log_text("ext_ctl cmd=%#lx idx=%u id=%#x size=%u byte0=%u",
                    cmd, i, id, size, v);
    }
}

static int fake_ioctl(unsigned long cmd, void *arg)
{
    if (cmd == GPIO_DESC_IOCTL && arg)
        return fill_gpio_desc(arg);
    if (cmd == STBC_V4L2_CMD && arg) {
        log_ext_controls(cmd, arg);
        return 0;
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
    return 0;
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
    return fake_ioctl(cmd, arg);
}

/* libv4l2 uses raw syscall() for v4l2_open/v4l2_ioctl, bypassing the libc
 * open/ioctl wrappers. ARM EABI numbers: open=5, close=6, ioctl=54,
 * openat=322. Without this, NDAL_VideoExt_OpenDevice gets fd -1 and the STBC
 * path never reaches the mock. */
#define OA_SYS_open 5
#define OA_SYS_close 6
#define OA_SYS_ioctl 54
#define OA_SYS_openat 322

static long (*real_syscall)(long, ...);

static long call_real_syscall(long number, long a1, long a2, long a3,
                              long a4, long a5, long a6)
{
    if (!real_syscall) {
        void *p = dlsym(RTLD_NEXT, "syscall");
        if (!p) _exit(122);
        memcpy(&real_syscall, &p, sizeof real_syscall);
    }
    return real_syscall(number, a1, a2, a3, a4, a5, a6);
}

long syscall(long number, ...)
{
    va_list ap;
    va_start(ap, number);
    if (number == OA_SYS_openat) {
        int dirfd = va_arg(ap, int);
        const char *path = va_arg(ap, const char *);
        int flags = va_arg(ap, int);
        unsigned mode = va_arg(ap, unsigned);
        va_end(ap);
        if (is_tv_path(path)) return fake_open(path);
        return call_real_syscall(number, dirfd, (long)path, flags, mode, 0, 0);
    }
    if (number == OA_SYS_open) {
        const char *path = va_arg(ap, const char *);
        int flags = va_arg(ap, int);
        unsigned mode = va_arg(ap, unsigned);
        va_end(ap);
        if (is_tv_path(path)) return fake_open(path);
        return call_real_syscall(number, (long)path, flags, mode, 0, 0, 0);
    }
    if (number == OA_SYS_ioctl) {
        int fd = va_arg(ap, int);
        unsigned long cmd = va_arg(ap, unsigned long);
        void *arg = va_arg(ap, void *);
        va_end(ap);
        if (fd == FAKE_FD) return fake_ioctl(cmd, arg);
        return call_real_syscall(number, fd, (long)cmd, (long)arg, 0, 0, 0);
    }
    if (number == OA_SYS_close) {
        int fd = va_arg(ap, int);
        va_end(ap);
        if (fd == FAKE_FD) { io_log("close", NULL, 0); return 0; }
        return call_real_syscall(number, fd, 0, 0, 0, 0, 0);
    }
    long a1 = va_arg(ap, long);
    long a2 = va_arg(ap, long);
    long a3 = va_arg(ap, long);
    long a4 = va_arg(ap, long);
    long a5 = va_arg(ap, long);
    long a6 = va_arg(ap, long);
    va_end(ap);
    return call_real_syscall(number, a1, a2, a3, a4, a5, a6);
}
