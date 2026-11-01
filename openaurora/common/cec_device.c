#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "openaurora/cec_device.h"
#include "openaurora/vendor_cec.h"

#define CEC_OPEN  UINT32_C(0x401c4301)
#define CEC_CLOSE UINT32_C(0x80014302)
#define CEC_RX    UINT32_C(0x80104303)
#define CEC_TX    UINT32_C(0xc0104304)

static int sys_open(void *context, const char *path, int flags)
{
    (void)context; return open(path, flags);
}
static int sys_ioctl(void *context, int fd, unsigned long request, void *arg)
{
    (void)context; return ioctl(fd, request, arg);
}
static int sys_close(void *context, int fd)
{
    (void)context; return close(fd);
}
static const oa_cec_device_io system_io = { NULL, sys_open, sys_ioctl, sys_close };

static int error(int number) { errno = number; return -1; }
static int ready(const oa_cec_device *d)
{
    return d && d->fd >= 0 && d->initialized && d->io;
}
int oa_cec_device_open(oa_cec_device *d, const char *path, const oa_cec_device_io *io)
{
    if (!d || !path || !*path) return error(EINVAL);
    if (d->fd != -1 || d->initialized || d->io) return error(EBUSY);
    if (!io) {
#if !defined(__arm__) || UINTPTR_MAX != UINT32_MAX
        return error(ENOTSUP);
#else
        io = &system_io;
#endif
    }
    /* Keep the real table referenced in host builds without enabling host I/O. */
    (void)system_io;
    if (!io->open || !io->ioctl || !io->close) return error(EINVAL);
    int fd = io->open(io->context, path, O_RDWR | O_CLOEXEC);
    if (fd < 0) return -1;
    /* DRV_CEC_Open's built-in hardware parameters, expressed as the 28-byte
     * KER_CEC_IOCTL_OPEN argument, not the different 36-byte VPL input. Last
     * words are logical address 0 and hardware id 0. No userspace callback. */
    uint32_t params[7] = { 300, 3, UINT32_C(0xedd5c5ad), UINT32_C(0x8b64573f),
                          UINT32_C(0x8b642a12), 0, 0 };
    if (io->ioctl(io->context, fd, CEC_OPEN, params) < 0) {
        int primary = errno;
        (void)io->close(io->context, fd);
        errno = primary; return -1;
    }
    d->fd = fd; d->initialized = 1; d->io = io;
    return 0;
}
int oa_cec_device_send(oa_cec_device *d, const oa_cec_message *message)
{
    if (!ready(d)) return error(ENODEV);
    if (!message) return error(EINVAL);
    if (message->initiator != 0) return error(EADDRNOTAVAIL);
    uint8_t buffer[OA_CEC_MAX_FRAME]; oa_cec_raw_packet packet;
    if (oa_cec_raw_encode(&packet, buffer, sizeof buffer, message)) return error(EINVAL);
    return d->io->ioctl(d->io->context, d->fd, CEC_TX, &packet) < 0 ? -1 : 0;
}
int oa_cec_device_receive(oa_cec_device *d, oa_cec_message *message)
{
    if (!ready(d)) return error(ENODEV);
    if (!message) return error(EINVAL);
    uint8_t buffer[OA_CEC_MAX_FRAME]; oa_cec_raw_packet packet;
    if (oa_cec_raw_prepare(&packet, buffer, sizeof buffer)) return error(EINVAL);
    if (d->io->ioctl(d->io->context, d->fd, CEC_RX, &packet) < 0) return -1;
    int rc = oa_cec_raw_decode(message, &packet, buffer, sizeof buffer);
    return rc < 0 ? error(EPROTO) : rc;
}
int oa_cec_device_close(oa_cec_device *d)
{
    if (!d) return error(EINVAL);
    if (d->fd == -1 && !d->initialized && !d->io) return 0;
    if (!ready(d)) return error(EINVAL);
    uint8_t device_id = 0;
    /* Kernel close wants a pointer to hardware id 0, never a NULL argument. */
    if (d->io->ioctl(d->io->context, d->fd, CEC_CLOSE, &device_id) < 0) return -1;
    int fd = d->fd;
    const oa_cec_device_io *io = d->io;
    d->fd = -1; d->initialized = 0; d->io = NULL;
    return io->close(io->context, fd);
}
