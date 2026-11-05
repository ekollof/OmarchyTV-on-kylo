/* Minimal Unix control-socket client for live/staged acceptance runs.
 *
 *   oa-client SOCKET COMMAND [COMMAND...]
 *
 * Sends each command as one line, half-closes the socket, then prints the
 * replies until EOF. No device access.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s SOCKET COMMAND [COMMAND...]\n", argv[0]);
        return 2;
    }
    if (strlen(argv[1]) >= sizeof(((struct sockaddr_un *)0)->sun_path)) {
        fprintf(stderr, "socket path too long\n");
        return 2;
    }
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, argv[1], sizeof addr.sun_path - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }
    for (int i = 2; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (write(fd, argv[i], len) != (ssize_t)len ||
            write(fd, "\n", 1) != 1) {
            perror("write");
            close(fd);
            return 1;
        }
    }
    shutdown(fd, SHUT_WR);
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof buf)) > 0)
        if (fwrite(buf, 1, (size_t)n, stdout) != (size_t)n) {
            close(fd);
            return 1;
        }
    close(fd);
    return n < 0 ? 1 : 0;
}
