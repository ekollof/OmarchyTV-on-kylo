/*
 * Read-only testtool probe for the vendor active service list.
 *
 * The vendor binds a testtool TCP server on port 1986 when
 * devprofile/TesttoolSocket is enabled. Only the two active-list read
 * commands are accepted here; setters and debug commands are rejected before
 * any socket is opened.
 *
 * ACTIVECHANNELLIST returns:
 *   <active_list network='...' count='N' total_size='...'>
 *     <service rsn='...' name='...' handle='...' onid='...' tsid='...'
 *              sid='...' type='...'/>
 *   </active_list>
 *
 * This is runtime EASI data, not the settingsDB inventory. It is a probe, not
 * a UI provider; no TV state is modified.
 */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_REPLY (256u * 1024u)

static int connect_tcp(const char *host, const char *port, int timeout_ms)
{
    struct addrinfo hints;
    struct addrinfo *list = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int rc = getaddrinfo(host, port, &hints, &list);
    if (rc) {
        fprintf(stderr, "resolve %s:%s: %s\n", host, port, gai_strerror(rc));
        return -1;
    }
    int fd = -1;
    for (struct addrinfo *ai = list; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC, ai->ai_protocol);
        if (fd < 0) continue;
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        rc = connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (rc && errno == EINPROGRESS) {
            struct pollfd p = { .fd = fd, .events = POLLOUT };
            rc = poll(&p, 1, timeout_ms);
            if (rc > 0) {
                int error = 0;
                socklen_t length = sizeof error;
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) == 0 &&
                    !error)
                    rc = 0;
                else
                    errno = error ? error : EIO;
            } else {
                errno = rc == 0 ? ETIMEDOUT : errno;
            }
        }
        if (!rc) {
            if (flags >= 0) (void)fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
            struct timeval tv = {
                .tv_sec = timeout_ms / 1000,
                .tv_usec = (timeout_ms % 1000) * 1000
            };
            (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
            (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(list);
    if (fd < 0) fprintf(stderr, "connect %s:%s: %s\n", host, port, strerror(errno));
    return fd;
}

static int send_all(int fd, const char *data, size_t size)
{
    while (size) {
        ssize_t n = send(fd, data, size, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        data += n;
        size -= (size_t)n;
    }
    return 0;
}

static ssize_t read_reply(int fd, char *buf, size_t cap, const char *until)
{
    size_t n = 0;
    while (n + 1 < cap) {
        ssize_t got = recv(fd, buf + n, cap - n - 1, 0);
        if (got < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return -1;
        }
        if (!got) break;
        n += (size_t)got;
        buf[n] = '\0';
        if (until && *until && strstr(buf, until)) break;
    }
    buf[n] = '\0';
    return (ssize_t)n;
}

static int attr(const char *start, const char *name, char *out, size_t cap)
{
    char pattern[32];
    snprintf(pattern, sizeof pattern, "%s='", name);
    const char *p = strstr(start, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    const char *end = strchr(p, '\'');
    if (!end || (size_t)(end - p) >= cap) return -1;
    memcpy(out, p, (size_t)(end - p));
    out[end - p] = '\0';
    return 0;
}

static int print_active_list(const char *reply)
{
    const char *list = strstr(reply, "<active_list");
    if (!list) {
        fprintf(stderr, "response is not an active_list\n");
        return -1;
    }
    char network[64], count[32];
    if (attr(list, "network", network, sizeof network) ||
        attr(list, "count", count, sizeof count)) {
        fprintf(stderr, "active_list missing network/count\n");
        return -1;
    }
    printf("active list network='%s' count='%s'\n", network, count);
    const char *p = list;
    unsigned rows = 0;
    while ((p = strstr(p, "<service ")) != NULL) {
        char rsn[32], name[256], handle[32], onid[32], tsid[32], sid[32], type[32];
        if (attr(p, "rsn", rsn, sizeof rsn) ||
            attr(p, "name", name, sizeof name) ||
            attr(p, "handle", handle, sizeof handle) ||
            attr(p, "onid", onid, sizeof onid) ||
            attr(p, "tsid", tsid, sizeof tsid) ||
            attr(p, "sid", sid, sizeof sid) ||
            attr(p, "type", type, sizeof type)) {
            fprintf(stderr, "malformed service element\n");
            return -1;
        }
        printf("rsn=%s name='%s' handle=%s onid=%s tsid=%s sid=%s type=%s\n",
            rsn, name, handle, onid, tsid, sid, type);
        rows++;
        p += 9;
    }
    return (int)rows;
}

int main(int argc, char **argv)
{
    const char *host = "127.0.0.1";
    const char *port = "1986";
    const char *command = "ACTIVECHANNELLIST";
    int timeout_ms = 2000;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--host") && i + 1 < argc) host = argv[++i];
        else if (!strcmp(argv[i], "--port") && i + 1 < argc) port = argv[++i];
        else if (!strcmp(argv[i], "--command") && i + 1 < argc) command = argv[++i];
        else if (!strcmp(argv[i], "--timeout") && i + 1 < argc) {
            timeout_ms = atoi(argv[++i]);
            if (timeout_ms <= 0) timeout_ms = 2000;
        } else {
            fprintf(stderr,
                "usage: %s [--host HOST] [--port PORT] [--command CMD] [--timeout MS]\n"
                "read-only commands: ACTIVECHANNELLIST, GETCHANNELLIST\n", argv[0]);
            return 2;
        }
    }
    if (strcmp(command, "ACTIVECHANNELLIST") && strcmp(command, "GETCHANNELLIST")) {
        fprintf(stderr, "refusing non-read command '%s'\n", command);
        return 2;
    }

    int fd = connect_tcp(host, port, timeout_ms);
    if (fd < 0) return 1;
    char line[64];
    int length = snprintf(line, sizeof line, "%s\n", command);
    if (length < 0 || length >= (int)sizeof line ||
        send_all(fd, line, (size_t)length)) {
        fprintf(stderr, "send failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    char *reply = calloc(MAX_REPLY, 1);
    if (!reply) { close(fd); return 1; }
    ssize_t got = read_reply(fd, reply, MAX_REPLY,
        !strcmp(command, "ACTIVECHANNELLIST") ? "</active_list>" : NULL);
    close(fd);
    if (got <= 0) {
        fprintf(stderr, "empty/short response\n");
        free(reply);
        return 1;
    }
    if (!strcmp(command, "ACTIVECHANNELLIST")) {
        int rows = print_active_list(reply);
        free(reply);
        return rows < 0 ? 1 : 0;
    }
    fputs(reply, stdout);
    free(reply);
    return 0;
}
