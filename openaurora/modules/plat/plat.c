/*
 * mod_plat — the vendor platform bring-up, in the vendor's order.
 *
 * Everything below is read out of the firmware, not guessed:
 *
 *   aurora main()                       @006b7320
 *     └─ FUN_007431d8
 *          ICE_InitialiseSystemMemory → ICE_DebugInit → profileManagerStart
 *          → ICE_HardwareInit → ICE_InitRTC → eclipseStart
 *
 *   ICE_HardwareInit                    @007551a0   (waits for /tmp/ntkdriver_completed)
 *     └─ PLAT_DRV_INI_Init              @0089b6e4
 *          Hal_Audio_Protect_Init()    (no semantic argument; int return)
 *          Hal_Audio_Mixer_Ctrl_Init()
 *          Hal_Audio_Proxy_Init()
 *          plat_common_init("DVIF")     @009247f8
 *              Hal_Video_Vpltio_Init()
 *              Hal_Video_V4l2InfoInit()
 *              Hal_Video_DrmShareMemCreate()
 *              Hal_Video_DrmInit(1)
 *              Hal_Video_Vpltio_SendCmd(0x302, 0, &src, …)   src = 1 (DVIF)
 *          Hal_Gpio_Init()
 *          Hal_Iic_Init()
 *          plat_vin_slot_init()         (PLAT_*, internal to aurora — see the log line)
 *          NDAL_VideoExt_OpenDevice(0, 10)
 *
 * Why this module exists: the audio HAL cannot be called before the prelude (calling it cold killed
 * the process, MEMORY 2026-10-30), and the display bring-up must run after it — openaurora used to
 * call plat_common_init from mod_display with the prelude never having run, and never did GPIO, I2C,
 * VIN or the video-device open at all.
 *
 *   plat.enable   0 = do nothing (default 1)
 *   plat.vin      0 = skip the VIN-slot stage (default 1; it is PLAT_* inside aurora, so this is the
 *                 one stage we can only approximate)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/vendor_audio.h"

struct ctx {
    const oa_host_api *api;
    int ready;
    int errors;   /* bitmask, see STAGE_* below */
};

#define STAGE_AUDIO_PRECHECK (1u << 0)
#define STAGE_COMMON         (1u << 1)
#define STAGE_GPIO           (1u << 2)
#define STAGE_IIC            (1u << 3)
#define STAGE_VIN            (1u << 4)
#define STAGE_VIDEO_DEVICE   (1u << 5)

#define STAGE_TAIL   (STAGE_GPIO | STAGE_IIC | STAGE_VIDEO_DEVICE)

/* aurora's VIN slot tables: 7 slots of 9 words, plus a 3-word side table per slot, plus 0x150 bytes
 * of slot state. Defaults from plat_vin_slot_init @0092e9c4. */
#define VIN_SLOTS     7
#define VIN_SLOT_WORDS 9
static uint32_t g_vin_slots[VIN_SLOTS][VIN_SLOT_WORDS];
static uint32_t g_vin_side[VIN_SLOTS][3];
static unsigned char g_vin_state[0x150];

static struct ctx *g_ctx;

/* ------------------------------------------------------------- vendor HAL */

#ifndef OA_NO_VENDOR
extern void Hal_Video_Vpltio_Init(void);
extern void Hal_Video_V4l2InfoInit(void);
extern void Hal_Video_DrmShareMemCreate(void);
extern void Hal_Video_DrmInit(int enable);
extern int Hal_Video_Vpltio_SendCmd(int cmd, int a, void *buf, int b, int c, int d, int e, int f,
                                    int g);
extern int Hal_Gpio_Init(void);
extern int Hal_Iic_Init(void);
extern void NDAL_VideoExt_OpenDevice(int flag, unsigned index);
extern int NDAL_VideoExt_STBC_FreshWDT(void);
#else
static int Hal_Audio_Protect_Init(void) { return 0; }
static int Hal_Audio_Mixer_Ctrl_Init(void) { return 0; }
static int Hal_Audio_Proxy_Init(void) { return 0; }
static void Hal_Video_Vpltio_Init(void) {}
static void Hal_Video_V4l2InfoInit(void) {}
static void Hal_Video_DrmShareMemCreate(void) {}
static void Hal_Video_DrmInit(int enable) { (void)enable; }
static int Hal_Video_Vpltio_SendCmd(int cmd, int a, void *buf, int b, int c, int d, int e, int f,
                                    int g)
{
    (void)cmd; (void)a; (void)buf; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g;
    return 0;
}
static int Hal_Gpio_Init(void) { return 0; }
static int Hal_Iic_Init(void) { return 0; }
static void NDAL_VideoExt_OpenDevice(int flag, unsigned index) { (void)flag; (void)index; }
static int NDAL_VideoExt_STBC_FreshWDT(void) { return 0; }
#endif

/* ---------------------------------------------------------------- iface */

static int iface_ready(void)
{
    return g_ctx ? g_ctx->ready : -1;
}

static int iface_errors(void)
{
    return g_ctx ? g_ctx->errors : -1;
}

static const oa_plat_iface g_iface = { iface_ready, iface_errors };

/* --------------------------------------------------------------- module */

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->size < sizeof(oa_host_api) || api->abi != OA_ABI_VERSION)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c)
        return -1;
    c->api = api;
    *out = c;
    g_ctx = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->api->config_get(c->api->priv, "plat.enable", "1")[0] != '1') {
        c->api->log(c->api->priv, OA_LOG_WARN, "plat: disabled by config, HAL left uninitialised");
        return -1;
    }

    /* 1. audio prelude — mandatory before anything else touches the HAL */
    c->ready = 0; c->errors = 0;
    int protect_rc = Hal_Audio_Protect_Init();
    int mixer_rc = Hal_Audio_Mixer_Ctrl_Init();
    int proxy_rc = Hal_Audio_Proxy_Init();
    if (protect_rc || mixer_rc || proxy_rc) {
        c->errors |= STAGE_AUDIO_PRECHECK;
        c->api->log(c->api->priv, OA_LOG_ERROR,
                    "plat: audio prelude failed (protect %d, mixer %d, proxy %d)",
                    protect_rc, mixer_rc, proxy_rc);
        return -1;
    }
    c->api->log(c->api->priv, OA_LOG_INFO, "plat: audio prelude (Protect/Mixer/Proxy)");

    /* 2. plat_common_init("DVIF") */
    Hal_Video_Vpltio_Init();
    Hal_Video_V4l2InfoInit();
    Hal_Video_DrmShareMemCreate();
    Hal_Video_DrmInit(1);
    int src = 1;                                  /* DVIF */
    Hal_Video_Vpltio_SendCmd(0x302, 0, &src, 0, 0, 0, 0, 0, 0);
    c->api->log(c->api->priv, OA_LOG_INFO, "plat: common init (vpltio/v4l2/sharemem/drm, DVIF)");

    /* 3. GPIO and I2C */
    if (Hal_Gpio_Init() != 0) {
        c->errors |= STAGE_GPIO;
        c->api->log(c->api->priv, OA_LOG_WARN, "plat: Hal_Gpio_Init failed");
    }
    if (Hal_Iic_Init() != 0) {
        c->errors |= STAGE_IIC;
        c->api->log(c->api->priv, OA_LOG_WARN, "plat: Hal_Iic_Init failed");
    }

    /* 4. VIN slots: plat_vin_slot_init @0092e9c4 is pure data (no HAL calls) - it fills 7 slot
     *    descriptors with {0,7,7,1,1,0,4,0,0} plus a parallel {7,0,25} table, then zeroes 0x150
     *    bytes of slot state. Reproduced here so our slot model starts from the vendor's defaults;
     *    mod_source drives slot 0 through Hal_Vin_*. */
    for (int i = 0; i < VIN_SLOTS; i++) {
        static const uint32_t defaults[VIN_SLOT_WORDS] = { 0, 7, 7, 1, 1, 0, 4, 0, 0 };
        static const uint32_t side[3] = { 7, 0, 0x19 };
        memcpy(g_vin_slots[i], defaults, sizeof defaults);
        memcpy(g_vin_side[i], side, sizeof side);
    }
    memset(g_vin_state, 0, sizeof g_vin_state);
    c->api->log(c->api->priv, OA_LOG_INFO, "plat: VIN slot defaults (%d slots)", VIN_SLOTS);

    /* 5. the video device aurora opens last */
    NDAL_VideoExt_OpenDevice(0, 10);
    c->api->log(c->api->priv, OA_LOG_INFO, "plat: NDAL_VideoExt_OpenDevice(0,10)");

    /* 5b. Watchdog feasibility test.
     *
     * aurora feeds the MCU watchdog from a thread: ICE_HardwareInit does
     *     if (ICE_HardwareWatchdog(1) == 1) FROST_ThreadCreate(...)   - default timeout 10 s
     * and the kick itself is PLAT_SYS_KickWatchDog() -> NDAL_VideoExt_STBC_FreshWDT().
     * Measured 2026-10-31 with this probe: after a full plat bring-up the call no longer resets the
     * set (the cold call does), but it returns 1 - the STBC path refuses a kick from a plain process.
     * So the MCU watchdog cannot be maintained by openaurora; a takeover keeps it disabled and relies
     * on the hook's heartbeat/fallback instead. Kept as a probe for when that changes.
     */
    if (c->api->config_get(c->api->priv, "plat.wdt_probe", "0")[0] == '1') {
        int rc = NDAL_VideoExt_STBC_FreshWDT();
        c->api->log(c->api->priv, rc == 0 ? OA_LOG_INFO : OA_LOG_WARN,
                    "plat: FreshWDT probe -> %d (%#x)", rc, rc);
    }

    c->ready = 1;
    c->api->publish(c->api->priv, OA_SVC_PLAT, 1, &g_iface);
    /* Be precise about what this means: the vendor's bring-up calls mostly return void, so a stage
     * having "run" is not the same as having succeeded. Audio prelude and GPIO/I2C report status; the display
     * path (common init, video device) can only be judged by its effect. */
    c->api->log(c->api->priv, OA_LOG_INFO,
                "plat: sequence done (gpio/iic errors %#x; common/video unverifiable: void returns)",
                c->errors);
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    c->api->unpublish(c->api->priv, OA_SVC_PLAT);
    c->ready = 0;
}

static void m_destroy(void *v)
{
    if (g_ctx == v)
        g_ctx = NULL;
    free(v);
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof(oa_module),
    .name = "plat",
    .version = "0.1",
    .requires = { "" },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
};

OA_MODULE_EXPORT(g_module)
