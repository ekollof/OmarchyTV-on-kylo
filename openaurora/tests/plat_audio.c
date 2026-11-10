/* mod_plat cold-start sequence with fake HAL.
 *
 * Vendor PLAT_DRV_INI_Init @0089b6e4:
 *   Hal_Audio_Protect_Init(0); Mixer; Proxy;   (all returns ignored)
 *   plat_common_init("DVIF")                   (idempotent, void)
 *   Hal_Gpio_Init(); Hal_Iic_Init();           (nonzero logged, continue)
 *   plat_vin_slot_init();                      (always 0)
 *   NDAL_VideoExt_OpenDevice(0,10)
 *
 * This test proves the call order, the explicit Protect argument, the byte
 * DVIF value, that prelude failures do not gate later stages, and that the
 * common init is idempotent. Hardware effects are not proven. */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/vendor_audio.h"

extern const oa_module *oa_module_entry(void);
extern void oa_plat_test_reset_common(void);

enum ev {
    EV_PROTECT, EV_MIXER, EV_PROXY,
    EV_VPLTIO, EV_V4L2, EV_DRMSHARE, EV_DRMINIT, EV_SENDCMD,
    EV_GPIO, EV_IIC, EV_OPENDEV
};
static enum ev g_events[32];
static unsigned g_count;
static int g_fail;                  /* bit0 protect, bit1 mixer, bit2 proxy */
static int g_drm;                   /* plat.drm_init config */
static const oa_plat_iface *g_service;
static char g_log[4096];
static size_t g_log_at;

static void push(enum ev e)
{
    assert(g_count < sizeof g_events / sizeof g_events[0]);
    g_events[g_count++] = e;
}

int Hal_Audio_Protect_Init(int arg)
{
    assert(arg == 0);               /* vendor passes an explicit 0 */
    push(EV_PROTECT);
    return (g_fail & 1) ? -9 : 0;
}
int Hal_Audio_Mixer_Ctrl_Init(void) { push(EV_MIXER); return (g_fail & 2) ? -9 : 0; }
int Hal_Audio_Proxy_Init(void) { push(EV_PROXY); return (g_fail & 4) ? -9 : 0; }
void Hal_Video_Vpltio_Init(void) { push(EV_VPLTIO); }
void Hal_Video_V4l2InfoInit(void) { push(EV_V4L2); }
void Hal_Video_DrmShareMemCreate(void) { push(EV_DRMSHARE); }
void Hal_Video_DrmInit(int enable) { assert(enable == 1); push(EV_DRMINIT); }
int Hal_Video_Vpltio_SendCmd(int cmd, int a, void *buf, int b, int c, int d,
                             int e, int f, int g)
{
    assert(cmd == 0x302 && a == 0 && buf);
    assert(*(unsigned char *)buf == 1); /* DVIF, one byte as the vendor passes */
    assert(!(b | c | d | e | f | g));
    push(EV_SENDCMD);
    return 0;
}
int Hal_Gpio_Init(void) { push(EV_GPIO); return 0; }
int Hal_Iic_Init(void) { push(EV_IIC); return 0; }
void NDAL_VideoExt_OpenDevice(int flag, unsigned index)
{
    assert(flag == 0 && index == 10);
    push(EV_OPENDEV);
}
int NDAL_VideoExt_STBC_FreshWDT(void) { assert(0); return -1; }

static void log_msg(void *p, int level, const char *fmt, ...)
{
    (void)p; (void)level;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(g_log + g_log_at, sizeof g_log - g_log_at, fmt, ap);
    va_end(ap);
    if (n > 0) {
        g_log_at += (size_t)n;
        if (g_log_at >= sizeof g_log) g_log_at = sizeof g_log - 1;
    }
}
static const char *config(void *p, const char *key, const char *def)
{
    (void)p;
    if (!strcmp(key, "plat.enable")) return "1";
    if (!strcmp(key, "plat.driver_wait")) return "0";
    if (!strcmp(key, "plat.drm_init")) return g_drm ? "1" : "0";
    return def;
}
static int publish(void *p, const char *name, uint32_t version, const void *iface)
{
    (void)p;
    assert(!strcmp(name, OA_SVC_PLAT) && version == 1);
    g_service = iface;
    return 0;
}
static int unpublish(void *p, const char *name)
{
    (void)p;
    assert(!strcmp(name, OA_SVC_PLAT));
    g_service = NULL;
    return 0;
}

static void reset(int fail, int drm)
{
    oa_plat_test_reset_common();
    g_fail = fail;
    g_drm = drm;
    g_count = 0;
    g_log_at = 0;
    g_log[0] = 0;
    g_service = NULL;
}

static void expect(const enum ev *want, unsigned n)
{
    assert(g_count == n);
    for (unsigned i = 0; i < n; i++) assert((int)g_events[i] == (int)want[i]);
}

int main(void)
{
    const oa_host_api api = { .size = sizeof api, .abi = OA_ABI_VERSION,
        .log = log_msg, .config_get = config, .publish = publish,
        .unpublish = unpublish };
    const oa_module *m = oa_module_entry();
    static const enum ev full[] = {
        EV_PROTECT, EV_MIXER, EV_PROXY,
        EV_VPLTIO, EV_V4L2, EV_DRMSHARE, EV_DRMINIT, EV_SENDCMD,
        EV_GPIO, EV_IIC, EV_OPENDEV
    };
    static const enum ev no_drm[] = {
        EV_PROTECT, EV_MIXER, EV_PROXY,
        EV_VPLTIO, EV_V4L2, EV_DRMSHARE, EV_SENDCMD,
        EV_GPIO, EV_IIC, EV_OPENDEV
    };

    /* Vendor ignores all three audio-prelude returns and still runs every
     * later stage. The module must start and report an error bit, not abort. */
    reset(7, 1);
    void *ctx = NULL;
    assert(!m->init(&api, &ctx) && ctx);
    assert(m->start(ctx) == 0);
    expect(full, sizeof full / sizeof full[0]);
    assert(g_service && g_service->is_ready() == 1);
    assert(g_service->stage_errors() & 1u);
    assert(strstr(g_log, "audio prelude returned"));
    m->stop(ctx);
    m->destroy(ctx);

    /* Fresh start: exact vendor order. */
    reset(0, 1);
    assert(!m->init(&api, &ctx) && ctx);
    assert(m->start(ctx) == 0);
    expect(full, sizeof full / sizeof full[0]);
    assert(g_service && g_service->is_ready() == 1);
    assert(g_service->stage_errors() == 0);
    assert(!strstr(g_log, "already done"));

    /* Same process, second start: plat_common_init is idempotent; the audio
     * prelude and the later HAL stages still run. */
    m->stop(ctx);
    g_count = 0;
    g_log_at = 0;
    g_log[0] = 0;
    assert(m->start(ctx) == 0);
    static const enum ev again[] = {
        EV_PROTECT, EV_MIXER, EV_PROXY, EV_GPIO, EV_IIC, EV_OPENDEV
    };
    expect(again, sizeof again / sizeof again[0]);
    assert(strstr(g_log, "common init already done"));
    assert(g_service && g_service->is_ready() == 1);
    m->stop(ctx);
    m->destroy(ctx);

    /* Takeover gate still skips only Hal_Video_DrmInit(1). */
    reset(0, 0);
    assert(!m->init(&api, &ctx) && ctx);
    assert(m->start(ctx) == 0);
    expect(no_drm, sizeof no_drm / sizeof no_drm[0]);
    assert(strstr(g_log, "Hal_Video_DrmInit skipped"));
    m->stop(ctx);
    m->destroy(ctx);

    puts("ok plat cold start: prelude returns do not gate; exact PLAT_DRV_INI_Init order; "
         "Protect(0), byte DVIF, idempotent common init, takeover drm gate");
    return 0;
}
