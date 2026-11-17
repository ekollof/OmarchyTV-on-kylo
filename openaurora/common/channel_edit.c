/* Channel edit-options model (record 100). Pure; no UI or host I/O. */
#include "openaurora/channel_edit.h"

#include <errno.h>
#include <string.h>

static const oa_channel_edit_option CATALOGUE[OA_CHANNEL_EDIT_OPTIONS] = {
    { "ADD_TO_HOME", 0 },
    { "MOVE_TO_POSITION", 1 },
    { "DELETE", 2 },            /* label switches to REMOVE for multiple */
    { "EDIT_NAME", 3 },
    { "HLP_LOCK_UNLOCK", 4 },
    { "HLP_FAVOURITE_ADD_REMOVE", 5 }, /* combined with FAVOURITES */
    { "HLP_TAG", 6 },
    { "HLP_TAG_ALL", 7 }
};

int oa_channel_edit_layout_default(oa_channel_edit_layout *out)
{
    if (!out)
        return -EINVAL;
    memset(out, 0, sizeof *out);
    out->panel = (oa_channel_edit_rect){ 0, 35, 780, 1080 };
    out->strip = (oa_channel_edit_rect){ 0, 160, 142, 1080 };
    out->content = (oa_channel_edit_rect){ 142, 160, 782, 1080 };
    out->list = (oa_channel_edit_rect){ 152, 211, 768, 1059 };
    out->row_pitch = 70;
    out->row_height = 66;
    out->visible_rows = 12;
    const unsigned char panel[3] = { 255, 255, 255 };
    /* #010101 at 6/10 over the white panel. */
    const unsigned char strip[3] = { 103, 103, 103 };
    const unsigned char content[3] = { 63, 11, 19 };   /* #3F0B13 */
    const unsigned char focus[3] = { 254, 148, 25 };
    const unsigned char text[3] = { 255, 255, 255 };
    const unsigned char focus_text[3] = { 126, 0, 10 };
    memcpy(out->panel_rgb, panel, sizeof panel);
    memcpy(out->strip_rgb, strip, sizeof strip);
    memcpy(out->content_rgb, content, sizeof content);
    memcpy(out->focus_rgb, focus, sizeof focus);
    memcpy(out->text_rgb, text, sizeof text);
    memcpy(out->focus_text_rgb, focus_text, sizeof focus_text);
    return 0;
}

const oa_channel_edit_option *oa_channel_edit_catalogue(void)
{
    return CATALOGUE;
}

int oa_channel_edit_visible(const oa_channel_edit_inputs *in,
                            unsigned *out, unsigned cap, unsigned *count)
{
    if (!in || !out || !count)
        return -EINVAL;
    unsigned n = 0;
    for (unsigned i = 0; i < OA_CHANNEL_EDIT_OPTIONS; i++) {
        unsigned id = CATALOGUE[i].case_id;
        int shown = 1;
        switch (id) {
        case 0: shown = in->single_selection && !in->at_home; break;
        case 1: shown = in->multiple_selection; break;
        case 2: shown = !in->rovi && in->deletable; break;
        case 3: shown = in->name_editable && in->single_selection; break;
        default: shown = 1; break;
        }
        if (!shown)
            continue;
        if (n >= cap)
            return -EOVERFLOW;
        out[n++] = id;
    }
    *count = n;
    return 0;
}

const char *oa_channel_edit_label(unsigned case_id, int multiple_selection)
{
    switch (case_id) {
    case 0: return "ADD_TO_HOME";
    case 1: return "MOVE_TO_POSITION";
    case 2: return multiple_selection ? "REMOVE" : "DELETE";
    case 3: return "EDIT_NAME";
    case 4: return "HLP_LOCK_UNLOCK";
    case 5: return "HLP_FAVOURITE_ADD_REMOVE"; /* caller appends " FAVOURITES" */
    case 6: return "HLP_TAG";
    case 7: return "HLP_TAG_ALL";
    default: return NULL;
    }
}
