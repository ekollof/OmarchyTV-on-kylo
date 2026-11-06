/*
 * mod_power — incomplete RAM-suspend primitive, not native standby parity.
 *
 * Rechecked against aurora's ice_hardware_SuspendToRam() and idtvSuspend/Resume:
 * artifacts/reports/POWER-STANDBY-CONTRACT-2026-10-31.md.
 *
 *   park CI -> disable IR -> flush NVRAM -> set the MCU wake timer -> sync()
 *   open("/sys/power/state"), write("mem", 3)      <-- the actual suspend
 *   ... returns after resume ...
 *   re-init NVRAM -> clean UART -> re-enable IR -> tell middleware
 *
 * The vendor's idtv stages additionally own WoL/WoW/PHY policy, audio/tuner/VBI,
 * picture settings, UART synchronization and device restoration. Those stages
 * remain incomplete here. The host coordinates quiescence/entry/restoration;
 * missing hooks refuse before entry. Historical raw mem/WoL
 * success does not prove that native ownership can omit them. The monitor-reset
 * helper changes vendor task state; it is not the hardware watchdog off-switch.
 *
 * Safety: writing "mem" suspends *any* Linux box, including the build laptop. So the
 * module only arms itself where the vendor node exists (/dev/ntkled on this chassis),
 * and can be forced either way with power.enable=0|1.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "openaurora/module.h"
#include "openaurora/services.h"

#define POWER_STATE "/sys/power/state"

struct ctx {
    const oa_host_api *api;
    const oa_sleep_iface *sleep;
    uint32_t owner;
    int enabled;
    int sleeps; /* accepted full mem writes, not independent proof of RAM sleep */
    int requesting;
    int published;
    oa_sleep_result last;
};

static struct ctx *g_ctx;

static int tv_looks_like_the_set(void)
{
    return access("/dev/ntkled", F_OK) == 0;
}

static int enter_ram(void *v)
{
    struct ctx *c = v;
    c->api->log(c->api->priv, OA_LOG_INFO, "entering RAM suspend: %s", POWER_STATE);
    sync(); /* vendor outer path syncs after preparation, before power-state open */
    int fd = open(POWER_STATE, O_WRONLY);
    if (fd < 0) {
        c->api->log(c->api->priv, OA_LOG_ERROR, "open %s failed", POWER_STATE);
        return -1;
    }

    /* this call returns once the machine has resumed */
    ssize_t n = write(fd, "mem", 3);
    close(fd);

    if (n == 3) c->sleeps++;
    c->api->log(c->api->priv, n == 3 ? OA_LOG_INFO : OA_LOG_ERROR,
                 "RAM entry write returned %zd", n);
    return n == 3 ? 0 : -1;
}

static int iface_standby(void)
{
    struct ctx *c = g_ctx;
    if (!c || !c->enabled || !c->sleep || c->requesting) return -1;
    c->requesting = 1;
    int rc = c->sleep->cycle(c->sleep->context, c->owner, enter_ram, c, &c->last);
    c->requesting = 0;
    if (rc != 0)
        c->api->log(c->api->priv, OA_LOG_WARN, "standby failed: prepare=%d attempted=%d entry=%d restore=%d",
                     c->last.prepare_rc, c->last.entry_attempted, c->last.entry_rc, c->last.restore_rc);
    return rc;
}

static int iface_available(void)
{
    return g_ctx && g_ctx->sleep ? g_ctx->enabled : 0;
}

static const oa_power_iface g_iface = { iface_standby, iface_available };

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    c->owner = api->owner_id(api->priv);
    *out = c;
    g_ctx = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    c->sleep = c->api->lookup(c->api->priv, OA_SVC_SLEEP, 1);
    if (!c->sleep) return -1;
    const char *force = c->api->config_get(c->api->priv, "power.enable", NULL);
    if (force)
        c->enabled = atoi(force) != 0;
    else
        c->enabled = tv_looks_like_the_set();

    if (c->enabled)
        c->api->log(c->api->priv, OA_LOG_INFO, "standby enabled (%s)", POWER_STATE);
    else
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "standby disabled: not on the TV and power.enable not set (safe for host tests)");

    int rc = c->api->publish(c->api->priv, OA_SVC_POWER, 1, &g_iface);
    c->published = rc == 0;
    return rc;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    /* No quiesce hook: nothing is deferable. The host.sleep hold is a refcount
     * drop, and the only sysfs fd exists inside the final entry callback (which
     * cannot overlap this loop-owned lifecycle call). */
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_POWER);
    c->published = 0;
    c->api->release(c->api->priv, OA_SVC_SLEEP);
    c->sleep = NULL;
}

static void m_destroy(void *v)
{
    if (g_ctx == v)
        g_ctx = NULL;
    free(v);
}

/* No persistent device or worker is owned by this module. Preparation/resume
 * are empty; the sysfs fd exists only inside the host's final entry callback.
 * Ordinary suspend/resume commands must never enter RAM or announce ON early. */
static int m_suspend(void *v)
{
    (void)v;
    return 0;
}

static int m_resume(void *v)
{
    (void)v;
    return 0;
}

static size_t m_save(void *v, void *buf, size_t cap)
{
    struct ctx *c = v;
    if (!buf || cap < sizeof c->sleeps)
        return sizeof c->sleeps;
    memcpy(buf, &c->sleeps, sizeof c->sleeps);
    return sizeof c->sleeps;
}

static int m_restore(void *v, const void *buf, size_t len)
{
    struct ctx *c = v;
    if (len != sizeof c->sleeps)
        return -1;
    memcpy(&c->sleeps, buf, sizeof c->sleeps);
    return 0;
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "power",
    .version = "0.1",
    .requires = { OA_SVC_SLEEP, "", 0 },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
    .suspend = m_suspend,
    .resume = m_resume,
    .save = m_save,
    .restore = m_restore,
};

OA_MODULE_EXPORT(g_module)
