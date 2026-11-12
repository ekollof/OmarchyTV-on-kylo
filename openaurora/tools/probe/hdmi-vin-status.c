/* Read-only HDMI RX status capture through the private V4L2 GET control.
 *
 * The stock Hal_Vin_GetHdmiStatus builds a v4l2_ext_controls request:
 *   ctrl_class = 0xa00000
 *   count      = 1
 *   control.id = 0xa00967
 *   control.size = 0x14
 *   control.ptr  = 5-word output buffer
 * and issues VIDIOC_G_EXT_CTRLS (0xc0185647) on the HDMI RX node. This probe
 * reproduces only that getter. It does not select an input, change a format,
 * start a stream, claim GPIO or write a control.
 *
 *   oa-hdmi-vin --seconds 60 [--poll-ms 100] [--path /dev/video21]
 *
 * Output is raw because the full field meanings are not all recovered:
 *   # start pid=... path=... seconds=... poll_ms=...
 *   status n=1 mono=... w0=... w1=... w2=... w3=... w4=...
 *   # heartbeat ...
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#if !defined(__arm__) || UINTPTR_MAX != UINT32_MAX
#error "oa-hdmi-vin uses the target 32-bit v4l2_ext_control layout"
#endif
_Static_assert(sizeof(struct v4l2_ext_control) == 20,
               "target v4l2_ext_control size");
_Static_assert(offsetof(struct v4l2_ext_control, ptr) == 12,
               "target v4l2_ext_control ptr offset");
_Static_assert(sizeof(struct v4l2_ext_controls) == 24,
               "target v4l2_ext_controls size");

#define OA_HDMI_STATUS_CTRL_CLASS 0xa00000u
#define OA_HDMI_STATUS_CTRL_ID 0xa00967u
#define OA_HDMI_STATUS_SIZE 0x14u
#define OA_HDMI_STATUS_WORDS 5u

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
            "  PATH      HDMI RX V4L2 node (default /dev/video21)\n"
            "  seconds   capture window, 1..300 (default 60)\n"
            "  poll-ms   poll interval in ms, 20..5000 (default 100)\n",
            program);
}

static void print_words(const uint32_t words[OA_HDMI_STATUS_WORDS])
{
    printf("w0=%u w1=%u w2=%u w3=%u w4=%u", words[0], words[1], words[2],
           words[3], words[4]);
}

static int read_status(int fd, uint32_t words[OA_HDMI_STATUS_WORDS])
{
    struct v4l2_ext_control control;
    memset(&control, 0, sizeof control);
    control.id = OA_HDMI_STATUS_CTRL_ID;
    control.size = OA_HDMI_STATUS_SIZE;
    control.ptr = words;

    struct v4l2_ext_controls controls;
    memset(&controls, 0, sizeof controls);
    controls.ctrl_class = OA_HDMI_STATUS_CTRL_CLASS;
    controls.count = 1;
    controls.controls = &control;

    errno = 0;
    int ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &controls);
    if (ret < 0) {
        fprintf(stderr,
                "G_EXT_CTRLS ret=%d errno=%d class=0x%x count=%u "
                "error_idx=%u id=0x%x size=%u ptr=%p\n",
                ret, errno, controls.ctrl_class, controls.count,
                controls.error_idx, control.id, control.size, control.ptr);
        return -(errno ? errno : EIO);
    }
    /* Hal_Vin_GetHdmiStatus ignores error_idx and checks only the ioctl
     * return. The target kernel leaves error_idx == count on success, so do
     * not turn that sentinel into a probe failure. */
    return 0;
}

int main(int argc, char **argv)
{
    const char *path = "/dev/video21";
    unsigned long seconds = 60;
    unsigned long poll_ms = 100;

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
    if (!*path || seconds < 1 || seconds > 300 || poll_ms < 20 ||
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

    /* The getter needs no write access; stock opens the external-input nodes
     * read-only. O_RDWR was rejected with EPERM on video20. */
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", path, strerror(errno));
        return 1;
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    double start = monotonic_seconds();
    double deadline = start + (double)seconds;
    double next_heartbeat = start + 10.0;
    printf("# start pid=%ld path=%s seconds=%lu poll_ms=%lu\n", (long)getpid(),
           path, seconds, poll_ms);

    uint32_t previous[OA_HDMI_STATUS_WORDS] = {0};
    int have_previous = 0;
    unsigned long polls = 0;
    unsigned long changes = 0;
    unsigned long errors = 0;
    int fatal = 0;
    struct timespec delay;
    delay.tv_sec = (time_t)(poll_ms / 1000);
    delay.tv_nsec = (long)(poll_ms % 1000) * 1000000L;

    while (!stop_requested && !fatal) {
        double now = monotonic_seconds();
        if (now >= deadline) break;

        uint32_t words[OA_HDMI_STATUS_WORDS] = {0};
        int rc = read_status(fd, words);
        if (rc < 0) {
            fprintf(stderr, "GET 0xa00967: %s\n", strerror(-rc));
            errors++;
            if (rc == -ENODEV || rc == -EIO) fatal = 1;
            break;
        }
        polls++;
        if (!have_previous ||
            memcmp(words, previous, sizeof words) != 0) {
            changes++;
            printf("status n=%lu mono=%.6f ", changes,
                   monotonic_seconds());
            print_words(words);
            putchar('\n');
            memcpy(previous, words, sizeof previous);
            have_previous = 1;
        }

        if (monotonic_seconds() >= next_heartbeat) {
            printf("# heartbeat mono=%.6f polls=%lu changes=%lu ",
                   monotonic_seconds(), polls, changes);
            print_words(words);
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
    printf("# summary polls=%lu changes=%lu errors=%lu elapsed=%.3f\n", polls,
           changes, errors, elapsed);
    return fatal ? 1 : 0;
}
