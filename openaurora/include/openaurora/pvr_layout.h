#ifndef OPENAURORA_PVR_LAYOUT_H
#define OPENAURORA_PVR_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Stock PVR/recording-list layout from DialogPlayList.svg (BinSVG 692) and
 * PlayListDialog:
 *   panel 0,0,1920,1080 #DCDBDA; header 0,0,1921,162 #481730 (9/10 over the
 *   panel); subheader 0,162,1920,216 #010101 (3/10); list 1,216,1911,971;
 *   row 26,222,1886,288 (66 px); name/time/duration columns 181/815/1307;
 *   scrollbar 1910,217,1920,974; five action tabs 206..1714 at y101..160;
 *   title/clock and space-info rects.
 * PlayListDialog::init sets the title from the LIBRARY string key. The five
 * tab buttons are text buttons: SVGButton::create resolves each name through
 * UIResourceManager::lookupString (HLP_PLAY_OPTIONS/EDIT/DELETE/SORT/SETTINGS),
 * with the TabButtonFA/UFA/FI/UFI states (BinSVG 522..525). The row pitch is not in record 692; 75 px is the shared
 * full-screen-list step and is marked capture/class-derived, not resource.
 * Rectangles are EASIRect-style exclusive x1/y1. No UI/host I/O.
 */
typedef struct {
    int x, y, x1, y1;
} oa_pvr_rect;

typedef struct {
    oa_pvr_rect panel, header, subheader;
    oa_pvr_rect title_name, title_start, title_length;
    oa_pvr_rect list, row;
    oa_pvr_rect name, time, duration;
    oa_pvr_rect scrollbar;
    oa_pvr_rect used_space, reserved_space, free_space;
    /* 0,971,1920,1080 dark band behind the white space labels. Record 692
     * leaves the labels on the light panel with white text, so the runtime
     * theme must supply a band; this uses the full-screen dialog family
     * header #481730 as an inference pending a stock PVR capture. */
    oa_pvr_rect space_band;
    oa_pvr_rect tabs[5];
    oa_pvr_rect dialog_title, clock;
    int row_pitch;
    int row_height;
    uint8_t panel_rgb[3], header_rgb[3], subheader_rgb[3];
    uint8_t text_rgb[3], title_rgb[3], focus_rgb[3], space_band_rgb[3];
} oa_pvr_layout;

/* Fill the recovered defaults. Returns -EINVAL for NULL. */
int oa_pvr_layout_default(oa_pvr_layout *out);

/* 0..4 tab index, -1 outside. Exclusive edges. */
int oa_pvr_tab_at(const oa_pvr_layout *layout, int x, int y);

/* Full row rectangle for a visible row. Returns -EINVAL for bad args and
 * -ERANGE when the row starts below the list/screen; the caller clips the
 * last partial row. */
int oa_pvr_row_rect(const oa_pvr_layout *layout, int index, oa_pvr_rect *out);

/* Rows with a top inside the list window: (list.y1-list.y)/row_pitch. */
int oa_pvr_visible_rows(const oa_pvr_layout *layout);

#ifdef __cplusplus
}
#endif

#endif
