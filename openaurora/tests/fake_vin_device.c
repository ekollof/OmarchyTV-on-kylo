/* Host/ARM fixture: fake the VIN device node used by libNDAL3's Hal_Vin_*
 * wrappers. The shipped libNDAL3.so code runs unchanged; this shim supplies
 * open64/open, close and the two decoded ioctls.
 *
 * Hal_Vin_Open(index) builds a /dev/videoN (or ntkdvif fallback) path and
 * calls open64(path, O_RDWR|O_NONBLOCK), then fcntl(F_SETFD, FD_CLOEXEC).
 * Hal_Vin_SelectInput(fd,id) is ioctl(fd, 0xc0045627, &id);
 * Hal_Vin_SetStreamStart(fd) is ioctl(fd, 0x40045612, &1);
 * Hal_Vin_Close(fd) is close(fd).
 *
 * This is a device-boundary mock, not the kernel driver: it returns success
 * for the two known ioctls and logs the path/id/start for assertions. Unknown
 * ioctls return ENOTTY. Use it via LD_PRELOAD.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

static int fake_fd = -1;
static int (*real_open64)(const char *, int, ...);
static int (*real_open)(const char *, int, ...);
static int (*real_close)(int);
static int (*real_ioctl)(int, unsigned long, ...);

static void vin_log(const char *fmt, ...)
{
    const char *path = getenv("FAKE_VIN_LOG");
    if (!path || !path[0]) return;
    FILE *f = fopen(path, "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static int is_vin(const char *path)
{
    return path && (strstr(path, "ntkdvif") != NULL ||
                    strstr(path, "/dev/video") != NULL);
}

static int fake_open(const char *path)
{
    int fd = (int)syscall(SYS_openat, AT_FDCWD, "/dev/null",
                          O_RDWR | O_CLOEXEC, 0);
    if (fd < 0) return -1;
    fake_fd = fd;
    vin_log("open path=%s fd=%d", path, fd);
    return fd;
}

int open64(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    if (is_vin(path)) return fake_open(path);
    if (!real_open64) real_open64 = dlsym(RTLD_NEXT, "open64");
    if (!real_open64) { errno = ENOSYS; return -1; }
    return flags & O_CREAT ? real_open64(path, flags, mode)
                           : real_open64(path, flags);
}

int open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    if (is_vin(path)) return fake_open(path);
    if (!real_open) real_open = dlsym(RTLD_NEXT, "open");
    if (!real_open) { errno = ENOSYS; return -1; }
    return flags & O_CREAT ? real_open(path, flags, mode)
                           : real_open(path, flags);
}

int close(int fd)
{
    if (!real_close) real_close = dlsym(RTLD_NEXT, "close");
    if (fd == fake_fd) {
        vin_log("close fd=%d", fd);
        fake_fd = -1;
    }
    return real_close ? real_close(fd) : (errno = ENOSYS, -1);
}

int ioctl(int fd, unsigned long req, ...)
{
    va_list ap;
    va_start(ap, req);
    void *arg = va_arg(ap, void *);
    va_end(ap);
    if (!real_ioctl) real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    if (fd != fake_fd)
        return real_ioctl ? real_ioctl(fd, req, arg)
                          : (errno = ENOTTY, -1);
    if (req == 0xc0045627u) {
        int id = arg ? *(int *)arg : -1;
        vin_log("select fd=%d id=%d", fd, id);
        return 0;
    }
    if (req == 0x40045612u) {
        int on = arg ? *(int *)arg : -1;
        vin_log("start fd=%d on=%d", fd, on);
        return 0;
    }
    /* VIDIOC_G_EXT_CTRLS id 0xa00968, used by plat_drm_get_dpsource for
     * non-1..3 source types. The model accepts the request; it does not claim
     * driver control values. */
    if (req == 0xc0185647u) {
        vin_log("ext_ctrls fd=%d", fd);
        return 0;
    }
    vin_log("ioctl fd=%d req=%#lx", fd, req);
    errno = ENOTTY;
    return -1;
}

/* mod_plat_stub does not run Hal_Video_DrmInit(1); the real VIN/source module
 * still needs the post-init DRM id and plane type for drm_connect. These are
 * boundary values for the fixture, not a DRM emulation. */
int Hal_Video_Get_DrmDeviceId(int index)
{
    (void)index;
    return 13;
}

int Hal_Video_VideoPath_to_PlaneType(int path)
{
    (void)path;
    return 0;
}

int Hal_Video_Vpltio_SendCmd(int cmd, int a, void *buf, int b, int c, int d,
                             int e, int f, int g)
{
    (void)cmd; (void)a; (void)buf; (void)b; (void)c; (void)d;
    (void)e; (void)f; (void)g;
    return 0;
}
