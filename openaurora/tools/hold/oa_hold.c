/*
 * oa_hold.so — take the panel away from vendor aurora without stopping it.
 *
 * Loaded into aurora with LD_PRELOAD (opt-in: only when /conf/oa_hold_preload exists at boot),
 * because we control the script that execs the vendor UI (/conf/remotelf/aurora.elf).
 *
 * While /conf/oa_hold exists:
 *
 *   1. mmap() of /dev/fb1 (the upper OSD layer) is redirected to anonymous memory, so aurora still
 *      draws its UI, at full speed, into a buffer that no hardware scans out;
 *   2. FBIOPAN_DISPLAY on that fd is answered with success and not forwarded.
 *
 * aurora therefore keeps its event loop, its timers, DIAL, its MCU-watchdog kicking and appmgr's
 * liveness check — it simply stops touching the glass, so whatever openaurora painted stays.
 *
 * Remove /conf/oa_hold and the next mmap()/flip goes to the real layer again; aurora repaints
 * itself and owns the screen. No reboot in either direction.
 *
 * Measured context: fb1 is the upper OSD layer; aurora updates it by region (dirty rectangles) at
 * ~1 Hz on the no-signal banner, and it writes pixels through its own mapping, so two whole-frame
 * writers can never share it. Flips alone are not enough to stop it — the mapping is.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define FBIOPAN_DISPLAY 0x4606
#define HOLD_FILE "/conf/oa_hold"
#define SHADOW_FB "/fb1"          /* the upper OSD layer: the portal UI */

static void *(*real_mmap)(void *, size_t, int, int, int, off_t);
static int (*real_ioctl)(int, unsigned long, void *);
static int hold_state = -1;
static unsigned ticks;
static unsigned logmask;

static void logline(const char *what, int fd)
{
    unsigned bit = fd < 0 ? 0 : (1u << (fd & 15));
    if (logmask & bit)
        return;
    logmask |= bit;
    int f = open("/conf/oa_shim.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (f >= 0) {
        dprintf(f, "pid %d fd %d: %s\n", (int)getpid(), fd, what);
        close(f);
    }
}

static int holding(void)
{
    if (hold_state < 0 || (++ticks & 0xf) == 0)
        hold_state = access(HOLD_FILE, F_OK) == 0;
    return hold_state == 1;
}

/* is this fd one of the OSD layers we shadow? */
static int is_osd_layer(int fd)
{
    char path[64], link[96];
    snprintf(path, sizeof path, "/proc/self/fd/%d", fd);
    ssize_t n = readlink(path, link, sizeof link - 1);
    if (n <= 0)
        return 0;
    link[n] = '\0';
    return strstr(link, SHADOW_FB) != NULL;
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
    if (!real_mmap)
        real_mmap = (void *(*)(void *, size_t, int, int, int, off_t))dlsym(RTLD_NEXT, "mmap");

    if (fd >= 0 && holding() && is_osd_layer(fd)) {
        logline("shadowing OSD layer", fd);
        void *p = real_mmap(addr, len, prot, flags | MAP_ANONYMOUS, -1, 0);
        if (p != MAP_FAILED)
            return p;   /* anonymous: nothing we draw here can reach the panel */
    }
    return real_mmap(addr, len, prot, flags, fd, off);
}

void *mmap64(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
    return mmap(addr, len, prot, flags, fd, off);
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

    if (req == FBIOPAN_DISPLAY && holding() && is_osd_layer(fd)) {
        logline("swallowing flip", fd);
        return 0;
    }
    return real_ioctl(fd, req, arg);
}
