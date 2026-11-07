/* oa-app-launch — one-shot NAM app launch for the replacement UI.
 *
 * The stock idtv app registers as COM.NOVATEK.IDTV and sends NAM command 0x106
 * with a 40-byte process name, 8192-byte argument block and a state word (2).
 * This helper performs the same call in a child so the host loop never blocks
 * on app-manager IPC. Registration is mandatory: LIBNAM_APP_SendCommand does
 * not create a client for 0x106.
 *
 * TV-only: links the vendor /vendor/lib/nlib/libnam.so. The no-op callbacks are
 * used only for this short-lived launcher registration; they do not claim to be
 * the full idtv app-server event implementation.
 *
 * usage: oa-app-launch --name NAME [--args ARGS] [--linger-ms N]
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define NAM_APP_NAME "COM.NOVATEK.IDTV"
#define NAM_APP_FLAGS 0x007A0035u
#define NAM_CMD_START_PROCESS 0x106

extern int LIBNAM_APP_Register(const char *name, unsigned flags,
                               void *cb1, void *cb2);
extern int LIBNAM_APP_SendCommand(int cmd, void *payload);

struct launch_payload {
    char name[40];
    char args[8192];
    int state;
};

static int noop_callback(void *a, void *b)
{
    (void)a;
    (void)b;
    return 0;
}

static void nap(unsigned ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000u;
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) {}
}

static void usage(const char *arg0)
{
    fprintf(stderr, "usage: %s --name NAME [--args ARGS] [--linger-ms N]\n",
            arg0);
}

int main(int argc, char **argv)
{
    const char *name = NULL;
    const char *args = "";
    unsigned linger_ms = 2000;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--name") && i + 1 < argc) {
            name = argv[++i];
        } else if (!strcmp(argv[i], "--args") && i + 1 < argc) {
            args = argv[++i];
        } else if (!strcmp(argv[i], "--linger-ms") && i + 1 < argc) {
            linger_ms = (unsigned)strtoul(argv[++i], NULL, 0);
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (!name || !*name || strlen(name) >= sizeof ((struct launch_payload *)0)->name ||
        strlen(args) >= sizeof ((struct launch_payload *)0)->args) {
        usage(argv[0]);
        return 2;
    }
    int rc = LIBNAM_APP_Register(NAM_APP_NAME, NAM_APP_FLAGS,
                                 (void *)noop_callback, (void *)noop_callback);
    if (rc == -1) {
        fprintf(stderr, "oa-app-launch: NAM registration failed\n");
        return 1;
    }
    struct launch_payload *payload = calloc(1, sizeof *payload);
    if (!payload) {
        fprintf(stderr, "oa-app-launch: out of memory\n");
        return 1;
    }
    snprintf(payload->name, sizeof payload->name, "%s", name);
    snprintf(payload->args, sizeof payload->args, "%s", args);
    payload->state = 2;
    rc = LIBNAM_APP_SendCommand(NAM_CMD_START_PROCESS, payload);
    free(payload);
    if (rc == -1) {
        fprintf(stderr, "oa-app-launch: start-process command failed\n");
        return 1;
    }
    printf("ok %s\n", name);
    fflush(stdout);
    if (linger_ms) nap(linger_ms);
    return 0;
}
