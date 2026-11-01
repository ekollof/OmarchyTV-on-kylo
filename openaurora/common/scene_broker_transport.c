#include <errno.h>
#include <fcntl.h>
#include <linux/memfd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "openaurora/scene_broker.h"

int oa_scene_packet_send(int fd, const uint8_t bytes[OA_SCENE_PACKET_SIZE], int image, int nonblocking)
{
    if (!bytes || image < -1 || (nonblocking != 0 && nonblocking != 1)) { errno = EINVAL; return -1; }
    struct iovec vector = {(void *)bytes, OA_SCENE_PACKET_SIZE};
    union { struct cmsghdr alignment; uint8_t bytes[CMSG_SPACE(sizeof(int))]; } control;
    struct msghdr msg = {.msg_iov = &vector, .msg_iovlen = 1};
    if (image >= 0) {
        memset(&control, 0, sizeof control);
        msg.msg_control = control.bytes; msg.msg_controllen = sizeof control.bytes;
        struct cmsghdr *c = CMSG_FIRSTHDR(&msg);
        c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS; c->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(c), &image, sizeof image);
    }
    ssize_t n = sendmsg(fd, &msg, MSG_NOSIGNAL | (nonblocking ? MSG_DONTWAIT : 0));
    if (n < 0) return -1;
    if (n != OA_SCENE_PACKET_SIZE) { errno = EIO; return -1; }
    return 0;
}
int oa_scene_packet_receive(int fd, uint8_t bytes[OA_SCENE_PACKET_SIZE], int *image, int nonblocking)
{
    if (!bytes || !image || (nonblocking != 0 && nonblocking != 1)) { errno = EINVAL; return -1; }
    /* Room for Linux's entire SCM_MAX_FD record. Besides closing every rejected
     * descriptor, this avoids relying on qemu's truncated-control translation
     * (the installed ARM emulator leaks undisclosed host fds in that case). */
    enum { max_descriptors = 253 };
    union { struct cmsghdr alignment; uint8_t bytes[CMSG_SPACE(max_descriptors*sizeof(int))]; } control;
    struct iovec vector = {bytes, OA_SCENE_PACKET_SIZE};
    struct msghdr msg = {.msg_iov = &vector, .msg_iovlen = 1,
        .msg_control = control.bytes, .msg_controllen = sizeof control.bytes};
    ssize_t n = recvmsg(fd, &msg, MSG_CMSG_CLOEXEC | MSG_TRUNC | (nonblocking ? MSG_DONTWAIT : 0));
    if (n < 0) return -1;
    int descriptors[max_descriptors], count = 0, bad = (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0;
    for (struct cmsghdr *c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
        size_t remaining = msg.msg_controllen - (size_t)((uint8_t *)c - control.bytes);
        if (c->cmsg_len < CMSG_LEN(0) || c->cmsg_len > remaining ||
            c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS ||
            (c->cmsg_len-CMSG_LEN(0)) % sizeof(int)) { bad = 1; continue; }
        size_t size = c->cmsg_len-CMSG_LEN(0);
        for (size_t at = 0; at < size; at += sizeof(int)) {
            int received; memcpy(&received, CMSG_DATA(c)+at, sizeof received);
            if (count < max_descriptors) descriptors[count++] = received;
            else { (void)close(received); bad = 1; }
        }
    }
    if (count > 1 || (n != 0 && n != OA_SCENE_PACKET_SIZE) || (n == 0 && count)) bad = 1;
    if (bad) {
        for (int i = 0; i < count; i++) (void)close(descriptors[i]);
        errno = EPROTO; return -1;
    }
    *image = count ? descriptors[0] : -1;
    return n ? 1 : 0; /* zero-length records are treated as disconnect */
}
int oa_scene_image_create(const void *rgba, size_t size)
{
    if (!rgba || size != OA_SCENE_IMAGE_BYTES) { errno = EINVAL; return -1; }
#ifndef SYS_memfd_create
    errno = ENOSYS; return -1;
#else
    int fd = (int)syscall(SYS_memfd_create, "openaurora-scene", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) return -1;
    size_t done = 0;
    while (done < size) {
        ssize_t n = write(fd, (const uint8_t *)rgba+done, size-done);
        if (n < 0 && errno == EINTR) continue; /* regular-file write, not hardware dispatch */
        if (n <= 0) {
            int primary = n < 0 ? errno : EIO;
            (void)close(fd); errno = primary; return -1;
        }
        done += (size_t)n;
    }
    if (syscall(SYS_fcntl, fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) < 0) {
        int primary = errno; (void)close(fd); errno = primary; return -1;
    }
    return fd;
#endif
}
