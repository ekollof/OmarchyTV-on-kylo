/* Trace-only LD_PRELOAD for the TV hotplug test.
 *
 * Logs the NDAL STBC V4L2 ext-control write (0xc0185648) channel/value pairs
 * and forwards every ioctl/syscall unchanged. It does not fake returns, open
 * devices, or change state. This is a diagnostic for the real hardware pulse.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STBC_V4L2_CMD 0xc0185648UL
#define VIN_COLOR_CMD 0x40085618UL
#define ARM_SYS_ioctl 54

static int (*real_ioctl)(int, unsigned long, ...);
static long (*real_syscall)(long, ...);

static void log_controls(unsigned long cmd, const void *arg)
{
    const char *path = getenv("OA_TRACE_LOG");
    if (!path || !*path || !arg)
        return;
    const unsigned char *c = arg;
    uint32_t count;
    memcpy(&count, c + 4, sizeof count);
    uintptr_t cp;
    memcpy(&cp, c + 20, sizeof cp);
    if (!cp || count > 16)
        return;
    const unsigned char *base = (const unsigned char *)(uintptr_t)cp;
    unsigned channel = 0, value = 0;
    int have_channel = 0, have_value = 0;
    /* struct v4l2_ext_control is 20 bytes here: id, size, reserved2 and an
     * 8-byte union (pointer at +12 on ARM32). */
    for (uint32_t i = 0; i < count; i++) {
        const unsigned char *e = base + (size_t)i * 20u;
        uint32_t size, ptr;
        memcpy(&size, e + 4, 4);
        memcpy(&ptr, e + 12, 4);
        if (!ptr || !size)
            continue;
        unsigned v = *(const unsigned char *)(uintptr_t)ptr;
        if (i == 1) { channel = v; have_channel = 1; }
        else if (i == 2) { value = v; have_value = 1; }
    }
    FILE *f = fopen(path, "a");
    if (!f)
        return;
    fprintf(f, "stbc cmd=%#lx count=%u channel=%u value=%u\n",
            cmd, count, have_channel ? channel : 0, have_value ? value : 0);
    fclose(f);
}

static void log_color(const void *arg)
{
    const char *path = getenv("OA_TRACE_LOG");
    if (!path || !*path || !arg)
        return;
    const uint32_t *w = arg;
    FILE *f = fopen(path, "a");
    if (!f)
        return;
    fprintf(f, "vin_color cmd=%#lx v0=%08x v1=%08x\n",
            (unsigned long)VIN_COLOR_CMD, w[0], w[1]);
    fclose(f);
}

int ioctl(int fd, unsigned long cmd, ...)
{
    va_list ap;
    va_start(ap, cmd);
    void *arg = va_arg(ap, void *);
    va_end(ap);
    if (cmd == STBC_V4L2_CMD)
        log_controls(cmd, arg);
    else if (cmd == VIN_COLOR_CMD)
        log_color(arg);
    if (!real_ioctl) {
        void *p = dlsym(RTLD_NEXT, "ioctl");
        memcpy(&real_ioctl, &p, sizeof real_ioctl);
    }
    return real_ioctl ? real_ioctl(fd, cmd, arg) : -1;
}

long syscall(long number, ...)
{
    va_list ap;
    va_start(ap, number);
    if (number == ARM_SYS_ioctl) {
        int fd = va_arg(ap, int);
        unsigned long cmd = va_arg(ap, unsigned long);
        void *arg = va_arg(ap, void *);
        va_end(ap);
        if (cmd == STBC_V4L2_CMD)
            log_controls(cmd, arg);
        else if (cmd == VIN_COLOR_CMD)
            log_color(arg);
        if (!real_syscall) {
            void *p = dlsym(RTLD_NEXT, "syscall");
            memcpy(&real_syscall, &p, sizeof real_syscall);
        }
        return real_syscall(number, fd, cmd, arg);
    }
    long a1 = va_arg(ap, long);
    long a2 = va_arg(ap, long);
    long a3 = va_arg(ap, long);
    long a4 = va_arg(ap, long);
    long a5 = va_arg(ap, long);
    long a6 = va_arg(ap, long);
    va_end(ap);
    if (!real_syscall) {
        void *p = dlsym(RTLD_NEXT, "syscall");
        memcpy(&real_syscall, &p, sizeof real_syscall);
    }
    return real_syscall(number, a1, a2, a3, a4, a5, a6);
}
