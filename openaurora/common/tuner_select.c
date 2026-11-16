/* Pure model of the recovered TunerManager display-select body. No I/O. */
#include "openaurora/tuner_select.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

int oa_tuner_select_pre_mute_gate(int result, int prev, int country,
                                  int current_service, int handle,
                                  int member_8_zero)
{
    /* (result & ~4) == 0: vendor result is 0 or 4 (uVar5 & 0xfffffffb). */
    if (((unsigned int)result & ~4u) == 0u)
        return 1;
    /* prev == 4 && this[8] == 0, with a country split. */
    if (prev == 4 && member_8_zero) {
        if (country != 0x14)
            return 1;
        if (handle != current_service)
            return 1;
    }
    return 0;
}

int oa_tuner_select_display(const oa_tuner_select_ops *ops,
                            const oa_tuner_select_inputs *in,
                            oa_tuner_select_result *out)
{
    int result, prev, d0, e0, e4, b0, b4;
    int display_post;

    if (!ops || !in || !out)
        return -EINVAL;
    memset(out, 0, sizeof(*out));

    /* Recovered rejection gates, in vendor order. */
    if (in->service_count <= 0) {
        out->reject_reason = OA_TUNER_SELECT_EMPTY_LIST;
        return 0;
    }
    if (in->standby_requested) {
        out->reject_reason = OA_TUNER_SELECT_STANDBY;
        return 0;
    }
    if (!in->handle_valid) {
        out->reject_reason = OA_TUNER_SELECT_BAD_HANDLE;
        return 0;
    }
    out->ran = 1;

    result = in->select_result;
    prev = in->prev_select_result;
    d0 = 1; /* DAT_00f775d0 = 1 immediately after the first select. */
    e0 = 0;
    e4 = 0;
    b0 = in->pending_b0;
    b4 = in->pending_b4;

    /* notifyServiceSelected when display && !param_7. */
    if (in->display && !in->notify_suppressed && ops->notify_selected)
        ops->notify_selected(ops->ud);

    /* Pre-select mute/hide gate (0xf77588). */
    if (oa_tuner_select_pre_mute_gate(result, prev, in->country,
                                      in->current_service, in->handle,
                                      in->member_8_zero)) {
        if (in->display) {
            if (ops->mute_on) ops->mute_on(ops->ud);
            if (ops->headphone_mute_on) ops->headphone_mute_on(ops->ud);
            if (ops->mute_scart) ops->mute_scart(ops->ud, 1);
            if (ops->spdif_out) ops->spdif_out(ops->ud, 0);
            if (ops->decoder) ops->decoder(ops->ud, 0);
            if (ops->hide_video) ops->hide_video(ops->ud, 0);
            if (ops->subtitles_off) ops->subtitles_off(ops->ud, 1);
            if (ops->teletext_font) ops->teletext_font(ops->ud);
            out->pre_mute_applied = 1;
            e0 = 1; /* DAT_00f775e0 = 1 */

            /* If this condition fails the vendor jumps past the d0 clear,
             * leaving DAT_00f775d0 set (a pending mute). */
            if ((result == 0 || prev != 4) ||
                (in->country != 5 && in->country != 0xc &&
                 in->country != 0x14 && in->country != 0x1b))
                goto after_pre;
            d0 = 0;
        } else {
            if (ops->decoder) ops->decoder(ops->ud, 1);
            if (ops->hide_video) ops->hide_video(ops->ud, 1);
            out->pre_non_display = 1;
            e4 = 1; /* DAT_00f775e4 = 1 */
        }
    }
after_pre:
    /* DAT_00f775d4 = uVar5 (cache the current select result). */
    prev = result;

    if (in->dual_view_available && ops->dual_view_reset)
        ops->dual_view_reset(ops->ud);
    if (ops->select_internal) ops->select_internal(ops->ud);
    if (ops->second_select) ops->second_select(ops->ud);

    /* Post-select phase. display_post is true for the display unmute/show
     * block (LAB_00f7742c); false for the non-display block (LAB_00f775fe). */
    display_post = in->display;
    if (!in->post_select) {
        /* param_6 == 0: a pending b0/b4 can defer or redirect the phase. */
        if (!in->display)
            goto non_display_post;
        if (b0 == 0)
            goto display_post_block;
        b0 = in->post_select; /* clears DAT_00f776b0 to 0 */
        out->deferred = 1;
        goto done;
    }

    e0 = 0; /* DAT_00f775e0 = 0 on entry to the param_6 phase. */
    if (display_post)
        goto display_post_block;

non_display_post:
    /* LAB_00f775fe: gated by DAT_00f776b4 (b4). */
    if (b4 != 0) {
        b4 = 0; /* *DAT_00f776b4 = 0 */
        out->deferred = 1;
        goto done;
    }
    if (in->decoder_available == 0 && ops->mute_off)
        ops->mute_off(ops->ud);
    if (!in->dual_view_active && result != 0) {
        if (ops->decoder) ops->decoder(ops->ud, 1);
        if (ops->show_video) ops->show_video(ops->ud, 1);
        out->show_applied = 1;
        out->show_mode = 1;
    }
    goto done;

display_post_block:
    /* LAB_00f7742c. */
    if (result == 0 && ops->delay)
        ops->delay(ops->ud, 20); /* FROST_Sleep(0x14) */

    {
        int authorized = in->authorized;
        /* When current service is outside the list, vendor forces allowed. */
        if (in->current_service >= in->service_count)
            authorized = 1;
        if (!in->device_in_standby && in->decoder_available == 0 &&
            authorized) {
            if (ops->mute_off) ops->mute_off(ops->ud);
            if (ops->headphone_mute_off) ops->headphone_mute_off(ops->ud);
            if (ops->mute_scart) ops->mute_scart(ops->ud, 0);
            out->unmute_applied = 1;
        }
        if (in->subtitles_on && !in->subtitle_enabled) {
            if (ops->restore_subtitles) ops->restore_subtitles(ops->ud);
            out->subtitles_restored = 1;
        }
        /* showVideo is skipped when dual view is active or unauthorized. */
        if (in->dual_view_active || !authorized)
            goto done;
        if (ops->show_video) ops->show_video(ops->ud, 0);
        out->show_applied = 1;
        out->show_mode = 0;
    }

done:
    out->flag_d0 = d0;
    out->flag_e0 = e0;
    out->flag_e4 = e4;
    out->flag_b0 = b0;
    out->flag_b4 = b4;
    return 0;
}
