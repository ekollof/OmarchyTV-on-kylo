/*
 * oa-wdtfeed — keep the MCU watchdog armed while this process can still run.
 *
 * The watchdog is the only recovery that survives a kernel/MCU hang: if the
 * SoC stops scheduling this process, feeding stops and the MCU resets the set.
 * The boot hook starts this before the replacement's risky hardware init, so a
 * hung cold boot becomes a reset rather than a dark set. oa-supervise runs
 * with --wdt-keep in that mode; otherwise it would disable the watchdog first.
 *
 * Protocol (recovered from pcbconfig; identical to wdtctl):
 *   open("/proc/nvtstbcipc", O_RDWR) + fcntl(F_SETLK, F_WRLCK)
 *   write { 0, 0xae, sec>>8, sec&0xff, 1, 0xae }   (FIPC_set_watch_dog)
 *
 * A hang in this process is safe: the previously armed timeout expires.
 * A write failure is also safe for the same reason; it is logged and retried
 * until the beats limit (or forever in the default loop).
 *
 * Usage:
 *   oa-wdtfeed [--node PATH] [--interval SEC] [--timeout SEC]
 *              [--stop FILE] [--log FILE] [--foreground|--daemon]
 *              [--once] [--beats N] [--help]
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define IPC_NODE "/proc/nvtstbcipc"
#define CMD_WDT 0xae

static void usage(FILE *f)
{
    fprintf(f,
            "usage: oa-wdtfeed [--node PATH] [--interval SEC] [--timeout SEC]\n"
            "                  [--stop FILE] [--log FILE] [--foreground|--daemon]\n"
            "                  [--once] [--beats N] [--help]\n"
            "default: node " IPC_NODE ", interval 3 s, timeout 10 s,\n"
            "         stop /conf/oa_wdt_stop, daemon\n");
}

static int parse_int(const char *s, int lo, int hi, int *out)
{
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || !*s || *end || v < lo || v > hi)
        return -1;
    *out = (int)v;
    return 0;
}

static void sleep_seconds(int sec)
{
    struct timespec ts = { .tv_sec = sec, .tv_nsec = 0 };
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR)
        ;
}

static void ipc_lock(int fd)
{
    struct flock fl;
    memset(&fl, 0, sizeof fl);
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    /* fcntl is fcntl@GLIBC_2.28 in this toolchain and the set has 2.23. */
    if (syscall(SYS_fcntl, fd, F_SETLK, &fl) < 0)
        fprintf(stderr, "oa-wdtfeed: lock: %s (continuing)\n", strerror(errno));
}

static int ipc_open(const char *node)
{
    int fd = open(node, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "oa-wdtfeed: open %s: %s\n", node, strerror(errno));
        return -1;
    }
    ipc_lock(fd);
    return fd;
}

static int ipc_arm(int fd, int seconds)
{
    unsigned char pkt[6] = { 0, CMD_WDT, (unsigned char)((seconds >> 8) & 0xff),
                             (unsigned char)(seconds & 0xff), 1, CMD_WDT };
    ssize_t n = write(fd, pkt, sizeof pkt);
    if (n != (ssize_t)sizeof pkt) {
        fprintf(stderr, "oa-wdtfeed: write: %s\n",
                n < 0 ? strerror(errno) : "short write");
        return -1;
    }
    return 0;
}

static void daemonize(void)
{
    pid_t p = fork();
    if (p < 0) {
        perror("oa-wdtfeed: fork");
        exit(1);
    }
    if (p > 0)
        _exit(0);
    if (setsid() < 0) {
        perror("oa-wdtfeed: setsid");
        exit(1);
    }
}

int main(int argc, char **argv)
{
    const char *node = IPC_NODE;
    const char *stop = "/conf/oa_wdt_stop";
    const char *log_path = NULL;
    int interval = 3, timeout = 10;
    int foreground = 0, once = 0;
    long beats = -1;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--help")) {
            usage(stdout);
            return 0;
        }
        if (!strcmp(a, "--foreground")) {
            foreground = 1;
            continue;
        }
        if (!strcmp(a, "--daemon")) {
            foreground = 0;
            continue;
        }
        if (!strcmp(a, "--once")) {
            once = 1;
            continue;
        }
        if (i + 1 >= argc) {
            usage(stderr);
            return 2;
        }
        const char *v = argv[++i];
        if (!strcmp(a, "--node"))
            node = v;
        else if (!strcmp(a, "--stop"))
            stop = v;
        else if (!strcmp(a, "--log"))
            log_path = v;
        else if (!strcmp(a, "--interval")) {
            if (parse_int(v, 1, 60, &interval)) {
                fprintf(stderr, "oa-wdtfeed: bad interval '%s'\n", v);
                return 2;
            }
        } else if (!strcmp(a, "--timeout")) {
            if (parse_int(v, 2, 600, &timeout)) {
                fprintf(stderr, "oa-wdtfeed: bad timeout '%s'\n", v);
                return 2;
            }
        } else if (!strcmp(a, "--beats")) {
            char *end;
            errno = 0;
            beats = strtol(v, &end, 10);
            if (errno || !*v || *end || beats < 1 || beats > 1000000) {
                fprintf(stderr, "oa-wdtfeed: bad beats '%s'\n", v);
                return 2;
            }
        } else {
            usage(stderr);
            return 2;
        }
    }
    if (once)
        beats = 1;

    /* Open before daemonizing so a missing/unusable IPC node is reported to
     * the boot wrapper with a failed exit status instead of a silent daemon. */
    int fd = ipc_open(node);
    if (fd < 0)
        return 1;

    if (!foreground && !once)
        daemonize();
    /* Record locks are not inherited across fork. */
    ipc_lock(fd);

    FILE *log = NULL;
    if (log_path && *log_path) {
        log = fopen(log_path, "a");
        if (!log)
            fprintf(stderr, "oa-wdtfeed: log %s: %s\n", log_path,
                    strerror(errno));
    }

    long n = 0;
    for (;;) {
        if (stop && *stop && access(stop, F_OK) == 0) {
            if (log)
                fprintf(log, "oa-wdtfeed: stop file %s, exiting after %ld beat(s)\n",
                        stop, n);
            break;
        }
        if (ipc_arm(fd, timeout) == 0) {
            n++;
            if (log) {
                fprintf(log, "oa-wdtfeed: beat %ld, watchdog %ds\n", n, timeout);
                fflush(log);
            }
        } else if (log) {
            fprintf(log, "oa-wdtfeed: beat failed after %ld ok\n", n);
            fflush(log);
        }
        if (beats > 0 && n >= beats)
            break;
        sleep_seconds(interval);
    }
    if (log)
        fclose(log);
    close(fd);
    return 0;
}
