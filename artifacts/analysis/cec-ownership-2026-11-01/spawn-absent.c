/* Minimal evidence probe: no TV devices, no files written, no HAL. */
#include <stdio.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
int main(void)
{
    char *const args[] = { "/does/not/exist/oa-audio-broker", NULL };
    pid_t child = 0;
    int rc = posix_spawn(&child, args[0], NULL, NULL, args, environ);
    printf("spawn rc=%d child=%ld\n", rc, (long)child);
    if (!rc) {
        int status;
        pid_t waited = waitpid(child, &status, 0);
        printf("waited=%ld status=%d\n", (long)waited, status);
    }
    return 0;
}
