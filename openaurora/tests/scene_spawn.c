#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include "openaurora/scene_broker.h"
#define require(expression) do { if (!(expression)) { \
    fprintf(stderr, "%s:%d: %s errno=%d\n", __FILE__, __LINE__, #expression, errno); exit(1); } } while (0)
extern char **environ;
static void timeout_handler(int sig) { (void)sig; _exit(124); }
static void exchange(int fd, oa_scene_request request, int image)
{
    uint8_t bytes[64];
    require(!oa_scene_encode_request(bytes, sizeof bytes, &request));
    require(!oa_scene_packet_send(fd, bytes, image, 1));
    if (image >= 0) require(!close(image));
    struct pollfd watch = {.fd = fd, .events = POLLIN};
    require(poll(&watch, 1, 5000) == 1 && (watch.revents & POLLIN));
    int received; oa_scene_reply reply;
    require(oa_scene_packet_receive(fd, bytes, &received, 1) == 1 && received == -1);
    require(!oa_scene_decode_reply(&reply, bytes, sizeof bytes));
    require(reply.operation == request.operation && reply.ticket == request.ticket &&
        reply.revision == request.revision && !reply.error);
}
int main(int argc, char **argv)
{
    require(argc == 4 && signal(SIGALRM, timeout_handler) != SIG_ERR); alarm(20);
    char *helper = realpath(argv[1], NULL), *renderer = realpath(argv[2], NULL), *bad = realpath(argv[3], NULL);
    require(helper && renderer && bad);
    for (unsigned mode = 0; mode < 4; mode++) {
        int fds[2]; require(!socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds));
        int null = open("/dev/null", O_RDONLY | O_CLOEXEC); require(null >= 0);
        int spare = (int)syscall(SYS_fcntl, null, F_DUPFD, 128); require(spare == 128);
        posix_spawn_file_actions_t actions; require(!posix_spawn_file_actions_init(&actions));
        require(!posix_spawn_file_actions_addclose(&actions, fds[0]));
        require(!posix_spawn_file_actions_adddup2(&actions, fds[1], STDIN_FILENO));
        require(!posix_spawn_file_actions_addclose(&actions, fds[1]));
        require(!posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0));
        require(!posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0));
        char *args[] = {helper, "--serve", mode == 2 ? bad : mode == 3 ? "/nonexistent/oa-renderer.so" : renderer, NULL};
        pid_t child; require(!posix_spawn(&child, helper, &actions, NULL, args, environ));
        require(!posix_spawn_file_actions_destroy(&actions)); require(!close(fds[1]));
        require(!close(null)); require(!close(spare));
        if (mode < 2) {
            exchange(fds[0], (oa_scene_request){.operation = OA_SCENE_OPEN, .ticket = 1,
                .width = OA_SCENE_WIDTH, .height = OA_SCENE_HEIGHT}, -1);
            char path[96]; snprintf(path, sizeof path, "/proc/%ld/fd/128", (long)child);
            require(access(path, F_OK) == -1 && errno == ENOENT);
            uint8_t *pixels = calloc(1, OA_SCENE_IMAGE_BYTES); require(pixels); pixels[0] = 42;
            int image = oa_scene_image_create(pixels, OA_SCENE_IMAGE_BYTES); require(image >= 0); free(pixels);
            exchange(fds[0], (oa_scene_request){.operation = OA_SCENE_FRAME, .ticket = 2,
                .revision = 1, .type = 4, .progress = 0.5f, .upload = 1,
                .w = OA_SCENE_WIDTH, .h = OA_SCENE_HEIGHT}, image);
            exchange(fds[0], (oa_scene_request){.operation = OA_SCENE_FRAME, .ticket = 3,
                .revision = 1, .type = 4, .progress = 0.5f}, -1);
            if (!mode) exchange(fds[0], (oa_scene_request){.operation = OA_SCENE_CLOSE, .ticket = 4}, -1);
        }
        require(!close(fds[0]));
        int status; require(waitpid(child, &status, 0) == child && WIFEXITED(status));
        require(WEXITSTATUS(status) == (mode < 2 ? 0 : 1));
    }
    free(helper); free(renderer); free(bad);
    puts("  ok   bootstrap closes inherited fds before DSO init; constructor fd survives, close/EOF drain and reap");
    puts("  ok   missing renderer and wrong DSO ABI exit/reap without opening a renderer owner");
    return 0;
}
