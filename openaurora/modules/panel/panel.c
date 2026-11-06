/*
 * mod_panel — backlight and panel power, via the vendor HAL.
 *
 * We link against the firmware's own libNDAL3.so instead of re-implementing the PWM
 * controller. The firmware is fixed for us, so this is a stable link target, not a
 * moving dependency. Fetch the library with scripts/fetch-vendor.sh; the build links
 * it only for the TV glibc target (`make tv`).
 *
 * The call chain was recovered from aurora (see
 * artifacts/reports/OPENAURORA-VENDOR-HAL-2026-10-30.md):
 *
 *   plat_set_dimming()
 *     ├─ Hal_Pwm_Init()
 *     ├─ Hal_Pwm_Open(&{channel, freq, duty, shift, mode, extra})
 *     └─ Hal_Pwm_Set_ByChannel(ch, &{freq, duty 0..4095, shift, mode})
 *
 * The stock process's populated descriptor table and globals calibrate this
 * unit: DEV_PWM_Get_ChIdx(0x6c) -> channel 0 via astPwmBasePin, frequency
 * gu32PanelFreqInHz=30000, set-mode genPwmdimMode=0, current level 1000.
 * Keep them as configuration because the values are board/profile state.
 * panel.curve=1 selects the recovered DEV_PANEL_Init/SetPWMBacklightLv curve
 * (breakpoints 40/60/80/100, level scale 1000) with panel.min_percent as the
 * runtime limit; panel.duty_max is the final 12-bit calibration (default 4095).
 * Default 0 curve keeps the historical linear percent mapping.
 *
 * Host builds define OA_NO_VENDOR and get no-op stubs, so the module still loads and
 * the lifecycle tests keep running on a laptop.
 */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/panel_curve.h"
#include "openaurora/services.h"
#include "openaurora/vendor_pwm.h"

/* Layouts and scalar returns are checked against instructions, not Ghidra's
 * inferred void prototypes. A nonzero result alone does not prove a hardware
 * write (there is also a lock-result shortcut). Preserve the existing request
 * policy until panel behaviour is validated. Set mode must be < 9. */

#ifdef OA_NO_VENDOR
static int hal_ready(void)
{
    return 0;
}
static int Hal_Pwm_Init(void) { return 0; }
static int Hal_Pwm_Open(struct pwm_open_req *d)
{
    (void)d;
    return 0;
}
static int Hal_Pwm_Set_ByChannel(unsigned ch, struct pwm_set_req *r)
{
    (void)ch;
    (void)r;
    return 0;
}
static void Hal_Pwm_Close(void) {}
#else
static int hal_ready(void)
{
    return 1;
}
#endif

struct ctx {
    const oa_host_api *api;
    int channel;
    int freq;
    int mode;    /* DEV_PANEL_SetPWMBacklightLevel's genPwmdimMode */
    int percent; /* what we asked for, 0..100 */
    int raw;     /* optional: pass this duty verbatim, ignoring percent */
    int curve_on;/* panel.curve=1 uses the recovered vendor backlight curve */
    int duty_max;/* panel.duty_max, 1..4095 */
    oa_panel_curve curve;
    int opened;
    int published;
    int needs_cleanup; /* one start cycle is cleaned exactly once */
    /* ABI 3 quiesce: the vendor PWM close runs on a worker. */
    pthread_t cleanup_thread;
    int cleanup_running;
    unsigned cleanup_delay_ms;
};

static struct ctx *g_ctx;

static int apply(struct ctx *c, int pct)
{
    if (!hal_ready())
        return -1;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;

    if (!c->opened) {
        (void)Hal_Pwm_Init();
        struct pwm_open_req d;
        memset(&d, 0, sizeof d);
        d.channel = (uint32_t)c->channel;
        d.freq = (uint32_t)c->freq;
        d.duty = 4095;   /* aurora's call site opens at full range */
        d.shift = 0;
        d.mode = 1;      /* desc[4] == 1 at aurora's call site */
        (void)Hal_Pwm_Open(&d);
        c->opened = 1;
    }

    struct pwm_set_req r;
    memset(&r, 0, sizeof r);
    r.freq = (uint32_t)c->freq;
    r.duty = (uint32_t)(c->raw >= 0 ? c->raw :
             c->curve_on ? oa_panel_percent_to_duty(&c->curve, pct, c->duty_max) :
             pct * c->duty_max / 100);
    r.shift = 0;
    r.mode = (uint32_t)c->mode; /* genPwmdimMode; must be < 9 */
    (void)Hal_Pwm_Set_ByChannel((unsigned)c->channel, &r);
    c->percent = pct;
    return 0;
}

static int iface_set(int pct)
{
    return g_ctx ? apply(g_ctx, pct) : -1;
}

static int iface_get(void)
{
    return g_ctx ? g_ctx->percent : -1;
}

static const oa_panel_iface g_iface = { iface_set, iface_get };

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    c->raw = -1;
    *out = c;
    g_ctx = c;
    return 0;
}

/* No host API calls after this returns. */
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
    if (hal_ready() && c->opened) {
        Hal_Pwm_Close();
        c->opened = 0;
    }
    return NULL;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->published || c->cleanup_running) return -EBUSY;
    c->cleanup_running = 0;
    c->needs_cleanup = 1;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "panel.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    c->channel = atoi(c->api->config_get(c->api->priv, "panel.channel", "0"));
    c->freq = atoi(c->api->config_get(c->api->priv, "panel.freq", "30000"));
    c->mode = atoi(c->api->config_get(c->api->priv, "panel.mode", "0"));
    if (c->mode < 0 || c->mode > 8) c->mode = 0;
    c->percent = atoi(c->api->config_get(c->api->priv, "panel.start", "100"));
    c->raw = atoi(c->api->config_get(c->api->priv, "panel.raw", "-1"));
    c->curve_on = c->api->config_get(c->api->priv, "panel.curve", "0")[0] == '1';
    c->duty_max = atoi(c->api->config_get(c->api->priv, "panel.duty_max", "4095"));
    if (c->duty_max < 1 || c->duty_max > 4095) c->duty_max = 4095;
    oa_panel_curve_init(&c->curve, 40, 60, 80, 100,
        atoi(c->api->config_get(c->api->priv, "panel.min_percent", "0")) * 10);

    if (!hal_ready()) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "built without vendor HAL (host build): panel control disabled");
    } else {
        c->api->log(c->api->priv, OA_LOG_INFO, "vendor HAL linked: channel %d, %d Hz, start %d%%",
                    c->channel, c->freq, c->percent);
        apply(c, c->percent);
    }

    if (c->api->publish(c->api->priv, OA_SVC_PANEL, 1, &g_iface) != 0) {
        c->api->log(c->api->priv, OA_LOG_WARN, "panel service already published");
    } else {
        c->published = 1;
    }
    return 0;
}

/* ABI 3 gate: unpublish panel synchronously, then close the vendor PWM on a
 * worker after panel.cleanup_delay_ms. The diagnostic delay is honored even
 * when the host build has no vendor HAL, so the lifecycle is testable. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
    if (c->cleanup_running) {
        for (int i = 0; i < 10; i++) {
            int rc = pthread_tryjoin_np(c->cleanup_thread, NULL);
            if (rc == 0) { c->cleanup_running = 0; return 0; }
            if (rc != EBUSY) { c->cleanup_running = 0; return -EIO; }
            const struct timespec pause = { .tv_nsec = 5000000 };
            (void)nanosleep(&pause, NULL);
        }
        return -EAGAIN;
    }
    int published = c->published;
    c->published = 0;
    if (published) c->api->unpublish(c->api->priv, OA_SVC_PANEL);
    if (!c->needs_cleanup) return 0;
    c->needs_cleanup = 0;
    if (!c->cleanup_delay_ms) {
        if (hal_ready() && c->opened) {
            Hal_Pwm_Close();
            c->opened = 0;
        }
        return 0;
    }
    int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
    if (rc) return -EIO;
    c->cleanup_running = 1;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    int published = c->published;
    c->published = 0;
    c->needs_cleanup = 0;
    if (published) c->api->unpublish(c->api->priv, OA_SVC_PANEL);
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    /* deliberately no restore: the driver rejects Set_ByChannel(4095) after a lower
     * level ("Not Support Mode 0"), and the level persists until something sets it. */
    if (hal_ready() && c->opened) {
        Hal_Pwm_Close();
        c->opened = 0;
    }
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    if (hal_ready() && c->opened) Hal_Pwm_Close();
    if (g_ctx == c) g_ctx = NULL;
    free(c);
}

static int m_suspend(void *v)
{
    struct ctx *c = v;
    /* let the stock stack own the PWM controller while the TV is asleep */
    if (hal_ready() && c->opened) {
        Hal_Pwm_Close();
        c->opened = 0;
    }
    return 0;
}

static int m_resume(void *v)
{
    struct ctx *c = v;
    if (hal_ready())
        return apply(c, c->percent ? c->percent : 100);
    return 0;
}

static size_t m_save(void *v, void *buf, size_t cap)
{
    struct ctx *c = v;
    if (!buf || cap < 1)
        return 1;
    ((unsigned char *)buf)[0] = (unsigned char)c->percent;
    return 1;
}

static int m_restore(void *v, const void *buf, size_t len)
{
    struct ctx *c = v;
    if (len < 1)
        return -1;
    c->percent = ((const unsigned char *)buf)[0];
    return 0;
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "panel",
    .version = "0.2",
    .requires = { OA_SVC_PLAT, "" },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
    .suspend = m_suspend,
    .resume = m_resume,
    .quiesce = m_quiesce,
    .save = m_save,
    .restore = m_restore,
};

OA_MODULE_EXPORT(g_module)
