/*
 * wspoll - minimal WebSocket client for the TV's state feed (ws://127.0.0.1:7681/).
 *
 * The OMI/aurora WS server pushes <tv_state value='X'/> once on connect and then on
 * every state change. This tool connects, reads the first state (or watches for a
 * while) and prints it, so a shell script can use it as a state oracle.
 *
 *   wspoll                 # one state, then exit   -> prints e.g. PORTAL
 *   wspoll -t 20           # watch for 20 s, print every state change
 *   wspoll -v              # also print raw frames
 *
 * Build (cross, static):
 *   arm-linux-musleabihf-gcc -static -Os -s -o wspoll wspoll.c
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static int ws_open(const char *host, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) { close(fd); return -1; }
    if (connect(fd, (struct sockaddr *)&sa, sizeof sa) != 0) { close(fd); return -1; }

    /* Sec-WebSocket-Key: fixed value is legal (server echoes it back) */
    const char *req =
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1:7681\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Origin: http://192.168.178.87\r\n"
        "\r\n";
    if (write(fd, req, strlen(req)) != (ssize_t)strlen(req)) { close(fd); return -1; }

    /* read the HTTP response headers */
    char buf[1024];
    int n = 0, total = 0;
    while (total < (int)sizeof buf - 1) {
        n = read(fd, buf + total, sizeof buf - 1 - total);
        if (n <= 0) { close(fd); return -1; }
        total += n;
        buf[total] = 0;
        if (strstr(buf, "\r\n\r\n")) break;
    }
    if (!strstr(buf, " 101")) { close(fd); return -1; }

    /* the app pushes tv_state in response to a SmartCenter command, so ask for it */
    int d = socket(AF_INET, SOCK_STREAM, 0);
    if (d >= 0) {
        struct sockaddr_in ds;
        memset(&ds, 0, sizeof ds);
        ds.sin_family = AF_INET;
        ds.sin_port = htons(56789);
        inet_pton(AF_INET, "127.0.0.1", &ds.sin_addr);
        if (connect(d, (struct sockaddr *)&ds, sizeof ds) == 0) {
            const char *body = "<command>tvstate</command>";
            char req[512];
            int rl = snprintf(req, sizeof req,
                "POST /apps/SmartCenter HTTP/1.0\r\n"
                "Host: 127.0.0.1\r\n"
                "Content-Type: text/plain; charset=ISO-8859-1\r\n"
                "application_name: tv smart centre\r\n"
                "Content-Length: %d\r\n\r\n%s", (int)strlen(body), body);
            if (write(d, req, rl) < 0) { /* ignore */ }
        }
        close(d);
    }
    return fd;
}

/* read one WS frame payload into out; returns length or -1 */
static int ws_frame(int fd, char *out, int cap)
{
    unsigned char h[2];
    if (read(fd, h, 2) != 2) return -1;
    int len = h[1] & 0x7f;
    unsigned char ext[4];
    if (len == 126) { if (read(fd, ext, 2) != 2) return -1; len = (ext[0] << 8) | ext[1]; }
    else if (len == 127) { if (read(fd, ext, 8) != 8) return -1; len = (ext[4] << 24) | (ext[5] << 16) | (ext[6] << 8) | ext[7]; }
    if (len >= cap) len = cap - 1;
    int got = 0;
    while (got < len) {
        int r = read(fd, out + got, len - got);
        if (r <= 0) return -1;
        got += r;
    }
    if (len > 0) out[got] = 0;
    return got;
}

static void extract(const char *frame, char *state, int cap, int verbose)
{
    if (verbose) printf("frame: %s\n", frame);
    const char *p = strstr(frame, "tv_state value='");
    if (!p) return;
    p += strlen("tv_state value='");
    int i = 0;
    while (*p && *p != '\'' && i < cap - 1) state[i++] = *p++;
    state[i] = 0;
}

int main(int argc, char **argv)
{
    int watch = 0, verbose = 0, c;
    while ((c = getopt(argc, argv, "vt:")) != -1) {
        if (c == 'v') verbose = 1;
        else if (c == 't') watch = atoi(optarg);
    }
    int fd = ws_open("127.0.0.1", 7681);
    if (fd < 0) { fprintf(stderr, "wspoll: cannot connect to ws://127.0.0.1:7681/\n"); return 2; }

    char frame[4096], state[64];
    time_t start = time(NULL);

    for (;;) {
        struct timeval tv = { watch ? 2 : 3, 0 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        int n = ws_frame(fd, frame, sizeof frame);
        if (n < 0) {
            if (!watch) { close(fd); return 1; }
            if (time(NULL) - start >= watch) break;
            continue;
        }
        extract(frame, state, sizeof state, verbose);
        if (state[0]) {
            printf("%s\n", state);
            fflush(stdout);
            if (!watch) { close(fd); return 0; }
        }
        if (watch && time(NULL) - start >= watch) break;
    }
    close(fd);
    return 0;
}
