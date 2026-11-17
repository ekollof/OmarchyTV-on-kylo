#ifndef OPENAURORA_CHANNEL_LAYOUT_H
#define OPENAURORA_CHANNEL_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Stock channel-list layout recovered from DialogAdvancedChannelList.svg
 * (BinSVG record 80) and the read-only p0/04 capture (2026-10-30, BGRA
 * channel-corrected for this module):
 *   panel 0,0,780,1080          #BE1E2D
 *   banner 0,0,780,162          #811924 at opacity 2/10 over the panel
 *   title 51,2,698,67
 *   filterInfo 123,50,766,95
 *   watch/edit/filter tabs 12..262 / 264..514 / 516..766, y 100..159
 *   logo 610,21,766,66          channelListBox 19,179,769,1090
 * The list rows are not in record 80: the capture gives first row 19,195,
 * 748x66, pitch 75, white text, focus #FE9419 with #7E000A text, and the
 * type marker at the right edge. Rectangles are EASIRect-style exclusive x1/y1.
 *
 * This is layout data only. It performs no UI, host or device I/O and does not
 * select the screen or scroll: runtime EASI order/LCN, fonts, the header icon,
 * scrollbar and key path remain open.
 */
typedef struct {
    int x, y, x1, y1;
} oa_channel_rect;

typedef struct {
    oa_channel_rect panel, banner, title, filter_info;
    oa_channel_rect tabs[3]; /* watch, edit, filter */
    oa_channel_rect list, logo;
    oa_channel_rect row;     /* capture-derived first row surface */
    oa_channel_rect scroll;  /* listbox scrollbar window x768..771, y195..1011 */
    int row_pitch;           /* capture-derived */
    int row_height;
    int row_text_height;     /* capture: unfocused ink is 34 px high */
    int focus_text_height;   /* capture: focused ink is 40 px high */
    int list_top_inset;      /* listbox mTopM height */
    int list_bottom_inset;   /* listbox mBotM height */
    int scroll_min_bar;      /* scrollbarFA bar caps 20+16 */
    uint8_t panel_rgb[3], banner_rgb[3];
    uint8_t tab_rgb[3], tab_selected_rgb[3];
    uint8_t focus_rgb[3], text_rgb[3], focus_text_rgb[3], muted_rgb[3];
} oa_channel_layout;

/* Fill the recovered defaults. Returns -EINVAL for NULL. */
int oa_channel_layout_default(oa_channel_layout *out);

/* 0 watch, 1 edit, 2 filter, -1 outside all tabs. */
int oa_channel_tab_at(const oa_channel_layout *layout, int x, int y);

/* Row 0 is the first list row (row.y). Returns 0 and the full row rectangle,
 * or -EINVAL for bad arguments and -ERANGE once the row starts below the
 * panel (the last row may extend past the panel and must be clipped by the
 * caller; that is not an invalid row). */
int oa_channel_row_rect(const oa_channel_layout *layout, int index,
                        oa_channel_rect *out);

/* Visible rows from the listbox insets and row step:
 *   ((list.y1-list.y) - top_inset - bottom_inset) / row_pitch
 * For the recovered geometry this is 11. The 12th row starts below the
 * listbox visible window and is not painted. Returns -EINVAL for NULL. */
int oa_channel_visible_rows(const oa_channel_layout *layout);

#ifdef __cplusplus
}
#endif

#endif
