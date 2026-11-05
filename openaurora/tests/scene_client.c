#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "openaurora/scene_client.h"
#define require(expression) do { if (!(expression)) { \
    fprintf(stderr, "%s:%d: %s errno=%d\n", __FILE__, __LINE__, #expression, errno); exit(1); } } while (0)
static int fail_send, send_calls;
ssize_t __real_sendmsg(int fd, const struct msghdr *msg, int flags);
ssize_t __wrap_sendmsg(int fd, const struct msghdr *msg, int flags)
{
    send_calls++;
    if (fail_send) { errno = fail_send; fail_send = 0; return -1; }
    return __real_sendmsg(fd, msg, flags);
}
static void timeout_handler(int sig) { (void)sig; _exit(124); }
static void pause_tick(void)
{ struct timespec delay = {.tv_nsec = 1000000}; (void)nanosleep(&delay, NULL); }
static oa_scene_request opening(void)
{ return (oa_scene_request){.operation = OA_SCENE_OPEN, .width = OA_SCENE_WIDTH, .height = OA_SCENE_HEIGHT}; }
static oa_scene_request frame(int upload)
{
    return (oa_scene_request){.operation = OA_SCENE_FRAME, .revision = 7, .type = 4,
        .progress = 0.5f, .upload = (unsigned)upload,
        .w = upload ? OA_SCENE_WIDTH : 0u, .h = upload ? OA_SCENE_HEIGHT : 0u};
}
static oa_scene_reply completed(oa_scene_client *c, uint64_t ticket)
{
    oa_scene_reply reply;
    for (unsigned i = 0; i < 5000; i++) {
        (void)oa_scene_client_pump(c);
        int result = oa_scene_client_take(c, ticket, &reply);
        require(result >= 0); if (result) return reply;
        pause_tick();
    }
    require(0); return (oa_scene_reply){0};
}
static int drained(oa_scene_client **c)
{
    for (unsigned i = 0; i < 5000; i++) {
        int result = oa_scene_client_shutdown(*c); require(result >= 0);
        if (result) {
            int status; require(!oa_scene_client_status(*c, &status));
            require(!oa_scene_client_destroy(c) && !*c); return status;
        }
        pause_tick();
    }
    require(0); return 0;
}
static int image(void)
{
    uint8_t *pixels = calloc(1, OA_SCENE_IMAGE_BYTES); require(pixels); pixels[0] = 42;
    int fd = oa_scene_image_create(pixels, OA_SCENE_IMAGE_BYTES); free(pixels); require(fd >= 0); return fd;
}
static oa_scene_client *spawned(const char *helper, const char *renderer)
{ oa_scene_client *c = NULL; require(!oa_scene_client_spawn(&c, helper, renderer)); return c; }
static uint64_t opened(oa_scene_client *c)
{
    oa_scene_request r = opening(); uint64_t ticket;
    require(!oa_scene_client_submit(c, &r, -1, &ticket));
    oa_scene_reply reply = completed(c, ticket); require(!reply.error); return ticket;
}
static int connect_gate(const char *path)
{
    for (unsigned i = 0; i < 5000; i++) {
        int fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC); if (fd >= 0) return fd;
        require(errno == ENXIO); pause_tick();
    }
    require(0); return -1;
}
static void sessions(const char *helper, const char *renderer)
{
    oa_scene_client *c = spawned(helper, renderer); oa_scene_request r = opening();
    uint64_t ticket, unchanged = 999;
    require(!oa_scene_client_submit(c, &r, -1, &ticket));
    require(oa_scene_client_submit(c, &r, -1, &unchanged) == -1 && errno == EBUSY && unchanged == 999);
    oa_scene_reply reply;
    require(oa_scene_client_take(c, ticket+1, &reply) == -1 && errno == ENOENT);
    for (unsigned i = 0; i < 100; i++) { (void)oa_scene_client_pump(c); pause_tick(); }
    require(oa_scene_client_submit(c, &r, -1, &unchanged) == -1 && errno == EBUSY);
    require(oa_scene_client_take(c, ticket, &reply) == 1 && !reply.error);
    r = frame(1); int fd = image();
    require(!oa_scene_client_submit(c, &r, fd, &ticket)); require(!close(fd));
    reply = completed(c, ticket); require(!reply.error && reply.revision == 7);
    require(oa_scene_client_submit(c, &r, -1, &unchanged) == -1 && errno == EINVAL);
    r = frame(0); r.revision = 6;
    require(oa_scene_client_submit(c, &r, -1, &unchanged) == -1 && errno == EINVAL);
    r.revision = 7; require(!oa_scene_client_submit(c, &r, -1, &ticket)); require(!completed(c, ticket).error);
    pid_t forked = fork(); require(forked >= 0);
    if (!forked) _exit(oa_scene_client_pump(c) == -1 && errno == EPERM ? 0 : 1);
    int status; require(waitpid(forked, &status, 0) == forked && WIFEXITED(status) && !WEXITSTATUS(status));
    r = (oa_scene_request){.operation = OA_SCENE_CLOSE};
    require(!oa_scene_client_submit(c, &r, -1, &ticket)); require(!completed(c, ticket).error);
    require(oa_scene_client_submit(c, &r, -1, &unchanged) == -1 && errno == ESHUTDOWN);
    status = drained(&c); require(WIFEXITED(status) && !WEXITSTATUS(status));
    c = spawned(helper, renderer); uint64_t next = opened(c);
    require((next >> 32) != (ticket >> 32));
    require(oa_scene_client_take(c, ticket, &reply) == -1 && errno == ENOENT);
    status = drained(&c); require(WIFEXITED(status) && !WEXITSTATUS(status));
    puts("  ok   bounded pending/result slot, upload/cache revisions, scoped tickets, foreign-fork rejection and CLOSE");
}
static void gates(const char *helper, const char *renderer, const char *path)
{
    require(!mkfifo(path, 0600));
    require(!setenv("OA_SCENE_FIXTURE_CLOSE_GATE", path, 1));
    oa_scene_client *c = spawned(helper, renderer); (void)opened(c);
    require(oa_scene_client_shutdown(c) == 0);
    int writer = connect_gate(path), status;
    for (unsigned i = 0; i < 20; i++) {
        require(oa_scene_client_shutdown(c) == 0);
        require(oa_scene_client_destroy(&c) == -1 && errno == EBUSY && c);
        require(oa_scene_client_status(c, &status) == -1 && errno == EAGAIN);
        pause_tick();
    }
    require(write(writer, "x", 1) == 1 && !close(writer));
    status = drained(&c); require(WIFEXITED(status) && !WEXITSTATUS(status));
    require(!unsetenv("OA_SCENE_FIXTURE_CLOSE_GATE"));
    require(!setenv("OA_SCENE_FIXTURE_FINISH_GATE", path, 1));
    c = spawned(helper, renderer); (void)opened(c);
    oa_scene_request r = frame(1); int fd = image(); uint64_t ticket;
    require(!oa_scene_client_submit(c, &r, fd, &ticket)); require(!close(fd));
    writer = connect_gate(path); /* finish is executing, blocked on this writer */
    require(oa_scene_client_shutdown(c) == 0);
    oa_scene_reply reply; require(oa_scene_client_take(c, ticket, &reply) == 1 && reply.error == ECANCELED);
    require(oa_scene_client_shutdown(c) == 0 && oa_scene_client_destroy(&c) == -1 && errno == EBUSY);
    require(write(writer, "x", 1) == 1 && !close(writer));
    status = drained(&c); require(WIFEXITED(status)); /* reply to closed IPC can fail, owner still drains */
    require(!unsetenv("OA_SCENE_FIXTURE_FINISH_GATE")); require(!unlink(path));
    puts("  ok   executing finish and blocked close keep child owned; shutdown/poll/destroy never wait or signal it");
}
static int peer(void)
{
    const char *mode = getenv("OA_SCENE_TEST_PEER"); require(mode);
    uint8_t bytes[64]; int image;
    require(oa_scene_packet_receive(STDIN_FILENO, bytes, &image, 0) == 1 && image == -1);
    oa_scene_request r; require(!oa_scene_decode_request(&r, bytes, sizeof bytes));
    if (!strcmp(mode, "signal")) { (void)kill(getpid(), SIGKILL); _exit(1); }
    oa_scene_reply reply = {.operation = r.operation, .ticket = r.ticket};
    if (!strcmp(mode, "wrong")) reply.ticket++;
    require(!oa_scene_encode_reply(bytes, sizeof bytes, &reply));
    int extra = !strcmp(mode, "fd") ? open("/dev/null", O_RDONLY | O_CLOEXEC) : -1;
    require(!oa_scene_packet_send(STDIN_FILENO, bytes, extra, 0));
    if (extra >= 0) require(!close(extra));
    return 0;
}
static void peers(const char *self, const char *renderer)
{
    const char *modes[] = {"buffered", "wrong", "fd", "signal"};
    for (unsigned i = 0; i < 4; i++) {
        require(!setenv("OA_SCENE_TEST_PEER", modes[i], 1));
        oa_scene_client *c = spawned(self, renderer); oa_scene_request r = opening(); uint64_t ticket;
        require(!oa_scene_client_submit(c, &r, -1, &ticket));
        oa_scene_reply reply = completed(c, ticket);
        require(reply.error == (i == 0 ? 0 : i == 3 ? ECONNRESET : EPROTO));
        for (unsigned n = 0; n < 10; n++) { (void)oa_scene_client_pump(c); pause_tick(); }
        int status = drained(&c);
        require(i == 3 ? WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL : WIFEXITED(status));
    }
    require(!unsetenv("OA_SCENE_TEST_PEER"));
    puts("  ok   buffered completion survives exit; mismatched/descriptor replies fail and abnormal exit remains visible");
}
static void sends(const char *helper, const char *renderer)
{
    oa_scene_client *c = spawned(helper, renderer); oa_scene_request r = opening(); uint64_t ticket = 999;
    int calls = send_calls; fail_send = EAGAIN;
    require(oa_scene_client_submit(c, &r, -1, &ticket) == -1 && errno == EAGAIN && ticket == 999);
    require(send_calls == calls+1 && oa_scene_client_fd(c) >= 0);
    require(!oa_scene_client_submit(c, &r, -1, &ticket)); require(!completed(c, ticket).error);
    int status = drained(&c); require(WIFEXITED(status) && !WEXITSTATUS(status));
    c = spawned(helper, renderer); calls = send_calls; ticket = 999; fail_send = EINTR;
    require(oa_scene_client_submit(c, &r, -1, &ticket) == -1 && errno == EINTR && ticket == 999);
    require(send_calls == calls+1 && oa_scene_client_fd(c) == -1);
    status = drained(&c); require(WIFEXITED(status));
    puts("  ok   EAGAIN leaves request unaccepted/retryable; interrupted send breaks channel without replay");
}
static void exec_failure(const char *directory, const char *renderer)
{
    char path[160]; require(snprintf(path, sizeof path, "%s/bad-exec", directory) > 0);
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0700); require(fd >= 0);
    require(write(fd, "invalid executable\n", 19) == 19 && !close(fd));
    oa_scene_client *c = NULL;
    if (oa_scene_client_spawn(&c, path, renderer)) require(errno == ENOEXEC && !c);
    else {
        oa_scene_request r = opening(); uint64_t ticket = 999;
        if (!oa_scene_client_submit(c, &r, -1, &ticket)) require(completed(c, ticket).error != 0);
        else require(ticket == 999);
        int status = drained(&c); require(WIFEXITED(status) && WEXITSTATUS(status) == 127);
    }
    require(!unlink(path));
    puts("  ok   loader failure rejected immediately or completed through EOF and child127 reap on target libc");
}
int main(int argc, char **argv)
{
    require(signal(SIGALRM, timeout_handler) != SIG_ERR); alarm(30);
    if (argc == 3 && !strcmp(argv[1], "--serve")) return peer();
    require(argc == 3);
    char *helper = realpath(argv[1], NULL), *renderer = realpath(argv[2], NULL), *self = realpath(argv[0], NULL);
    require(helper && renderer && self);
    char directory[] = "/tmp/oa-scene-client-XXXXXX", gate[128]; require(mkdtemp(directory));
    require(snprintf(gate, sizeof gate, "%s/gate", directory) > 0);
    sessions(helper, renderer); gates(helper, renderer, gate); peers(self, renderer); sends(helper, renderer);
    exec_failure(directory, renderer);
    require(!rmdir(directory)); free(helper); free(renderer); free(self); return 0;
}
