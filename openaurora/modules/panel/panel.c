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
 *     └─ Hal_Pwm_Set_ByChannel(ch, &{freq, duty 0..1000, shift, mode})
 *
 * The stock process's populated descriptor table and globals calibrate this
 * unit: DEV_PWM_Get_ChIdx(0x6c) -> channel 0 via astPwmBasePin, frequency
 * gu32PanelFreqInHz=30000, set-mode genPwmdimMode=0, current level 1000.
 * Keep them as configuration because the values are board/profile state.
 * panel.curve=1 selects the recovered DEV_PANEL_Init/SetPWMBacklightLv curve
 * (breakpoints 40/60/80/100, level scale 1000) with panel.min_percent as the
 * runtime limit; panel.duty_max is the final 12-bit calibration (default 4095).
 * Default 0 curve keeps the historical linear percent mapping. The 12-bit
 * value is converted to the vendor HAL's 0..1000 domain exactly as
 * plat_set_dimming does before Hal_Pwm_Set_ByChannel.
 *
 * Host builds define OA_NO_VENDOR and get no-op stubs, so the module still loads and
 * the lifecycle tests keep running on a laptop.
 */
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
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
static int Hal_Gpio_Write(int pin, int level) { (void)pin; (void)level; return 0; }
static int Hal_Video_Vpltio_SendCmd(int cmd, int a, void *buf, int b, int c, int d,
                                    int e, int f, int g)
{
    (void)cmd; (void)a; (void)buf; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g;
    return 0;
}
#else
static int hal_ready(void)
{
    return 1;
}
extern int Hal_Gpio_Write(int pin, int level);
extern int Hal_Video_Vpltio_SendCmd(int cmd, int a, void *buf, int b, int c, int d,
                                    int e, int f, int g);
#endif

struct ctx {
    const oa_host_api *api;
    int channel;
    int freq;
    int mode;    /* DEV_PANEL_SetPWMBacklightLevel's genPwmdimMode */
    int percent; /* what we asked for, 0..100 */
    int raw;     /* optional: 12-bit duty, ignoring percent (HAL-converted) */
    int curve_on;/* panel.curve=1 uses the recovered vendor backlight curve */
    int duty_max;/* panel.duty_max, 1..4095 */
    oa_panel_curve curve;
    int opened;
    int have_percent; /* a PWM set has succeeded (or host simulation) */
    int restored;     /* a valid percent was handed across reload */
    int published;
    int needs_cleanup; /* one start cycle is cleaned exactly once */
    /* Opt-in standby panel-power sequence (DEV_PANEL_PowerSequence). The
     * defaults are the live profile's descriptors/panel-extra values. */
    int standby;
    int power_on_start; /* run the power-on sequence at m_start (cold boot) */
    int power_pin, power_on_level;
    int bl_pin, bl_on_level;
    int lvds_cmd;
    unsigned on_delay_ms, bl_on_delay_ms;
    unsigned off_bl_delay_ms, off_lvds_delay_ms, off_panel_delay_ms;
    /* ABI 3 quiesce: the vendor PWM close runs on a worker. */
    pthread_t cleanup_thread;
    int cleanup_running;
    unsigned cleanup_delay_ms;
};

static struct ctx *g_ctx;

/* Vendor scale correction: DEV_PANEL_SetPWMBacklightLevel builds a 12-bit
 * duty (0..4095) and passes it to DEV_PWM_Set_ByChannel, but plat_set_dimming
 * (@0x0092e668) converts that word to the HAL's 0..1000 domain before
 * Hal_Pwm_Set_ByChannel: duty < 0x1000 ? duty*1000/4095 : 1000. Calling the
 * HAL directly with 0..4095 made 50% and 100% both clamp at the device and
 * explained the "no visible change" panel runs. Keep the module's public
 * percent/duty_max/raw values in the 12-bit domain and convert exactly here. */
static uint32_t hal_duty(int duty)
{
    if (duty <= 0)
        return 0;
    if (duty >= 0x1000)
        return 1000;
    return (uint32_t)((unsigned)duty * 1000u / 4095u);
}

static int write_hal(struct ctx *c, uint32_t hal)
{
    if (!hal_ready())
        return -ENOTSUP;
    if (!c->opened) {
        /* Init: 1 = initialized now, 0 = already initialized. Both are the
         * vendor success paths (the semaphore failure is only printed). */
        (void)Hal_Pwm_Init();
        struct pwm_open_req d;
        memset(&d, 0, sizeof d);
        d.channel = (uint32_t)c->channel;
        d.freq = (uint32_t)c->freq;
        d.duty = hal;    /* same HAL domain as Set (see hal_duty) */
        d.shift = 0;
        d.mode = 1;      /* desc[4] == 1 at aurora's call site */
        if (Hal_Pwm_Open(&d) == 0) {
            /* 0 is a definite vendor failure (bad argument/fd/ioctl). Open
             * may already have cached the shared fd, so close it before
             * returning; the next call retries the open. */
            Hal_Pwm_Close();
            return -EIO;
        }
        c->opened = 1;
    }

    struct pwm_set_req r;
    memset(&r, 0, sizeof r);
    r.freq = (uint32_t)c->freq;
    r.duty = hal;
    r.shift = 0;
    r.mode = (uint32_t)c->mode; /* genPwmdimMode; must be < 9 */
    /* Same convention as Open: 0 is a definite failure, nonzero is the vendor
     * success path (not a hardware readback). */
    return Hal_Pwm_Set_ByChannel((unsigned)c->channel, &r) == 0 ? -EIO : 0;
}

static int apply(struct ctx *c, int pct)
{
    if (!hal_ready())
        return -ENOTSUP;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    int duty12 = c->raw >= 0 ? c->raw :
                 c->curve_on ? oa_panel_percent_to_duty(&c->curve, pct, c->duty_max) :
                 pct * c->duty_max / 100;
    int rc = write_hal(c, hal_duty(duty12));
    if (rc != 0)
        return rc;
    c->percent = pct;
    c->have_percent = 1;
    return 0;
}

static void delay_ms(unsigned ms)
{
    if (!ms)
        return;
    const struct timespec pause = {
        .tv_sec = ms / 1000u,
        .tv_nsec = (long)(ms % 1000u) * 1000000L
    };
    (void)nanosleep(&pause, NULL);
}

static void standby_gpio(struct ctx *c, int pin, int level)
{
    if (!c->standby || !hal_ready())
        return;
    if (Hal_Gpio_Write(pin, level) != 0)
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "panel: standby GPIO %d -> %d failed", pin, level);
}

/* plat_vpltio_mute_lvds @0x0092e0f8: cmd 0x1cc with the mute byte as the
 * second data argument. 0 = LVDS data off, 1 = on. */
static void standby_lvds(struct ctx *c, int on)
{
    if (!c->standby || !hal_ready())
        return;
    uint32_t unused = 0;
    unsigned char mute = on ? 1 : 0;
    (void)Hal_Video_Vpltio_SendCmd(c->lvds_cmd, (int)(uintptr_t)&unused, &mute,
                                   0, 0, 0, 0, 0, 0);
}

/* DEV_PANEL_PowerSequence(1) @0x00857ac0 order for the live profile: panel
 * power -> 30 ms -> LVDS on -> PWM restore -> 580 ms -> backlight power.
 * The TCON PMIC GPIO step is skipped because panel-extra +0x4a is 0. */
static int panel_power_on(struct ctx *c)
{
    standby_gpio(c, c->power_pin, c->power_on_level);
    delay_ms(c->on_delay_ms);
    standby_lvds(c, 1);
    int pct = c->percent >= 0 ? c->percent : 100;
    int rc = apply(c, pct);
    delay_ms(c->bl_on_delay_ms);
    standby_gpio(c, c->bl_pin, c->bl_on_level);
    return rc;
}

/* DEV_PANEL_PowerSequence(0): 70 ms -> backlight power off -> 220 ms ->
 * LVDS off -> 30 ms -> PWM 0 -> panel power off. PLAT_PQ_SHELL_SetAutoBacklight
 * (0) has no owner in this module and is omitted, not emulated. */
static void panel_power_off(struct ctx *c)
{
    delay_ms(c->off_bl_delay_ms);
    standby_gpio(c, c->bl_pin, !c->bl_on_level);
    delay_ms(c->off_lvds_delay_ms);
    standby_lvds(c, 0);
    delay_ms(c->off_panel_delay_ms);
    (void)write_hal(c, 0);
    standby_gpio(c, c->power_pin, !c->power_on_level);
}

static int iface_set(int pct)
{
    if (!g_ctx)
        return -ENODEV;
    return apply(g_ctx, pct);
}

static int iface_get(void)
{
    return g_ctx && g_ctx->have_percent ? g_ctx->percent : -1;
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
    int restored = c->restored;
    if (!restored) {
        c->percent = atoi(c->api->config_get(c->api->priv, "panel.start", "100"));
        /* Host builds have no HAL: keep the configured value as the simulated
         * state so lifecycle tests still report it. TV builds only report a
         * percent after a successful PWM set. */
        c->have_percent = !hal_ready();
    }
    c->raw = atoi(c->api->config_get(c->api->priv, "panel.raw", "-1"));
    c->curve_on = c->api->config_get(c->api->priv, "panel.curve", "0")[0] == '1';
    c->duty_max = atoi(c->api->config_get(c->api->priv, "panel.duty_max", "4095"));
    if (c->duty_max < 1 || c->duty_max > 4095) c->duty_max = 4095;
    oa_panel_curve_init(&c->curve, 40, 60, 80, 100,
        atoi(c->api->config_get(c->api->priv, "panel.min_percent", "0")) * 10);

    /* Opt-in standby sequence. Defaults are the live panel's descriptor pins,
     * polarity and PLAT_VIU panel-extra delays (30/580/220/30 ms). */
    c->standby = c->api->config_get(c->api->priv, "panel.standby", "0")[0] == '1';
    c->power_on_start = c->api->config_get(c->api->priv,
                                           "panel.power_on_start", "0")[0] == '1';
    if (c->power_on_start)
        c->standby = 1; /* the GPIO/LVDS helpers share the sequence gate */
    c->power_pin = atoi(c->api->config_get(c->api->priv, "panel.power_pin", "110"));
    c->power_on_level = atoi(c->api->config_get(c->api->priv,
                                                "panel.power_on_level", "1")) != 0;
    c->bl_pin = atoi(c->api->config_get(c->api->priv, "panel.bl_power_pin", "111"));
    c->bl_on_level = atoi(c->api->config_get(c->api->priv,
                                             "panel.bl_on_level", "0")) != 0;
    c->lvds_cmd = atoi(c->api->config_get(c->api->priv, "panel.lvds_cmd", "460"));
    long on = atol(c->api->config_get(c->api->priv, "panel.on_delay_ms", "30"));
    long bl_on = atol(c->api->config_get(c->api->priv, "panel.bl_on_delay_ms", "580"));
    long off_bl = atol(c->api->config_get(c->api->priv, "panel.off_bl_delay_ms", "70"));
    long off_lvds = atol(c->api->config_get(c->api->priv, "panel.off_lvds_delay_ms", "220"));
    long off_panel = atol(c->api->config_get(c->api->priv, "panel.off_panel_delay_ms", "30"));
    c->on_delay_ms = on > 0 && on <= 10000 ? (unsigned)on : 0;
    c->bl_on_delay_ms = bl_on > 0 && bl_on <= 10000 ? (unsigned)bl_on : 0;
    c->off_bl_delay_ms = off_bl > 0 && off_bl <= 10000 ? (unsigned)off_bl : 0;
    c->off_lvds_delay_ms = off_lvds > 0 && off_lvds <= 10000 ? (unsigned)off_lvds : 0;
    c->off_panel_delay_ms = off_panel > 0 && off_panel <= 10000 ? (unsigned)off_panel : 0;

    if (!hal_ready()) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "built without vendor HAL (host build): panel control disabled");
    } else {
        c->api->log(c->api->priv, OA_LOG_INFO, "vendor HAL linked: channel %d, %d Hz, start %d%%",
                    c->channel, c->freq, c->percent);
        if (c->power_on_start) {
            if (panel_power_on(c) != 0) {
                if (restored)
                    c->have_percent = 0;
                c->api->log(c->api->priv, OA_LOG_WARN,
                            "panel: cold power-on sequence failed; percent unknown");
            }
        } else if (apply(c, c->percent) != 0) {
            if (restored)
                c->have_percent = 0; /* handed-over value was not reapplied */
            c->api->log(c->api->priv, OA_LOG_WARN,
                        "panel: initial PWM apply failed; percent unknown");
        }
    }
    c->restored = 0;

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
    if (c->standby && hal_ready())
        panel_power_off(c);
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
    if (!hal_ready())
        return 0;
    if (c->standby)
        return panel_power_on(c);
    int rc = apply(c, c->percent); /* 0 is a valid applied value */
    if (rc != 0) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "panel: resume PWM apply failed; keeping last value");
        return rc;
    }
    return 0;
}

static size_t m_save(void *v, void *buf, size_t cap)
{
    struct ctx *c = v;
    if (!buf || cap < 1)
        return 1;
    ((unsigned char *)buf)[0] = c->have_percent ?
        (unsigned char)c->percent : 0xff;
    return 1;
}

static int m_restore(void *v, const void *buf, size_t len)
{
    struct ctx *c = v;
    if (len < 1)
        return -1;
    unsigned value = ((const unsigned char *)buf)[0];
    if (value == 0xff) {
        /* never applied: let m_start use the configured start */
        c->have_percent = 0;
        c->restored = 0;
        return 0;
    }
    if (value > 100)
        return -1;
    c->percent = (int)value;
    c->have_percent = 1;
    c->restored = 1;
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
