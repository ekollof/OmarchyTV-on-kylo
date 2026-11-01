#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include "host.h"

static int mode, report_fd = -1, reported;
static unsigned sends, partials, again;
ssize_t __real_send(int fd, const void *buf, size_t len, int flags);
int __real_accept4(int fd, struct sockaddr *addr, socklen_t *len, int flags);

int __wrap_accept4(int fd, struct sockaddr *addr, socklen_t *len, int flags)
{
    assert((flags & (SOCK_NONBLOCK | SOCK_CLOEXEC)) == (SOCK_NONBLOCK | SOCK_CLOEXEC));
    int c = __real_accept4(fd, addr, len, flags);
    if (c >= 0) {
        int size = 4096;
        assert(setsockopt(c, SOL_SOCKET, SO_SNDBUF, &size, sizeof size) == 0);
    }
    return c;
}

ssize_t __wrap_send(int fd, const void *buf, size_t len, int flags)
{
    assert(flags & MSG_NOSIGNAL);
    assert(flags & MSG_DONTWAIT);
#ifdef SYS_fcntl64
    const long fcntl_call = SYS_fcntl64;
#else
    const long fcntl_call = SYS_fcntl;
#endif
    /* GCC's libc headers otherwise select a GLIBC_2.28 fcntl wrapper, newer
     * than the target. These flag-only kernel requests need no libc wrapper. */
    long status_flags = syscall(fcntl_call, fd, F_GETFL);
    long fd_flags = syscall(fcntl_call, fd, F_GETFD);
    assert(status_flags >= 0 && (status_flags & O_NONBLOCK));
    assert(fd_flags >= 0 && (fd_flags & FD_CLOEXEC));
    sends++;
    if (mode == 2 && sends == 1) { errno = EPIPE; return -1; }
    if (mode == 1) {
        if (sends == 1) { errno = EINTR; return -1; }
        if (sends % 7 == 2) { again++; errno = EAGAIN; return -1; }
        if (len > 37) { len = 37; partials++; }
    }
    ssize_t n = __real_send(fd, buf, len, flags);
    if (n < 0 && errno == EAGAIN && report_fd >= 0 && !reported) {
        reported = 1;
        assert(write(report_fd, "B", 1) == 1);
    }
    return n;
}

static void noop(void *v) { (void)v; }
static int start(void *v) { (void)v; return 0; }
static const oa_module descriptor = { .version = "test", .destroy = noop, .stop = noop, .start = start };
static void seed(oa_host *h)
{
    for (int i = 0; i < MAX_MODULES; i++) {
        oa_mod *m = &h->mods[i];
        snprintf(m->name, sizeof m->name, "module_%02d", i);
        memset(m->path, 'p', sizeof m->path - 1);
        m->state = OA_ST_LOADED;
        m->id = (uint32_t)i + 1;
        m->desc = &descriptor;
    }
}

static unsigned watches(oa_host *h)
{
    unsigned n = 0;
    for (int i = 0; i < MAX_WATCHES; i++) n += h->watches[i].active != 0;
    return n;
}

struct server { pid_t pid; int report; char dir[64], path[108]; };
static struct server start_server(int test_mode)
{
    struct server s = { .dir = "/tmp/oa-transport-XXXXXX" };
    assert(mkdtemp(s.dir));
    snprintf(s.path, sizeof s.path, "%s/socket", s.dir);
    int ready[2], report[2];
    assert(pipe(ready) == 0 && pipe(report) == 0);
    s.pid = fork();
    assert(s.pid >= 0);
    if (!s.pid) {
        alarm(15);
        signal(SIGPIPE, SIG_DFL); /* no process-wide ignore hiding unsafe sends */
        close(ready[0]); close(report[0]);
        report_fd = report[1]; mode = test_mode;
        oa_host *h = calloc(1, sizeof *h);
        assert(h && oa_host_init(h, ".", NULL) == 0);
        h->log_level = OA_LOG_ERROR;
        seed(h);
        int nullfd = open("/dev/null", O_RDONLY);
        assert(nullfd >= 0 && dup2(nullfd, 0) == 0);
        close(nullfd);
        assert(oa_control_start(h, s.path) == 0);
        assert(write(ready[1], "R", 1) == 1);
        close(ready[1]);
        oa_host_run(h);
        assert(watches(h) == 0);
        if (mode == 1) assert(partials && again);
        close(h->selfpipe[0]); close(h->selfpipe[1]);
        pthread_mutex_destroy(&h->post_mu);
        free(h);
        close(report_fd);
        _exit(0);
    }
    close(ready[1]); close(report[1]);
    s.report = report[0];
    char byte;
    assert(read(ready[0], &byte, 1) == 1 && byte == 'R');
    close(ready[0]);
    return s;
}

static int connect_to(const struct server *s)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strcpy(addr.sun_path, s->path);
    assert(fd >= 0 && connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0);
    return fd;
}
static void write_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    while (len) {
        ssize_t n = write(fd, p, len);
        assert(n > 0);
        p += n; len -= (size_t)n;
    }
}
static void expect(int fd, const char *text)
{
    char buf[16384];
    size_t len = strlen(text), done = 0;
    assert(len <= sizeof buf);
    while (done < len) {
        ssize_t n = read(fd, buf + done, len - done);
        assert(n > 0);
        done += (size_t)n;
    }
    assert(!memcmp(buf, text, len));
}
static void stop_server(struct server *s)
{
    int fd = connect_to(s);
    write_all(fd, "quit\n", 5);
    expect(fd, "bye\n");
    close(fd);
    int status;
    assert(waitpid(s->pid, &status, 0) == s->pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(access(s->path, F_OK) != 0 && rmdir(s->dir) == 0);
    close(s->report);
}

static void cleanup_cases(void)
{
    oa_host *h = calloc(1, sizeof *h);
    assert(h && oa_host_init(h, ".", NULL) == 0);
    h->log_level = OA_LOG_ERROR;
    char dir[] = "/tmp/oa-ctl-clean-XXXXXX", path[108];
    assert(mkdtemp(dir));
    snprintf(path, sizeof path, "%s/socket", dir);
    for (int i = 0; i < 3; i++) {
        assert(oa_control_start(h, path) == 0);
        assert(watches(h) == 2 && oa_control_start(h, path) == -1);
        oa_control_stop(h); oa_control_stop(h);
        assert(watches(h) == 0 && access(path, F_OK) != 0);
    }
    int fd = open(path, O_CREAT | O_WRONLY | O_EXCL, 0600);
    assert(fd >= 0); close(fd);
    assert(oa_control_start(h, path) == -1 && watches(h) == 0);
    assert(access(path, F_OK) == 0 && unlink(path) == 0);
    int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strcpy(addr.sun_path, path);
    assert(listener >= 0 && bind(listener, (struct sockaddr *)&addr, sizeof addr) == 0);
    assert(listen(listener, 1) == 0);
    assert(oa_control_start(h, path) == -1 && watches(h) == 0);
    assert(access(path, F_OK) == 0);
    close(listener); /* same path is now a stale, refused socket */
    assert(oa_control_start(h, path) == 0);
    oa_control_stop(h);
    assert(watches(h) == 0 && access(path, F_OK) != 0);
    assert(oa_control_start(h, path) == 0 && unlink(path) == 0);
    fd = open(path, O_CREAT | O_WRONLY | O_EXCL, 0600);
    assert(fd >= 0); close(fd);
    oa_control_stop(h);
    assert(access(path, F_OK) == 0 && unlink(path) == 0);
    for (int i = 0; i < MAX_WATCHES; i++)
        assert(h->api.watch_add(h, h->selfpipe[0], OA_WATCH_READ, NULL, NULL));
    assert(oa_control_start(h, path) == -1 && watches(h) == MAX_WATCHES);
    assert(access(path, F_OK) != 0);
    h->watches[MAX_WATCHES - 1].active = 0;
    assert(oa_control_start(h, path) == -1 && watches(h) == MAX_WATCHES - 1);
    assert(access(path, F_OK) != 0);
    assert(rmdir(dir) == 0);
    close(h->selfpipe[0]); close(h->selfpipe[1]);
    pthread_mutex_destroy(&h->post_mu); free(h);
    puts("  ok   repeated stop/start, failed watch registration and socket identity cleanup");
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    alarm(30);
    oa_host *template = calloc(1, sizeof *template);
    assert(template);
    seed(template);
    char text[] = "list", list[16384], loaded[1024], started[1024];
    assert(oa_control_command(template, text, list, sizeof list) == 0);
    assert(strstr(list, "module_31"));
    char status[] = "status module_00";
    assert(oa_control_command(template, status, loaded, sizeof loaded) == 0);
    template->mods[0].state = OA_ST_STARTED;
    strcpy(status, "status module_00");
    assert(oa_control_command(template, status, started, sizeof started) == 0);
    free(template);

    struct server s = start_server(1);
    int fd = connect_to(&s);
    write_all(fd, "li", 2); write_all(fd, "st\nlist\n", 8);
    assert(shutdown(fd, SHUT_WR) == 0);
    expect(fd, list); expect(fd, list);
    char byte;
    assert(read(fd, &byte, 1) == 0);
    close(fd); stop_server(&s);
    puts("  ok   short writes, EINTR/EAGAIN, split/pipelined requests and half-close drain");

    s = start_server(0);
    fd = connect_to(&s);
    char flood[1000];
    for (int i = 0; i < 200; i++) memcpy(flood + i * 5, "list\n", 5);
    write_all(fd, flood, sizeof flood);
    write_all(fd, "start module_00\n", 16);
    assert(read(s.report, &byte, 1) == 1 && byte == 'B');
    int healthy = connect_to(&s);
    write_all(healthy, "loglevel 0\n", 11); expect(healthy, "loglevel 0\n");
    write_all(healthy, "status module_00\n", 17); expect(healthy, loaded);
    for (int i = 0; i < 200; i++) expect(fd, list);
    expect(fd, "ok\n");
    write_all(healthy, "status module_00\n", 17); expect(healthy, started);
    close(fd); close(healthy); stop_server(&s);
    puts("  ok   stalled reader cannot block another client; queued replies drain in order");

    s = start_server(2);
    fd = connect_to(&s);
    write_all(fd, "help\n", 5);
    assert(read(fd, &byte, 1) == 0); close(fd);
    for (int i = 0; i < 32; i++) {
        fd = connect_to(&s); write_all(fd, "help\n", 5); close(fd);
    }
    stop_server(&s);
    puts("  ok   EPIPE and disconnected readers do not raise SIGPIPE or kill host");

    s = start_server(0);
    fd = connect_to(&s);
    char bad[517]; memset(bad, 'x', 512); memcpy(bad + 512, "quit\n", 5);
    write_all(fd, bad, sizeof bad);
    static const char nul_line[] = "quit\0ignored\nloglevel 0\n";
    write_all(fd, nul_line, sizeof nul_line - 1);
    const char *error = "error: command too long or contains NUL\n";
    expect(fd, error); expect(fd, error); expect(fd, "loglevel 0\n");
    char boundary[512]; memset(boundary, ' ', sizeof boundary);
    memcpy(boundary, "loglevel 0", 10); boundary[511] = '\n';
    write_all(fd, boundary, sizeof boundary); expect(fd, "loglevel 0\n");
    close(fd); stop_server(&s);
    puts("  ok   overlong/NUL lines rejected whole; next valid and 511-byte commands work");
    cleanup_cases();
    return 0;
}
