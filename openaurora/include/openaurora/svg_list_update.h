#ifndef OPENAURORA_SVG_LIST_UPDATE_H
#define OPENAURORA_SVG_LIST_UPDATE_H
#include <stdint.h>
/* SVGListBoxBase::update: full list refresh. Reads the total through the
 * +0x6c virtual (SettingItemsListBox::totalNumberOfItems; pure virtual on
 * the base), stores it to the +0x1ec count word, then runs the full update
 * of the RowManager at this+0xf8 with (first-visible,total), refreshes the
 * scrollbar and restarts the selection animation. All arithmetic is plain
 * word copies; no ordering, clamping or visibility decisions live here.
 * Callbacks are synchronous, pinned and never retained. NULL ops or any NULL
 * callback returns EINVAL. The full-update/scrollbar/animation bodies stay
 * open. */
struct oa_svg_list_update_ops {
    void *context;
    uint32_t (*total_items)(void *);
    void (*store_count)(void *,uint32_t);
    uint32_t (*first_visible)(void *);
    void (*update_full)(void *,uint32_t first,uint32_t total);
    void (*update_scrollbar)(void *);
    void (*start_animation)(void *);
};
int oa_svg_list_update(const struct oa_svg_list_update_ops *o);
#endif
