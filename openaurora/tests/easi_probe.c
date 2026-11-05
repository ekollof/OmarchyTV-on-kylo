/* Local fixture for the read-only testtool probe. Never touches a TV. */
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static const char xml[] =
    "<active_list network='Cable' count='2' total_size='2'>"
    "<service rsn='1' name='Alpha' handle='10' onid='20' tsid='30' sid='40' type='1'/>"
    "<service rsn='2' name='Beta' handle='11' onid='20' tsid='30' sid='41' type='1'/>"
    "</active_list>\n";

static int listener(unsigned short *port)
{
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    assert(fd >= 0);
    int one = 1;
    assert(!setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(!bind(fd, (struct sockaddr *)&addr, sizeof addr));
    assert(!listen(fd, 1));
    socklen_t length = sizeof addr;
    assert(!getsockname(fd, (struct sockaddr *)&addr, &length));
    *port = ntohs(addr.sin_port);
    return fd;
}

static void serve(int fd)
{
    alarm(5);
    int client = accept(fd, NULL, NULL);
    if (client < 0) _exit(1);
    char line[64];
    ssize_t n = read(client, line, sizeof line - 1);
    if (n <= 0) _exit(1);
    line[n] = '\0';
    if (strcmp(line, "ACTIVECHANNELLIST\n")) _exit(1);
    if (write(client, xml, sizeof xml - 1) != (ssize_t)(sizeof xml - 1)) _exit(1);
    close(client);
    close(fd);
    _exit(0);
}

static void run_probe(const char *probe, const char *port, char *output,
    size_t cap, int *status)
{
    int pipefd[2];
    assert(!pipe(pipefd));
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execl(probe, probe, "--host", "127.0.0.1", "--port", port,
            "--timeout", "2000", NULL);
        _exit(127);
    }
    close(pipefd[1]);
    size_t used = 0;
    while (used + 1 < cap) {
        ssize_t n = read(pipefd[0], output + used, cap - used - 1);
        if (n <= 0) break;
        used += (size_t)n;
    }
    output[used] = '\0';
    close(pipefd[0]);
    assert(waitpid(child, status, 0) == child);
    assert(WIFEXITED(*status));
    *status = WEXITSTATUS(*status);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    unsigned short port;
    int fd = listener(&port);
    pid_t server = fork();
    assert(server >= 0);
    if (!server) serve(fd);
    close(fd);
    char port_text[16];
    snprintf(port_text, sizeof port_text, "%u", port);
    char output[4096];
    int status = -1;
    run_probe(argv[1], port_text, output, sizeof output, &status);
    int server_status = 0;
    assert(waitpid(server, &server_status, 0) == server);
    assert(WIFEXITED(server_status) && !WEXITSTATUS(server_status));
    assert(status == 0);
    assert(strstr(output, "active list network='Cable' count='2'"));
    assert(strstr(output, "rsn=1 name='Alpha'"));
    assert(strstr(output, "rsn=2 name='Beta'"));

    char refused[1024];
    int pipefd[2];
    assert(!pipe(pipefd));
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execl(argv[1], argv[1], "--command", "SETCHANNEL", NULL);
        _exit(127);
    }
    close(pipefd[1]);
    size_t used = 0;
    while (used + 1 < sizeof refused) {
        ssize_t n = read(pipefd[0], refused + used, sizeof refused - used - 1);
        if (n <= 0) break;
        used += (size_t)n;
    }
    refused[used] = '\0';
    close(pipefd[0]);
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 2);
    assert(strstr(refused, "refusing non-read command"));
    puts("ok easi probe: read-only ACTIVECHANNELLIST fixture and setter refusal");
    return 0;
}
