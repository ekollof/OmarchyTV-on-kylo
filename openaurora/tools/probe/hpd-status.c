/* Read-only HDMI HPD status capture from the /dev/ntgpio bulk getter.
 *
 * Sends kernel getter 0x6712 only. No HAL call, sysfs export, pin claim,
 * GPIO configuration, level write, stream/input change or vendor library.
 * The stock ICE thread performs the same getter every 30 ms, so this adds
 * reads but no state change. It is bounded by --seconds.
 *
 *   oa-hpd-status [--path /dev/ntgpio] [--seconds 60] [--poll-ms 30]
 *
 * Output:
 *   # start pid=... path=... seconds=... bulk=<5 bytes>
 *   event n=1 mono=... port=1 status=connected raw=<5 bytes>
 *   # heartbeat mono=... polls=... events=... raw=<5 bytes>
 *   # summary ...
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/source_hpd.h"

#define OA_HPD_GET_IDENT 0x6712UL

static volatile sig_atomic_t stop_requested;

static void on_signal(int signo)
{
    (void)signo;
    stop_requested = 1;
}

static double monotonic_seconds(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
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
            "  PATH      ntgpio node (default /dev/ntgpio, char132:0)\n"
            "  seconds   capture window, 1..300 (default 60)\n"
            "  poll-ms   poll interval in ms, 10..1000 (default 30)\n",
            program);
}

static const char *status_name(enum oa_source_hpd_status status)
{
    switch (status) {
    case OA_SOURCE_HPD_CONNECTED: return "connected";
    case OA_SOURCE_HPD_DISCONNECTED: return "disconnected";
    case OA_SOURCE_HPD_ATTRIBUTES_CHANGED: return "attributes-changed";
    default: return "none";
    }
}

static void print_raw(const uint8_t bytes[OA_SOURCE_HPD_BULK_BYTES])
{
    for (unsigned i = 0; i < OA_SOURCE_HPD_BULK_BYTES; i++)
        printf("%02x", bytes[i]);
}

int main(int argc, char **argv)
{
    const char *path = "/dev/ntgpio";
    unsigned long seconds = 60;
    unsigned long poll_ms = OA_SOURCE_HPD_POLL_MS;

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
        poll_ms > 1000) {
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

    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", path, strerror(errno));
        return 1;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "fstat %s: %s\n", path, strerror(errno));
        close(fd);
        return 1;
    }
    if (!S_ISCHR(st.st_mode) || major(st.st_rdev) != 132 ||
        minor(st.st_rdev) != 0) {
        fprintf(stderr, "%s is not char132:0\n", path);
        close(fd);
        return 1;
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    double start = monotonic_seconds();
    double deadline = start + (double)seconds;
    double next_heartbeat = start + 10.0;
    struct oa_source_hpd_state state;
    oa_source_hpd_init(&state);

    printf("# start pid=%ld path=%s seconds=%lu poll_ms=%lu\n", (long)getpid(),
           path, seconds, poll_ms);

    unsigned long polls = 0;
    unsigned long events = 0;
    unsigned long errors = 0;
    int fatal = 0;
    struct timespec delay;
    delay.tv_sec = (time_t)(poll_ms / 1000);
    delay.tv_nsec = (long)(poll_ms % 1000) * 1000000L;

    while (!stop_requested && !fatal) {
        double now = monotonic_seconds();
        if (now >= deadline) break;

        uint8_t bytes[OA_SOURCE_HPD_BULK_BYTES];
        memset(bytes, 0, sizeof bytes);
        if (ioctl(fd, OA_HPD_GET_IDENT, bytes) < 0) {
            int error = errno;
            fprintf(stderr, "GET6712: %s\n", strerror(error));
            errors++;
            if (error == ENODEV || error == EIO) {
                fatal = 1;
                break;
            }
        } else {
            polls++;
            struct oa_source_hpd_event sequence[2];
            for (unsigned port = 1; port <= OA_SOURCE_HPD_PORTS; port++) {
                unsigned index = oa_source_hpd_bulk_index(port);
                uint8_t level = bytes[index] ? 1 : 0;
                int count = oa_source_hpd_update(&state, port, level,
                                                 sequence, 2);
                if (count < 0) {
                    fprintf(stderr, "state port=%u: %s\n", port,
                            strerror(-count));
                    errors++;
                    continue;
                }
                for (int i = 0; i < count; i++) {
                    events++;
                    printf("event n=%lu mono=%.6f port=%u status=%s raw=",
                           events, monotonic_seconds(), sequence[i].port,
                           status_name(sequence[i].status));
                    print_raw(bytes);
                    putchar('\n');
                }
            }
        }

        if (monotonic_seconds() >= next_heartbeat) {
            printf("# heartbeat mono=%.6f polls=%lu events=%lu raw=",
                   monotonic_seconds(), polls, events);
            print_raw(bytes);
            putchar('\n');
            next_heartbeat += 10.0;
        }

        struct timespec remaining = delay;
        while (nanosleep(&remaining, &remaining) < 0 && errno == EINTR) {
            if (stop_requested) break;
        }
    }

    double elapsed = monotonic_seconds() - start;
    if (close(fd) < 0) {
        fprintf(stderr, "close: %s\n", strerror(errno));
        errors++;
    }
    printf("# summary polls=%lu events=%lu errors=%lu elapsed=%.3f\n", polls,
           events, errors, elapsed);
    return fatal ? 1 : 0;
}
