/*
 * mod_source — input switching through the vendor VIN HAL.
 *
 * Decompiled from libNDAL3.so / aurora (see OPENAURORA-T1-PLANS-2026-10-30.md):
 *
 *   int Hal_Vin_Open(int idx);                   // opens /dev/ntkdvif, returns fd
 *   void Hal_Vin_SelectInput(int fd, int id);    // ioctl(fd, 0xc0045627, &id)
 *   void Hal_Vin_SetStreamStart(int fd);         // ioctl(fd, 0x40045612, &{1})
 *
 * Aurora's `plat_vin_slot_connect(slot, type)` also uses a companion VPLT command
 * and Hal_Video_Set_V4l2Info; this module does not reproduce that complete path
 * or audio routing. The input id comes from a 26-entry table (`.data 0x013b9e78`):
 *
 *   type 0->1, 5->0, 10->2, 15->1, 16->2, 17/18/20/21->3, 19->66, 25->255, others 0
 *
 * Switching changes what is on screen, so it is **disabled by default**: the module opens the device and
 * publishes the service, but only acts when `source.enable=1`.
 */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

#ifdef OA_NO_VENDOR
static int have_hal(void)
{
    return 0;
}
static int Hal_Vin_Open(int idx)
{
    (void)idx;
    return -1;
}
static void Hal_Vin_Close(int fd)
{
    (void)fd;
}
static void Hal_Vin_SelectInput(int fd, int id)
{
    (void)fd;
    (void)id;
}
static void Hal_Vin_SetStreamStart(int fd)
{
    (void)fd;
}
#else
extern int Hal_Vin_Open(int idx);
extern void Hal_Vin_Close(int fd);
extern void Hal_Vin_SelectInput(int fd, int id);
extern void Hal_Vin_SetStreamStart(int fd);
static int have_hal(void)
{
    return 1;
}
#endif

/* source type -> Hal_Vin input id, read out of aurora's .data table */
static const int g_input_id[26] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0..9  */
    2, 0, 0, 0, 0, 1, 2, 3, 3, 66,  /* 10..19 */
    3, 3, 0, 0, 0, 255              /* 20..25 */
};

struct ctx {
    const oa_host_api *api;
    int fd;
    int enabled;
    int current; /* -1 = unknown */
    /* ABI 3 quiesce: Hal_Vin_Close is handed to a worker after the service
     * is unpublished and the fd is detached. */
    pthread_t cleanup_thread;
    int cleanup_running;
    int close_fd;
    unsigned cleanup_delay_ms;
};

static struct ctx *g_ctx;

static int select_type(int type)
{
    struct ctx *c = g_ctx;
    if (!c || c->fd < 0 || type < 0 || type > 25)
        return -1;
    if (!c->enabled)
        return -2;

    Hal_Vin_SelectInput(c->fd, g_input_id[type]);
    Hal_Vin_SetStreamStart(c->fd);
    c->current = type;
    c->api->log(c->api->priv, OA_LOG_INFO, "source -> type %d (vin id %d)", type, g_input_id[type]);
    return 0;
}

static int iface_select(int type)
{
    return select_type(type);
}
static int iface_current(void)
{
    return g_ctx ? g_ctx->current : -1;
}
static int iface_available(void)
{
    return g_ctx && g_ctx->fd >= 0 ? 1 : 0;
}

static const oa_source_iface g_iface = { iface_select, iface_current, iface_available };

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    c->fd = -1;
    c->close_fd = -1;
    c->current = -1;
    *out = c;
    g_ctx = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "source.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    c->cleanup_running = 0;
    c->close_fd = -1;
    c->current = -1;
    c->fd = -1;
    const char *en = c->api->config_get(c->api->priv, "source.enable", "0");
    c->enabled = atoi(en) != 0 && have_hal();

    if (!have_hal()) {
        c->api->log(c->api->priv, OA_LOG_WARN, "source: built without the vendor HAL");
    } else {
        c->fd = Hal_Vin_Open(0);
        if (c->fd < 0)
            c->api->log(c->api->priv, OA_LOG_WARN, "source: Hal_Vin_Open(0) failed (/dev/ntkdvif)");
        else
            c->api->log(c->api->priv, OA_LOG_INFO, "source: VIN open (fd %d), switching %s", c->fd,
                        c->enabled ? "enabled" : "disabled (source.enable=1 to allow)");
    }

    if (c->api->publish(c->api->priv, OA_SVC_SOURCE, 1, &g_iface) != 0)
        c->api->log(c->api->priv, OA_LOG_WARN, "source service already published");
    return 0;
}

/* No host API calls and no ctx access except the fd/delay fields captured
 * before the thread starts. */
static void *cleanup_worker(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_delay_ms) {
        const struct timespec pause = {
            .tv_sec = c->cleanup_delay_ms / 1000u,
            .tv_nsec = (long)(c->cleanup_delay_ms % 1000u) * 1000000L
        };
        (void)nanosleep(&pause, NULL);
    }
    int fd = c->close_fd;
    c->close_fd = -1;
    if (fd >= 0) Hal_Vin_Close(fd);
    return NULL;
}

/* ABI 3 gate: unpublish the source service and detach the fd synchronously,
 * then close it on a worker. A stale iface call sees fd < 0 and returns -1. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running) {
        for (int i = 0; i < 10; i++) {
            int rc = pthread_tryjoin_np(c->cleanup_thread, NULL);
            if (rc == 0) {
                c->cleanup_running = 0;
                return 0;
            }
            if (rc != EBUSY) {
                c->cleanup_running = 0;
                return -EIO;
            }
            const struct timespec pause = { .tv_nsec = 5000000 };
            (void)nanosleep(&pause, NULL);
        }
        return -EAGAIN;
    }

    c->api->release(c->api->priv, OA_SVC_SOURCE);
    c->api->unpublish(c->api->priv, OA_SVC_SOURCE);
    if (c->fd < 0) return 0;

    int fd = c->fd;
    c->fd = -1;
    c->current = -1;
    if (!c->cleanup_delay_ms) {
        Hal_Vin_Close(fd);
        return 0;
    }
    c->close_fd = fd;
    int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
    if (rc) {
        Hal_Vin_Close(c->close_fd);
        c->close_fd = -1;
        return -EIO;
    }
    c->cleanup_running = 1;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    c->api->release(c->api->priv, OA_SVC_SOURCE);
    c->api->unpublish(c->api->priv, OA_SVC_SOURCE);
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->close_fd >= 0) {
        Hal_Vin_Close(c->close_fd);
        c->close_fd = -1;
    }
    if (c->fd >= 0) {
        Hal_Vin_Close(c->fd);
        c->fd = -1;
    }
    c->current = -1;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->close_fd >= 0) Hal_Vin_Close(c->close_fd);
    if (c->fd >= 0) Hal_Vin_Close(c->fd);
    if (g_ctx == v)
        g_ctx = NULL;
    free(v);
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "source",
    .version = "0.1",
    .requires = { OA_SVC_PLAT, "" },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
    .quiesce = m_quiesce,
};

OA_MODULE_EXPORT(g_module)
