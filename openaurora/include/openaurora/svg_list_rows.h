#ifndef OPENAURORA_SVG_LIST_ROWS_H
#define OPENAURORA_SVG_LIST_ROWS_H
#include <stdint.h>
/* SVGListBoxRowManager internals behind the this+0xf8 manager used by every
 * list key/move body. Recovered layout, all offsets in words: +12 (0x0c)
 * first-visible item, +16 (0x10) row count, +20 (0x14) selected window-relative
 * row, +28 (0x1c) row-pointer array, +32 (0x20) row-pointer array end.
 * selectedItem is first-visible plus selected row; selectedRow 0 means no
 * upward move, not an absolute position. focusItem(index) computes
 * rel=index-first (wrapping), returns when the count is zero, rel reaches the
 * count, or rel is already selected; otherwise it unfocuses the old row and
 * focuses the new one through the row +0x1C setFocus virtual (0 unfocus then
 * 1 focus) and stores rel. updatePartial(index) forwards to
 * updateImpl(index,false). getTotalNumberOfRows is slots (end-begin)/4 with
 * an arithmetic shift; getLastVisibleItem is first+size-1 wrapping;
 * getItemRow returns null below first or at/above first+size, else the row.
 * Row handles are opaque: row_at maps a relative position to the handle the
 * row callbacks receive. All callbacks are synchronous, pinned and never
 * retained. NULL ops or any NULL callback returns EINVAL. Getters return raw
 * words or handles; focus/update return errno only. Row widget internals and
 * the scrollbar/animation targets stay open. */
struct oa_svg_list_manager_ops {
    void *context;
    uint32_t (*first)(void *);
    uint32_t (*size)(void *);
    uint32_t (*selected_rel)(void *);
    void *(*row_at)(void *,uint32_t rel);
    void (*row_focus)(void *row,int focused);
    void (*store_selected)(void *,uint32_t rel);
    void (*update_impl)(void *list,uint32_t index,int full);
    uint32_t (*slots)(void *);
};
uint32_t oa_svg_list_manager_size(const struct oa_svg_list_manager_ops *o,int *error);
uint32_t oa_svg_list_manager_selected_row(const struct oa_svg_list_manager_ops *o,int *error);
uint32_t oa_svg_list_manager_selected_item(const struct oa_svg_list_manager_ops *o,int *error);
uint32_t oa_svg_list_manager_first_visible(const struct oa_svg_list_manager_ops *o,int *error);
uint32_t oa_svg_list_manager_total_rows(const struct oa_svg_list_manager_ops *o,int *error);
uint32_t oa_svg_list_manager_last_visible(const struct oa_svg_list_manager_ops *o,int *error);
void *oa_svg_list_manager_item_row(const struct oa_svg_list_manager_ops *o,uint32_t index,int *error);
int oa_svg_list_manager_focus_item(const struct oa_svg_list_manager_ops *o,uint32_t index);
int oa_svg_list_manager_update_partial(const struct oa_svg_list_manager_ops *o,uint32_t index);
#endif
