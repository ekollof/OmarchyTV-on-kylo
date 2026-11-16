/* Pure replacement EASI effect backend test. No tuner, TV, HAL or UI. */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/easi_effects.h"

struct recorder {
    enum oa_easi_effect ev[64];
    int arg[64];
    int n;
};

static void record(void *ud, enum oa_easi_effect effect, int arg)
{
    struct recorder *r = ud;
    if (r->n >= (int)(sizeof r->ev / sizeof r->ev[0]))
        return;
    r->ev[r->n] = effect;
    r->arg[r->n] = arg;
    r->n++;
}

static oa_tuner_select_inputs display_inputs(void)
{
    oa_tuner_select_inputs in;
    memset(&in, 0, sizeof in);
    in.service_count = 10;
    in.handle_valid = 1;
    in.handle = 100;
    in.select_result = 0;
    in.display = 1;
    in.post_select = 1;
    in.decoder_available = 0;
    in.authorized = 1;
    in.current_service = 3;
    return in;
}

static void test_init_and_snapshot(void)
{
    oa_easi_effects e;
    oa_easi_effects_state st;
    oa_easi_effects_init(&e, NULL);
    assert(oa_easi_effects_snapshot(&e, &st) == 0);
    assert(st.muted == -1 && st.headphone_muted == -1 && st.scart_muted == -1);
    assert(st.spdif_enabled == -1 && st.decoder == -1 && st.video_hidden == -1);
    assert(st.subtitles_off == -1 && st.teletext_font == -1 && st.effects == 0);
    assert(oa_easi_effects_snapshot(NULL, &st) == -EINVAL);
    assert(oa_easi_effects_snapshot(&e, NULL) == -EINVAL);
}

static void test_display_sequence(void)
{
    struct recorder r;
    oa_easi_effects e;
    oa_tuner_select_ops ops;
    oa_tuner_select_result out;
    oa_easi_effects_state st;
    oa_tuner_select_inputs in = display_inputs();

    memset(&r, 0, sizeof r);
    oa_easi_effects_init(&e, &(oa_easi_effects_io){ &r, record });
    oa_easi_effects_bind(&e, &ops);
    assert(ops.ud == &e);

    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(out.ran && out.pre_mute_applied && out.unmute_applied &&
           out.show_applied && out.show_mode == 0);
    assert(oa_easi_effects_snapshot(&e, &st) == 0);
    assert(st.muted == 0 && st.headphone_muted == 0 && st.scart_muted == 0);
    assert(st.spdif_enabled == 0 && st.decoder == 0 && st.video_hidden == 0);
    assert(st.subtitles_off == 1 && st.teletext_font == -1);
    assert(st.effects == (unsigned)r.n);

    /* Firmware order: notify, mute block (incl. decoder 0 / hide 0), select
     * internal + second select, delay(20), unmute block, show(0). */
    const enum oa_easi_effect want[] = {
        OA_EASI_NOTIFY_SELECTED, OA_EASI_MUTE_ON, OA_EASI_HEADPHONE_MUTE_ON,
        OA_EASI_SCART_MUTE, OA_EASI_SPDIF_OUT, OA_EASI_DECODER_SELECT,
        OA_EASI_HIDE_VIDEO, OA_EASI_SUBTITLES_OFF, OA_EASI_TELETEXT_FONT,
        OA_EASI_SELECT_INTERNAL, OA_EASI_SELECT_SECOND, OA_EASI_DELAY,
        OA_EASI_MUTE_OFF, OA_EASI_HEADPHONE_MUTE_OFF, OA_EASI_SCART_MUTE,
        OA_EASI_SHOW_VIDEO
    };
    assert(r.n == (int)(sizeof want / sizeof want[0]));
    for (int i = 0; i < r.n; i++)
        assert(r.ev[i] == want[i]);
    assert(r.arg[5] == 0 && r.arg[6] == 0);   /* decoder 0, hide 0 */
    assert(r.arg[11] == 20);                  /* delay 20 ms */
    assert(r.arg[14] == 0 && r.arg[15] == 0); /* scart off, show 0 */
}

static void test_rejected_has_no_effects(void)
{
    struct recorder r;
    oa_easi_effects e;
    oa_tuner_select_ops ops;
    oa_tuner_select_result out;
    oa_tuner_select_inputs in = display_inputs();

    memset(&r, 0, sizeof r);
    oa_easi_effects_init(&e, &(oa_easi_effects_io){ &r, record });
    oa_easi_effects_bind(&e, &ops);
    in.service_count = 0;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(!out.ran && r.n == 0);
    oa_easi_effects_state st;
    assert(oa_easi_effects_snapshot(&e, &st) == 0 && st.effects == 0);
}

static void test_non_display_and_deferral(void)
{
    struct recorder r;
    oa_easi_effects e;
    oa_tuner_select_ops ops;
    oa_tuner_select_result out;
    oa_easi_effects_state st;
    oa_tuner_select_inputs in = display_inputs();

    memset(&r, 0, sizeof r);
    oa_easi_effects_init(&e, &(oa_easi_effects_io){ &r, record });
    oa_easi_effects_bind(&e, &ops);
    in.display = 0;
    in.select_result = 4;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(out.pre_non_display && out.show_applied && out.show_mode == 1);
    assert(oa_easi_effects_snapshot(&e, &st) == 0);
    assert(st.decoder == 1 && st.video_hidden == 0);
    int saw_alternate_show = 0;
    for (int i = 0; i < r.n; i++)
        if (r.ev[i] == OA_EASI_SHOW_VIDEO && r.arg[i] == 1)
            saw_alternate_show = 1;
    assert(saw_alternate_show);

    /* A deferred phase emits no show and leaves the backend unchanged. */
    memset(&r, 0, sizeof r);
    oa_easi_effects_init(&e, &(oa_easi_effects_io){ &r, record });
    oa_easi_effects_bind(&e, &ops);
    in = display_inputs();
    in.post_select = 0;
    in.pending_b0 = 1;
    assert(oa_tuner_select_display(&ops, &in, &out) == 0);
    assert(out.deferred && !out.show_applied);
    int saw_show = 0;
    for (int i = 0; i < r.n; i++)
        if (r.ev[i] == OA_EASI_SHOW_VIDEO)
            saw_show = 1;
    assert(!saw_show);
}

int main(void)
{
    test_init_and_snapshot();
    test_display_sequence();
    test_rejected_has_no_effects();
    test_non_display_and_deferral();
    puts("ok easi effects: replace mute/decoder/hide/show state, ordered emit, "
         "rejection and deferral");
    return 0;
}
