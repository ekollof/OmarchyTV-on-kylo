#ifndef OPENAURORA_CHANNEL_FILTER_H
#define OPENAURORA_CHANNEL_FILTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Stock channel filter/sort popup, recovered from
 * ChannelListFilterSortDialog::initialiseFields 0x00e8f1a4 and
 * DialogFilterSortSelector.svg (BinSVG 636):
 *   panel 60,117,666,467 fill #494949, border #DCDCDC, dialog name
 *   "ChannelListFilter"; filterSortTypeList covers the same rect.
 * The item labels/values are the recovered language keys: CHANNEL_TYPE with
 * ALL/DVB_AERIAL_ONLY/DVB_CABLE_ONLY/DVB_SATELLITE_ONLY/IPTV_ONLY/ANALOG_ONLY,
 * FILTER_TV_RADIO_LABEL with TV_ONLY/RADIO_ONLY/TEXT_ONLY, FREE_PAY_LABEL with
 * FILTER_FREE/FILTER_ENCRYPTED, FILTER_LETTER_LABEL (A-Z) and SORT_LABEL with
 * SORT_NUMERIC/SORT_ALPHABETIC. The HD filter values UHD/HD/SD, HD/SD, SD, HD,
 * UHD are also literal, but their item label is not in the langprofile, so
 * that item is not modelled.
 *
 * Row pitch 70 is the captured settings-list step (rows at 186+70*N in the
 * p0/03 panel); the popup list is 350 px tall, so 5 rows are visible. The
 * vendor's current filter values come from the CA/filter view at runtime and
 * are not recovered; the project defaults are the first value of each item.
 * This helper performs no UI or host I/O.
 */
typedef struct {
    int x, y, x1, y1;
} oa_channel_filter_rect;

typedef struct {
    const char *key;                 /* language key, NULL if literal label */
    const char *const *values;       /* language keys */
    unsigned value_count;
} oa_channel_filter_item;

typedef struct {
    int selected;                    /* item index */
    unsigned value[10];              /* value index per item */
} oa_channel_filter;

typedef struct {
    /* Authored BinSVG record 636 layout (not opaque paint): main_rectangle
     * 60,117,666,467 and filterSortTypeList, fill #494949, border #DCDCDC. */
    oa_channel_filter_rect panel;
    int row_pitch;                   /* 70, captured settings-list step */
    int row_height;                  /* 66, SettingListBoxRowFA art */
    int visible_rows;                /* 5 */
    unsigned char panel_rgb[3], border_rgb[3];
    unsigned char focus_rgb[3], text_rgb[3], focus_text_rgb[3];
    /* Live render, read-only on-TV fb1 capture 2026-11-19: the shared
     * SettingItemsListBox paints a 638x920 #3F0B13 panel at 142,160..780,1080
     * (the same panel as the edit pane), a 75 px row step and a 614x65 #FE9419
     * focus bar starting at 152,228. Ten rows are visible, including the
     * UHD/HD/SD, FAVOURITES, greyed PROFILE_OPTIONS/SATELLITE_LIST and RESET
     * rows that record 636 does not name. */
    oa_channel_filter_rect render_panel;
    oa_channel_filter_rect render_focus;
    int render_row_pitch;
    int render_rows;
    unsigned char render_panel_rgb[3];
} oa_channel_filter_layout;

#define OA_CHANNEL_FILTER_ITEMS 10

/* Fill the recovered popup defaults. -EINVAL for NULL. */
int oa_channel_filter_layout_default(oa_channel_filter_layout *out);

/* The fixed item catalogue (length OA_CHANNEL_FILTER_ITEMS), or NULL. */
const oa_channel_filter_item *oa_channel_filter_catalogue(void);

/* First value of every item and selected 0. -EINVAL for NULL. */
int oa_channel_filter_default(oa_channel_filter *out);

/* Move the selected item by delta, clamped to 0..OA_CHANNEL_FILTER_ITEMS-1.
 * Returns the new index or -EINVAL for NULL. */
int oa_channel_filter_move(oa_channel_filter *f, int delta);

/* Change the selected item's value by delta (wrapping within its values).
 * Returns the new value index or -EINVAL for NULL. */
int oa_channel_filter_change(oa_channel_filter *f, int delta);

#ifdef __cplusplus
}
#endif

#endif
