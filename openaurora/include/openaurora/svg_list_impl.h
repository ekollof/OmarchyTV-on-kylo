#ifndef OPENAURORA_SVG_LIST_IMPL_H
#define OPENAURORA_SVG_LIST_IMPL_H
#include <stdint.h>
/* SVGListBoxRowManager::updateImpl/index/full plus rotateRows. The manager
 * layout is first+12/first-visible, +16 count, +20 selected-rel, +28 row
 * array, +32 row array end, +8 capacity, +4 owning list. updateImpl clamps
 * start to 0 when capacity<=slots else to index or capacity-slots, unfocuses
 * a selected row in range, rotates by start-first unless full, then per row:
 * shows newly added rows, draws out-of-old-window rows, and draws in-window
 * rows only when full. Trailing writeback stores count/first, clamps a
 * selected row past the end, refocuses on gotFocus, and on empty count
 * stores selected 0 and draws the empty message/row when enabled. rotateRows
 * swaps row pairs (i,i+delta) from the front (positive delta) or the back
 * (negative delta) for fp iterations, exchanging full positions via
 * getWindowRect temporaries and setWindowPosition. All index arithmetic wraps
 * uint32_t; window comparisons are unsigned; the rotate direction test and
 * loop bound are signed. Field reads/array indexing are pure and untraced;
 * only row/owner virtual calls are compared. Row +0x1C is setFocus,
 * +0x28 getWindowRect (sret into caller buffer), +0x2C setWindowPosition,
 * +0x34 setVisible; owner +0x58 displayMessageIfListEmpty, +0x5c
 * drawRowWithListEmptyMessage, +0x60 drawRow (per-list override); isVisible
 * and gotFocus are stubbed UI queries. NULL ops or any NULL callback
 * returns EINVAL. Row/update bodies stay open. */
struct oa_svg_list_impl_ops {
    void *context;
    uint32_t (*cap)(void *);
    uint32_t (*first)(void *);
    uint32_t (*size)(void *);
    uint32_t (*selected_rel)(void *);
    uint32_t (*slots)(void *);
    void *(*row_at)(void *,uint32_t rel);
    void (*store_row)(void *,uint32_t rel,void *row);
    void *(*owner)(void *);
    void (*store_first)(void *,uint32_t);
    void (*store_size)(void *,uint32_t);
    void (*store_selected)(void *,uint32_t);
    int (*row_visible)(void *,void *row);
    void (*row_set_focus)(void *,void *row,int focused);
    void (*row_set_visible)(void *,void *row,int visible);
    void (*row_rect)(void *,void *row,uint32_t rect[4]);
    void (*row_set_position)(void *,void *row,uint32_t x,uint32_t y);
    int (*owner_empty_message)(void *,void *owner);
    void (*owner_draw_row)(void *,void *owner,uint32_t index,void *row);
    void (*owner_draw_empty_row)(void *,void *owner,void *row);
    int (*owner_got_focus)(void *,void *owner);
};
int oa_svg_list_manager_update_impl(const struct oa_svg_list_impl_ops *,uint32_t index,int full);
int oa_svg_list_manager_rotate_rows(const struct oa_svg_list_impl_ops *,int32_t delta);
#endif
