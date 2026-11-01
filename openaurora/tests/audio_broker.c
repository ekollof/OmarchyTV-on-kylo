#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <spawn.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include "openaurora/audio_broker.h"
extern char **environ;

static struct { atomic_int opens, closes, sends, fail_send; int pending; uint8_t device; } hw;
static int socket_io(void *ctx, int domain, int type, int protocol)
{ (void)domain; (void)type; (void)protocol; assert(ctx == &hw); atomic_fetch_add(&hw.opens, 1); return 7; }
static int bind_io(void *ctx, int fd, uint32_t pid)
{ assert(ctx == &hw && fd == 7 && pid == (uint32_t)getpid()); return 0; }
static ssize_t send_io(void *ctx, int fd, const void *data, size_t length)
{
    assert(ctx == &hw && fd == 7 && length == OA_AUDIO_NETLINK_FRAME_SIZE);
    const uint8_t *bytes = data;
    assert(!memcmp(bytes + 16, "GetOPUGain", 10));
    atomic_fetch_add(&hw.sends, 1);
    if (atomic_load(&hw.fail_send)) { errno = EINTR; return -1; }
    hw.device = bytes[26]; hw.pending = 1;
    return (ssize_t)length;
}
static int wait_io(void *ctx, int fd, int timeout)
{ assert(ctx == &hw && fd == 7 && timeout == 25); return 1; }
static ssize_t receive_io(void *ctx, int fd, void *out, size_t cap, uint32_t *peer, int *truncated)
{
    assert(ctx == &hw && fd == 7 && cap >= 84);
    if (!hw.pending) { errno = EAGAIN; return -1; }
    hw.pending = 0;
    uint8_t bytes[84] = {83};
    memcpy(bytes + 16, "GetOPUGain", 10); bytes[26] = hw.device;
    bytes[30] = 0x34; bytes[31] = 0x12; bytes[35] = 0x80;
    memcpy(out, bytes, sizeof bytes); *peer = 0; *truncated = 0;
    return sizeof bytes;
}
static int close_io(void *ctx, int fd)
{ assert(ctx == &hw && fd == 7); atomic_fetch_add(&hw.closes, 1); return 0; }
static const oa_audio_client_io io = { &hw, socket_io, bind_io, send_io, wait_io, receive_io, close_io };
struct server { int fd, result, error; };
static void *serve(void *ud)
{
    struct server *s = ud;
    s->result = oa_audio_broker_serve(s->fd, &io); s->error = errno;
    close(s->fd);
    return NULL;
}
static void pair(int fds[2]) { assert(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds) == 0); }
static oa_audio_broker_reply exchange(int fd, uint64_t ticket, unsigned device, unsigned timeout)
{
    oa_audio_broker_request request = { ticket, device, timeout };
    uint8_t bytes[OA_AUDIO_BROKER_PACKET_SIZE];
    assert(oa_audio_broker_encode_request(bytes, sizeof bytes, &request) == 0);
    assert(send(fd, bytes, sizeof bytes, MSG_NOSIGNAL) == sizeof bytes);
    struct pollfd watch = { .fd = fd, .events = POLLIN };
    assert(poll(&watch, 1, 5000) == 1 && (watch.revents & POLLIN));
    ssize_t n = recv(fd, bytes, sizeof bytes, MSG_TRUNC);
    oa_audio_broker_reply reply;
    assert(n == sizeof bytes && oa_audio_broker_decode_reply(&reply, bytes, (size_t)n) == 0);
    assert(reply.ticket == ticket && reply.gain.device == device);
    return reply;
}
static void codecs(void)
{
    uint8_t bytes[33], original[33]; memset(bytes, 0xa5, sizeof bytes);
    oa_audio_broker_request request = { UINT64_C(0x123456789abcdef0), 5, 25 }, decoded = {0};
    assert(!oa_audio_broker_encode_request(bytes, 32, &request));
    assert(!memcmp(bytes, "OAB1\1\0\0\0\xf0\xde\xbc\x9a\x78\x56\x34\x12", 16));
    assert(!oa_audio_broker_decode_request(&decoded, bytes, 32) && decoded.ticket == request.ticket && decoded.device == 5);
    memcpy(original, bytes, sizeof bytes);
    for (unsigned i = 0; i < 32; i++) {
        memcpy(bytes, original, sizeof bytes);
        if ((i >= 8 && i < 16) || i == 20) continue; /* other tickets/255ms are valid */
        bytes[i] = 0xff;
        oa_audio_broker_request before = decoded;
        assert(oa_audio_broker_decode_request(&decoded, bytes, 32) == -1);
        assert(!memcmp(&before, &decoded, sizeof before));
    }
    memcpy(bytes, original, sizeof bytes);
    assert(oa_audio_broker_decode_request(&decoded, bytes, 31) == -1);
    assert(oa_audio_broker_decode_request(&decoded, bytes, 33) == -1);
    memset(bytes + 8, 0, 8);
    assert(oa_audio_broker_decode_request(&decoded, bytes, 32) == -1);
    memcpy(bytes, original, sizeof bytes); bytes[20] = 0xd1; bytes[21] = 7; /*2001*/
    assert(oa_audio_broker_decode_request(&decoded, bytes, 32) == -1);
    oa_audio_broker_reply reply = { .ticket = 9, .error = EINTR, .gain = { .device = 4 } }, out;
    assert(!oa_audio_broker_encode_reply(bytes, 32, &reply));
    assert(!oa_audio_broker_decode_reply(&out, bytes, 32) && out.error == EINTR && out.ticket == 9);
    bytes[24] = 1;
    assert(oa_audio_broker_decode_reply(&out, bytes, 32) == -1);
    memcpy(original, bytes, sizeof bytes);
    reply.gain.left = 1;
    assert(oa_audio_broker_encode_reply(bytes, 32, &reply) == -1 && !memcmp(bytes, original, sizeof bytes));
    puts("  ok   fixed endian/ticket protocol rejects malformed, reserved, short/long and inconsistent error packets atomically");
}
static void sessions(void)
{
    int fds[2]; pair(fds);
    struct server s = { .fd = fds[1] }; pthread_t thread;
    assert(pthread_create(&thread, NULL, serve, &s) == 0);
    oa_audio_broker_reply reply = exchange(fds[0], 1, 5, 25);
    assert(!reply.error && reply.gain.left == 0x1234 && reply.gain.right == 0x8000);
    atomic_store(&hw.fail_send, 1);
    reply = exchange(fds[0], 2, 4, 25);
    assert(reply.error == EINTR && !reply.gain.left && !reply.gain.right);
    close(fds[0]); assert(pthread_join(thread, NULL) == 0 && s.result == 0);
    assert(atomic_load(&hw.opens) == 1 && atomic_load(&hw.sends) == 2 && atomic_load(&hw.closes) == 1);
    puts("  ok   broker reuses one lazy session, echoes tickets/errors without replay and closes on IPC EOF");

    pair(fds); s = (struct server){ .fd = fds[1] };
    assert(pthread_create(&thread, NULL, serve, &s) == 0);
    uint8_t oversized[33] = {0};
    oa_audio_broker_request request = { 3, 0, 25 };
    assert(!oa_audio_broker_encode_request(oversized, sizeof oversized, &request));
    assert(send(fds[0], oversized, sizeof oversized, MSG_NOSIGNAL) == sizeof oversized);
    close(fds[0]); assert(pthread_join(thread, NULL) == 0 && s.result == -1 && s.error == EPROTO);
    assert(atomic_load(&hw.opens) == 1 && atomic_load(&hw.sends) == 2);
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    assert(oa_audio_broker_serve(fds[1], &io) == -1 && errno == EPROTOTYPE);
    close(fds[0]); close(fds[1]);
    puts("  ok   truncated request and wrong IPC socket type fail before any mixer open/send");
}
static void spawn(const char *path, int native)
{
    int fds[2], spare[2]; pair(fds); assert(pipe(spare) == 0);
    /* This integer-only operation needs no fcntl64 layout. The cross libc's
     * fcntl wrapper requires GLIBC2.28; target glibc is2.23. */
    int extra = (int)syscall(SYS_fcntl, spare[0], F_DUPFD, 128); assert(extra >= 128);
    posix_spawn_file_actions_t actions;
    assert(posix_spawn_file_actions_init(&actions) == 0);
    assert(posix_spawn_file_actions_addclose(&actions, fds[0]) == 0);
    assert(posix_spawn_file_actions_adddup2(&actions, fds[1], STDIN_FILENO) == 0);
    if (fds[1] != STDIN_FILENO) assert(posix_spawn_file_actions_addclose(&actions, fds[1]) == 0);
    char *const args[] = { (char *)path, "--serve", NULL };
    pid_t child;
    assert(posix_spawn(&child, path, &actions, NULL, args, environ) == 0);
    posix_spawn_file_actions_destroy(&actions); close(fds[1]);
    oa_audio_broker_reply reply = exchange(fds[0], UINT64_C(0x123456789abcdef0), 0, 2000);
    if (native) {
        assert(!reply.error);
        printf("  native broker pid=%ld reply device=%u left=%u right=%u; no operation ACK\n",
               (long)child, reply.gain.device, reply.gain.left, reply.gain.right);
    } else assert(reply.error == ENOTSUP);
    char fd_path[96]; snprintf(fd_path, sizeof fd_path, "/proc/%ld/fd/%d", (long)child, extra);
    assert(access(fd_path, F_OK) == -1 && errno == ENOENT);
    close(extra); close(spare[0]); close(spare[1]); close(fds[0]);
    int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    puts("  ok   posix_spawn child owns its session, drops unrelated inherited fds and exits cleanly after EOF");
}
int main(int argc, char **argv)
{
    alarm(15);
    assert(argc == 1 || (argc == 3 && (!strcmp(argv[1], "--spawn") || !strcmp(argv[1], "--native"))));
    codecs(); sessions();
    if (argc == 3) spawn(argv[2], !strcmp(argv[1], "--native"));
    return 0;
}
