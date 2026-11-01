/*
 * oa_trace.so — log what the vendor UI actually tells the display stack.
 *
 * Loaded into aurora with LD_PRELOAD (only when /conf/oa_trace_enable exists, see the hook).
 *
 * The interesting question is why the OSD layer stops being visible when the vendor process is
 * frozen: the fbdev ioctl set only sizes the layer (NVTGA_OSD_SetRegion in libdirectfb_tdga), while
 * plane z-order/visibility is configured through DRM (Hal_Video_Set_DrmPlaneInfo / drmModeSetPlane
 * in libNDAL3). So both are logged, with payloads, for every fd that is a framebuffer or a DRM node.
 *
 * Payload dumping uses the _IOC size field and only for write-direction requests, so it cannot read
 * past a read-only argument. Each distinct (fd kind, request) is logged a bounded number of times.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define IOC_WRITE 1

static int (*real_ioctl)(int, unsigned long, void *);
static int logfd = -1;

struct hit { int fd_kind; unsigned long req; int count; };
static struct hit hits[64];
static int nhits;

/* 1 = fb, 2 = dri, 0 = other */
static int fd_kind(int fd)
{
    char p[64], l[96];
    snprintf(p, sizeof p, "/proc/self/fd/%d", fd);
    ssize_t n = readlink(p, l, sizeof l - 1);
    if (n <= 0)
        return 0;
    l[n] = '\0';
    if (strstr(l, "/dri/"))
        return 2;
    if (strstr(l, "/fb"))
        return 1;
    return 0;
}

int ioctl(int fd, unsigned long req, ...)
{
    va_list ap;
    void *arg;

    va_start(ap, req);
    arg = va_arg(ap, void *);
    va_end(ap);

    if (!real_ioctl)
        real_ioctl = (int (*)(int, unsigned long, void *))dlsym(RTLD_NEXT, "ioctl");
    if (!real_ioctl)
        return -1;

    int kind = (arg != NULL) ? fd_kind(fd) : 0;
    if (!kind)
        return real_ioctl(fd, req, arg);

    struct hit *h = NULL;
    for (int i = 0; i < nhits; i++)
        if (hits[i].fd_kind == kind && hits[i].req == req) { h = &hits[i]; break; }
    if (!h && nhits < 64) {
        h = &hits[nhits++];
        h->fd_kind = kind;
        h->req = req;
        h->count = 0;
    }
    if (h)
        h->count++;

    int rc = real_ioctl(fd, req, arg);

    /* log the first 3 occurrences of each request, with payload when it is a write buffer */
    if (h && h->count <= 3) {
        if (logfd < 0)
            logfd = open("/conf/oa_trace.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (logfd >= 0) {
            char line[512];
            int n = snprintf(line, sizeof line, "%s fd=%d req=%#lx rc=%d: ",
                             kind == 2 ? "drm" : "fb ", fd, req, rc);
            unsigned dir = (unsigned)((req >> 30) & 3);
            unsigned size = (unsigned)((req >> 16) & 0x3fff);
            /* dump payloads for the small fbdev structs and for the Nuvoton vendor DRM ioctl
             * (0xc1a46451, 420 bytes) that carries the plane/VPP attributes */
            if ((dir & IOC_WRITE) && size > 0 && (size <= 64 || req == 0xc1a46451)) {
                if (size > 420)
                    size = 420;
                const unsigned char *b = arg;
                for (unsigned i = 0; i < size && n < (int)sizeof line - 12; i++)
                    n += snprintf(line + n, sizeof line - n, "%02x%s", b[i],
                                  (i % 4 == 3) ? " " : "");
            } else {
                n += snprintf(line + n, sizeof line - n, "(r)");
            }
            n += snprintf(line + n, sizeof line - n, "\n");
            write(logfd, line, n);
            fsync(logfd);
        }
    }
    return rc;
}
