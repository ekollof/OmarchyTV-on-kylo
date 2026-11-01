#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "host.h"
#include "phases.h"
#include "openaurora/events.h"

const oa_module *oa_module_entry(void);
int __real_open(const char *, int, ...);
ssize_t __real_write(int, const void *, size_t);
int __real_close(int);
static oa_host *host;
static struct phase_observer observer;
static const oa_power_iface *power;
static unsigned opens, writes, closes, syncs;
static int fail_open, write_result, initial[3];
static int test_enabled;
#define POWER_FD 9876

static void record(char c)
{
    assert(observer.length + 1 < sizeof observer.sequence);
    observer.sequence[observer.length++] = c;
    observer.sequence[observer.length] = 0;
}
static void entry_state(void)
{
    assert(host->sleep_cycle);
    assert(host->cur_module == oa_host_find_module(host, "power")->id);
    for (int i = 0; i < 3; i++) assert(!observer.running[i]);
    for (int i = 0; i < MAX_MODULES; i++)
        assert(host->mods[i].state != OA_ST_STARTED);
}
int __wrap_open(const char *path, int flags, ...)
{
    if (!strcmp(path, "/sys/power/state")) {
        assert(flags == O_WRONLY); entry_state(); opens++;
        assert(syncs == opens);
        if (fail_open) { errno = EIO; return -1; }
        return POWER_FD;
    }
    if (flags & O_CREAT) {
        va_list ap; va_start(ap, flags); mode_t mode = va_arg(ap, mode_t); va_end(ap);
        return __real_open(path, flags, mode);
    }
    return __real_open(path, flags);
}
ssize_t __wrap_write(int fd, const void *buf, size_t len)
{
    if (fd != POWER_FD) return __real_write(fd, buf, len);
    entry_state(); assert(len == 3 && !memcmp(buf, "mem", 3));
    writes++; record('X');
    if (write_result < 0) errno = EIO;
    return write_result;
}
int __wrap_close(int fd)
{
    if (fd != POWER_FD) return __real_close(fd);
    entry_state(); closes++; return 0;
}
void __wrap_sync(void) { entry_state(); syncs++; }

static void on_power(const void *v, void *ud)
{
    (void)ud;
    const oa_power_event *ev = v;
    assert(host->sleep_cycle);
    assert(oa_module_unload(host, "power", 1) == -1);
    assert(power->standby() == -1); /* synchronous notification reentry */
    if (ev->state == OA_POWER_STANDBY || ev->state == OA_POWER_ON) {
        for (int i = 0; i < 3; i++) assert(observer.running[i] == initial[i]);
        record(ev->state == OA_POWER_STANDBY ? 'S' : 'O');
    } else {
        assert(ev->state == OA_POWER_RESUMING);
        assert(!observer.running[0] && !observer.running[1] && observer.running[2]);
        record('R');
    }
}
static void install_power(void)
{
    /* Link the actual module into this executable so linker wrappers cannot
     * miss its syscalls. Other modules below are real dynamically loaded DSOs. */
    oa_mod *m = &host->mods[0];
    assert(m->state == OA_ST_EMPTY);
    m->desc = oa_module_entry(); m->id = ++host->next_id; m->state = OA_ST_LOADED;
    strcpy(m->name, m->desc->name);
    host->cur_module = m->id;
    assert(m->desc->init(&host->api, &m->ctx) == 0);
    host->cur_module = 0;
}
static const char *power_config(void *priv, const char *key, const char *def)
{
    (void)priv;
    return !strcmp(key, "power.enable") ? (test_enabled ? "1" : "0") : def;
}
static void setup(const char *dir, int enabled, int missing_hooks)
{
    host = calloc(1, sizeof *host);
    assert(host && oa_host_init(host, dir, NULL) == 0);
    test_enabled = enabled;
    host->api.config_get = power_config;
    host->log_level = OA_LOG_ERROR;
    memset(&observer, 0, sizeof observer);
    opens = writes = closes = syncs = 0; fail_open = 0; write_result = 3;
    install_power();
    assert(oa_module_start(host, &host->mods[0]) == 0);
    power = host->api.lookup(host, OA_SVC_POWER, 1); assert(power);
    assert(host->api.publish(host, PHASE_OBSERVER, 1, &observer) == 0);
    oa_mod *a = oa_module_load(host, missing_hooks ? "phase_no_suspend" : "phase_a");
    oa_mod *b = oa_module_load(host, "phase_b"), *c = oa_module_load(host, "phase_c");
    assert(a && b && c);
    assert(oa_module_start(host, c) == 0 && oa_module_start(host, b) == 0 && oa_module_start(host, a) == 0);
    observer.length = 0; observer.sequence[0] = 0;
    for (int i = 0; i < 3; i++) initial[i] = 1;
    assert(host->api.subscribe(host, OA_TOPIC_POWER, on_power, NULL));
}
static int accepted(void)
{
    oa_mod *m = oa_host_find_module(host, "power");
    int count = -1;
    assert(m->desc->save(m->ctx, &count, sizeof count) == sizeof count);
    return count;
}
static int command(const char *text)
{
    char line[80], reply[80]; strcpy(line, text);
    int rc = oa_control_command(host, line, reply, sizeof reply);
    assert(!strcmp(reply, rc == 0 ? "ok\n" : "standby failed\n"));
    assert(!host->sleep_cycle && host->cur_module == 0);
    return rc;
}
static void finish(void)
{
    const char *names[] = { "phase_a", "phase_b", "phase_c", "power" };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
        assert(oa_module_unload(host, names[i], 0) == 0);
    close(host->selfpipe[0]); close(host->selfpipe[1]);
    pthread_mutex_destroy(&host->post_mu); free(host); host = NULL;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    setup(argv[1], 1, 0);
    assert(oa_module_suspend_all(host) == 0 && !strcmp(observer.sequence, "abc"));
    assert(!opens && !writes && !syncs && !accepted());
    assert(power->standby() == -1); /* suspended provider cannot enter */
    assert(oa_module_resume_all(host) == 0 && !strcmp(observer.sequence, "abcCBA"));
    observer.length = 0; observer.sequence[0] = 0;
    assert(command("standby") == 0 && !strcmp(observer.sequence, "SabcXCBAO"));
    assert(opens == 1 && writes == 1 && closes == 1 && accepted() == 1);
    observer.length = 0; observer.sequence[0] = 0;
    assert(power->standby() == 0 && !strcmp(observer.sequence, "SabcXCBAO") && accepted() == 2);
    finish();
    puts("  ok   manual hooks never sleep; CLI/service enter last and notify ON only after restore");

    setup(argv[1], 1, 0); observer.fail_suspend = 2;
    assert(command("standby") == -1 && !strcmp(observer.sequence, "SabAO"));
    assert(!opens && !writes && !syncs && accepted() == 0);
    finish();
    puts("  ok   preparation failure restores completed transitions without opening power state");

    for (int outcome = -1; outcome <= 2; outcome += 3) {
        setup(argv[1], 1, 0); write_result = outcome;
        assert(command("standby") == -1 && !strcmp(observer.sequence, "SabcXCBAO"));
        assert(opens == 1 && writes == 1 && closes == 1 && accepted() == 0);
        finish();
    }
    setup(argv[1], 1, 0); fail_open = 1;
    assert(command("standby") == -1 && !strcmp(observer.sequence, "SabcCBAO"));
    assert(opens == 1 && !writes && !closes && accepted() == 0);
    finish();
    puts("  ok   failed open/short/failed mem write still restore; failed writes never count as sleep");

    setup(argv[1], 1, 0); observer.fail_resume = 2;
    assert(command("standby") == -1 && !strcmp(observer.sequence, "SabcXCBR"));
    assert(accepted() == 1);
    observer.fail_resume = 0;
    assert(oa_module_resume_all(host) == 0);
    finish();
    puts("  ok   restore failure reports error and RESUMING, never premature ON; hooks remain retryable");

    setup(argv[1], 0, 0);
    assert(!power->is_available() && command("standby") == -1);
    assert(!observer.length && !opens && !syncs); finish();
    setup(argv[1], 1, 1);
    assert(power->is_available() && command("standby") == -1);
    assert(!observer.length && !opens && !syncs); finish();
    puts("  ok   disabled power and incomplete quiescence hooks refuse without I/O or state events");

    setup(argv[1], 1, 0);
    assert(oa_module_suspend_all(host) == 0);
    assert(oa_module_start(host, oa_host_find_module(host, "power")) == 0);
    assert(oa_module_start(host, oa_host_find_module(host, "phase_c")) == 0);
    assert(oa_module_start(host, oa_host_find_module(host, "phase_b")) == 0);
    initial[0] = 0; observer.length = 0; observer.sequence[0] = 0;
    assert(command("standby") == 0 && !strcmp(observer.sequence, "SbcXCBO"));
    assert(!observer.running[0] && oa_host_find_module(host, "phase_a")->state == OA_ST_SUSPENDED);
    finish();
    puts("  ok   service preserves preexisting suspended modules and restores prior state before ON");

    setup(argv[1], 1, 0);
    oa_mod *m = oa_host_find_module(host, "power");
    assert(oa_module_stop(host, m) == 0 && !power->is_available());
    int peer = 0;
    assert(host->api.publish(host, OA_SVC_POWER, 1, &peer) == 0);
    assert(oa_module_start(host, m) == -1);
    assert(host->api.lookup(host, OA_SVC_POWER, 1) == &peer);
    for (int i = 0; i < MAX_HOLDS; i++)
        assert(!host->holds[i].used || host->holds[i].holder != m->id);
    assert(host->api.unpublish(host, OA_SVC_POWER) == 0);
    assert(host->api.unpublish(host, OA_SVC_SLEEP) == 0);
    assert(oa_module_start(host, m) == -1 && !power->is_available());
    assert(!opens && !syncs);
    finish();
    puts("  ok   failed publication retains peer service; missing coordinator refuses and releases holds");
    return 0;
}
