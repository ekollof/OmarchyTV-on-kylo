/* mod_panel standby panel-power order, fake PWM/GPIO/VPLTIO HAL.
 *
 * Firmware order (DEV_PANEL_PowerSequence @0x00857ac0, live panel-extra
 * delays 30/580/220/30 ms and TCON PMIC disabled by +0x4a == 0):
 *   off: BL power off -> LVDS off -> PWM 0 -> panel power off
 *   on:  panel power -> LVDS on -> PWM restore -> BL power on
 * GPIO defaults: PANELPWR 110 on=1, BKLTPWR 111 on=0.
 */
#include <assert.h>
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
static int g_start_on;

enum event_kind { EV_INIT, EV_OPEN, EV_SET, EV_CLOSE, EV_GPIO, EV_LVDS };
struct event {
    enum event_kind kind;
    int a, b;
};
static struct event events[64];
static unsigned event_count;

static void push(enum event_kind kind, int a, int b)
{
    assert(event_count < sizeof events / sizeof events[0]);
    events[event_count++] = (struct event){ kind, a, b };
}

int Hal_Pwm_Init(void) { push(EV_INIT, 0, 0); return 1; }
int Hal_Pwm_Open(struct pwm_open_req *r)
{
    push(EV_OPEN, (int)r->duty, (int)r->mode);
    return 1;
}
int Hal_Pwm_Set_ByChannel(unsigned ch, struct pwm_set_req *r)
{
    push(EV_SET, (int)r->duty, (int)ch);
    return 1;
}
void Hal_Pwm_Close(void) { push(EV_CLOSE, 0, 0); }
int Hal_Pwm_Terminate(void) { return 0; }
int Hal_Gpio_Write(int pin, int level)
{
    push(EV_GPIO, pin, level);
    return 0;
}
int Hal_Video_Vpltio_SendCmd(int cmd, int a, void *buf, int b, int c, int d,
                             int e, int f, int g)
{
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g;
    assert(buf && cmd == 460);
    push(EV_LVDS, cmd, *(unsigned char *)buf);
    return 0;
}

static void log_message(void *p, int level, const char *fmt, ...)
{
    (void)p; (void)level; (void)fmt;
}
static const char *config(void *p, const char *key, const char *def)
{
    (void)p;
    if (!strcmp(key, "panel.standby")) return "1";
    if (!strcmp(key, "panel.power_on_start")) return g_start_on ? "1" : "0";
    if (!strcmp(key, "panel.start")) return "100";
    if (!strcmp(key, "panel.on_delay_ms")) return "0";
    if (!strcmp(key, "panel.bl_on_delay_ms")) return "0";
    if (!strcmp(key, "panel.off_bl_delay_ms")) return "0";
    if (!strcmp(key, "panel.off_lvds_delay_ms")) return "0";
    if (!strcmp(key, "panel.off_panel_delay_ms")) return "0";
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

static void expect(const struct event *want, unsigned count)
{
    assert(event_count == count);
    for (unsigned i = 0; i < count; i++) {
        assert(events[i].kind == want[i].kind);
        assert(events[i].a == want[i].a);
        assert(events[i].b == want[i].b);
    }
}

int main(void)
{
    void *ctx = NULL;
    assert(oa_module_entry()->init(&api, &ctx) == 0 && ctx);
    assert(oa_module_entry()->start(ctx) == 0 && panel_iface);
    /* initial apply: open at the converted 1000 level, then set it. */
    assert(event_count == 3);
    assert(events[0].kind == EV_INIT);
    assert(events[1].kind == EV_OPEN && events[1].a == 1000 && events[1].b == 1);
    assert(events[2].kind == EV_SET && events[2].a == 1000);

    event_count = 0;
    assert(oa_module_entry()->suspend(ctx) == 0);
    const struct event off[] = {
        { EV_GPIO, 111, 1 },   /* backlight power off (active low) */
        { EV_LVDS, 460, 0 },   /* LVDS data off */
        { EV_SET, 0, 0 },      /* PWM 0 */
        { EV_GPIO, 110, 0 },   /* panel power off */
        { EV_CLOSE, 0, 0 },
    };
    expect(off, sizeof off / sizeof off[0]);

    event_count = 0;
    assert(oa_module_entry()->resume(ctx) == 0);
    const struct event on[] = {
        { EV_GPIO, 110, 1 },   /* panel power on */
        { EV_LVDS, 460, 1 },   /* LVDS data on */
        { EV_INIT, 0, 0 },
        { EV_OPEN, 1000, 1 },
        { EV_SET, 1000, 0 },   /* restore the last applied 100% */
        { EV_GPIO, 111, 0 },   /* backlight power on (active low) */
    };
    expect(on, sizeof on / sizeof on[0]);

    assert(panel_iface->get_backlight_percent() == 100);
    for (unsigned i = 0;; i++) {
        int rc = oa_module_entry()->quiesce(ctx);
        if (!rc) break;
        assert(i < 1000);
    }
    oa_module_entry()->stop(ctx);
    oa_module_entry()->destroy(ctx);

    /* Cold-start hook: the same recovered order runs at m_start, before the
     * panel service is published. Default is off so a takeover preserves the
     * stock panel state. */
    g_start_on = 1;
    event_count = 0;
    ctx = NULL;
    assert(oa_module_entry()->init(&api, &ctx) == 0 && ctx);
    assert(oa_module_entry()->start(ctx) == 0 && panel_iface);
    const struct event cold_on[] = {
        { EV_GPIO, 110, 1 },   /* panel power on */
        { EV_LVDS, 460, 1 },   /* LVDS data on */
        { EV_INIT, 0, 0 },
        { EV_OPEN, 1000, 1 },
        { EV_SET, 1000, 0 },   /* panel.start 100% */
        { EV_GPIO, 111, 0 },   /* backlight power on (active low) */
    };
    expect(cold_on, sizeof cold_on / sizeof cold_on[0]);
    oa_module_entry()->stop(ctx);
    oa_module_entry()->destroy(ctx);
    puts("ok panel standby: recovered power/LVDS/PWM order and pins");
    puts("ok panel cold start: opt-in power-on sequence at m_start");
    puts("all panel standby tests passed");
    return 0;
}
