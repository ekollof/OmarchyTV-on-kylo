/*
 * mod_audio — volume and mute through the vendor audio HAL.
 *
 * Hal_Audio_Ctrl returns an int status in r0; earlier void declarations were
 * incorrect. See CEC-AUDIO-CONTRACT-2026-10-31. The parameter block is 56 bytes; each
 * opcode uses the words it cares about:
 *
 *   0x33  main volume   params[0] = volume            (aurora: PLAT_DEC_SetAudioMainVolume)
 *   0x35  speaker mute  params[0] = speaker dev id,
 *                       params[1] = 0/1               (aurora: PLAT_AOUT_SetSpeakerHWMute)
 *   0x3a  I2S MCLK      params[0] = 0x100             (aurora: DEV_AMP_Init)
 *
 * This controls decoder main volume and one speaker HAL mute, not the complete
 * vendor UI volume/routing path. Speaker gain also uses Hal_Audio_EasySet and
 * amplifier control has a separate vtable. The live amp is ESMT AD82587_D;
 * AD82586C is another firmware-supported driver, not this set's selected amp.
 * See AUDIO-ROUTING-2026-11-01. mod_plat owns the audio prelude; do not repeat it
 * on start/reload. Amp init/routing and audible acceptance remain open.
 */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/audio_owner.h"
#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/vendor_audio.h"

#define AUDIO_PARAM_WORDS OA_AUDIO_PARAM_WORDS

#ifdef OA_NO_VENDOR
static int have_hal(void)
{
    return 0;
}
static int Hal_Audio_Ctrl(int opcode, uint32_t *params)
{
    (void)opcode;
    (void)params;
    return -1;
}
#else
static int have_hal(void)
{
    return 1;
}
#endif

struct ctx {
    const oa_host_api *api;
    oa_audio_owner *owner;
    uint32_t id;
    int enabled;
    int volume;
    int muted;
    int restored;
    int published;
    const oa_plat_iface *plat;
    /* ABI 3 quiesce: the owner is detached and finished on a worker. */
    pthread_t cleanup_thread;
    int cleanup_running;
    oa_audio_owner *cleanup_owner;
    int cleanup_result;
    unsigned cleanup_delay_ms;
    int quiescing;
};

static struct ctx *g_ctx;

static int on_the_tv(void)
{
    return access("/dev/ntkaud0", F_OK) == 0;
}

/* CEC 1.3a 13.15: while System Audio Mode is on the TV must not change its
 * own volume. Absence of the policy service leaves the local path alone. */
static int cec_blocks_local(struct ctx *c)
{
    if (!c->api || !c->api->lookup) return 0;
    const oa_cec_audio_iface *cec = c->api->lookup(c->api->priv, OA_SVC_CEC_AUDIO, 1);
    if (!cec) return 0;
    int blocked = cec->local_volume_permitted && !cec->local_volume_permitted();
    if (c->api->release) c->api->release(c->api->priv, OA_SVC_CEC_AUDIO);
    return blocked;
}

static int apply_volume(struct ctx *c, int v)
{
    if (cec_blocks_local(c) || !c->enabled)
        return -1;
    if (v < 0) v = 0;
    if (v > 100) v = 100;

    uint32_t params[AUDIO_PARAM_WORDS];
    memset(params, 0, sizeof params);
    params[0] = (unsigned)v;
    int rc = Hal_Audio_Ctrl(0x33, params);
    if (rc) return rc;

    c->volume = v;
    c->api->log(c->api->priv, OA_LOG_INFO, "volume -> %d", v);
    return 0;
}

static int apply_mute(struct ctx *c, int on)
{
    if (cec_blocks_local(c) || !c->enabled)
        return -1;

    uint32_t params[AUDIO_PARAM_WORDS];
    memset(params, 0, sizeof params);
    params[0] = 0; /* Registered speaker 0 confirmed; physical amp/routing ownership remains open. */
    params[1] = on ? 1u : 0u;
    int rc = Hal_Audio_Ctrl(0x35, params);
    if (rc) return rc;

    c->muted = on ? 1 : 0;
    c->api->log(c->api->priv, OA_LOG_INFO, "mute -> %d", c->muted);
    return 0;
}

static int iface_set_volume(int v)
{
    return g_ctx ? apply_volume(g_ctx, v) : -1;
}
static int iface_get_volume(void)
{
    return g_ctx ? g_ctx->volume : -1;
}
static int iface_set_mute(int on)
{
    return g_ctx ? apply_mute(g_ctx, on) : -1;
}
static int iface_available(void)
{
    return g_ctx ? g_ctx->enabled : 0;
}
static int iface_bind(const void *io)
{
    /* owner_open is a synchronous RPC even though I/O executes on a worker.
     * A service call runs on the host loop. Do not expose blocking injected
     * I/O here until the service has an asynchronous completion contract. */
    (void)io;
    errno = ENOTSUP;
    return -1;
}

static const oa_audio_iface g_iface = { iface_set_volume, iface_get_volume, iface_set_mute,
                                        iface_available, iface_bind };

static void poke(void *ud) { (void)ud; }
static void settled(void *ud)
{
    struct ctx *c = ud;
    if (c && !c->quiescing && c->api && c->api->post && c->id)
        (void)c->api->post(c->api->priv, c->id, poke, c);
}
/* Finish one owner off the host loop. Quiesce signals shutdown; join may
 * return EBUSY until the worker observes it, so poll with a short sleep. */
static int finish_owner_ptr(oa_audio_owner *owner)
{
    if (!owner) return 0;
    (void)oa_audio_owner_quiesce(owner);
    for (;;) {
        if (oa_audio_owner_join(owner) == 0) break;
        if (errno != EBUSY) {
            oa_audio_owner_destroy(owner);
            return -1;
        }
        const struct timespec pause = { .tv_nsec = 5000000 };
        (void)nanosleep(&pause, NULL);
    }
    oa_audio_owner_destroy(owner);
    return 0;
}

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
    oa_audio_owner *owner = c->cleanup_owner;
    c->cleanup_owner = NULL;
    c->cleanup_result = finish_owner_ptr(owner);
    return NULL;
}

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    c->id = api->owner_id ? api->owner_id(api->priv) : 0;
    c->volume = -1; c->muted = -1; /* Cached accepted requests, not verified hardware readback. */
    *out = c;
    g_ctx = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->owner || c->cleanup_running || c->cleanup_owner) return -EBUSY;
    c->quiescing = 0;
    c->cleanup_result = 0;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "audio.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    c->plat = c->api->lookup(c->api->priv, OA_SVC_PLAT, 1);
    if (!c->plat || c->plat->is_ready() != 1 || c->plat->stage_errors()) return -1;
    /* Reserve the service before applying any settings. On collision stop()
     * must preserve the peer's publication. */
    if (c->api->publish(c->api->priv, OA_SVC_AUDIO, 1, &g_iface)) return -1;
    c->published = 1;
    if (oa_audio_owner_create(&c->owner, settled, c) != 0) return -1;
    const char *force = c->api->config_get(c->api->priv, "audio.enable", NULL);
    if (force)
        c->enabled = atoi(force) != 0 && have_hal();
    else
        c->enabled = have_hal() && on_the_tv();

    if (!c->enabled) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "audio disabled (no vendor HAL or not on the TV; audio.enable overrides)");
    } else {
        c->api->log(c->api->priv, OA_LOG_INFO, "vendor audio HAL linked");

        const char *vol = c->api->config_get(c->api->priv, "audio.volume", NULL);
        if (c->restored) {
            if ((c->volume >= 0 && apply_volume(c, c->volume)) ||
                (c->muted >= 0 && apply_mute(c, c->muted))) return -1;
        } else if (vol && apply_volume(c, atoi(vol))) return -1;
    }

    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    c->quiescing = 1;
    int published = c->published;
    c->published = 0;
    c->enabled = 0;
    if (published) c->api->unpublish(c->api->priv, OA_SVC_AUDIO);
    if (c->plat) {
        c->api->release(c->api->priv, OA_SVC_PLAT);
        c->plat = NULL;
    }
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->cleanup_owner) {
        (void)finish_owner_ptr(c->cleanup_owner);
        c->cleanup_owner = NULL;
    }
    if (c->owner) {
        oa_audio_owner *owner = c->owner;
        c->owner = NULL;
        (void)finish_owner_ptr(owner);
    }
}

/* ABI 3 gate: unpublish audio and release plat synchronously, then detach the
 * owner and finish it on a worker. The owner can block in a client RPC, so the
 * loop must not join it. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
    if (c->cleanup_running) {
        for (int i = 0; i < 10; i++) {
            int rc = pthread_tryjoin_np(c->cleanup_thread, NULL);
            if (rc == 0) {
                c->cleanup_running = 0;
                int result = c->cleanup_result;
                c->cleanup_result = 0;
                if (result) {
                    c->api->log(c->api->priv, OA_LOG_ERROR,
                                "audio: owner cleanup failed (%d)", result);
                    return -EIO;
                }
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

    c->quiescing = 1;
    int published = c->published;
    c->published = 0;
    c->enabled = 0;
    if (published) c->api->unpublish(c->api->priv, OA_SVC_AUDIO);
    if (c->plat) {
        c->api->release(c->api->priv, OA_SVC_PLAT);
        c->plat = NULL;
    }
    oa_audio_owner *owner = c->owner;
    c->owner = NULL;
    if (!owner) return 0;
    c->cleanup_owner = owner;
    c->cleanup_result = 0;
    int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
    if (rc) {
        c->cleanup_owner = NULL;
        int result = finish_owner_ptr(owner);
        return result ? -EIO : 0;
    }
    c->cleanup_running = 1;
    return -EAGAIN;
}

/* Enabled HAL volume is not a recovered standby sequence, so suspend refuses
 * instead of reporting success. A disabled module only owns an idle worker. */
static int m_suspend(void *v)
{
    struct ctx *c = v;
    if (!c || c->enabled || c->cleanup_running || c->cleanup_owner)
        return -1;
    c->quiescing = 1;
    oa_audio_owner *owner = c->owner;
    c->owner = NULL;
    return finish_owner_ptr(owner) == 0 ? 0 : -1;
}

static int m_resume(void *v)
{
    struct ctx *c = v;
    if (!c || c->cleanup_running || c->cleanup_owner) return -1;
    c->quiescing = 0;
    if (c->owner) return 0;
    return oa_audio_owner_create(&c->owner, settled, c) == 0 ? 0 : -1;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (c->cleanup_owner) {
        (void)finish_owner_ptr(c->cleanup_owner);
        c->cleanup_owner = NULL;
    }
    if (c->owner) {
        oa_audio_owner *owner = c->owner;
        c->owner = NULL;
        (void)finish_owner_ptr(owner);
    }
    if (g_ctx == v)
        g_ctx = NULL;
    free(v);
}

static size_t m_save(void *v, void *buf, size_t cap)
{
    struct ctx *c = v;
    if (!buf || cap < 2)
        return 2;
    ((unsigned char *)buf)[0] = (unsigned char)c->volume;
    ((unsigned char *)buf)[1] = (unsigned char)c->muted;
    return 2;
}

static int m_restore(void *v, const void *buf, size_t len)
{
    struct ctx *c = v;
    if (!buf || len != 2)
        return -1;
    const unsigned char *bytes = buf;
    if ((bytes[0] > 100 && bytes[0] != 255) || (bytes[1] > 1 && bytes[1] != 255)) return -1;
    c->volume = bytes[0] == 255 ? -1 : bytes[0];
    c->muted = bytes[1] == 255 ? -1 : bytes[1];
    c->restored = 1;
    return 0;
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "audio",
    .version = "0.1",
    .requires = { OA_SVC_PLAT, "" },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
    .save = m_save,
    .restore = m_restore,
    .suspend = m_suspend,
    .resume = m_resume,
    .quiesce = m_quiesce,
};

OA_MODULE_EXPORT(g_module)
