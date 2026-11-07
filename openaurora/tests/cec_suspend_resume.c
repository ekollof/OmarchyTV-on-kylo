/* CEC suspend/resume regression: an open session must be cleared and its
 * owner joined/closed on the cleanup worker during suspend; resume must reap
 * that generation, reopen authority and submit a fresh acquire. Fake host and
 * wrapped authority/device I/O keep it on the host without devices.
 */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "openaurora/cec_owner.h"
#include "openaurora/cec_authority_mem.h"
#include "openaurora/cec_device.h"
#include "openaurora/module.h"

#include "../modules/cec/cec.c"

static int authority_open_calls;
static int authority_close_calls;
static int device_open_calls;
static int device_close_calls;
static int publish_calls;
static int join_calls;
static pthread_t join_tid;
static void *const sentinel = (void *)(uintptr_t)0x1234;

int __real_oa_cec_owner_join(oa_cec_owner *owner);
int __wrap_oa_cec_owner_join(oa_cec_owner *owner)
{
    join_calls++;
    join_tid = pthread_self();
    return __real_oa_cec_owner_join(owner);
}

static int fake_read(void *context, uint32_t *value)
{
    (void)context;
    *value = 2;
    return 0;
}
static int fake_write(void *context, uint16_t reg, uint8_t value)
{
    (void)context;
    (void)reg;
    (void)value;
    return 0;
}

int __wrap_oa_cec_authority_mem_open(oa_cec_authority_mem *m,
                                     const oa_cec_authority_mem_io *io)
{
    (void)io;
    memset(m, 0, sizeof *m);
    m->fd = -1;
    m->map = sentinel;
    m->authority.context = m;
    m->authority.read_mirror = fake_read;
    m->authority.write_register = fake_write;
    authority_open_calls++;
    return 0;
}
void __wrap_oa_cec_authority_mem_close(oa_cec_authority_mem *m)
{
    assert(m && m->map == sentinel);
    authority_close_calls++;
    m->map = NULL;
}

int __wrap_oa_cec_device_open(oa_cec_device *device, const char *path,
                              const oa_cec_device_io *io)
{
    assert(device && path);
    assert(device->fd == -1 && !device->initialized);
    device->fd = 42;
    device->initialized = 1;
    device->io = io;
    device_open_calls++;
    return 0;
}
int __wrap_oa_cec_device_close(oa_cec_device *device)
{
    assert(device && device->initialized);
    device->fd = -1;
    device->initialized = 0;
    device->io = NULL;
    device_close_calls++;
    return 0;
}

static void fake_log(void *priv, int level, const char *fmt, ...)
{
    (void)priv;
    (void)level;
    (void)fmt;
}
static int fake_publish(void *priv, const char *name, uint32_t version,
                        const void *iface)
{
    (void)priv;
    (void)name;
    (void)version;
    (void)iface;
    publish_calls++;
    return 0;
}
static int fake_unpublish(void *priv, const char *name)
{
    (void)priv;
    (void)name;
    return 0;
}
static uint64_t fake_timer_add(void *priv, uint32_t delay, uint32_t repeat,
                               void (*cb)(void *), void *ud)
{
    (void)priv;
    (void)delay;
    (void)repeat;
    (void)cb;
    (void)ud;
    return 1;
}
static void fake_timer_del(void *priv, uint64_t id)
{
    (void)priv;
    (void)id;
}
static int fake_post(void *priv, uint32_t owner, void (*cb)(void *), void *ud)
{
    (void)priv;
    (void)owner;
    (void)cb;
    (void)ud;
    return 0;
}
static const char *fake_config(void *priv, const char *key, const char *def)
{
    (void)priv;
    if (!strcmp(key, "cec.enable")) return "1";
    if (!strcmp(key, "cec.device")) return "/tmp/oa-cec-suspend-node";
    if (!strcmp(key, "cec.poll_ms")) return "50";
    if (!strcmp(key, "cec.cleanup_delay_ms")) return "0";
    return def;
}
static uint32_t fake_owner_id(void *priv)
{
    (void)priv;
    return 1;
}

static void pause_ms(unsigned ms)
{
    const struct timespec pause = {
        .tv_sec = ms / 1000u,
        .tv_nsec = (long)(ms % 1000u) * 1000000L
    };
    (void)nanosleep(&pause, NULL);
}

int main(void)
{
    const pthread_t main_tid = pthread_self();
    oa_host_api api;
    memset(&api, 0, sizeof api);
    api.size = sizeof api;
    api.abi = OA_ABI_VERSION;
    api.log = fake_log;
    api.publish = fake_publish;
    api.unpublish = fake_unpublish;
    api.timer_add = fake_timer_add;
    api.timer_del = fake_timer_del;
    api.post = fake_post;
    api.config_get = fake_config;
    api.owner_id = fake_owner_id;

    void *opaque = NULL;
    assert(init(&api, &opaque) == 0);
    struct ctx *c = opaque;
    assert(start(c) == 0);
    assert(authority_open_calls == 1 && publish_calls == 2);

    /* Complete the initial acquire so session_open is set. */
    for (int i = 0; i < 100 && c->session_open == 0; i++) {
        pause_ms(5);
        cec_poll(c);
    }
    assert(c->session_open == 1 && c->job_pending == 0 &&
           device_open_calls == 1);

    /* Suspend with an open session: state clears immediately and the cleanup
     * worker performs the join/close. The host provides no retry for a module
     * left SUSPENDED, so suspend must not return success before that worker
     * has finished. */
    assert(suspend(c) == 0);
    assert(c->session_open == 0 && c->opening == 0 &&
           c->job_pending == 0 && c->closing == 0);
    assert(c->poll_timer == 0);
    assert(c->owner == NULL && c->cleanup_owner == NULL);
    assert(c->cleanup_running == 0);
    assert(c->authority_open == 0 && c->authority.map == NULL);
    assert(authority_close_calls == 1);
    assert(device_close_calls == 1);
    /* The blocking join ran on the cleanup worker, not the caller thread. */
    assert(join_calls == 1 && !pthread_equal(join_tid, main_tid));
    /* A retry after the worker finished must be idempotent, not stuck. */
    assert(suspend(c) == 0);

    /* Resume reopens authority and submits a fresh acquire. */
    int rc = resume(c);
    assert(rc == 0);
    assert(c->cleanup_running == 0 && c->cleanup_owner == NULL);
    assert(c->owner != NULL);
    assert(c->authority_open == 1 && c->authority.map == sentinel);
    assert(authority_close_calls == 1 && authority_open_calls == 2);
    assert(device_close_calls == 1);
    assert(c->poll_timer != 0);
    assert(c->opening == 1 && c->job_pending == 1 && c->session_open == 0);

    /* The resumed owner must complete a fresh acquire. */
    for (int i = 0; i < 100 && c->session_open == 0; i++) {
        pause_ms(5);
        cec_poll(c);
    }
    assert(c->session_open == 1 && c->opening == 0 && c->job_pending == 0);
    assert(device_open_calls == 2);

    assert(quiesce(c) == -EAGAIN);
    for (int i = 0; i < 100 && quiesce(c) != 0; i++)
        pause_ms(10);
    stop(c);
    destroy(c);
    puts("ok cec suspend/resume: open state cleared, cleanup worker joins, "
         "resume reopens and re-acquires");
    return 0;
}
