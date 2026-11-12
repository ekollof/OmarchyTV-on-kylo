/* Read-only capture of the ndp-hdmi private V4L2 event.
 *
 * Opens /dev/video21, subscribes to event 0xc000001 and prints the 28-byte
 * payload for each event. It never issues a mutating V4L2 ioctl: no input
 * selection, no format change and no stream start. The subscription and fd
 * are per-file-handle, so stock can keep running and keeping its own events.
 *
 *   oa-hdmi-event [--path /dev/video21] [--seconds 60] [--poll-ms 250]
 *
 * Output is line-oriented for the capture log:
 *   # start pid=... path=... type=0xc000001 seconds=...
 *   event n=1 mono=... src=0x... status=... raw=<28 bytes>
 *   # summary events=... errors=... elapsed=...
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/source_hdmi_event_io.h"

static volatile sig_atomic_t stop_requested;

static void on_signal(int signo)
{
    (void)signo;
    stop_requested = 1;
}

static int sys_open(void *context, const char *path, int flags)
{
    (void)context;
    return open(path, flags);
}

static int sys_ioctl(void *context, int fd, unsigned long request, void *arg)
{
    (void)context;
    return ioctl(fd, request, arg);
}

static int sys_close(void *context, int fd)
{
    (void)context;
    return close(fd);
}

static double monotonic_seconds(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static void print_event(unsigned long count, double at,
                        const struct oa_source_hdmi_event *event)
{
    printf("event n=%lu mono=%.6f src=0x%08x update=%u setting0c=0x%02x "
           "setting86=0x%04x raw=",
           count, at, (unsigned)event->source_type,
           (unsigned)event->update_flag, (unsigned)event->video_setting_0c,
           (unsigned)event->video_setting_86);
    for (size_t i = 0; i < OA_SOURCE_HDMI_EVENT_SIZE; i++)
        printf("%02x", event->raw[i]);
    putchar('\n');
}

static int parse_unsigned(const char *text, unsigned long *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(text, &end, 0);
    if (errno || !end || *end || end == text) return -1;
    *out = value;
    return 0;
}

static void usage(FILE *stream, const char *program)
{
    fprintf(stream,
            "usage: %s [--path PATH] [--seconds N] [--poll-ms N]\n"
            "  PATH      V4L2 node (default /dev/video21, ndp-hdmi)\n"
            "  seconds   capture window, 1..300 (default 60)\n"
            "  poll-ms   poll timeout in ms, 10..5000 (default 250)\n",
            program);
}

int main(int argc, char **argv)
{
    const char *path = "/dev/video21";
    unsigned long seconds = 60;
    unsigned long poll_ms = 250;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--path") && i + 1 < argc) {
            path = argv[++i];
        } else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) {
            if (parse_unsigned(argv[++i], &seconds) < 0) {
                fprintf(stderr, "invalid --seconds value\n");
                return 2;
            }
        } else if (!strcmp(argv[i], "--poll-ms") && i + 1 < argc) {
            if (parse_unsigned(argv[++i], &poll_ms) < 0) {
                fprintf(stderr, "invalid --poll-ms value\n");
                return 2;
            }
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage(stdout, argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            usage(stderr, argv[0]);
            return 2;
        }
    }
    if (!*path || seconds < 1 || seconds > 300 || poll_ms < 10 ||
        poll_ms > 5000) {
        usage(stderr, argv[0]);
        return 2;
    }

    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGTERM, &action, NULL) < 0) {
        fprintf(stderr, "sigaction: %s\n", strerror(errno));
        return 1;
    }

    struct oa_source_hdmi_event_io io;
    memset(&io, 0, sizeof io);
    io.open = sys_open;
    io.ioctl = sys_ioctl;
    io.close = sys_close;

    int fd = -1;
    int rc = oa_source_hdmi_event_subscribe(&io, path, &fd);
    if (rc < 0) {
        fprintf(stderr, "subscribe %s: %s\n", path, strerror(-rc));
        return 1;
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    double start = monotonic_seconds();
    double deadline = start + (double)seconds;
    printf("# start pid=%ld path=%s type=0x%08x seconds=%lu\n", (long)getpid(),
           path, (unsigned)OA_SOURCE_HDMI_EVENT_TYPE, seconds);
    printf("# subscribed fd=%d poll_ms=%lu\n", fd, poll_ms);

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLPRI;
    pfd.revents = 0;

    unsigned long events = 0;
    unsigned long errors = 0;
    int fatal = 0;

    while (!stop_requested && !fatal) {
        double now = monotonic_seconds();
        if (now >= deadline) break;

        double remaining = deadline - now;
        int timeout = (int)poll_ms;
        if (remaining * 1000.0 < (double)timeout)
            timeout = remaining > 0.0 ? (int)(remaining * 1000.0) + 1 : 0;

        int ready = poll(&pfd, 1, timeout);
        if (ready < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "poll: %s\n", strerror(errno));
            errors++;
            break;
        }
        if (ready == 0) continue;
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "poll revents=0x%x\n", (unsigned)pfd.revents);
            errors++;
            break;
        }

        for (;;) {
            struct oa_source_hdmi_event event;
            rc = oa_source_hdmi_event_read(&io, fd, &event);
            if (rc == 0) break;
            if (rc < 0) {
                fprintf(stderr, "dequeue: %s\n", strerror(-rc));
                errors++;
                if (rc == -ENODEV || rc == -EIO) fatal = 1;
                break;
            }
            events++;
            print_event(events, monotonic_seconds(), &event);
        }
    }

    double elapsed = monotonic_seconds() - start;
    int close_rc = oa_source_hdmi_event_close(&io, fd);
    if (close_rc < 0) {
        fprintf(stderr, "close: %s\n", strerror(-close_rc));
        errors++;
    }
    printf("# summary events=%lu errors=%lu elapsed=%.3f closed=%d\n", events,
           errors, elapsed, close_rc == 0);
    return fatal ? 1 : 0;
}
