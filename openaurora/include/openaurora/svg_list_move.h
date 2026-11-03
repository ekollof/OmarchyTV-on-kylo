#ifndef OPENAURORA_SVG_LIST_MOVE_H
#define OPENAURORA_SVG_LIST_MOVE_H
#include <stdint.h>
/* Primitive SVGListBoxBase move/selection/window bodies called by the key
 * bodies in svg_list_bodies.h. All arithmetic wraps uint32_t exactly as
 * shipped; no signed interpretation here. moveWindowToBottom reads the
 * list-box word +0x1ec without a null check (callers establish it) and posts
 * aux-size. moveWindowUp/Down reposition by first-visible minus/plus n and do
 * NOT animate; only moveWindowToTop/Bottom end with startSelectAnimation.
 * moveSelectionUp/Down focus selected-item minus/plus n; the to-top/to-bottom
 * of-window variants focus first-visible and first-visible+size-1.
 * updatePartial/focusItem target the RowManager at this+0xf8; updateScrollBar
 * and startSelectAnimation target the list box itself (native differential
 * asserts the receiver). These are void in firmware: return errno only, no
 * handled. NULL ops or any NULL callback returns EINVAL. The callees stay
 * controlled stubs for the next layer. */
struct oa_svg_list_move_ops {
    void *context;
    uint32_t (*aux)(void *);
    uint32_t (*size)(void *);
    uint32_t (*selected_item)(void *);
    uint32_t (*first_visible)(void *);
    void (*focus_item)(void *,uint32_t index);
    void (*update_partial)(void *,uint32_t index);
    void (*update_scrollbar)(void *);
    void (*start_animation)(void *);
};
int oa_svg_list_move_window_top(const struct oa_svg_list_move_ops *);
int oa_svg_list_move_window_bottom(const struct oa_svg_list_move_ops *);
int oa_svg_list_move_selection_to_top(const struct oa_svg_list_move_ops *);
int oa_svg_list_move_selection_to_bottom(const struct oa_svg_list_move_ops *);
int oa_svg_list_move_window_up(const struct oa_svg_list_move_ops *,uint32_t n);
int oa_svg_list_move_window_down(const struct oa_svg_list_move_ops *,uint32_t n);
int oa_svg_list_move_selection_up(const struct oa_svg_list_move_ops *,uint32_t n);
int oa_svg_list_move_selection_down(const struct oa_svg_list_move_ops *,uint32_t n);
#endif
