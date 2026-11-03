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
 * callback returns EINVAL. Concrete provider/rendering integration remains
 * separate from these recovered dispatch bodies. */
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
/* Joined SVGListBoxBase::updateRow -> RowManager::updateRow. The base adds
 * 0xf8 to its receiver; our context owns the manager and parent instead.
 * first is manager+0xc, total is manager+8 (NOT visible size at+0x10),
 * slots is the arithmetic-shifted array span. Fields are pure/pinned for the
 * call. If index>=first and index<first+min(total,slots), using wrapping
 * uint32 addition and unsigned comparisons, call parent virtual+0x60 with
 * (absolute index,row_at(index-first)). No draw/focus/animation follows here.
 * All callbacks receive context, are synchronous and retain nothing.
 * Invalid ops return EINVAL before callbacks. Parent implementation and
 * rendering remain separate responsibilities. */
struct oa_svg_list_refresh_ops {
    void *context;
    uint32_t (*first)(void *);
    uint32_t (*total)(void *);
    uint32_t (*slots)(void *);
    void *(*row_at)(void *,uint32_t relative);
    void (*refresh)(void *,uint32_t index,void *row);
};
int oa_svg_list_refresh_row(const struct oa_svg_list_refresh_ops *o,uint32_t index);
#endif
