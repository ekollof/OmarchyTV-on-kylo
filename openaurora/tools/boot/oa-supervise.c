/* Separate-process recovery for an opt-in openaurora boot. No vendor HAL calls.
 * The host loop publishes "pid monotonic_ms" once a second; ui_vendor publishes a
 * separate first-present receipt. Neither receipt proves physical scanout. */
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stopping;
static void on_signal(int sig) { (void)sig; stopping = 1; }

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts); /* excludes system suspend, like the host */
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int receipt(const char *path, pid_t pid, uint64_t *stamp)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    long owner = 0;
    unsigned long long value = 0;
    int n = fscanf(f, "%ld %llu", &owner, &value);
    fclose(f);
    if (owner != (long)pid || n < (stamp ? 2 : 1)) return 0;
    if (stamp) *stamp = value;
    return 1;
}

static int watchdog(const char *tool, const char *command)
{
    pid_t p = fork();
    if (p < 0) return -1;
    if (!p) {
        /* A tracer belongs to the UI child only, never to recovery tools. */
        if (!strcmp(command, "set")) execl(tool, tool, command, "10", (char *)NULL);
        else execl(tool, tool, command, (char *)NULL);
        _exit(127);
    }
    int status;
    /* Bound helper failure too: a stuck IPC write must not block fallback. */
    uint64_t deadline = now_ms() + 2000;
    while (now_ms() < deadline) {
        pid_t r = waitpid(p, &status, WNOHANG);
        if (r == p) return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
        if (r < 0 && errno != EINTR) return -1;
        usleep(20000);
    }
    kill(p, SIGKILL);
    waitpid(p, NULL, 0);
    return -1;
}

static void record_failure(const char *path, const char *enable)
{
    unsigned n = 0;
    FILE *f = fopen(path, "r");
    if (f) { if (fscanf(f, "%u", &n) != 1) n = 0; fclose(f); }
    if (n < 3) n++;
    f = fopen(path, "w");
    if (f) { fprintf(f, "%u\n", n); fclose(f); }
    if (n >= 3) unlink(enable);
    fprintf(stderr, "oa-supervise: failure streak %u/3\n", n);
}

static unsigned duration(const char *s)
{
    char *end;
    errno = 0;
    unsigned long n = strtoul(s, &end, 10);
    if (errno || !*s || *end || n < 100 || n > 600000) {
        fprintf(stderr, "invalid duration: %s (100..600000 ms)\n", s);
        exit(2);
    }
    return (unsigned)n;
}

int main(int argc, char **argv)
{
    const char *heartbeat = "/tmp/openaurora.alive", *ready = "/tmp/openaurora.ready";
    const char *wdt = "/buffer/wdtctl", *fallback = "/vendor/com.nvt.idtv/aurora.elf";
    const char *fail = "/conf/oa_fail", *enable = "/conf/oa_boot", *safe = "/conf/oa_safe";
    unsigned startup_ms = 75000, timeout_ms = 5000, stable_ms = 30000;
    int child_arg = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--")) { child_arg = i + 1; break; }
        if (i + 1 >= argc) goto usage;
        const char *v = argv[++i];
        if (!strcmp(a, "--heartbeat")) heartbeat = v;
        else if (!strcmp(a, "--ready")) ready = v;
        else if (!strcmp(a, "--wdt")) wdt = v;
        else if (!strcmp(a, "--fallback")) fallback = v;
        else if (!strcmp(a, "--fail")) fail = v;
        else if (!strcmp(a, "--enable")) enable = v;
        else if (!strcmp(a, "--safe")) safe = v;
        else if (!strcmp(a, "--startup-ms")) startup_ms = duration(v);
        else if (!strcmp(a, "--timeout-ms")) timeout_ms = duration(v);
        else if (!strcmp(a, "--stable-ms")) stable_ms = duration(v);
        else goto usage;
    }
    if (!child_arg || child_arg >= argc) goto usage;
    if (access(fallback, X_OK) != 0) { perror(fallback); return 1; }
    if (access(safe, F_OK) == 0) goto fallback;
    if (access(argv[child_arg], X_OK) != 0 || access(wdt, X_OK) != 0) {
        fprintf(stderr, "oa-supervise: child or watchdog tool missing\n");
        goto failed;
    }
    struct sigaction sa = { .sa_handler = on_signal };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    unlink(heartbeat);
    unlink(ready);
    if (watchdog(wdt, "disable") != 0) {
        fprintf(stderr, "oa-supervise: watchdog disable failed\n");
        goto failed;
    }
    uint64_t start = now_ms(), healthy_since = 0;
    int stable = 0;
    pid_t child = fork();
    if (child < 0) goto failed;
    if (!child) {
        const char *preload = getenv("OA_CHILD_PRELOAD");
        if (preload && *preload) setenv("LD_PRELOAD", preload, 1);
        execv(argv[child_arg], argv + child_arg);
        _exit(127);
    }
    fprintf(stderr, "oa-supervise: child %ld, startup %u ms, heartbeat timeout %u ms\n",
            (long)child, startup_ms, timeout_ms);
    for (;;) {
        int status;
        pid_t r = waitpid(child, &status, WNOHANG);
        if (r == child) {
            fprintf(stderr, "oa-supervise: child exited (wait status %#x)\n", status);
            break;
        }
        if (r < 0 && errno != EINTR) break;
        if (stopping || access(safe, F_OK) == 0) {
            fprintf(stderr, "oa-supervise: vendor UI requested\n");
            goto kill_child;
        }
        uint64_t now = now_ms(), stamp = 0;
        int alive = receipt(heartbeat, child, &stamp) && stamp >= start && stamp <= now &&
                    now - stamp <= timeout_ms;
        if (!healthy_since) {
            if (alive && receipt(ready, child, NULL)) {
                healthy_since = now;
                fprintf(stderr, "oa-supervise: host loop alive, first present acknowledged\n");
            } else if (now - start >= startup_ms) {
                fprintf(stderr, "oa-supervise: startup deadline expired\n");
                goto kill_child;
            }
        } else if (!alive) {
            fprintf(stderr, "oa-supervise: host heartbeat missing, invalid or stale\n");
            goto kill_child;
        } else if (!stable && now - healthy_since >= stable_ms) {
            FILE *f = fopen(fail, "w");
            if (f) { fputs("0\n", f); fclose(f); }
            stable = 1;
            fprintf(stderr, "oa-supervise: stable; failure streak cleared\n");
        }
        usleep(100000);
    }
    goto child_done;
kill_child:
    /* SIGKILL works even with a blocked callback or a SIGSTOPped process. Reap
     * before re-arming the watchdog and starting the vendor as sole owner. */
    kill(child, SIGKILL);
    while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {}
child_done:
    unlink(heartbeat);
    unlink(ready);
failed:
    record_failure(fail, enable);
    if (watchdog(wdt, "set") != 0)
        fprintf(stderr, "oa-supervise: WARNING: watchdog re-arm failed\n");
fallback:
    fprintf(stderr, "oa-supervise: exec vendor fallback %s\n", fallback);
    execl(fallback, fallback, (char *)NULL);
    perror(fallback);
    return 1;
usage:
    fprintf(stderr, "usage: %s [--heartbeat PATH] [--ready PATH] [--wdt PATH]\n"
            "  [--fallback PATH] [--fail PATH] [--enable PATH] [--safe PATH]\n"
            "  [--startup-ms N] [--timeout-ms N] [--stable-ms N] -- PROGRAM [ARGS...]\n", argv[0]);
    return 2;
}
