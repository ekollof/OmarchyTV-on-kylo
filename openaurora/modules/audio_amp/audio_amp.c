/*
 * mod_audio_amp — gated AD82587 Init lifecycle.
 *
 * The blocking vendor Init runs on the existing amp_owner worker; the host
 * loop only submits, polls the completion and copies state. Hardware is
 * disabled by default:
 *
 *   audio_amp.enable = 0|1        explicit hardware gate
 *   audio_amp.hwprofile = PATH    EFSL profile (default /conf/mb230_hwprofile.bin)
 *   audio_amp.cleanup_delay_ms    diagnostic async-quiesce delay
 *
 * When enabled, start requires a ready plat service (platform GPIO/video
 * prelude, including NDAL_VideoExt_OpenDevice(0,10) for the STBC global fd),
 * reads the profile, opens the fixed MB230 AD82587 I2C session for writing and
 * creates the amp owner with the recovered cold-boot image. Nothing is
 * written until `amp-init` is requested. The service is reloadable: quiesce
 * unpublishes it, detaches the owner/I2C session and finishes the blocking
 * join plus close on a worker.
 */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/amp_board.h"
#include "openaurora/amp_i2c.h"
#include "openaurora/amp_init.h"
#include "openaurora/amp_owner.h"
#include "openaurora/amp_vendor.h"
#include "openaurora/audio_amp.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    int published;
    int enabled;
    const oa_plat_iface *plat;
    oa_amp_i2c *i2c;
    oa_amp_owner *owner;
    oa_amp_board board;
    oa_amp_vendor vendor;
    oa_amp_init_io init_io;
    oa_ad82587_profile profile;
    char power[32];
    int submitted;
    int have_result;
    oa_amp_completion result;
    int mute_known;
    int mute_state;

    /* ABI 3 quiesce: detach the owner/session, then join and close on a
     * worker. The host retries until the cleanup thread exits. */
    pthread_t cleanup_thread;
    int cleanup_running;
    oa_amp_owner *cleanup_owner;
    oa_amp_i2c *cleanup_i2c;
    int cleanup_result;
    unsigned cleanup_delay_ms;
};

static struct ctx *g_ctx;

static int load_profile(const char *path, oa_ad82587_profile *profile,
                        char power[32])
{
    if (!path || !*path || !profile || !power)
        return -EINVAL;
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0x44 ||
        st.st_size > 16 * 1024 * 1024) {
        close(fd);
        return -EINVAL;
    }
    size_t size = (size_t)st.st_size;
    unsigned char *data = malloc(size + 1);
    if (!data) {
        close(fd);
        return -ENOMEM;
    }
    size_t got = 0;
    while (got < size) {
        ssize_t n = read(fd, data + got, size - got);
        if (n > 0) {
            got += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        free(data);
        close(fd);
        return -EIO;
    }
    close(fd);
    int rc = oa_ad82587_profile_from_hw(data, size, profile, power);
    free(data);
    return rc ? -rc : 0;
}

static void poll_result(struct ctx *c)
{
    if (!c->owner || !c->submitted)
        return;
    oa_amp_completion completion;
    int rc = oa_amp_owner_take(c->owner, &completion);
    if (rc == 1) {
        c->result = completion;
        c->have_result = 1;
        c->submitted = 0;
        /* Init ends with the vendor Mute(1) step. The caller must unmute
         * through amp-mute 0 when the audio path is ready; the stock boot
         * flow does the same after DEV_AMP_Init. */
        c->mute_known = 1;
        c->mute_state = completion.state.mute_cached ? 1 : 0;
    } else if (rc < 0) {
        c->submitted = 0;
        c->api->log(c->api->priv, OA_LOG_ERROR,
                    "audio_amp: completion take failed (%d)", rc);
    }
}

static int iface_init(void)
{
    struct ctx *c = g_ctx;
    if (!c || !c->published || !c->enabled || !c->owner)
        return -ENODEV;
    poll_result(c);
    if (c->submitted || c->have_result)
        return -EBUSY;
    uint64_t ticket = 0;
    int rc = oa_amp_owner_submit(c->owner, &c->profile, &ticket);
    if (rc)
        return rc;
    c->submitted = 1;
    c->have_result = 0;
    c->mute_known = 0;
    memset(&c->result, 0, sizeof c->result);
    c->api->log(c->api->priv, OA_LOG_INFO,
                "audio_amp: init submitted (ticket %llu)",
                (unsigned long long)ticket);
    return 0;
}

static int iface_state(oa_audio_amp_status *out)
{
    struct ctx *c = g_ctx;
    if (!c || !out || !c->published)
        return -ENODEV;
    poll_result(c);
    memset(out, 0, sizeof *out);
    out->enabled = c->enabled;
    out->busy = c->submitted;
    out->have_result = c->have_result;
    if (c->have_result) {
        out->ticket = c->result.ticket;
        out->native_result = c->result.native_result;
        out->reset_cached = c->result.state.reset_cached;
        out->mute_cached = c->result.state.mute_cached;
    } else {
        out->native_result = -1;
    }
    out->mute_known = c->mute_known;
    out->mute = c->mute_known ? c->mute_state : 0;
    return 0;
}

static int iface_set_mute(int on)
{
    struct ctx *c = g_ctx;
    if (!c || !c->published || !c->enabled)
        return -ENODEV;
    if (on != 0 && on != 1)
        return -EINVAL;
    int ok = oa_amp_vendor_level(&c->vendor, c->board.mute.pin,
                                 (uint32_t)on);
    if (!ok)
        return -EIO;
    c->mute_known = 1;
    c->mute_state = on;
    c->api->log(c->api->priv, OA_LOG_INFO, "audio_amp: mute -> %d", on);
    return 0;
}

static const oa_audio_amp_iface g_iface = { iface_init, iface_state,
                                            iface_set_mute };

static void release_plat(struct ctx *c)
{
    if (c->plat) {
        c->api->release(c->api->priv, OA_SVC_PLAT);
        c->plat = NULL;
    }
}

static void finish_owner(oa_amp_owner *owner, int *result)
{
    if (!owner)
        return;
    int rc = oa_amp_owner_quiesce(owner);
    while (rc == -EBUSY) {
        const struct timespec pause = { .tv_nsec = 5000000 };
        (void)nanosleep(&pause, NULL);
        rc = oa_amp_owner_join(owner);
    }
    if (rc == 0) {
        if (oa_amp_owner_destroy(owner) != 0 && result)
            *result = -1;
    } else if (result) {
        *result = -1;
    }
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
    oa_amp_owner *owner = c->cleanup_owner;
    c->cleanup_owner = NULL;
    int result = 0;
    finish_owner(owner, &result);
    oa_amp_i2c *i2c = c->cleanup_i2c;
    c->cleanup_i2c = NULL;
    if (i2c && oa_amp_i2c_close(&i2c) != 0)
        result = -1;
    c->cleanup_result = result;
    return NULL;
}

static void teardown_now(struct ctx *c)
{
    if (c->published) {
        c->api->unpublish(c->api->priv, OA_SVC_AUDIO_AMP);
        c->published = 0;
    }
    release_plat(c);
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    oa_amp_owner *owner = c->cleanup_owner;
    c->cleanup_owner = NULL;
    if (!owner)
        owner = c->owner;
    c->owner = NULL;
    int result = 0;
    finish_owner(owner, &result);
    oa_amp_i2c *i2c = c->cleanup_i2c;
    c->cleanup_i2c = NULL;
    if (!i2c)
        i2c = c->i2c;
    c->i2c = NULL;
    if (i2c)
        (void)oa_amp_i2c_close(&i2c);
}

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION ||
        !out)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    g_ctx = c;
    *out = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->published || c->cleanup_running || c->owner || c->i2c)
        return -EBUSY;
    c->submitted = 0;
    c->have_result = 0;
    c->mute_known = 0;
    c->mute_state = 0;
    memset(&c->result, 0, sizeof c->result);
    long delay = atol(c->api->config_get(c->api->priv,
                                         "audio_amp.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    const char *enable = c->api->config_get(c->api->priv,
                                             "audio_amp.enable", "0");
    c->enabled = enable && (!strcmp(enable, "1") || !strcmp(enable, "true"));

    if (c->enabled) {
        c->plat = c->api->lookup(c->api->priv, OA_SVC_PLAT, 1);
        if (!c->plat || c->plat->is_ready() != 1 ||
            c->plat->stage_errors() != 0) {
            c->api->log(c->api->priv, OA_LOG_ERROR,
                        "audio_amp: platform is not ready; refusing hardware");
            release_plat(c);
            return -ENODEV;
        }
        const char *path = c->api->config_get(
            c->api->priv, "audio_amp.hwprofile",
            "/conf/mb230_hwprofile.bin");
        int rc = load_profile(path, &c->profile, c->power);
        if (rc) {
            c->api->log(c->api->priv, OA_LOG_ERROR,
                        "audio_amp: profile '%s' rejected (%d)", path, rc);
            release_plat(c);
            return rc;
        }
        rc = oa_amp_i2c_open(&c->i2c, 1, NULL);
        if (rc) {
            c->api->log(c->api->priv, OA_LOG_ERROR,
                        "audio_amp: I2C open failed (%d)", rc);
            release_plat(c);
            return rc;
        }
        oa_amp_board_defaults(&c->board);
        oa_amp_vendor_io vio;
        memset(&vio, 0, sizeof vio);
        if (oa_amp_vendor_init(&c->vendor, &vio, c->i2c, &c->board,
                               &c->init_io) != 0) {
            (void)oa_amp_i2c_close(&c->i2c);
            release_plat(c);
            return -EINVAL;
        }
        oa_amp_init_state initial;
        memset(&initial, 0, sizeof initial);
        memcpy(initial.image, oa_ad82587_baseline, OA_AD82587_REGISTERS);
        rc = oa_amp_owner_create(&c->owner, &initial, &c->init_io);
        if (rc) {
            c->api->log(c->api->priv, OA_LOG_ERROR,
                        "audio_amp: owner create failed (%d)", rc);
            (void)oa_amp_i2c_close(&c->i2c);
            release_plat(c);
            return rc;
        }
    }

    if (c->api->publish(c->api->priv, OA_SVC_AUDIO_AMP, 1, &g_iface) != 0) {
        teardown_now(c);
        return -1;
    }
    c->published = 1;
    c->api->log(c->api->priv, OA_LOG_INFO,
                "audio_amp: %s (profile '%s', %u registers)",
                c->enabled ? "hardware enabled" : "disabled",
                c->enabled ? c->profile.speaker_power : "-",
                OA_AD82587_REGISTERS);
    return 0;
}

static void m_stop(void *v)
{
    teardown_now(v);
}

static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c)
        return 0;
    if (c->cleanup_running) {
        for (int i = 0; i < 10; i++) {
            int rc = pthread_tryjoin_np(c->cleanup_thread, NULL);
            if (rc == 0) {
                c->cleanup_running = 0;
                int result = c->cleanup_result;
                c->cleanup_result = 0;
                if (result) {
                    c->api->log(c->api->priv, OA_LOG_ERROR,
                                "audio_amp: cleanup failed (%d)", result);
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

    if (c->published) {
        c->api->unpublish(c->api->priv, OA_SVC_AUDIO_AMP);
        c->published = 0;
    }
    c->enabled = 0;
    release_plat(c);
    if (!c->owner && !c->i2c) {
        c->submitted = 0;
        return 0;
    }
    c->cleanup_owner = c->owner;
    c->owner = NULL;
    c->cleanup_i2c = c->i2c;
    c->i2c = NULL;
    c->submitted = 0;
    int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
    if (rc) {
        /* Rare fallback: finish on the loop rather than leaking devices. */
        int result = 0;
        finish_owner(c->cleanup_owner, &result);
        c->cleanup_owner = NULL;
        if (c->cleanup_i2c) {
            (void)oa_amp_i2c_close(&c->cleanup_i2c);
            c->cleanup_i2c = NULL;
        }
        return -EIO;
    }
    c->cleanup_running = 1;
    return -EAGAIN;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (!c)
        return;
    teardown_now(c);
    if (g_ctx == c)
        g_ctx = NULL;
    free(c);
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof g_module,
    .name = "audio_amp",
    .version = "0.1",
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
    .quiesce = m_quiesce,
};

OA_MODULE_EXPORT(g_module)
