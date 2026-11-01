#ifndef TEST_LIFECYCLE_RETRY_H
#define TEST_LIFECYCLE_RETRY_H
#include <stdio.h>
#include <time.h>
#include "host.h"
/* Test-client retry, never a wait inside the host lifecycle callback. */
static int lifecycle_retry(oa_host *host, const char *text, char *out, size_t cap)
{
    for (int i = 0; i < 1000; i++) {
        char line[160];
        snprintf(line, sizeof line, "%s", text);
        int rc = oa_control_command(host, line, out, cap);
        if (rc == 0) return 0;
        oa_host_dispatch_posts(host);
        struct timespec pause = { .tv_nsec = 1000000 };
        nanosleep(&pause, NULL);
    }
    return -1;
}
#endif
