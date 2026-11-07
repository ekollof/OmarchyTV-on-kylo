/* CEC authority-lifetime regression: a timed-out quiesce generation must
 * keep the /dev/mem mapping alive while cleanup_owner is retained; only a
 * successful join may unmap it. Uses a fake host and wrapped owner/authority
 * functions, so it runs on the host without devices.
 *
 * The module source is included to reach its static lifecycle functions and
 * ctx internals; this is a focused test binary, not a module.
 */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "openaurora/cec_owner.h"
#include "openaurora/cec_authority_mem.h"
#include "openaurora/module.h"

#include "../modules/cec/cec.c"

static int join_fail = 1;
static int open_calls;
static int close_calls;
static int publish_calls;
static void *const sentinel = (void *)(uintptr_t)0x1234;

int __real_oa_cec_owner_join(oa_cec_owner *owner);
int __wrap_oa_cec_owner_join(oa_cec_owner *owner)
{
    if (join_fail) {
        errno = EBUSY;
        return -EBUSY;
    }
    return __real_oa_cec_owner_join(owner);
}

static int fake_read(void *context, uint32_t *value)
{
    (void)context;
    *value = 2; /* authority bit 1 already set */
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
    open_calls++;
    return 0;
}
void __wrap_oa_cec_authority_mem_close(oa_cec_authority_mem *m)
{
    assert(m && m->map == sentinel);
    close_calls++;
    m->map = NULL;
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
    if (!strcmp(key, "cec.device")) return "/tmp/oa-cec-lifetime-missing";
    if (!strcmp(key, "cec.poll_ms")) return "50";
    if (!strcmp(key, "cec.cleanup_delay_ms")) return "0";
    return def;
}
static uint32_t fake_owner_id(void *priv)
{
    (void)priv;
    return 1;
}

int main(void)
{
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
    assert(open_calls == 1 && publish_calls == 2);

    /* First generation: the wrapped join always returns EBUSY, so the owner
     * is retained and the authority mapping must stay alive. */
    assert(quiesce(c) == -EAGAIN);
    for (int i = 0; i < 100 && c->cleanup_running; i++) {
        const struct timespec pause = { .tv_nsec = 10000000 };
        (void)nanosleep(&pause, NULL);
        (void)quiesce(c);
    }
    assert(!c->cleanup_running);
    assert(c->cleanup_owner != NULL);
    assert(c->authority_open == 1);
    assert(c->authority.map == sentinel);
    assert(close_calls == 0); /* regression: must not unmap yet */

    /* Second generation: joins succeed, so the retained owner and mapping are
     * finally released. */
    join_fail = 0;
    int rc = -EAGAIN;
    for (int i = 0; i < 100; i++) {
        rc = quiesce(c);
        if (rc == 0) break;
        const struct timespec pause = { .tv_nsec = 10000000 };
        (void)nanosleep(&pause, NULL);
    }
    assert(rc == 0);
    assert(c->cleanup_owner == NULL);
    assert(c->authority_open == 0);
    assert(c->authority.map == NULL);
    assert(close_calls == 1);

    destroy(c);
    puts("ok cec authority lifetime: mapping retained across a timed-out "
         "quiesce, unmapped after the successful generation");
    return 0;
}
