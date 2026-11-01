#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include "openaurora/audio_client.h"

static struct {
    int sockets, binds, sends, waits, receives, closes;
    int fail_socket, fail_bind, fail_send, short_send, fail_close;
    int fail_wait, timeout, fail_receive, fail_drain;
    int stale, pending, request_device, wrong_peer, truncated, malformed, wrong_device;
    uint8_t sent[OA_AUDIO_NETLINK_FRAME_SIZE];
} f;
static int fail(int code) { errno = code; return -1; }
static int fake_socket(void *ctx, int domain, int type, int protocol)
{
    assert(ctx == &f && domain == AF_NETLINK && protocol == NETLINK_USERSOCK);
    assert(type == (SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC));
    f.sockets++; return f.fail_socket ? fail(EMFILE) : 0;
}
static int fake_bind(void *ctx, int fd, uint32_t port)
{
    assert(ctx == &f && fd == 0 && port == (uint32_t)getpid());
    f.binds++; return f.fail_bind ? fail(EADDRINUSE) : 0;
}
static ssize_t fake_send(void *ctx, int fd, const void *bytes, size_t size)
{
    assert(ctx == &f && fd == 0 && size == sizeof f.sent);
    memcpy(f.sent, bytes, size); f.sends++;
    if (f.fail_send) return fail(f.fail_send);
    if (f.short_send) return (ssize_t)size - 1;
    if (!memcmp(f.sent + 16, "GetOPUGain", 10)) {
        f.pending = 1; f.request_device = f.sent[26];
    }
    return (ssize_t)size;
}
static int fake_wait(void *ctx, int fd, int timeout)
{
    assert(ctx == &f && fd == 0 && timeout == 25); f.waits++;
    if (f.fail_wait) return fail(f.fail_wait);
    return f.timeout ? 0 : 1;
}
static ssize_t fake_receive(void *ctx, int fd, void *bytes, size_t cap, uint32_t *peer, int *truncated)
{
    assert(ctx == &f && fd == 0 && cap >= 84); f.receives++;
    if (f.fail_drain) return fail(EIO);
    if (f.stale) {
        f.stale--; *(uint8_t *)bytes = 0; *peer = 9; *truncated = 1; return 1;
    }
    if (!f.pending) return fail(EAGAIN);
    if (f.fail_receive) return fail(f.fail_receive);
    f.pending = 0;
    uint8_t reply[84] = {83};
    memcpy(reply + 16, "GetOPUGain", 10);
    reply[26] = (uint8_t)(f.wrong_device ? (f.request_device + 1) % 6 : f.request_device);
    reply[30] = 0x34; reply[31] = 0x12; reply[35] = 0x80;
    if (f.malformed) reply[0] = 0;
    memcpy(bytes, reply, sizeof reply);
    *peer = f.wrong_peer ? 1 : 0; *truncated = f.truncated;
    return sizeof reply;
}
static int fake_close(void *ctx, int fd)
{
    assert(ctx == &f && fd == 0); f.closes++;
    return f.fail_close ? fail(EINTR) : 0;
}
static const oa_audio_client_io io = {&f, fake_socket, fake_bind, fake_send, fake_wait, fake_receive, fake_close};
static void reset(void) { memset(&f, 0, sizeof f); }
int main(void)
{
    oa_audio_client c = OA_AUDIO_CLIENT_INIT;
    oa_audio_opu_gain gain = {99,88,77}, saved = gain;
    assert(!oa_audio_client_open(&c, &io) && c.fd == 0 && f.binds == 1);
    assert(oa_audio_client_open(&c, &io) == -1 && errno == EBUSY && f.sockets == 1);
    f.stale = 2;
    assert(!oa_audio_client_gain(&c, 5, 25, &gain) && gain.device == 5 && gain.left == 0x1234 && gain.right == 0x8000);
    assert(!f.stale && f.sends == 1 && f.receives == 4);
    assert(!oa_audio_client_close(&c) && c.fd == -1 && !c.io && !c.port && f.closes == 1);
    assert(!oa_audio_client_close(&c) && f.closes == 1);
    puts("  ok   fd zero, exact socket flags/process binding, stale-reply drain, matching reply and idempotent close");

    reset(); f.fail_socket = 1;
    assert(oa_audio_client_open(&c, &io) == -1 && errno == EMFILE && !f.closes && !f.binds);
    reset(); f.fail_bind = f.fail_close = 1;
    assert(oa_audio_client_open(&c, &io) == -1 && errno == EADDRINUSE && f.closes == 1 && c.fd == -1);
    reset(); oa_audio_client_io bad = io; bad.receive = NULL;
    assert(oa_audio_client_open(&c, &bad) == -1 && errno == EINVAL && !f.sockets);
    assert(!oa_audio_client_open(&c, &io)); f.fail_close = 1;
    assert(oa_audio_client_close(&c) == -1 && errno == EINTR && c.fd == -1 && !c.io);
    assert(!oa_audio_client_close(&c) && f.closes == 1);
    f.fail_close = 0; assert(!oa_audio_client_open(&c, &io) && !oa_audio_client_close(&c));
    puts("  ok   socket/bind failure unwind and primary errno; invalid I/O table; final close error consumes fd and permits reopen");

    reset(); assert(!oa_audio_client_open(&c, &io));
    assert(!oa_audio_client_table(&c, 0, 0x12345678) && !memcmp(f.sent + 16, "VOLCfg", 6));
    const uint8_t pointer[4] = {0x78,0x56,0x34,0x12};
    assert(!memcmp(f.sent + 26, pointer, 4) && f.sent[30] == 101 && !f.waits);
    assert(!oa_audio_client_select(&c, 5, 9) && !memcmp(f.sent + 16, "SetOPUVolCfg", 12) && f.sent[28] == 5 && f.sent[32] == 9);
    assert(oa_audio_client_table(&c, 10, 1) == -1 && errno == EINVAL);
    assert(oa_audio_client_table(&c, 0, 0) == -1 && errno == EINVAL);
    assert(oa_audio_client_select(&c, 6, 0) == -1 && errno == EINVAL && f.sends == 2);
    f.fail_send = EINTR;
    assert(oa_audio_client_table(&c, 0, 1) == -1 && errno == EINTR && f.sends == 3);
    f.fail_send = 0; f.short_send = 1;
    assert(oa_audio_client_select(&c, 0, 0) == -1 && errno == EIO && f.sends == 4);
    assert(!oa_audio_client_close(&c));
    puts("  ok   table/selection dispatch with exact pointer/indices; invalid input never sends; short send rejects and EINTR never replays");

    reset(); assert(!oa_audio_client_open(&c, &io));
    assert(!oa_audio_client_set_opu_gain(&c, 0, 0x8000, 100));
    assert(f.sends == 1 && !f.waits && !f.receives && !memcmp(f.sent + 16, "SetOPUGain", 10));
    const uint8_t channels[12] = {0,0,0,0,0,0x80,0,0,100,0,0,0};
    assert(!memcmp(f.sent + 26, channels, sizeof channels));
    assert(oa_audio_client_set_opu_gain(&c, 6, 0, 0) == -1 && errno == EINVAL);
    assert(oa_audio_client_set_opu_gain(&c, 0, 0x8001, 0) == -1 && errno == EINVAL);
    assert(oa_audio_client_set_opu_gain(&c, 0, 0, 0x8001) == -1 && errno == EINVAL && f.sends == 1);
    f.fail_send = EINTR;
    assert(oa_audio_client_set_opu_gain(&c, 0, 0, 0) == -1 && errno == EINTR && f.sends == 2);
    f.fail_send = 0; f.short_send = 1;
    assert(oa_audio_client_set_opu_gain(&c, 0, 0, 0) == -1 && errno == EIO && f.sends == 3);
    assert(!oa_audio_client_close(&c));
    assert(oa_audio_client_set_opu_gain(&c, 0, 0, 0) == -1 && errno == ENODEV && f.sends == 3);
    puts("  ok   direct OPU-gain dispatch uses bounded channel units; invalid/short/EINTR/closed-session paths never replay or fabricate an ACK");

    reset(); assert(!oa_audio_client_open(&c, &io)); gain = saved;
    assert(oa_audio_client_gain(&c, 0, -1, &gain) == -1 && errno == EINVAL && !f.sends);
    assert(oa_audio_client_gain(&c, 6, 25, &gain) == -1 && errno == EINVAL && !f.receives);
    f.stale = 17;
    assert(oa_audio_client_gain(&c, 0, 25, &gain) == -1 && errno == ENOBUFS && !f.sends && f.stale == 1);
    f.timeout = 1;
    assert(oa_audio_client_gain(&c, 0, 25, &gain) == -1 && errno == ETIMEDOUT && f.sends == 1);
    f.timeout = 0; f.fail_wait = EINTR;
    assert(oa_audio_client_gain(&c, 0, 25, &gain) == -1 && errno == EINTR && f.sends == 2);
    f.fail_wait = 0;
    int *failures[] = {&f.wrong_peer, &f.truncated, &f.malformed, &f.wrong_device};
    for (unsigned i = 0; i < sizeof failures / sizeof failures[0]; i++) {
        *failures[i] = 1;
        assert(oa_audio_client_gain(&c, 0, 25, &gain) == -1 && errno == EPROTO && !memcmp(&gain, &saved, sizeof gain));
        *failures[i] = 0;
    }
    f.fail_receive = EIO;
    assert(oa_audio_client_gain(&c, 0, 25, &gain) == -1 && errno == EIO);
    f.fail_receive = 0; f.fail_drain = 1;
    int sent = f.sends;
    assert(oa_audio_client_gain(&c, 0, 25, &gain) == -1 && errno == EIO && f.sends == sent);
    assert(!memcmp(&gain, &saved, sizeof gain));
    f.fail_drain = 0;
    assert(!oa_audio_client_gain(&c, 0, 25, &gain) && gain.device == 0);
    puts("  ok   bounded drain, timeout/error recovery, no wait replay, kernel peer/truncation/device/shape rejection and atomic query output");

    pid_t child = fork(); assert(child >= 0);
    if (!child) {
        int before = f.sends;
        int rejected = oa_audio_client_gain(&c, 0, 25, &gain) == -1 && errno == EPERM && f.sends == before;
        rejected = rejected && oa_audio_client_set_opu_gain(&c, 0, 0, 0) == -1 && errno == EPERM && f.sends == before;
        _exit(rejected && !oa_audio_client_close(&c) ? 0 : 1);
    }
    int status; assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    assert(!f.closes && !oa_audio_client_gain(&c, 0, 25, &gain));
    assert(!oa_audio_client_close(&c));
    assert(oa_audio_client_gain(&c, 0, 25, &gain) == -1 && errno == ENODEV);
    puts("  ok   inherited child cannot query parent's session; child close leaves parent ownership usable");
    return 0;
}
