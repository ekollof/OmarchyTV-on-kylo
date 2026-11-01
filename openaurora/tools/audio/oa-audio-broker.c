/* Exec this helper with an inherited AF_UNIX/SOCK_SEQPACKET on stdin.
 * It owns its PID-bound netlink session and never loads/initializes the HAL. */
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "openaurora/audio_broker.h"
static int close_inherited(void)
{
    DIR *dir = opendir("/proc/self/fd");
    if (!dir) return -1;
    int primary = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (!entry) { if (errno) primary = errno; break; }
        char *end;
        long fd = strtol(entry->d_name, &end, 10);
        if (*end || fd < 3 || fd > INT_MAX || fd == dirfd(dir)) continue;
        if (close((int)fd) != 0 && errno != EBADF) { primary = errno; break; }
    }
    if (closedir(dir) != 0 && !primary) primary = errno;
    if (primary) { errno = primary; return -1; }
    return 0;
}
int main(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "--serve")) {
        fprintf(stderr, "usage: %s --serve (local seqpacket connection on stdin)\n", argv[0]);
        return 2;
    }
    if (close_inherited() || oa_audio_broker_serve(STDIN_FILENO, NULL)) { perror("audio broker"); return 1; }
    return 0;
}
