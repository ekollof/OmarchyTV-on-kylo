/* Test child for oa-supervise: writes the pid + CLOCK_MONOTONIC heartbeat the
 * supervisor expects (atomically, like core/host.c), and can instead exit or
 * hang to exercise fallback. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static unsigned long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int write_atomic(const char *path, const char *data)
{
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s.%ld.tmp", path, (long)getpid());
    FILE *f = fopen(tmp, "w");
    if (!f)
        return -1;
    int bad = fputs(data, f) < 0;
    if (fclose(f))
        bad = 1;
    if (!bad && rename(tmp, path))
        bad = 1;
    return bad ? -1 : 0;
}

int main(int argc, char **argv)
{
    if (argc < 4)
        return 2;
    const char *heartbeat = argv[1];
    const char *ready = argv[2];
    const char *mode = argv[3];

    if (!strcmp(mode, "exit"))
        return 1;
    if (!strcmp(mode, "hang"))
        for (;;)
            pause();

    char line[128];
    snprintf(line, sizeof line, "%ld 0\n", (long)getpid());
    (void)write_atomic(ready, line);
    for (;;) {
        snprintf(line, sizeof line, "%ld %llu\n", (long)getpid(), now_ms());
        (void)write_atomic(heartbeat, line);
        usleep(100000);
    }
}
