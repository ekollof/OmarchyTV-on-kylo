/* Minimal Unix control-socket client for live/staged acceptance runs.
 *
 *   oa-client [--timeout SECONDS] [--no-wait] SOCKET COMMAND [COMMAND...]
 *
 * Sends each command as one line. By default it half-closes the socket, prints
 * replies until EOF, and exits 1 on a bounded timeout. --no-wait queues the
 * commands and exits without reading a reply, which is the async form for a
 * reload that the host may finish later.
 *
 * No device access.
 */
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

static int wait_fd(int fd, short events, int timeout_s)
{
    struct pollfd p = { .fd = fd, .events = events };
    for (;;) {
        int rc = poll(&p, 1, timeout_s * 1000);
        if (rc > 0) return 1;
        if (rc == 0) return 0;
        if (errno != EINTR) return -1;
    }
}

static int write_all(int fd, const char *buf, size_t len, int timeout_s)
{
    while (len) {
        ssize_t n = write(fd, buf, len);
        if (n > 0) {
            buf += n;
            len -= (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            int ready = wait_fd(fd, POLLOUT, timeout_s);
            if (ready > 0) continue;
            fprintf(stderr, "oa-client: send timeout\n");
            return -1;
        }
        perror("write");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int timeout_s = 5;
    int no_wait = 0;
    int arg = 1;

    while (arg < argc && argv[arg][0] == '-' && argv[arg][1] == '-') {
        if (!strcmp(argv[arg], "--no-wait")) {
            no_wait = 1;
            arg++;
        } else if (!strcmp(argv[arg], "--timeout") && arg + 1 < argc) {
            char *end = NULL;
            long value = strtol(argv[arg + 1], &end, 10);
            if (!end || *end || value < 0 || value > 3600) {
                fprintf(stderr, "oa-client: invalid timeout '%s'\n", argv[arg + 1]);
                return 2;
            }
            timeout_s = (int)value;
            arg += 2;
        } else {
            fprintf(stderr, "usage: %s [--timeout SECONDS] [--no-wait] SOCKET COMMAND [COMMAND...]\n",
                    argv[0]);
            return 2;
        }
    }
    if (argc - arg < 2) {
        fprintf(stderr, "usage: %s [--timeout SECONDS] [--no-wait] SOCKET COMMAND [COMMAND...]\n",
                argv[0]);
        return 2;
    }
    const char *path = argv[arg++];
    if (strlen(path) >= sizeof(((struct sockaddr_un *)0)->sun_path)) {
        fprintf(stderr, "socket path too long\n");
        return 2;
    }
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }
    struct timeval tv = { .tv_sec = timeout_s, .tv_usec = 0 };
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof addr.sun_path - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }
    for (int i = arg; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (write_all(fd, argv[i], len, timeout_s) ||
            write_all(fd, "\n", 1, timeout_s)) {
            close(fd);
            return 1;
        }
    }
    if (no_wait) {
        close(fd);
        return 0;
    }
    if (shutdown(fd, SHUT_WR) < 0) {
        perror("shutdown");
        close(fd);
        return 1;
    }
    char buf[4096];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);
        if (n > 0) {
            if (fwrite(buf, 1, (size_t)n, stdout) != (size_t)n) {
                close(fd);
                return 1;
            }
            continue;
        }
        if (n == 0) break;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stderr, "oa-client: timeout after %ds waiting for a reply\n", timeout_s);
            close(fd);
            return 1;
        }
        perror("read");
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}
