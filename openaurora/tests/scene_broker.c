#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "openaurora/scene_broker.h"

/* A failed expectation must not invoke the TV's vendor core/reset handler. */
#undef assert
#define assert(expression) do { if (!(expression)) { \
    fprintf(stderr, "%s:%d: %s (errno=%d)\n", __FILE__, __LINE__, #expression, errno); \
    exit(1); } } while (0)

static int fd_count(void)
{
    DIR *dir = opendir("/proc/self/fd"); assert(dir);
    int count = 0;
    while (readdir(dir)) count++;
    assert(!closedir(dir)); return count;
}
static void pair(int fds[2])
{ assert(!socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds)); }
static void codecs(void)
{
    uint8_t bytes[65], original[65]; memset(bytes, 0xa5, sizeof bytes);
    oa_scene_request r = {.operation = OA_SCENE_FRAME, .ticket = UINT64_C(0x123456789abcdef0),
        .revision = 9, .type = 8, .progress = 0.5f, .upload = 1,
        .w = OA_SCENE_WIDTH, .h = OA_SCENE_HEIGHT}, out;
    assert(!oa_scene_encode_request(bytes, 64, &r));
    assert(!memcmp(bytes, "OGS1\2\0\0\0\xf0\xde\xbc\x9a\x78\x56\x34\x12", 16));
    assert(bytes[16] == 9 && bytes[24] == 8 && bytes[30] == 0 && bytes[31] == 0x3f && bytes[32] == 1);
    assert(bytes[36] == 0 && bytes[37] == 0 && bytes[38] == 0 && bytes[39] == 0);
    assert(bytes[44] == 0x80 && bytes[45] == 0x07 && bytes[46] == 0 && bytes[47] == 0);
    assert(bytes[48] == 0x38 && bytes[49] == 0x04 && bytes[50] == 0 && bytes[51] == 0);
    assert(bytes[64] == 0xa5);
    assert(!oa_scene_decode_request(&out, bytes, 64) && out.progress == 0.5f && out.ticket == r.ticket &&
           out.w == OA_SCENE_WIDTH && out.h == OA_SCENE_HEIGHT);
    memcpy(original, bytes, sizeof bytes);
    for (unsigned i = 52; i < 64; i++) {
        memcpy(bytes, original, sizeof bytes); bytes[i] = 1;
        oa_scene_request before = out;
        assert(oa_scene_decode_request(&out, bytes, 64) == -1 && !memcmp(&before, &out, sizeof out));
    }
    memcpy(bytes, original, sizeof bytes);
    r.w = 0;
    assert(oa_scene_encode_request(bytes, 64, &r) == -1);
    r.w = OA_SCENE_WIDTH; r.x = 1;
    assert(oa_scene_encode_request(bytes, 64, &r) == -1);
    r.x = 0;
    assert(oa_scene_decode_request(&out, bytes, 63) == -1);
    assert(oa_scene_decode_request(&out, bytes, 65) == -1);
    const float invalid[] = {NAN, INFINITY, -INFINITY, -0.1f, 1.01f};
    for (unsigned i = 0; i < sizeof invalid/sizeof *invalid; i++) {
        r.progress = invalid[i];
        assert(oa_scene_encode_request(bytes, 64, &r) == -1 && !memcmp(bytes, original, sizeof bytes));
    }
    r.progress = 1; r.upload = 2; assert(oa_scene_encode_request(bytes, 64, &r) == -1);
    r.upload = 1; r.type = 9; assert(oa_scene_encode_request(bytes, 64, &r) == -1);
    oa_scene_reply reply = {.operation = OA_SCENE_FRAME, .ticket = 1, .revision = 9, .error = EINTR}, result;
    assert(!oa_scene_encode_reply(bytes, 64, &reply));
    assert(!oa_scene_decode_reply(&result, bytes, 64) && result.error == EINTR && result.revision == 9);
    for (unsigned i = 28; i < 64; i++) {
        assert(!oa_scene_encode_reply(bytes, 64, &reply)); bytes[i] = 1;
        oa_scene_reply before = result;
        assert(oa_scene_decode_reply(&result, bytes, 64) == -1 && !memcmp(&before, &result, sizeof result));
    }
    puts("  ok   OGS1 endian/float records, reserved fields, atomic rejection and bounds");
}
static void send_fds(int fd, const uint8_t *bytes, size_t size, int image, unsigned count)
{
    union { struct cmsghdr alignment; uint8_t bytes[CMSG_SPACE(16*sizeof(int))]; } control;
    memset(&control, 0, sizeof control);
    struct iovec vector = {(void *)bytes, size};
    struct msghdr msg = {.msg_iov = &vector, .msg_iovlen = 1};
    if (count) {
        msg.msg_control = control.bytes; msg.msg_controllen = CMSG_SPACE(count*sizeof(int));
        struct cmsghdr *c = CMSG_FIRSTHDR(&msg);
        c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS; c->cmsg_len = CMSG_LEN(count*sizeof(int));
        for (unsigned i = 0; i < count; i++) memcpy(CMSG_DATA(c)+i*sizeof(int), &image, sizeof image);
    }
    assert(sendmsg(fd, &msg, MSG_NOSIGNAL) == (ssize_t)size);
}
static uint8_t *pixels;
static void transport(void)
{
    int baseline = fd_count(), fds[2]; pair(fds);
    int image = oa_scene_image_create(pixels, OA_SCENE_IMAGE_BYTES); assert(image >= 0);
    assert(syscall(SYS_fcntl, image, F_GETFD) & FD_CLOEXEC);
    assert(pwrite(image, "x", 1, 0) == -1 && errno == EPERM);
    assert(ftruncate(image, 0) == -1 && errno == EPERM);
    uint8_t bytes[65] = {0}; int received = -1;
    assert(oa_scene_packet_receive(fds[1], bytes, &received, 1) == -1 && errno == EAGAIN);
    assert(!oa_scene_packet_send(fds[0], bytes, image, 1));
    assert(oa_scene_packet_receive(fds[1], bytes, &received, 0) == 1 && received >= 0);
    assert(syscall(SYS_fcntl, received, F_GETFD) & FD_CLOEXEC);
    assert(!close(received));
    int before = fd_count();
    const unsigned counts[] = {0, 1, 2, 16};
    for (unsigned i = 0; i < 4; i++) {
        send_fds(fds[0], bytes, i < 2 ? 65 : 64, image, counts[i]);
        assert(oa_scene_packet_receive(fds[1], bytes, &received, 0) == -1 && errno == EPROTO);
        assert(fd_count() == before);
    }
    send_fds(fds[0], bytes, 63, image, 1);
    assert(oa_scene_packet_receive(fds[1], bytes, &received, 0) == -1 && errno == EPROTO);
    assert(fd_count() == before);
    assert(!close(image)); assert(!close(fds[0])); assert(!close(fds[1]));
    assert(fd_count() == baseline);
    puts("  ok   immutable sealed images, CLOEXEC transfer, nonblocking reads and rejected-FD cleanup");
}

static struct {
    int opens, renders, finishes, closes, fail_open, fail_render, fail_finish, fail_close;
    int block, entered, release;
    const uint8_t *failed_image, *current_image;
    size_t current_size;
} hw;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
static int open_scene(void **owner, unsigned w, unsigned h)
{
    assert(w == OA_SCENE_WIDTH && h == OA_SCENE_HEIGHT && !*owner);
    hw.opens++; *owner = &hw;
    if (hw.fail_open) { errno = ENOMEM; return -1; }
    return 0;
}
static int render(void *owner, const uint8_t *rgba, size_t size,
                  unsigned x, unsigned y, unsigned w, unsigned h,
                  unsigned type, float progress)
{
    assert(owner == &hw && type <= 8 && progress >= 0 && progress <= 1);
    hw.renders++;
    hw.current_image = rgba;
    hw.current_size = size;
    if (rgba) {
        if (x == 0 && y == 0 && w == OA_SCENE_WIDTH && h == OA_SCENE_HEIGHT) {
            assert(size == OA_SCENE_IMAGE_BYTES);
            assert(rgba[0] == 0x12 && rgba[size-1] == 0x34);
            if (hw.fail_render || hw.fail_finish) hw.failed_image = rgba;
        } else {
            assert(x == 1 && y == 1 && w == 2 && h == 2 && size == 16);
            assert(rgba[0] == 0x5a && rgba[15] == 0x5a);
        }
    } else assert(!size && !x && !y && !w && !h);
    if (hw.fail_render) { errno = EINTR; return -1; }
    return 0;
}
static int finish(void *owner)
{
    assert(owner == &hw); hw.finishes++;
    assert(!pthread_mutex_lock(&mutex)); hw.entered = 1;
    assert(!pthread_cond_broadcast(&condition));
    while (hw.block && !hw.release) assert(!pthread_cond_wait(&condition, &mutex));
    if (hw.current_image) {
        if (hw.current_size == OA_SCENE_IMAGE_BYTES)
            assert(hw.current_image[0] == 0x12 && hw.current_image[OA_SCENE_IMAGE_BYTES-1] == 0x34);
        else
            assert(hw.current_size == 16 && hw.current_image[0] == 0x5a);
    }
    assert(!pthread_mutex_unlock(&mutex));
    if (hw.fail_finish) { errno = EIO; return -1; }
    return 0;
}
static int close_scene(void **owner)
{
    assert(*owner == &hw); hw.closes++;
    if (hw.failed_image) {
        assert(hw.failed_image[0] == 0x12 && hw.failed_image[OA_SCENE_IMAGE_BYTES-1] == 0x34);
    }
    if (hw.fail_close) { hw.fail_close--; errno = EBUSY; return -1; }
    *owner = NULL; return 0;
}
static const oa_scene_backend backend = {open_scene, render, finish, close_scene};
struct server { int fd, result, error; };
static void *serve(void *data)
{
    struct server *s = data;
    s->result = oa_scene_broker_serve(s->fd, &backend); s->error = errno;
    if (s->result) fprintf(stderr, "  broker exit errno=%d\n", s->error);
    assert(!close(s->fd)); return NULL;
}
static void submit(int fd, unsigned op, uint64_t ticket, uint64_t revision, int image)
{
    oa_scene_request r = {.operation = op, .ticket = ticket, .revision = revision,
        .upload = image >= 0};
    if (op == OA_SCENE_OPEN) { r.width = OA_SCENE_WIDTH; r.height = OA_SCENE_HEIGHT; }
    if (op == OA_SCENE_FRAME && image >= 0) {
        r.w = OA_SCENE_WIDTH; r.h = OA_SCENE_HEIGHT;
    }
    uint8_t bytes[64]; assert(!oa_scene_encode_request(bytes, sizeof bytes, &r));
    assert(!oa_scene_packet_send(fd, bytes, image, 0));
}

static void submit_rect(int fd, uint64_t ticket, uint64_t revision, int image,
                        unsigned x, unsigned y, unsigned w, unsigned h)
{
    oa_scene_request r = {.operation = OA_SCENE_FRAME, .ticket = ticket, .revision = revision,
        .upload = 1, .x = x, .y = y, .w = w, .h = h};
    uint8_t bytes[64]; assert(!oa_scene_encode_request(bytes, sizeof bytes, &r));
    assert(!oa_scene_packet_send(fd, bytes, image, 0));
}
static int reply(int fd, unsigned op, uint64_t ticket, uint64_t revision)
{
    struct pollfd watch = {.fd = fd, .events = POLLIN};
    assert(poll(&watch, 1, 5000) == 1 && (watch.revents & POLLIN));
    uint8_t bytes[64]; int image; oa_scene_reply r;
    assert(oa_scene_packet_receive(fd, bytes, &image, 0) == 1 && image == -1);
    assert(!oa_scene_decode_reply(&r, bytes, sizeof bytes));
    assert(r.operation == op && r.ticket == ticket && r.revision == revision); return r.error;
}
static void sessions(void)
{
    int baseline = fd_count(), fds[2]; pair(fds); memset(&hw, 0, sizeof hw); hw.block = 1;
    struct server s = {.fd = fds[1]}; pthread_t thread;
    assert(!pthread_create(&thread, NULL, serve, &s));
    submit(fds[0], OA_SCENE_OPEN, 1, 0, -1); assert(!reply(fds[0], OA_SCENE_OPEN, 1, 0));
    int image = oa_scene_image_create(pixels, OA_SCENE_IMAGE_BYTES); assert(image >= 0);
    submit(fds[0], OA_SCENE_FRAME, 2, 7, image); assert(!close(image));
    assert(!pthread_mutex_lock(&mutex));
    while (!hw.entered) assert(!pthread_cond_wait(&condition, &mutex));
    uint8_t bytes[64]; int received;
    assert(oa_scene_packet_receive(fds[0], bytes, &received, 1) == -1 && errno == EAGAIN);
    pixels[0] = 0xff; /* sealed snapshot already transferred */
    hw.release = 1; assert(!pthread_cond_broadcast(&condition)); assert(!pthread_mutex_unlock(&mutex));
    assert(!reply(fds[0], OA_SCENE_FRAME, 2, 7)); pixels[0] = 0x12;
    submit(fds[0], OA_SCENE_FRAME, 3, 7, -1); assert(!reply(fds[0], OA_SCENE_FRAME, 3, 7));
    unsigned char part[16];
    memset(part, 0x5a, sizeof part);
    int partial = oa_scene_image_create(part, sizeof part); assert(partial >= 0);
    submit_rect(fds[0], 4, 8, partial, 1, 1, 2, 2); assert(!close(partial));
    assert(!reply(fds[0], OA_SCENE_FRAME, 4, 8));
    submit(fds[0], OA_SCENE_CLOSE, 5, 0, -1); assert(!reply(fds[0], OA_SCENE_CLOSE, 5, 0));
    assert(!close(fds[0])); assert(!pthread_join(thread, NULL) && !s.result);
    assert(hw.opens == 1 && hw.renders == 3 && hw.finishes == 3 && hw.closes == 1);
    assert(fd_count() == baseline);
    puts("  ok   upload/cached revisions and completion held until GPU finish; caller FD released safely");
}
static void failures(void)
{
    for (unsigned mode = 0; mode < 9; mode++) {
        int baseline = fd_count(), fds[2]; pair(fds); memset(&hw, 0, sizeof hw);
        hw.fail_open = mode == 0; hw.fail_render = mode == 1; hw.fail_finish = mode == 2;
        hw.fail_close = 2;
        struct server s = {.fd = fds[1]}; pthread_t thread;
        assert(!pthread_create(&thread, NULL, serve, &s));
        submit(fds[0], OA_SCENE_OPEN, 1, 0, -1);
        assert(reply(fds[0], OA_SCENE_OPEN, 1, 0) == (mode == 0 ? ENOMEM : 0));
        if (mode == 1 || mode == 2 || mode == 6 || mode == 8) {
            int image = mode == 6 ? open("/dev/zero", O_RDONLY | O_CLOEXEC) :
                mode == 8 ? (int)syscall(SYS_memfd_create, "unsealed-test", 3) :
                oa_scene_image_create(pixels, OA_SCENE_IMAGE_BYTES);
            if (mode == 8) assert(image >= 0 && !ftruncate(image, OA_SCENE_IMAGE_BYTES));
            assert(image >= 0); submit(fds[0], OA_SCENE_FRAME, 2, 7, image); assert(!close(image));
            assert(reply(fds[0], OA_SCENE_FRAME, 2, 7) == (mode == 1 ? EINTR : mode == 2 ? EIO : EPROTO));
        } else if (mode == 3) {
            submit(fds[0], OA_SCENE_CLOSE, 2, 0, -1); assert(reply(fds[0], OA_SCENE_CLOSE, 2, 0) == EBUSY);
            submit(fds[0], OA_SCENE_CLOSE, 3, 0, -1); assert(reply(fds[0], OA_SCENE_CLOSE, 3, 0) == EBUSY);
            submit(fds[0], OA_SCENE_CLOSE, 4, 0, -1); assert(!reply(fds[0], OA_SCENE_CLOSE, 4, 0));
        } else if (mode == 4) {
            submit(fds[0], OA_SCENE_FRAME, 2, 7, -1); assert(reply(fds[0], OA_SCENE_FRAME, 2, 7) == ENODATA);
        } else if (mode == 5) {
            submit(fds[0], OA_SCENE_CLOSE, 1, 0, -1); /* replayed ticket is fatal */
        }
        assert(!close(fds[0])); assert(!pthread_join(thread, NULL));
        assert((mode == 3 ? s.result == 0 : s.result == -1) && hw.closes == 3);
        if (mode == 1) assert(hw.renders == 1 && !hw.finishes && s.error == EINTR);
        if (mode == 2) assert(hw.renders == 1 && hw.finishes == 1 && s.error == EIO);
        if (mode >= 4) assert(!hw.renders);
        assert(fd_count() == baseline);
    }
    int fds[2]; assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    assert(oa_scene_broker_serve(fds[1], &backend) == -1 && errno == EPROTOTYPE);
    assert(!close(fds[0])); assert(!close(fds[1]));
    puts("  ok   partial open/render/finish failure retains owner and image across failed close; no replay");
}
static void timed_out(int signal_number)
{ (void)signal_number; _exit(124); }
int main(void)
{
    assert(signal(SIGALRM, timed_out) != SIG_ERR);
    assert(!setvbuf(stdout, NULL, _IONBF, 0));
    alarm(30); pixels = calloc(1, OA_SCENE_IMAGE_BYTES); assert(pixels);
    pixels[0] = 0x12; pixels[OA_SCENE_IMAGE_BYTES-1] = 0x34;
    codecs(); puts("  running transport"); transport();
    puts("  running sessions"); sessions(); puts("  running failures"); failures(); free(pixels); return 0;
}
