/* IR helper wire/reap regression.
 *
 * 1. Bounded pipe reads: a 7-byte partial record followed by 65 bytes used to
 *    reset the 64-byte assembly buffer and drop/desync records.
 * 2. The record really is 8 bytes with the pinned offsets.
 * 3. The forked helper's inherited fds are marked close-on-exec.
 * 4. Reaping is bounded/nonblocking: a surviving child never blocks the loop.
 *
 * The module source is included to reach its static helper functions; this is
 * a focused test binary, not the module.
 */
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/events.h"
#include "openaurora/ir_blast.h"
#include "openaurora/module.h"

#include "../modules/ir/ir.c"

static oa_key_event events[64];
static int nevents;

static void fake_log(void *priv, int level, const char *fmt, ...)
{
    (void)priv;
    (void)level;
    (void)fmt;
}

static void fake_emit(void *priv, const char *topic, const void *ev)
{
    (void)priv;
    assert(!strcmp(topic, OA_TOPIC_KEY));
    assert(nevents < (int)(sizeof events / sizeof events[0]));
    events[nevents++] = *(const oa_key_event *)ev;
}

static uint32_t fake_watch_add(void *priv, int fd, unsigned events_mask,
                               void (*cb)(void *, int, unsigned), void *ud)
{
    (void)priv;
    (void)fd;
    (void)events_mask;
    (void)cb;
    (void)ud;
    return 1;
}

static void fake_watch_del(void *priv, uint32_t id)
{
    (void)priv;
    (void)id;
}

static const char *fake_config(void *priv, const char *key, const char *def)
{
    (void)priv;
    (void)key;
    return def;
}

static const oa_host_api g_api = {
    .size = sizeof g_api,
    .abi = OA_ABI_VERSION,
    .log = fake_log,
    .emit = fake_emit,
    .watch_add = fake_watch_add,
    .watch_del = fake_watch_del,
    .config_get = fake_config,
};

static long elapsed_ms(const struct timespec *a, const struct timespec *b)
{
    return (b->tv_sec - a->tv_sec) * 1000L +
           (b->tv_nsec - a->tv_nsec) / 1000000L;
}

static struct ctx *new_ctx(void)
{
    void *opaque = NULL;
    assert(m_init(&g_api, &opaque) == 0);
    assert(opaque != NULL);
    return opaque;
}

static void test_framing(void)
{
    struct ctx *c = new_ctx();
    nevents = 0;

    int p[2];
    assert(pipe2(p, O_NONBLOCK | O_CLOEXEC) == 0);
    c->blast_pipe = -1;
    c->blast_pid = -1;
    c->blast_watch = 0;

    struct oa_ir_blast_record rec[9];
    for (int i = 0; i < 9; i++) {
        memset(&rec[i], 0, sizeof rec[i]);
        rec[i].code = (uint32_t)(0x30 + i * 3);
        rec[i].aux = (uint8_t)(0x10 + i);
        rec[i].flag = (uint8_t)(i == 8);
    }

    /* First seven bytes of record 0, then its last byte plus eight records.
     * The old 64-byte read would overflow the pending 7 and reset. */
    assert(write(p[1], &rec[0], 7) == 7);
    blast_helper_read(c, p[0], 0);
    assert(c->blast_rxn == 7 && nevents == 0);

    unsigned char tail[65];
    tail[0] = ((const unsigned char *)&rec[0])[7];
    memcpy(tail + 1, &rec[1], 8 * sizeof rec[1]);
    assert(write(p[1], tail, sizeof tail) == (ssize_t)sizeof tail);

    for (int i = 0; i < 10 && nevents < 9; i++)
        blast_helper_read(c, p[0], 0);
    assert(c->blast_rxn == 0);
    assert(nevents == 9);
    for (int i = 0; i < 9; i++) {
        assert(events[i].id == (uint8_t)(rec[i].code & 0x3f));
        assert(events[i].kind ==
               (rec[i].flag ? OA_KEY_RELEASE : OA_KEY_PRESS));
    }

    /* EOF must not touch a pipe/pid the test did not start. */
    close(p[1]);
    blast_helper_read(c, p[0], 0);
    close(p[0]);
    m_destroy(c);
    puts("ok ir helper: bounded reads keep 7+N-byte partials aligned");
}

static void test_record_layout(void)
{
    assert(sizeof(struct oa_ir_blast_record) == 8);
    assert(offsetof(struct oa_ir_blast_record, code) == 0);
    assert(offsetof(struct oa_ir_blast_record, aux) == 4);
    assert(offsetof(struct oa_ir_blast_record, flag) == 5);
    printf("ok ir helper: record is %zu bytes (code@%zu aux@%zu flag@%zu)\n",
           sizeof(struct oa_ir_blast_record),
           offsetof(struct oa_ir_blast_record, code),
           offsetof(struct oa_ir_blast_record, aux),
           offsetof(struct oa_ir_blast_record, flag));
}

static void test_cloexec(void)
{
    int fd = open("/dev/null", O_RDONLY);
    assert(fd >= 3);
    int flags = fcntl(fd, F_GETFD);
    assert(flags >= 0 && fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC) == 0);
    mark_inherited_fds_cloexec();
    flags = fcntl(fd, F_GETFD);
    assert(flags >= 0 && (flags & FD_CLOEXEC));
    close(fd);
    puts("ok ir helper: inherited fds are marked close-on-exec");
}

static pid_t spawn_stubborn_child(void)
{
    int ready[2];
    assert(pipe2(ready, O_CLOEXEC) == 0);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        close(ready[0]);
        signal(SIGTERM, SIG_IGN);
        char b = 'r';
        (void)write(ready[1], &b, 1);
        close(ready[1]);
        for (;;)
            pause();
    }
    close(ready[1]);
    char b = 0;
    assert(read(ready[0], &b, 1) == 1 && b == 'r');
    close(ready[0]);
    return pid;
}

static void test_bounded_reap(void)
{
    pid_t pid = spawn_stubborn_child();
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    assert(reap_child_bounded(pid, 2) == 0); /* still running */
    clock_gettime(CLOCK_MONOTONIC, &b);
    long waited = elapsed_ms(&a, &b);
    assert(waited >= 5 && waited < 500);

    assert(kill(pid, SIGKILL) == 0);
    assert(reap_child_bounded(pid, 100) == 1);
    int status = 0;
    errno = 0;
    assert(waitpid(pid, &status, WNOHANG) == -1 && errno == ECHILD);
    puts("ok ir helper: bounded reap returns without blocking on a live child");
}

static void test_stop_is_bounded(void)
{
    struct ctx *c = new_ctx();
    pid_t pid = spawn_stubborn_child();
    c->blast_pid = pid;
    c->blast_pipe = -1;
    c->blast_watch = 0;

    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    blast_helper_stop(c);
    clock_gettime(CLOCK_MONOTONIC, &b);
    long waited = elapsed_ms(&a, &b);
    assert(c->blast_pid == -1);
    assert(waited >= 50 && waited < 2000);
    int status = 0;
    errno = 0;
    assert(waitpid(pid, &status, WNOHANG) == -1 && errno == ECHILD);
    m_destroy(c);
    printf("ok ir helper: SIGTERM-ignoring helper killed and reaped in %ld ms\n",
           waited);
}

int main(void)
{
    test_framing();
    test_record_layout();
    test_cloexec();
    test_bounded_reap();
    test_stop_is_bounded();
    puts("all ir helper tests passed");
    return 0;
}
