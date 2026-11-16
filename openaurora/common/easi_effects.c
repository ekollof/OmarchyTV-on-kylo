/* Pure EASI effect-state backend for the TunerManager display-select model. */
#include "openaurora/easi_effects.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

void oa_easi_effects_init(oa_easi_effects *e, const oa_easi_effects_io *io)
{
    if (!e)
        return;
    memset(e, 0, sizeof(*e));
    e->state.muted = -1;
    e->state.headphone_muted = -1;
    e->state.scart_muted = -1;
    e->state.spdif_enabled = -1;
    e->state.decoder = -1;
    e->state.video_hidden = -1;
    e->state.subtitles_off = -1;
    e->state.teletext_font = -1;
    if (io)
        e->io = *io;
}

int oa_easi_effects_snapshot(const oa_easi_effects *e,
                             oa_easi_effects_state *out)
{
    if (!e || !out)
        return -EINVAL;
    *out = e->state;
    return 0;
}

static void emit(oa_easi_effects *e, enum oa_easi_effect effect, int arg)
{
    e->state.effects++;
    if (e->io.emit)
        e->io.emit(e->io.ud, effect, arg);
}

static void f_notify(void *u)
{
    emit((oa_easi_effects *)u, OA_EASI_NOTIFY_SELECTED, 0);
}
static void f_mute_on(void *u)
{
    oa_easi_effects *e = u;
    e->state.muted = 1;
    emit(e, OA_EASI_MUTE_ON, 1);
}
static void f_mute_off(void *u)
{
    oa_easi_effects *e = u;
    e->state.muted = 0;
    emit(e, OA_EASI_MUTE_OFF, 0);
}
static void f_hp_on(void *u)
{
    oa_easi_effects *e = u;
    e->state.headphone_muted = 1;
    emit(e, OA_EASI_HEADPHONE_MUTE_ON, 1);
}
static void f_hp_off(void *u)
{
    oa_easi_effects *e = u;
    e->state.headphone_muted = 0;
    emit(e, OA_EASI_HEADPHONE_MUTE_OFF, 0);
}
static void f_scart(void *u, int on)
{
    oa_easi_effects *e = u;
    e->state.scart_muted = on ? 1 : 0;
    emit(e, OA_EASI_SCART_MUTE, on);
}
static void f_spdif(void *u, int enable)
{
    oa_easi_effects *e = u;
    e->state.spdif_enabled = enable ? 1 : 0;
    emit(e, OA_EASI_SPDIF_OUT, enable);
}
static void f_decoder(void *u, int set)
{
    oa_easi_effects *e = u;
    e->state.decoder = set ? 1 : 0;
    emit(e, OA_EASI_DECODER_SELECT, set);
}
static void f_hide(void *u, int hide)
{
    oa_easi_effects *e = u;
    e->state.video_hidden = hide ? 1 : 0;
    emit(e, OA_EASI_HIDE_VIDEO, hide);
}
static void f_show(void *u, int mode)
{
    oa_easi_effects *e = u;
    e->state.video_hidden = 0;
    emit(e, OA_EASI_SHOW_VIDEO, mode);
}
static void f_subs_off(void *u, int on)
{
    oa_easi_effects *e = u;
    e->state.subtitles_off = on ? 1 : 0;
    emit(e, OA_EASI_SUBTITLES_OFF, on);
}
static void f_teletext(void *u)
{
    oa_easi_effects *e = u;
    e->state.teletext_font = -1;
    emit(e, OA_EASI_TELETEXT_FONT, -1);
}
static void f_subs_restore(void *u)
{
    oa_easi_effects *e = u;
    e->state.subtitles_off = 0;
    emit(e, OA_EASI_SUBTITLES_RESTORE, 0);
}
static void f_select_internal(void *u)
{
    emit((oa_easi_effects *)u, OA_EASI_SELECT_INTERNAL, 0);
}
static void f_second_select(void *u)
{
    emit((oa_easi_effects *)u, OA_EASI_SELECT_SECOND, 0);
}
static void f_dual_reset(void *u)
{
    emit((oa_easi_effects *)u, OA_EASI_DUAL_VIEW_RESET, 0);
}
static void f_delay(void *u, unsigned ms)
{
    emit((oa_easi_effects *)u, OA_EASI_DELAY, (int)ms);
}

void oa_easi_effects_bind(oa_easi_effects *e, oa_tuner_select_ops *ops)
{
    if (!e || !ops)
        return;
    memset(ops, 0, sizeof(*ops));
    ops->ud = e;
    ops->mute_on = f_mute_on;
    ops->headphone_mute_on = f_hp_on;
    ops->mute_scart = f_scart;
    ops->spdif_out = f_spdif;
    ops->decoder = f_decoder;
    ops->hide_video = f_hide;
    ops->subtitles_off = f_subs_off;
    ops->teletext_font = f_teletext;
    ops->notify_selected = f_notify;
    ops->select_internal = f_select_internal;
    ops->second_select = f_second_select;
    ops->dual_view_reset = f_dual_reset;
    ops->delay = f_delay;
    ops->mute_off = f_mute_off;
    ops->headphone_mute_off = f_hp_off;
    ops->restore_subtitles = f_subs_restore;
    ops->show_video = f_show;
}
