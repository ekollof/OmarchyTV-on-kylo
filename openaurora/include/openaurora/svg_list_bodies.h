#ifndef OPENAURORA_SVG_LIST_BODIES_H
#define OPENAURORA_SVG_LIST_BODIES_H
#include <stdint.h>
/* Lower SVGListBoxBase key bodies called by the router in svg_list_input.h.
 * Each body first reads the list-box word +0x1ec and returns unhandled when it
 * is zero, then reads the RowManager at this+0xf8 (size/selectedRow/
 * selectedItem/getFirstVisibleItem/focusItem) and drives movement, window,
 * selection-placement and animation primitives on the list box itself through
 * owned callbacks.
 * Callbacks are synchronous, pinned for the call and never retained. The
 * +0x1ec word is an opaque count: first-visible+visible arithmetic and the
 * last-group index (count-1) are compared against it exactly as shipped.
 * Signed page comparisons use int32_t; all other arithmetic wraps uint32_t.
 * select posts UIListItemSelectedNotifyMessage(item,window id) to the parent
 * through notify() and still consumes when the parent is null; getSelectedItem
 * runs before the window-id read. first/last invoke go_group() with the
 * synthesized indices 0 and count-1; the router-supplied message is not
 * forwarded. first skips the call when the selected item is already 0, last
 * always calls. Every body ends a handled run with start_animation().
 * Unhandled paths (zero count word, zero size) run no callbacks. NULL ops,
 * any NULL callback or NULL handled returns EINVAL with handled untouched.
 * Return errno separately from normalized handled 0/1. These are bodies, not
 * the movement/window/notify primitives themselves, which stay open. */
struct oa_svg_list_box_ops {
    void *context;
    uint32_t (*aux)(void *);
    uint32_t (*size)(void *);
    uint32_t (*selected_row)(void *);
    uint32_t (*selected_item)(void *);
    uint32_t (*first_visible)(void *);
    void (*focus_item)(void *,uint32_t index);
    void (*move_selection_up)(void *,uint32_t n);
    void (*move_selection_down)(void *,uint32_t n);
    void (*move_window_up)(void *,uint32_t n);
    void (*move_window_down)(void *,uint32_t n);
    void (*move_window_top)(void *);
    void (*move_window_bottom)(void *);
    void (*selection_to_top)(void *);
    void (*selection_to_bottom)(void *);
    void (*start_animation)(void *);
    void *(*parent)(void *);
    uint32_t (*selected_item_id)(void *);
    uint32_t (*window_id)(void *);
    uint32_t (*notify)(void *context,void *parent,uint32_t item,uint32_t wid);
    void (*go_group)(void *,uint32_t index);
};
int oa_svg_list_select_body(const struct oa_svg_list_box_ops *,int *handled);
int oa_svg_list_down_body(const struct oa_svg_list_box_ops *,int *handled);
int oa_svg_list_up_body(const struct oa_svg_list_box_ops *,int *handled);
int oa_svg_list_page_down_body(const struct oa_svg_list_box_ops *,int *handled);
int oa_svg_list_page_up_body(const struct oa_svg_list_box_ops *,int *handled);
int oa_svg_list_first_body(const struct oa_svg_list_box_ops *,int *handled);
int oa_svg_list_last_body(const struct oa_svg_list_box_ops *,int *handled);
#endif
