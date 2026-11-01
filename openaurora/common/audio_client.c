#include <errno.h>
#include <linux/netlink.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "openaurora/audio_client.h"

_Static_assert(sizeof(struct nlmsghdr) == 16, "target netlink header ABI");
_Static_assert(sizeof(struct sockaddr_nl) == 12, "target netlink address ABI");

static int sys_socket(void *ctx, int domain, int type, int protocol)
{ (void)ctx; return socket(domain, type, protocol); }
static int sys_bind(void *ctx, int fd, uint32_t port)
{
    (void)ctx;
    struct sockaddr_nl address = {.nl_family = AF_NETLINK, .nl_pid = port};
    return bind(fd, (const struct sockaddr *)&address, sizeof address);
}
static ssize_t sys_send(void *ctx, int fd, const void *bytes, size_t size)
{
    (void)ctx;
    struct sockaddr_nl kernel = {.nl_family = AF_NETLINK};
    return sendto(fd, bytes, size, MSG_DONTWAIT, (const struct sockaddr *)&kernel, sizeof kernel);
}
static int sys_wait(void *ctx, int fd, int timeout)
{
    (void)ctx;
    struct pollfd watch = {.fd = fd, .events = POLLIN};
    int rc = poll(&watch, 1, timeout);
    if (rc <= 0) return rc;
    if (watch.revents & POLLIN) return 1;
    errno = EIO; return -1;
}
static ssize_t sys_receive(void *ctx, int fd, void *bytes, size_t cap, uint32_t *peer, int *truncated)
{
    (void)ctx;
    struct sockaddr_nl address = {0};
    struct iovec vector = {bytes, cap};
    struct msghdr message = {.msg_name = &address, .msg_namelen = sizeof address,
                            .msg_iov = &vector, .msg_iovlen = 1};
    ssize_t rc = recvmsg(fd, &message, MSG_DONTWAIT);
    if (rc >= 0) {
        *peer = message.msg_namelen == sizeof address && address.nl_family == AF_NETLINK ? address.nl_pid : UINT32_MAX;
        *truncated = (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0;
    }
    return rc;
}
static int sys_close(void *ctx, int fd) { (void)ctx; return close(fd); }
static const oa_audio_client_io system_io = {NULL, sys_socket, sys_bind, sys_send, sys_wait, sys_receive, sys_close};
static int error(int code) { errno = code; return -1; }
static int ready(const oa_audio_client *c)
{
    if (!c || c->fd < 0 || !c->port || !c->io) return error(ENODEV);
    if (c->port != (uint32_t)getpid()) return error(EPERM);
    return 0;
}
int oa_audio_client_open(oa_audio_client *c, const oa_audio_client_io *io)
{
    if (!c) return error(EINVAL);
    if (c->fd != -1 || c->port || c->io) return error(EBUSY);
    if (!io) {
#if !defined(__arm__) || UINTPTR_MAX != UINT32_MAX || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
        return error(ENOTSUP);
#else
        io = &system_io;
#endif
    }
    (void)system_io;
    if (!io->socket || !io->bind || !io->send || !io->wait || !io->receive || !io->close) return error(EINVAL);
    int fd = io->socket(io->context, AF_NETLINK, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, NETLINK_USERSOCK);
    if (fd < 0) return -1;
    uint32_t port = (uint32_t)getpid();
    if (io->bind(io->context, fd, port) < 0) {
        int primary = errno;
        (void)io->close(io->context, fd);
        errno = primary; return -1;
    }
    c->fd = fd; c->port = port; c->io = io;
    return 0;
}
static int send_frame(oa_audio_client *c, const uint8_t *bytes)
{
    ssize_t size = c->io->send(c->io->context, c->fd, bytes, OA_AUDIO_NETLINK_FRAME_SIZE);
    if (size < 0) return -1;
    return size == OA_AUDIO_NETLINK_FRAME_SIZE ? 0 : error(EIO);
}
int oa_audio_client_table(oa_audio_client *c, unsigned index, uint32_t address)
{
    if (ready(c)) return -1;
    uint8_t frame[OA_AUDIO_NETLINK_FRAME_SIZE];
    if (oa_audio_netlink_set_table(frame, sizeof frame, c->port, index, address, 101)) return error(EINVAL);
    return send_frame(c, frame);
}
int oa_audio_client_select(oa_audio_client *c, unsigned device, unsigned index)
{
    if (ready(c)) return -1;
    uint8_t frame[OA_AUDIO_NETLINK_FRAME_SIZE];
    if (oa_audio_netlink_select_table(frame, sizeof frame, c->port, device, index)) return error(EINVAL);
    return send_frame(c, frame);
}
int oa_audio_client_set_opu_gain(oa_audio_client *c, unsigned device, unsigned left, unsigned right)
{
    if (ready(c)) return -1;
    uint8_t frame[OA_AUDIO_NETLINK_FRAME_SIZE];
    if (oa_audio_netlink_set_opu_gain(frame, sizeof frame, c->port, device, left, right)) return error(EINVAL);
    return send_frame(c, frame);
}
static int drain(oa_audio_client *c)
{
    uint8_t bytes[128];
    for (unsigned i = 0; i < 16; i++) {
        uint32_t peer; int truncated;
        ssize_t size = c->io->receive(c->io->context, c->fd, bytes, sizeof bytes, &peer, &truncated);
        if (size >= 0) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
    return error(ENOBUFS);
}
int oa_audio_client_gain(oa_audio_client *c, unsigned device, int timeout, oa_audio_opu_gain *out)
{
    if (ready(c)) return -1;
    if (!out || timeout < 0) return error(EINVAL);
    uint8_t frame[OA_AUDIO_NETLINK_FRAME_SIZE], reply[128];
    if (oa_audio_netlink_get_gain(frame, sizeof frame, c->port, device)) return error(EINVAL);
    if (drain(c) || send_frame(c, frame)) return -1;
    int rc = c->io->wait(c->io->context, c->fd, timeout);
    if (rc < 0) return -1;
    if (!rc) return error(ETIMEDOUT);
    uint32_t peer = UINT32_MAX; int truncated = 1;
    ssize_t size = c->io->receive(c->io->context, c->fd, reply, sizeof reply, &peer, &truncated);
    if (size < 0) return -1;
    oa_audio_opu_gain gain;
    if (peer || truncated || oa_audio_netlink_gain_reply(&gain, reply, (size_t)size) || gain.device != device)
        return error(EPROTO);
    *out = gain;
    return 0;
}
int oa_audio_client_close(oa_audio_client *c)
{
    if (!c) return error(EINVAL);
    if (c->fd == -1 && !c->port && !c->io) return 0;
    if (c->fd < 0 || !c->io || !c->port) return error(EINVAL);
    int fd = c->fd;
    const oa_audio_client_io *io = c->io;
    c->fd = -1; c->port = 0; c->io = NULL;
    return io->close(io->context, fd);
}
