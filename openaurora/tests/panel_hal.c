/* mod_panel with the recovered backlight curve, fake PWM HAL. */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/vendor_pwm.h"

extern const oa_module *oa_module_entry(void);
static const oa_panel_iface *panel_iface;
static int g_curve, g_min_percent = 20, g_duty_max = 4095, g_raw = -1;
static int init_rc = 1, open_rc = 1, set_rc = 1;
static unsigned init_calls, open_calls, set_calls, close_calls;
static struct pwm_open_req last_open;
static struct pwm_set_req last_set;
static unsigned last_channel;

int Hal_Pwm_Init(void) { init_calls++; return init_rc; }
int Hal_Pwm_Open(struct pwm_open_req *r) { last_open = *r; open_calls++; return open_rc; }
int Hal_Pwm_Set_ByChannel(unsigned ch, struct pwm_set_req *r)
{
    last_channel = ch;
    last_set = *r;
    set_calls++;
    return set_rc;
}
void Hal_Pwm_Close(void) { close_calls++; }
int Hal_Pwm_Terminate(void) { return 0; }

static void log_message(void *p, int level, const char *fmt, ...)
{
    (void)p; (void)level; (void)fmt;
}
static const char *config(void *p, const char *key, const char *def)
{
    (void)p;
    if (!strcmp(key, "panel.curve")) return g_curve ? "1" : "0";
    if (!strcmp(key, "panel.min_percent")) return g_min_percent ? "20" : "0";
    if (!strcmp(key, "panel.duty_max")) return g_duty_max == 1000 ? "1000" : "4095";
    if (!strcmp(key, "panel.raw")) return g_raw == 4095 ? "4095" : g_raw == 2047 ? "2047" : "-1";
    if (!strcmp(key, "panel.start")) return "100";
    if (!strcmp(key, "panel.channel")) return "0";
    if (!strcmp(key, "panel.freq")) return "30000";
    if (!strcmp(key, "panel.mode")) return "0";
    return def;
}
static int publish(void *p, const char *name, uint32_t version, const void *iface)
{
    (void)p;
    assert(!strcmp(name, OA_SVC_PANEL) && version == 1);
    panel_iface = iface;
    return 0;
}
static int unpublish(void *p, const char *name)
{
    (void)p;
    assert(!strcmp(name, OA_SVC_PANEL));
    panel_iface = NULL;
    return 0;
}
static void release(void *p, const char *name) { (void)p; (void)name; }
static const oa_host_api api = { .size = sizeof(oa_host_api), .abi = OA_ABI_VERSION,
    .log = log_message, .config_get = config, .publish = publish,
    .unpublish = unpublish, .release = release };

static void *init_module(void)
{
    void *ctx = NULL;
    assert(oa_module_entry()->init(&api, &ctx) == 0);
    return ctx;
}
static void start_ctx(void *ctx)
{
    assert(oa_module_entry()->start(ctx) == 0 && panel_iface);
}
static void *start_module(void)
{
    void *ctx = init_module();
    start_ctx(ctx);
    return ctx;
}
static void stop_module(void *ctx)
{
    for (unsigned i = 0;; i++) {
        int rc = oa_module_entry()->quiesce(ctx);
        if (!rc) break;
        assert(i < 1000);
    }
    oa_module_entry()->stop(ctx);
    oa_module_entry()->destroy(ctx);
}

int main(void)
{
    g_curve = 1;
    void *ctx = start_module();
    assert(init_calls == 1 && open_calls == 1 && set_calls == 1);
    assert(last_open.channel == 0 && last_open.freq == 30000 &&
           last_open.duty == 1000);
    assert(last_channel == 0 && last_set.freq == 30000 &&
           last_set.mode == 0 && last_set.duty == 1000);
    /* min 20%: level=pct*10 -> curve -> 12-bit duty -> vendor HAL 0..1000. */
    assert(panel_iface->set_backlight_percent(50) == 0 && last_set.duty == 600);
    assert(panel_iface->set_backlight_percent(0) == 0 && last_set.duty == 200);
    assert(panel_iface->set_backlight_percent(100) == 0 && last_set.duty == 1000);
    /* 50% must not saturate: the old 12-bit-direct path sent 2047. */
    assert(panel_iface->set_backlight_percent(50) == 0 && last_set.duty != 2047);
    stop_module(ctx);
    assert(close_calls == 1);

    /* Final 12-bit calibration knob: duty_max=1000 with min 20%. */
    g_curve = 1; g_min_percent = 20; g_duty_max = 1000;
    ctx = start_module();
    assert(last_set.duty == 244);
    assert(panel_iface->set_backlight_percent(50) == 0 && last_set.duty == 146);
    assert(panel_iface->set_backlight_percent(0) == 0 && last_set.duty == 48);
    stop_module(ctx);

    /* Default remains the historical linear percentages. */
    g_curve = 0; g_min_percent = 0; g_duty_max = 1000;
    unsigned before = set_calls;
    ctx = start_module();
    assert(set_calls == before + 1 && last_set.duty == 244);
    assert(panel_iface->set_backlight_percent(50) == 0 && last_set.duty == 122);
    stop_module(ctx);

    puts("ok panel hal: vendor 0..1000 HAL scale, curve, duty_max and linear path");

    /* panel.raw is also 12-bit and crosses the same HAL boundary. */
    g_curve = 0; g_min_percent = 0; g_duty_max = 4095; g_raw = 2047;
    ctx = start_module();
    assert(last_set.duty == 499);
    assert(panel_iface->set_backlight_percent(0) == 0 && last_set.duty == 499);
    stop_module(ctx);
    g_raw = 4095;
    ctx = start_module();
    assert(last_set.duty == 1000);
    stop_module(ctx);
    g_raw = -1;
    puts("ok panel hal: panel.raw converts from 12-bit to the HAL domain");

    /* Vendor return semantics: Open/Set 0 is a definite failure. A failed
     * open is retried, and panel-state keeps the last applied value. */
    init_rc = 0; /* already initialized: still a success path */
    g_curve = 0; g_min_percent = 0; g_duty_max = 4095;
    open_rc = 1; set_rc = 1;
    ctx = start_module();
    assert(panel_iface->get_backlight_percent() == 100);
    unsigned opens = open_calls, sets = set_calls, closes = close_calls;
    assert(oa_module_entry()->suspend(ctx) == 0); /* close before reopening */
    assert(close_calls == closes + 1);

    open_rc = 0;
    assert(panel_iface->set_backlight_percent(40) == -EIO);
    assert(panel_iface->get_backlight_percent() == 100);
    assert(open_calls == opens + 1 && set_calls == sets);
    assert(close_calls == closes + 2); /* failed open releases the cached fd */

    open_rc = 1;
    assert(panel_iface->set_backlight_percent(40) == 0);
    assert(panel_iface->get_backlight_percent() == 40);
    assert(open_calls == opens + 2 && set_calls == sets + 1);

    set_rc = 0;
    assert(panel_iface->set_backlight_percent(80) == -EIO);
    assert(panel_iface->get_backlight_percent() == 40);
    set_rc = 1;
    assert(panel_iface->set_backlight_percent(80) == 0);
    assert(panel_iface->get_backlight_percent() == 80);
    stop_module(ctx);
    puts("ok panel hal: failed Open/Set keep the last applied percent; open retries");

    /* A failed initial open leaves the percent unknown (-1) but publishes the
     * service; a later set retries and recovers. */
    init_rc = 0; open_rc = 0; set_rc = 1;
    closes = close_calls;
    ctx = start_module();
    assert(panel_iface->get_backlight_percent() == -1);
    assert(close_calls == closes + 1);
    open_rc = 1;
    assert(panel_iface->set_backlight_percent(30) == 0);
    assert(panel_iface->get_backlight_percent() == 30);
    stop_module(ctx);
    puts("ok panel hal: failed start reports unknown percent and recovers on set");

    /* 0 is a real applied value and must survive suspend/resume. */
    init_rc = 0; open_rc = 1; set_rc = 1;
    g_curve = 0; g_min_percent = 0; g_duty_max = 4095;
    ctx = start_module();
    assert(panel_iface->set_backlight_percent(0) == 0 && last_set.duty == 0);
    assert(oa_module_entry()->suspend(ctx) == 0);
    assert(oa_module_entry()->resume(ctx) == 0);
    assert(last_set.duty == 0 && panel_iface->get_backlight_percent() == 0);
    stop_module(ctx);
    puts("ok panel hal: suspend/resume keeps an applied 0 percent");

    /* Save/restore a real 0 across a started reload instead of panel.start. */
    ctx = start_module();
    assert(panel_iface->set_backlight_percent(0) == 0);
    unsigned char blob = 0xa5;
    assert(oa_module_entry()->save(ctx, &blob, 1) == 1 && blob == 0);
    stop_module(ctx);
    void *next = init_module();
    assert(oa_module_entry()->restore(next, &blob, 1) == 0);
    start_ctx(next);
    assert(panel_iface->get_backlight_percent() == 0 && last_set.duty == 0);
    stop_module(next);
    puts("ok panel hal: restored 0 percent overrides the configured start");

    /* A never-applied state saves as 0xFF and restores as unknown, not 0. */
    init_rc = 0; open_rc = 0; set_rc = 1;
    ctx = start_module();
    assert(panel_iface->get_backlight_percent() == -1);
    blob = 0xa5;
    assert(oa_module_entry()->save(ctx, &blob, 1) == 1 && blob == 0xff);
    assert(oa_module_entry()->restore(ctx, &blob, 1) == 0);
    assert(panel_iface->get_backlight_percent() == -1);
    unsigned char bad = 101;
    assert(oa_module_entry()->restore(ctx, &bad, 1) == -1);
    stop_module(ctx);
    puts("ok panel hal: unknown percent saves as 0xFF and restores unknown");
    return 0;
}
