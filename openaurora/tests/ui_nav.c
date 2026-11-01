/* Loads shipped mod_ir, mod_ui_fb, mod_ui_anim and mod_settings.
 * Frames go through ir's parser. The display is a recorder, not /dev/fb.
 * No UART and no CEC/audio device. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host.h"
#include "openaurora/events.h"
#include "openaurora/services.h"
#include "openaurora/ui_anim.h"
#include "ui_nav_trace.h"

static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[160];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}

static oa_action_event g_act;
static char g_name[48];
static int g_acts;

static void on_action(const void *evp, void *ud)
{
    const oa_action_event *ev = evp;
    (void)ud;
    g_act = *ev;
    g_name[0] = 0;
    if (ev->name)
        snprintf(g_name, sizeof g_name, "%s", ev->name);
    g_acts++;
}

static void frame(uint8_t out[9], uint8_t a1, uint8_t cmd, int good)
{
    out[0] = 0xff;
    out[1] = 0xff;
    out[2] = 0x50;
    out[3] = a1;
    out[4] = 0x11;
    out[5] = cmd;
    out[6] = good ? (uint8_t)~cmd : 0;
    out[7] = 0;
    out[8] = 0;
}

static void press(const oa_input_iface *input, uint8_t a1)
{
    uint8_t bytes[9];
    frame(bytes, a1, 0x00, 1);
    assert(!input->feed(bytes, 9));
}

static void accent(const oa_nav_trace *trace, int *x, int *y, int *w, int *h)
{
    assert(!trace->accent(x, y, w, h));
}

/* Center of the topmost recorded fill whose center the shipped hit() accepts. */
static void top_hit(const oa_nav_trace *trace, const oa_portal_iface *portal,
                    int *cx, int *cy, int *band, int *index)
{
    int n = trace->fill_count();
    int found = 0, best_x = 0, best_y = 0;
    assert(n > 0);
    for (int i = 0; i < n; i++) {
        int x = 0, y = 0, w = 0, h = 0, r = 0, g = 0, b = 0;
        int hb = 0, hi = 0, px, py;
        assert(!trace->fill_at(i, &x, &y, &w, &h, &r, &g, &b));
        if (w <= 0 || h <= 0)
            continue;
        px = x + w / 2;
        py = y + h / 2;
        if (!portal->hit(px, py, &hb, &hi))
            continue;
        if (!found || y < best_y || (y == best_y && x < best_x)) {
            found = 1;
            best_x = x;
            best_y = y;
            *cx = px;
            *cy = py;
            *band = hb;
            *index = hi;
        }
    }
    assert(found);
}

static void card_hit(const oa_nav_trace *trace, const oa_portal_iface *portal,
                     int *cx, int *cy, int *index)
{
    int n = trace->fill_count();
    int found = 0;
    for (int i = 0; i < n; i++) {
        int x = 0, y = 0, w = 0, h = 0, r = 0, g = 0, b = 0;
        int hb = 0, hi = 0, px, py;
        assert(!trace->fill_at(i, &x, &y, &w, &h, &r, &g, &b));
        if (w <= 0 || h <= 0)
            continue;
        px = x + w / 2;
        py = y + h / 2;
        if (!portal->hit(px, py, &hb, &hi) || hb != 1)
            continue;
        *cx = px;
        *cy = py;
        *index = hi;
        found = 1;
        break;
    }
    assert(found);
}

/* Walk remote keys until the accent matches the rail focus saved before the hit. */
static void restore_rail(const oa_input_iface *input, const oa_nav_trace *trace, int moved_y)
{
    for (int n = 0; n < 12; n++) {
        int x = 0, y = 0, w = 0, h = 0;
        accent(trace, &x, &y, &w, &h);
        if (y == moved_y && w == 160 && h == 160)
            return;
        press(input, w == 181 ? 21 : (y < moved_y ? 19 : 20));
    }
    assert(!"rail focus did not return");
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    oa_host host;
    char out[512];
    assert(!oa_host_init(&host, argv[1], argv[2]));
    host.log_level = OA_LOG_ERROR;
    assert(!command(&host, "load plat", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start plat", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load nav_display", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start nav_display", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ir", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ir", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ticker", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ticker", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_clock", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_clock", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_anim", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_anim", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load settings", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start settings", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_fb", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_fb", out, sizeof out) && !strcmp(out, "ok\n"));

    const oa_input_iface *input = host.api.lookup(&host, OA_SVC_INPUT, 1);
    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    const oa_portal_iface *portal = host.api.lookup(&host, OA_SVC_PORTAL, 1);
    const oa_ui_anim_iface *anim = host.api.lookup(&host, OA_SVC_UI_ANIM, 1);
    assert(input && trace && portal && anim && !input->device_open());
    assert(!anim->enabled() && portal->screen() == 0);
    assert(host.api.subscribe(&host, OA_TOPIC_ACTION, on_action, NULL));

    int clears = 0, presents = 0, sw = 0, sh = 0;
    assert(!trace->counts(&clears, &presents, &sw, &sh));
    assert(clears >= 1 && presents >= 1 && sw == 1920 && sh == 1080);
    int x = 0, y = 0, w = 0, h = 0;
    accent(trace, &x, &y, &w, &h);
    assert(w == 160 && h == 160);
    int rail_y = y;

    uint8_t bytes[9];
    frame(bytes, 19, 0x00, 1); /* Down, profile RC in A1 */
    assert(!input->feed(bytes, 9));
    accent(trace, &x, &y, &w, &h);
    assert(y > rail_y && w == 160 && h == 160);
    int moved_y = y;

    frame(bytes, 19, 0x00, 0); /* bad complement: ignored */
    assert(!input->feed(bytes, 9));
    accent(trace, &x, &y, &w, &h);
    assert(y == moved_y && w == 160);

    frame(bytes, 22, 0x00, 1); /* Right, into the card row */
    assert(!input->feed(bytes, 9));
    accent(trace, &x, &y, &w, &h);
    assert(w == 181 && h == 105);
    int card_x = x, card_y = y;

    frame(bytes, 21, 0x00, 1); /* Left, back to the rail */
    assert(!input->feed(bytes, 9));
    accent(trace, &x, &y, &w, &h);
    assert(w == 160 && h == 160 && y == moved_y);

    assert(!command(&host, "reload ui_fb", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "status ticker", out, sizeof out) && strstr(out, "state   started"));
    assert(!command(&host, "status ui_fb", out, sizeof out));
    assert(strstr(out, "state   started"));
    assert(!command(&host, "status ui_anim", out, sizeof out) && strstr(out, "state   started"));
    assert(!command(&host, "status settings", out, sizeof out) && strstr(out, "state   started"));
    input = host.api.lookup(&host, OA_SVC_INPUT, 1);
    trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    portal = host.api.lookup(&host, OA_SVC_PORTAL, 1);
    anim = host.api.lookup(&host, OA_SVC_UI_ANIM, 1);
    assert(input && trace && portal && anim && !input->device_open());
    assert(portal->screen() == 0);
    accent(trace, &x, &y, &w, &h);
    assert(x >= 0 && y == moved_y && w == 160 && h == 160);

    int rail_cx = x + w / 2, rail_cy = y + h / 2, rail_band = -1, rail_index = -1;
    assert(portal->hit(rail_cx, rail_cy, &rail_band, &rail_index) == 1);
    int card_cx = 0, card_cy = 0, card_index = -1;
    card_hit(trace, portal, &card_cx, &card_cy, &card_index);

    int top_x = 0, top_y = 0, top_band = -1, top_index = -1;
    int before_y = y;
    top_hit(trace, portal, &top_x, &top_y, &top_band, &top_index);
    assert(portal->activate_at(top_x, top_y) == 1);
    int again_band = -1, again_index = -1;
    assert(portal->hit(top_x, top_y, &again_band, &again_index) == 1);
    assert(again_band == top_band && again_index == top_index);
    accent(trace, &x, &y, &w, &h);
    assert(y < before_y);
    int miss_band = -1, miss_index = -1;
    assert(portal->hit(0, 0, &miss_band, &miss_index) == 0);
    assert(miss_band == -1 && miss_index == -1 && portal->activate_at(0, 0) == 0);
    accent(trace, &x, &y, &w, &h);
    int miss_y = y;
    assert(portal->hit(top_x, top_y, &again_band, &again_index) == 1);
    assert(again_band == top_band && again_index == top_index && y == miss_y);

    restore_rail(input, trace, moved_y);
    assert(portal->screen() == 0);
    assert(portal->activate_at(card_cx, card_cy) == 1);
    assert(portal->hit(card_cx, card_cy, &again_band, &again_index) == 1);
    assert(again_band == 1 && again_index == card_index);
    accent(trace, &x, &y, &w, &h);
    assert(w == 181 && h == 105 && x == card_x && y == card_y);
    restore_rail(input, trace, moved_y);

    press(input, 56); /* Aux: source index is the settings column */
    assert(g_acts == 1 && g_act.kind == OA_ACTION_SOURCE && g_act.index == 4);
    assert(g_name[0] == 0 && portal->screen() == 0);

    press(input, 53); /* Select, animation still disabled: open at once */
    assert(g_acts == 2 && g_act.kind == OA_ACTION_SELECT && g_act.index == 3);
    assert(!strcmp(g_name, "rail4") && portal->screen() == 1);
    assert(portal->hit(rail_cx, rail_cy, &again_band, &again_index) == 1);
    assert(again_band == rail_band && again_index == rail_index);
    assert(portal->hit(card_cx, card_cy, &again_band, &again_index) == 1);
    assert(again_band == 1 && again_index == card_index);

    uint8_t release[9] = { 0xff, 0xff, 0x5e, 53, 0, 0, 0, 0, 0 };
    assert(!input->feed(release, 9));
    assert(g_acts == 2);

    frame(bytes, 0x66, 0x8a, 1); /* NEC address, command OK */
    assert(!input->feed(bytes, 9));
    assert(g_acts == 3 && g_act.kind == OA_ACTION_SELECT && !strcmp(g_name, "rail4"));
    assert(portal->screen() == 1);

    press(input, 48); /* Menu closes the open tile */
    assert(g_acts == 4 && g_act.kind == OA_ACTION_HOME && g_act.index == 3);
    assert(!strcmp(g_name, "rail4") && portal->screen() == 0);

    press(input, 10); /* Exit on the portal still reports back */
    assert(g_acts == 5 && g_act.kind == OA_ACTION_BACK && g_act.index == 3);
    assert(portal->screen() == 0);

    press(input, 22);
    accent(trace, &x, &y, &w, &h);
    assert(w == 181 && x == card_x && y == card_y);
    press(input, 21);
    accent(trace, &x, &y, &w, &h);
    assert(w == 160 && y == moved_y);

    oa_anim_rect ignored = { 0, 0, 1, 1 };
    uint32_t started = 0;
    assert(anim->start(1, OA_ANIM_DIALOG, NULL, NULL, 1000, &started) == -1);
    assert(anim->set_enabled(2) == -1 && !anim->enabled());
    assert(!anim->set_enabled(1) && anim->enabled());

    press(input, 53);
    /* the dialog content is shown from progress 0 so the transform slides it up into place */
    assert(g_acts == 6 && g_act.kind == OA_ACTION_SELECT && portal->screen() == 1);
    int kind = -1;
    oa_anim_rect rect;
    float eased = -1.f;
    uint32_t dur = 0, st = 0;
    memset(&rect, 0, sizeof rect);
    assert(!anim->get(1, &kind, &rect, &eased, &st, &dur));
    assert(kind == OA_ANIM_DIALOG && dur == 1000 && eased == 0.f);
    int paints_before = 0, paints_after = 0;
    assert(!trace->counts(NULL, &paints_before, NULL, NULL));
    assert(anim->sample(st + 500) == 1 && portal->screen() == 1);
    assert(!trace->counts(NULL, &paints_after, NULL, NULL));
    assert(paints_after > paints_before); /* ui.fps=0 and no highlight assets/timer */
    assert(!anim->get(1, &kind, &rect, &eased, &st, &dur));
    assert(eased > 0.8f && eased < 0.95f);
    {
        int fst = -1, fsc = 0;
        float fsp = -1.f;
        assert(trace->transform(&fst, &fsp, &fsc) == 0);
        /* vendor first-present rule: DialogLauncher declares animation_slide_up_fade_in (type 4) */
        assert(fst == 4 && fsp > 0.8f && fsp < 0.95f && fsc > 0);
    }
    assert(anim->sample(st + 1000) == 0 && portal->screen() == 1);
    {
        int fst = -1, fsc = 0;
        float fsp = -1.f;
        assert(trace->transform(&fst, &fsp, &fsc) == 0 && fst == 0 && fsp == 0.0f);
    }
    assert(anim->get(1, NULL, NULL, NULL, NULL, NULL) == -1);

    press(input, 10);
    assert(g_acts == 7 && g_act.kind == OA_ACTION_BACK && portal->screen() == 0);

    press(input, 53);
    assert(g_acts == 8 && portal->screen() == 1); /* dialog content shown at progress 0 */
    assert(!anim->get(1, &kind, NULL, NULL, &st, &dur) && dur == 1000);
    assert(anim->sample(st + 100) == 1);
    assert(!anim->cancel(1));
    /* a raw controller cancel drops the entry without notifying the consumer (recovered), so the
     * UI keeps the dialog content; its own Exit path is what returns to the portal. */
    assert(portal->screen() == 1 && anim->get(1, NULL, NULL, NULL, NULL, NULL) == -1);
    press(input, 10);
    assert(portal->screen() == 0);

    oa_anim_rect from = { 0, 0, 10, 10 };
    oa_anim_rect to = { 10, 20, 30, 40 };
    assert(!anim->start(3, OA_ANIM_LIST, &from, &to, 2, &started));
    assert(anim->sample(started + 1) == 1);
    assert(!anim->get(3, &kind, &rect, &eased, &st, &dur));
    assert(kind == OA_ANIM_LIST && dur == 2);
    assert(rect.x == 5 && rect.y == 10 && rect.w == 20 && rect.h == 25);
    accent(trace, &x, &y, &w, &h);
    assert(x == rect.x && y == rect.y && w == rect.w && h == rect.h);
    assert(!anim->cancel(3));

    oa_anim_rect zero = { 0, 0, 0, 0 };
    oa_anim_rect ten = { 10, 0, 0, 0 };
    assert(!anim->start(4, OA_ANIM_LIST, &zero, &ten, 3, &started));
    assert(anim->sample(started + 1) == 1);
    assert(!anim->get(4, NULL, &rect, NULL, NULL, &dur) && dur == 3 && rect.x == 3);
    assert(anim->start(4, OA_ANIM_LIST, &zero, &ten, 0, &started) == -1);
    assert(!anim->get(4, NULL, NULL, NULL, NULL, &dur) && dur == 3);
    assert(!anim->start(4, OA_ANIM_LIST, &zero, &ten, 9, &started));
    assert(!anim->get(4, NULL, NULL, NULL, NULL, &dur) && dur == 9);
    assert(anim->sample(started) == 1);
    assert(!anim->finalise(4));
    assert(anim->get(4, NULL, NULL, NULL, NULL, NULL) == -1);
    accent(trace, &x, &y, &w, &h);
    assert(x == 10 && y == 0 && w == 0 && h == 0 && portal->screen() == 0);

    assert(!anim->start(11, OA_ANIM_LIST, &from, &to, 100, &started));
    assert(!anim->start(12, OA_ANIM_LIST, &from, &to, 100, &started));
    assert(!anim->start(13, OA_ANIM_LIST, &from, &to, 100, &started));
    assert(!anim->start(14, OA_ANIM_LIST, &from, &to, 100, &started));
    assert(anim->start(15, OA_ANIM_LIST, &from, &to, 100, &started) == -1);
    assert(anim->sample(started) == 4);
    assert(!anim->cancel(11) && !anim->cancel(12) && !anim->cancel(13) && !anim->cancel(14));
    assert(anim->sample(started) == 0);
    (void)ignored;

    assert(!command(&host, "reload ui_fb", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "status ticker", out, sizeof out) && strstr(out, "state   started"));
    portal = host.api.lookup(&host, OA_SVC_PORTAL, 1);
    assert(portal && portal->screen() == 0);

    /* An interrupted dialog must not later reopen the old selection or leave
     * the scene transformed. No periodic repaint is needed for this cleanup. */
    anim = host.api.lookup(&host, OA_SVC_UI_ANIM, 1);
    for (int pass = 0; pass < 2; pass++) {
        press(input, 53);
        assert(!anim->get(1, NULL, NULL, NULL, &st, &dur));
        assert(anim->sample(st + 100) == 1);
        press(input, pass ? 48 : 10);
        assert(anim->sample(st + 1000) == 0 && portal->screen() == 0);
        int fst = -1, fsc = 0;
        float fsp = -1.f;
        assert(!trace->transform(&fst, &fsp, &fsc) && fst == 0);
    }

    host.stop_requested = 1;
    assert(!oa_host_run(&host));
    puts("  ok   full-screen portal follows remote frames; focus survives reload");
    return 0;
}
