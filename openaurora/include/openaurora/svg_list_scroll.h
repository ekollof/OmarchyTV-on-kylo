#ifndef OPENAURORA_SVG_LIST_SCROLL_H
#define OPENAURORA_SVG_LIST_SCROLL_H
#include <stdint.h>
/* SVGListBoxBase::updateScrollBar called by every move primitive. size comes
 * from the RowManager at this+0xf8; total from the +0x6c virtual
 * (SettingItemsListBox::totalNumberOfItems; pure virtual on the base).
 * Create path iff size is nonzero with signed size < signed total, or the
 * +0x22c flag word is nonzero; otherwise an existing scrollbar is hidden.
 * On the create path an uncreated scrollbar is built from the literal list
 * fields x=[0x214] y=[0x1fc] w=[0x21c] h=([0x1f0]*[0x1f4]+[0x1fc])-
 * ([0x1f4]-[0x1f8]) with wrapping uint32 arithmetic, shown iff the list is
 * visible, then set to (total,size,first-visible). On the hide path a
 * created scrollbar is set to the flag value (zero there). scrollbar() and
 * list_handle() are pure address derivations with no native call and are not
 * traced; every other callback maps to one shipped call in order. field()
 * reads the named list offsets exactly once each in shipped order when the
 * rect is built. NULL ops or any NULL callback returns EINVAL. Scrollbar
 * construction/positioning bodies stay open. */
struct oa_svg_list_scroll_ops {
    void *context;
    uint32_t (*size)(void *);
    uint32_t (*total_items)(void *);
    uint32_t (*flag)(void *);
    uint32_t (*field)(void *,uint32_t offset);
    void *(*scrollbar)(void *);
    void *(*list_handle)(void *);
    int (*list_visible)(void *);
    uint32_t (*first_visible)(void *);
    int (*scroll_created)(void *ctx,void *sb);
    void (*scroll_create)(void *ctx,void *sb,int created,uint32_t x,uint32_t y,uint32_t w,uint32_t h,void *parent);
    void (*scroll_set_visible)(void *ctx,void *sb,int visible);
    void (*scroll_set_position)(void *ctx,void *sb,uint32_t vis,uint32_t size,uint32_t first);
};
int oa_svg_list_update_scrollbar(const struct oa_svg_list_scroll_ops *o);
#endif
