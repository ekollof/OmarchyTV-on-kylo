/* Pure TunerManager display-select test. No tuner, TV, HAL or UI. */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/tuner_select.h"

struct recorder {
    char log[1024];
    size_t len;
    int delay_ms;
    int scart_on;
    int decoder_set;
    int hide;
    int show_mode;
};

static void rec(struct recorder *r, const char *fmt, ...)
{
    va_list ap;
    int n;
    if (r->len >= sizeof r->log)
        return;
    va_start(ap, fmt);
    n = vsnprintf(r->log + r->len, sizeof r->log - r->len, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if ((size_t)n >= sizeof r->log - r->len)
        r->len = sizeof r->log - 1;
    else
        r->len += (size_t)n;
}

static void f_mute_on(void *u) { rec((struct recorder *)u, "mute_on|"); }
static void f_hp_on(void *u) { rec((struct recorder *)u, "hp_on|"); }
static void f_scart(void *u, int on)
{
    struct recorder *r = u;
    r->scart_on = on;
    rec(r, "scart%d|", on);
}
static void f_spdif(void *u, int enable)
{
    rec((struct recorder *)u, "spdif%d|", enable);
}
static void f_decoder(void *u, int set)
{
    struct recorder *r = u;
    r->decoder_set = set;
    rec(r, "dec%d|", set);
}
static void f_hide(void *u, int hide)
{
    struct recorder *r = u;
    r->hide = hide;
    rec(r, "hide%d|", hide);
}
static void f_subs_off(void *u, int on)
{
    rec((struct recorder *)u, "subsoff%d|", on);
}
static void f_teletext(void *u) { rec((struct recorder *)u, "teletext|"); }
static void f_notify(void *u) { rec((struct recorder *)u, "notify|"); }
static void f_select_internal(void *u)
{
    rec((struct recorder *)u, "sel_internal|");
}
static void f_second_select(void *u)
{
    rec((struct recorder *)u, "sel_second|");
}
static void f_dual_reset(void *u) { rec((struct recorder *)u, "dual_reset|"); }
static void f_delay(void *u, unsigned ms)
{
    struct recorder *r = u;
    r->delay_ms = (int)ms;
    rec(r, "delay%d|", ms);
}
static void f_mute_off(void *u) { rec((struct recorder *)u, "mute_off|"); }
static void f_hp_off(void *u) { rec((struct recorder *)u, "hp_off|"); }
static void f_restore_subs(void *u)
{
    rec((struct recorder *)u, "subs_restore|");
}
static void f_show(void *u, int mode)
{
    struct recorder *r = u;
    r->show_mode = mode;
    rec(r, "show%d|", mode);
}

static oa_tuner_select_ops ops = {
    NULL,
    f_mute_on, f_hp_on, f_scart, f_spdif, f_decoder, f_hide,
    f_subs_off, f_teletext, f_notify, f_select_internal, f_second_select,
    f_dual_reset, f_delay, f_mute_off, f_hp_off, f_restore_subs, f_show,
    NULL
};

/* A display/mute select that proceeds (result 0) and completes the unmute
 * phase, with all backends available. */
static oa_tuner_select_inputs base_display(void)
{
    oa_tuner_select_inputs in;
    memset(&in, 0, sizeof in);
    in.service_count = 10;
    in.handle_valid = 1;
    in.handle = 100;
    in.select_result = 0;
    in.display = 1;
    in.post_select = 1;
    in.dual_view_available = 1;
    in.decoder_available = 0; /* 0 == available */
    in.authorized = 1;
    in.current_service = 3;
    in.subtitles_on = 0;
    in.country = 0;
    in.member_8_zero = 1;
    return in;
}

static void test_gates(void)
{
    struct recorder r;
    oa_tuner_select_result out;
    oa_tuner_select_inputs in;

    memset(&r, 0, sizeof r);
    ops.ud = &r;
    in = base_display();

    in.service_count = 0;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(!out.ran && out.reject_reason == OA_TUNER_SELECT_EMPTY_LIST);
    assert(r.log[0] == '\0');

    in = base_display();
    in.standby_requested = 1;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(!out.ran && out.reject_reason == OA_TUNER_SELECT_STANDBY);
    assert(r.log[0] == '\0');

    in = base_display();
    in.handle_valid = 0;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(!out.ran && out.reject_reason == OA_TUNER_SELECT_BAD_HANDLE);
    assert(r.log[0] == '\0');

    assert(oa_tuner_select_display(NULL, &in, &out) == -EINVAL);
    assert(oa_tuner_select_display(&ops, NULL, &out) == -EINVAL);
    assert(oa_tuner_select_display(&ops, &in, NULL) == -EINVAL);
}

static void test_pre_mute_gate_predicate(void)
{
    /* result 0 or 4 always enters. */
    assert(oa_tuner_select_pre_mute_gate(0, 0, 0, 0, 0, 1) == 1);
    assert(oa_tuner_select_pre_mute_gate(4, 0, 0, 0, 0, 1) == 1);
    /* other results reject unless the prev==4 path applies. */
    assert(oa_tuner_select_pre_mute_gate(2, 0, 0, 0, 0, 1) == 0);
    assert(oa_tuner_select_pre_mute_gate(2, 4, 0, 0, 0, 1) == 1);
    assert(oa_tuner_select_pre_mute_gate(2, 4, 0x14, 100, 100, 1) == 0);
    assert(oa_tuner_select_pre_mute_gate(2, 4, 0x14, 100, 200, 1) == 1);
    /* this[8] != 0 kills the prev==4 path. */
    assert(oa_tuner_select_pre_mute_gate(2, 4, 0, 0, 0, 0) == 0);
}

static void test_display_full_order(void)
{
    struct recorder r;
    oa_tuner_select_result out;
    oa_tuner_select_inputs in = base_display();

    memset(&r, 0, sizeof r);
    ops.ud = &r;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(out.ran && out.pre_mute_applied && out.unmute_applied);
    assert(out.show_applied && out.show_mode == 0);
    assert(out.subtitles_restored == 0);
    assert(r.delay_ms == 20);
    assert(strcmp(r.log,
        "notify|mute_on|hp_on|scart1|spdif0|dec0|hide0|subsoff1|teletext|"
        "dual_reset|sel_internal|sel_second|delay20|mute_off|hp_off|"
        "scart0|show0|") == 0);
    /* result 0 jumps past the d0 clear, so d0 stays set; e0 is cleared in
     * the post phase; e4 is untouched. */
    assert(out.flag_d0 == 1 && out.flag_e0 == 0 && out.flag_e4 == 0);
}

static void test_notify_suppressed(void)
{
    struct recorder r;
    oa_tuner_select_result out;
    oa_tuner_select_inputs in = base_display();

    memset(&r, 0, sizeof r);
    ops.ud = &r;
    in.notify_suppressed = 1;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(strncmp(r.log, "mute_on|", 8) == 0);
    assert(strstr(r.log, "notify") == NULL);
}

static void test_result_four_country_split(void)
{
    struct recorder r;
    oa_tuner_select_result out;
    oa_tuner_select_inputs in = base_display();

    memset(&r, 0, sizeof r);
    ops.ud = &r;
    in.select_result = 4;
    in.prev_select_result = 4;

    /* result==4 && prev==4 && country 5: vendor clears d0. */
    in.country = 5;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(out.pre_mute_applied && out.flag_d0 == 0);
    assert(strstr(r.log, "delay") == NULL); /* result!=0 skips FROST_Sleep */
    assert(out.unmute_applied && out.show_applied && out.show_mode == 0);

    /* Same result/prev but a country outside {5,c,14,1b} holds d0 set. */
    memset(&r, 0, sizeof r);
    ops.ud = &r;
    in.country = 0;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(out.pre_mute_applied && out.flag_d0 == 1);
}

static void test_unauthorized_and_standby(void)
{
    struct recorder r;
    oa_tuner_select_result out;
    oa_tuner_select_inputs in = base_display();

    memset(&r, 0, sizeof r);
    ops.ud = &r;
    in.authorized = 0;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(!out.unmute_applied && !out.show_applied);
    assert(strstr(r.log, "mute_off") == NULL && strstr(r.log, "show") == NULL);

    /* current_service outside list forces authorized, even when input is 0. */
    memset(&r, 0, sizeof r);
    ops.ud = &r;
    in.authorized = 0;
    in.current_service = 10; /* == service_count */
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(out.unmute_applied && out.show_applied);

    /* device standby blocks unmute but not show. */
    memset(&r, 0, sizeof r);
    ops.ud = &r;
    in = base_display();
    in.device_in_standby = 1;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(!out.unmute_applied && out.show_applied);
}

static void test_dual_view_and_subtitles(void)
{
    struct recorder r;
    oa_tuner_select_result out;
    oa_tuner_select_inputs in = base_display();

    memset(&r, 0, sizeof r);
    ops.ud = &r;
    in.dual_view_active = 1;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(out.unmute_applied && !out.show_applied);
    assert(strstr(r.log, "dual_reset") != NULL);

    memset(&r, 0, sizeof r);
    ops.ud = &r;
    in = base_display();
    in.subtitles_on = 1;
    in.subtitle_enabled = 0;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(out.subtitles_restored);
    assert(strstr(r.log, "subs_restore|") != NULL);
}

static void test_non_display(void)
{
    struct recorder r;
    oa_tuner_select_result out;
    oa_tuner_select_inputs in = base_display();

    memset(&r, 0, sizeof r);
    ops.ud = &r;
    in.display = 0;
    in.select_result = 4;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(out.pre_non_display && out.show_applied && out.show_mode == 1);
    assert(r.decoder_set == 1 && r.show_mode == 1);
    assert(out.flag_e4 == 1 && out.flag_e0 == 0);
    /* dec1/hide1 pre-block runs before select_internal. */
    assert(strstr(r.log, "dec1|hide1|dual_reset|sel_internal|") != NULL);
}

static void test_deferred_flags(void)
{
    struct recorder r;
    oa_tuner_select_result out;
    oa_tuner_select_inputs in = base_display();

    /* param_6 == 0, display, b0 set -> deferred, no unmute/show. */
    memset(&r, 0, sizeof r);
    ops.ud = &r;
    in.post_select = 0;
    in.pending_b0 = 1;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(out.deferred && !out.show_applied);
    assert(out.flag_b0 == 0);
    assert(strstr(r.log, "mute_off") == NULL);

    /* param_6 == 0, non-display, b4 set -> deferred, b4 cleared. */
    memset(&r, 0, sizeof r);
    ops.ud = &r;
    in = base_display();
    in.post_select = 0;
    in.display = 0;
    in.pending_b4 = 1;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(out.deferred && !out.show_applied);
    assert(out.flag_b4 == 0);
}

int main(void)
{
    test_gates();
    test_pre_mute_gate_predicate();
    test_display_full_order();
    test_notify_suppressed();
    test_result_four_country_split();
    test_unauthorized_and_standby();
    test_dual_view_and_subtitles();
    test_non_display();
    test_deferred_flags();
    puts("ok tuner select: gates, pre-mute gate, effect order, unmute/show, "
         "authorization, deferral and rejection");
    return 0;
}
