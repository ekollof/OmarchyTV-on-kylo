#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "openaurora/source_hdmi_event_io.h"

static int errno_or(int fallback)
{
    return errno ? errno : fallback;
}

static int valid_io(const struct oa_source_hdmi_event_io *io)
{
    return io && io->open && io->ioctl && io->close;
}

int oa_source_hdmi_event_subscribe(const struct oa_source_hdmi_event_io *io,
                                   const char *path, int *fd_out)
{
    if (!valid_io(io) || !path || !*path || !fd_out) return -EINVAL;

    int fd = io->open(io->context, path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return -errno_or(EIO);

    struct oa_source_hdmi_event_subscription sub;
    memset(&sub, 0, sizeof sub);
    sub.type = OA_SOURCE_HDMI_EVENT_TYPE;
    if (io->ioctl(io->context, fd, OA_SOURCE_HDMI_EVENT_IOC_SUBSCRIBE,
                  &sub) < 0) {
        int error = errno_or(EIO);
        (void)io->close(io->context, fd);
        return -error;
    }
    *fd_out = fd;
    return 0;
}

int oa_source_hdmi_event_read(const struct oa_source_hdmi_event_io *io, int fd,
                              struct oa_source_hdmi_event *out)
{
    if (!valid_io(io) || !out || fd < 0) return -EINVAL;

    struct oa_source_hdmi_event_buffer event;
    memset(&event, 0, sizeof event);
    if (io->ioctl(io->context, fd, OA_SOURCE_HDMI_EVENT_IOC_DQEVENT,
                  &event) < 0) {
        int error = errno;
        if (error == EAGAIN || error == EWOULDBLOCK || error == ENODATA)
            return 0;
        return -errno_or(EIO);
    }
    if (event.type != OA_SOURCE_HDMI_EVENT_TYPE) return 0;
    return oa_source_hdmi_event_decode(event.data, OA_SOURCE_HDMI_EVENT_SIZE,
                                       out) == 0 ? 1 : -EPROTO;
}

int oa_source_hdmi_event_close(const struct oa_source_hdmi_event_io *io, int fd)
{
    if (!valid_io(io) || fd < 0) return -EINVAL;
    if (io->close(io->context, fd) < 0) return -errno_or(EIO);
    return 0;
}
