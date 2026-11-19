/* Channel filter/sort popup model. Pure; no UI or host I/O. */
#include "openaurora/channel_filter.h"

#include <errno.h>
#include <string.h>

static const char *const TYPE_VALUES[] = {
    "ALL", "DVB_AERIAL_ONLY", "DVB_CABLE_ONLY", "DVB_SATELLITE_ONLY",
    "IPTV_ONLY", "ANALOG_ONLY"
};
static const char *const TV_RADIO_VALUES[] = {
    "TV_ONLY", "RADIO_ONLY", "TEXT_ONLY"
};
/* ALL is the literal Dutch "Alles" fallback (no i18n key recovered) used by
 * the free/pay, letter and quality rows in the live popup. */
static const char *const FREE_PAY_VALUES[] = {
    "Alles", "FILTER_FREE", "FILTER_ENCRYPTED"
};
static const char *const LETTER_VALUES[] = { "Alles", "A-Z" };
static const char *const SORT_VALUES[] = {
    "SORT_NUMERIC", "SORT_ALPHABETIC"
};
/* The extra five rows are visible on live firmware but not named in record
 * 636; their labels are the observed Dutch strings and the quality values are
 * the vendor literals already noted in the header. */
static const char *const QUALITY_VALUES[] = {
    "Alles", "UHD_HD_SD", "UHD_HD", "SD", "UHD"
};
static const char *const FAVOURITES_VALUES[] = { "Geen", "Alles" };
static const char *const NONE_ALL_VALUES[] = { "Geen", "Alles" };
static const char *const ALL_ONLY_VALUES[] = { "Alles" };

static const oa_channel_filter_item CATALOGUE[OA_CHANNEL_FILTER_ITEMS] = {
    { "CHANNEL_TYPE", TYPE_VALUES, 6 },
    { "FILTER_TV_RADIO_LABEL", TV_RADIO_VALUES, 3 },
    { "FREE_PAY_LABEL", FREE_PAY_VALUES, 3 },
    { "FILTER_LETTER_LABEL", LETTER_VALUES, 2 },
    { "SORT_LABEL", SORT_VALUES, 2 },
    { "UHD/HD/SD", QUALITY_VALUES, 5 },
    { "Favorieten", FAVOURITES_VALUES, 2 },
    { "Profielopties", NONE_ALL_VALUES, 2 },
    { "Satellietlijst", ALL_ONLY_VALUES, 1 },
    { "Reset", NULL, 0 }             /* action row: no value */
};

int oa_channel_filter_layout_default(oa_channel_filter_layout *out)
{
    if (!out)
        return -EINVAL;
    memset(out, 0, sizeof *out);
    out->panel = (oa_channel_filter_rect){ 60, 117, 666, 467 };
    out->row_pitch = 70;
    out->row_height = 66;
    out->visible_rows = 5;
    const unsigned char panel[3] = { 73, 73, 73 };      /* #494949 */
    const unsigned char border[3] = { 220, 220, 220 };   /* #DCDCDC */
    const unsigned char focus[3] = { 254, 148, 25 };     /* #FE9419 */
    const unsigned char text[3] = { 255, 255, 255 };
    const unsigned char focus_text[3] = { 126, 0, 10 };  /* #7E000A */
    const unsigned char rendered[3] = { 63, 11, 19 };     /* #3F0B13 */
    memcpy(out->panel_rgb, panel, sizeof panel);
    memcpy(out->border_rgb, border, sizeof border);
    memcpy(out->focus_rgb, focus, sizeof focus);
    memcpy(out->text_rgb, text, sizeof text);
    memcpy(out->focus_text_rgb, focus_text, sizeof focus_text);
    out->render_panel = (oa_channel_filter_rect){ 142, 160, 780, 1080 };
    out->render_focus = (oa_channel_filter_rect){ 152, 228, 766, 293 };
    out->render_row_pitch = 75;
    out->render_rows = 10;
    memcpy(out->render_panel_rgb, rendered, sizeof rendered);
    return 0;
}

const oa_channel_filter_item *oa_channel_filter_catalogue(void)
{
    return CATALOGUE;
}

int oa_channel_filter_default(oa_channel_filter *out)
{
    if (!out)
        return -EINVAL;
    memset(out, 0, sizeof *out);
    return 0;
}

int oa_channel_filter_move(oa_channel_filter *f, int delta)
{
    if (!f)
        return -EINVAL;
    int selected = f->selected + delta;
    if (selected < 0)
        selected = 0;
    if (selected >= OA_CHANNEL_FILTER_ITEMS)
        selected = OA_CHANNEL_FILTER_ITEMS - 1;
    f->selected = selected;
    return selected;
}

int oa_channel_filter_change(oa_channel_filter *f, int delta)
{
    if (!f || f->selected < 0 || f->selected >= OA_CHANNEL_FILTER_ITEMS)
        return -EINVAL;
    unsigned count = CATALOGUE[f->selected].value_count;
    if (count == 0)
        return -EINVAL;
    int value = (int)f->value[f->selected] + delta;
    int wrapped = value % (int)count;
    if (wrapped < 0)
        wrapped += (int)count;
    f->value[f->selected] = (unsigned)wrapped;
    return wrapped;
}
