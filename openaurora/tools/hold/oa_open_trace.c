/*
 * oa_open_trace.so — log what the vendor UI opens during its first seconds.
 *
 * Measured on the set: /dev/fb0, /dev/fb1 and /dev/dri/card0 do not exist until ~2 s after vendor
 * aurora starts, so aurora itself is what brings the display driver up. This tracer records every
 * open()/openat() of a device, module or driver path so that bring-up can be reproduced by
 * openaurora instead of guessed (see artifacts/reports/PANEL-TAKEOVER-2026-10-31.md).
 *
 * Loaded with LD_PRELOAD, gated on /conf/oa_trace_enable (see the boot hook). Appends to
 * /conf/oa_open.log.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int (*real_open)(const char *, int, ...);
static int (*real_openat)(int, const char *, int, ...);
static int logfd = -1;
static int count;

static void trace(const char *how, const char *path, int rc)
{
    if (!path)
        return;
    /* only the interesting ones: devices, modules, firmware, driver nodes */
    if (!(strncmp(path, "/dev/", 5) == 0 || strncmp(path, "/lib/modules", 12) == 0 ||
          strncmp(path, "/sys/", 5) == 0 || strncmp(path, "/proc/", 6) == 0))
        return;
    if (count++ > 400)
        return;
    if (logfd < 0)
        logfd = open("/conf/oa_open.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (logfd >= 0) {
        char line[256];
        int n = snprintf(line, sizeof line, "%s %s -> %d\n", how, path, rc);
        if (n > 0) {
            write(logfd, line, (size_t)n);
            fsync(logfd);
        }
    }
}

int open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    va_list ap;
    va_start(ap, flags);
    if (flags & O_CREAT)
        mode = (mode_t)va_arg(ap, int);
    va_end(ap);

    if (!real_open)
        real_open = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open");
    int rc = real_open(path, flags, mode);
    trace("open  ", path, rc);
    return rc;
}

int open64(const char *path, int flags, ...) __attribute__((alias("open")));

int openat(int dirfd, const char *path, int flags, ...)
{
    mode_t mode = 0;
    va_list ap;
    va_start(ap, flags);
    if (flags & O_CREAT)
        mode = (mode_t)va_arg(ap, int);
    va_end(ap);

    if (!real_openat)
        real_openat = (int (*)(int, const char *, int, ...))dlsym(RTLD_NEXT, "openat");
    int rc = real_openat(dirfd, path, flags, mode);
    trace("openat", path, rc);
    return rc;
}

int openat64(int dirfd, const char *path, int flags, ...) __attribute__((alias("openat")));
