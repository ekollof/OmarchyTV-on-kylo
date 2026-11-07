#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "host.h"
#include "lifecycle_retry.h"
#include "openaurora/cec_owner.h"
#include "openaurora/services.h"
#include "openaurora/vendor_cec.h"

static struct {
    uint32_t mirror;
    int reads, writes, opens, inits, closes, sends;
    atomic_int shuts, fail_shut;
    int fail_read, write_result;
    uint16_t reg;
    uint8_t value;
    pthread_t thread;
} hw;

static int read_mirror(void *context, uint32_t *value)
{
    assert(context == &hw);
    hw.reads++;
    hw.thread = pthread_self();
    if (hw.fail_read) { errno = EIO; return -1; }
    *value = hw.mirror;
    return 0;
}
static int write_register(void *context, uint16_t reg, uint8_t value)
{
    assert(context == &hw);
    hw.writes++;
    hw.reg = reg;
    hw.value = value;
    if (hw.write_result != 1) return 0;
    hw.mirror = (hw.mirror & ~0xffu) | value;
    return 1;
}
static const oa_cec_authority_io authority = { &hw, read_mirror, write_register };

static int fake_open(void *context, const char *path, int flags)
{
    assert(context == &hw && !strcmp(path, "/fixture/ntkcec"));
    assert((flags & O_ACCMODE) == O_RDWR && (flags & O_CLOEXEC));
    hw.opens++;
    return 7;
}
static int fake_close(void *context, int fd)
{
    assert(context == &hw && fd == 7);
    hw.closes++;
    return 0;
}
static int fake_ioctl(void *context, int fd, unsigned long request, void *argument)
{
    assert(context == &hw && fd == 7 && argument);
    hw.thread = pthread_self();
    if (request == 0x401c4301) { hw.inits++; return 0; }
    if (request == 0x80014302) {
        hw.shuts++;
        if (hw.fail_shut) { errno = EIO; return -1; }
        return 0;
    }
    if (request == 0xc0104304) { hw.sends++; return 0; }
    if (request == 0x80104303) {
        oa_cec_raw_packet *packet = argument;
        const uint8_t bytes[] = { 0x50, 0x7a, 0xa8 };
        memcpy(packet->bytes, bytes, sizeof bytes);
        packet->length = sizeof bytes;
        packet->status = 1;
        errno = EIO; /* stale worker errno is irrelevant to positive success */
        return 0;
    }
    assert(0);
    return -1;
}
static const oa_cec_device_io device_io = { &hw, fake_open, fake_ioctl, fake_close };

static void settle_until(oa_cec_owner *owner)
{
    for (int i = 0; i < 200; i++) {
        if (oa_cec_owner_quiesce(owner) == 0) return;
        struct timespec delay = { .tv_nsec = 1000000 };
        nanosleep(&delay, NULL);
    }
    assert(oa_cec_owner_quiesce(owner) == 0);
}

static void authority_cases(void)
{
    uint32_t mirror = 0;
    oa_cec_authority_io missing = authority;
    missing.read_mirror = NULL;
    assert(oa_cec_authority_ensure(NULL) == -1 && errno == EINVAL);
    assert(oa_cec_authority_ensure(&missing) == -1 && errno == EINVAL);
    hw.mirror = 0x102;
    assert(oa_cec_authority_ensure(&authority) == 0 && hw.reads == 1 && hw.writes == 0);
    hw.mirror = 0x15;
    assert(oa_cec_authority_ensure(&authority) == 0 && hw.writes == 1);
    assert(hw.reg == OA_CEC_AUTHORITY_REGISTER && hw.value == 0x17 && hw.mirror == 0x17);
    hw.write_result = 0;
    hw.mirror = 0x100;
    assert(oa_cec_authority_ensure(&authority) == -1 && errno == EIO && hw.writes == 2);
    hw.write_result = 1;
    hw.fail_read = 1;
    assert(oa_cec_authority_ensure(&authority) == -1 && errno == EIO && hw.writes == 2);
    (void)mirror;
    puts("  ok   authority keeps an existing bit and writes 0xf011 low-byte|2 only when it is clear");
}

static void owner_cases(void)
{
    memset(&hw, 0, sizeof hw);
    hw.write_result = 1;
    hw.mirror = 0x15;
    oa_cec_owner *owner = NULL;
    assert(oa_cec_owner_create(&owner, NULL, NULL) == 0);
    oa_cec_acquire missing = { .authority = &authority, .device = NULL, .path = "/fixture/ntkcec" };
    assert(oa_cec_owner_acquire(owner, &missing) == -1 && errno == EINVAL);
    assert(hw.reads == 0 && hw.opens == 0);
    oa_cec_acquire request = { .authority = &authority, .device = &device_io, .path = "/fixture/ntkcec" };
    assert(oa_cec_owner_acquire(owner, &request) == 0);
    assert(hw.thread != pthread_self());
    assert(hw.reads == 1 && hw.writes == 1 && hw.value == 0x17 && hw.opens == 1 && hw.inits == 1);
    oa_cec_message message = { .destination = 5, .opcode = 0x44, .count = 1, .operands = {0x41} };
    assert(oa_cec_owner_send(owner, &message) == 0 && hw.sends == 1);
    oa_cec_message incoming = {0};
    errno = ETIMEDOUT;
    assert(oa_cec_owner_receive(owner, &incoming) == 1 && errno == ETIMEDOUT);
    assert(incoming.initiator == 5 && incoming.destination == 0 && incoming.opcode == 0x7a &&
           incoming.count == 1 && incoming.operands[0] == 0xa8);
    assert(oa_cec_owner_acquire(owner, &request) == -1 && errno == EBUSY && hw.writes == 1 && hw.opens == 1);
    assert(hw.reads == 1); /* rejected acquire must not touch authority */
    hw.fail_shut = 1;
    assert(oa_cec_owner_quiesce(owner) == -1);
    for (int i = 0; i < 200 && hw.shuts < 1; i++) {
        struct timespec delay = { .tv_nsec = 1000000 };
        nanosleep(&delay, NULL);
    }
    assert(hw.shuts >= 1 && hw.closes == 0);
    assert(oa_cec_owner_quiesce(owner) == -1);
    for (int i = 0; i < 200 && hw.shuts < 2; i++) {
        struct timespec delay = { .tv_nsec = 1000000 };
        nanosleep(&delay, NULL);
    }
    assert(hw.shuts >= 2 && hw.closes == 0);
    hw.fail_shut = 0;
    settle_until(owner);
    assert(hw.closes == 1);
    assert(oa_cec_owner_join(owner) == 0);
    oa_cec_owner_destroy(owner);
    puts("  ok   worker serializes authority, open and send; failed close stays owned until a later quiesce");
}

static int thread_count(void)
{
    FILE *file = fopen("/proc/self/status", "r");
    assert(file);
    char line[80];
    int count = -1;
    while (fgets(line, sizeof line, file))
        if (sscanf(line, "Threads: %d", &count) == 1) break;
    fclose(file);
    assert(count > 0);
    return count;
}
static void wait_threads(int want)
{
    for (int i = 0; i < 200; i++) {
        if (thread_count() == want) return;
        const struct timespec pause = { .tv_nsec = 1000000 };
        (void)nanosleep(&pause, NULL);
    }
    assert(thread_count() == want);
}
static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[128];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}
static int mark_entered(void *ud)
{
    *(int *)ud = 1;
    return 0;
}
static void module_case(const char *modules)
{
    oa_host host;
    char out[512];
    assert(oa_host_init(&host, modules, NULL) == 0);
    host.log_level = OA_LOG_ERROR;
    int base = thread_count();
    assert(!command(&host, "load cec", out, sizeof out) && strstr(out, "ok cec"));
    assert(!command(&host, "start cec", out, sizeof out) && !strcmp(out, "ok\n"));
    wait_threads(base + 1);
    assert(!command(&host, "status cec", out, sizeof out));
    assert(strstr(out, "state   started") && strstr(out, "cec.owner:v1"));
    assert(!lifecycle_retry(&host, "reload cec", out, sizeof out) && !strcmp(out, "ok\n"));
    oa_mod *mod = oa_host_find_module(&host, "cec");
    assert(mod && mod->state == OA_ST_STARTED);
    wait_threads(base + 1);
    assert(!lifecycle_retry(&host, "suspend", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(mod->state == OA_ST_SUSPENDED);
    wait_threads(base);
    assert(!command(&host, "status cec", out, sizeof out) && strstr(out, "state   suspended"));
    assert(!command(&host, "resume", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(mod->state == OA_ST_STARTED);
    wait_threads(base + 1);
    int entered = 0;
    oa_sleep_result result;
    int sleep_rc = -1;
    for (int i = 0; i < 1000 && sleep_rc != 0; i++) {
        sleep_rc = oa_module_sleep_cycle(&host, 0, mark_entered, &entered, &result);
        if (sleep_rc) {
            oa_host_dispatch_posts(&host);
            struct timespec pause = { .tv_nsec = 1000000 };
            nanosleep(&pause, NULL);
        }
    }
    assert(!sleep_rc);
    assert(entered && !result.prepare_rc && !result.entry_rc && !result.restore_rc);
    assert(mod->state == OA_ST_STARTED);
    wait_threads(base + 1);
    host.cur_module = 0;
    assert(!command(&host, "load ticker", out, sizeof out) && !command(&host, "start ticker", out, sizeof out));
    oa_mod *ticker = oa_host_find_module(&host, "ticker");
    assert(ticker);
    host.cur_module = ticker->id;
    const oa_cec_service *service = host.api.lookup(&host, OA_SVC_CEC_OWNER, 1);
    assert(service && !service->hardware_open_supported());
    assert(command(&host, "unload cec", out, sizeof out) == -1 && strstr(out, "failed"));
    assert(oa_host_find_module(&host, "cec") && oa_host_find_module(&host, "cec")->state == OA_ST_STARTED);
    host.api.release(&host, OA_SVC_CEC_OWNER);
    host.cur_module = 0;
    assert(!lifecycle_retry(&host, "unload cec", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!oa_host_find_module(&host, "cec"));
    wait_threads(base);
    assert(!command(&host, "load cec", out, sizeof out) && !command(&host, "start cec", out, sizeof out));
    host.stop_requested = 1;
    assert(!oa_host_run(&host) && !oa_host_find_module(&host, "cec"));
    wait_threads(base);
    puts("  ok   control reload/suspend/resume joins the idle worker; a held service stays loaded");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    alarm(10);
    memset(&hw, 0, sizeof hw);
    hw.write_result = 1;
    authority_cases();
    owner_cases();
    module_case(argv[1]);
    return 0;
}
