#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <linux/netlink.h>
#include <sys/socket.h>
#include "openaurora/audio_owner.h"

static struct {
    int sockets, closes, pending;
    pthread_t thread;
} hw;

static int fake_socket(void *ctx, int domain, int type, int protocol)
{
    assert(ctx == &hw && domain == AF_NETLINK && protocol == NETLINK_USERSOCK);
    assert(type == (SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC));
    hw.sockets++;
    hw.thread = pthread_self();
    return 4;
}
static int fake_bind(void *ctx, int fd, uint32_t port)
{
    assert(ctx == &hw && fd == 4 && port == (uint32_t)getpid());
    return 0;
}
static ssize_t fake_send(void *ctx, int fd, const void *bytes, size_t size)
{
    (void)bytes;
    assert(ctx == &hw && fd == 4 && size == OA_AUDIO_NETLINK_FRAME_SIZE);
    hw.pending = 1;
    return (ssize_t)size;
}
static int fake_wait(void *ctx, int fd, int timeout)
{
    assert(ctx == &hw && fd == 4 && timeout == 25);
    return 1;
}
static ssize_t fake_receive(void *ctx, int fd, void *bytes, size_t cap, uint32_t *peer, int *truncated)
{
    assert(ctx == &hw && fd == 4 && cap >= 84);
    if (!hw.pending) {
        errno = EAGAIN;
        return -1;
    }
    hw.pending = 0;
    uint8_t reply[84] = {83};
    memcpy(reply + 16, "GetOPUGain", 10);
    reply[26] = 5;
    reply[30] = 0x34;
    reply[31] = 0x12;
    reply[35] = 0x80;
    memcpy(bytes, reply, sizeof reply);
    *peer = 0;
    *truncated = 0;
    return sizeof reply;
}
static int fake_close(void *ctx, int fd)
{
    assert(ctx == &hw && fd == 4);
    hw.closes++;
    hw.thread = pthread_self();
    return 0;
}
static const oa_audio_client_io io = {
    &hw, fake_socket, fake_bind, fake_send, fake_wait, fake_receive, fake_close
};

static void settle(oa_audio_owner *owner)
{
    for (int i = 0; i < 200; i++) {
        if (oa_audio_owner_quiesce(owner) == 0) return;
        struct timespec delay = { .tv_nsec = 1000000 };
        nanosleep(&delay, NULL);
    }
    assert(oa_audio_owner_quiesce(owner) == 0);
}

int main(void)
{
    alarm(5);
    oa_audio_owner *owner = NULL;
    assert(oa_audio_owner_create(&owner, NULL, NULL) == 0);
    assert(oa_audio_owner_open(owner, NULL) == -1 && errno == ENOTSUP && hw.sockets == 0);
    assert(oa_audio_owner_open(owner, &io) == 0 && hw.sockets == 1 && hw.thread != pthread_self());
    oa_audio_opu_gain gain = {0};
    assert(oa_audio_owner_gain(owner, 5, 25, &gain) == 0);
    assert(gain.device == 5 && gain.left == 0x1234 && gain.right == 0x8000);
    settle(owner);
    assert(hw.closes == 1 && hw.thread != pthread_self());
    assert(oa_audio_owner_join(owner) == 0);
    oa_audio_owner_destroy(owner);
    puts("  ok   audio worker rejects a real socket, runs gain off the caller, and closes on quiesce");
    return 0;
}
