#ifndef OPENAURORA_SVG_LIST_SCROLLBAR_H
#define OPENAURORA_SVG_LIST_SCROLLBAR_H
#include <stdint.h>
/* SVGScrollBar construction and position bodies called by updateScrollBar.
 * create forwards (created,rect,parent) to the common construction step;
 * construction internals stay open. setScrollPosition stores
 * vis/size/first at +0xbc/+0xc0/+0xc4 when the bar is created and signed
 * first does not exceed signed vis, renders the background only when the
 * +0xc8 byte is clear, then always renders the bar; otherwise it stores and
 * renders nothing. isCreated/renderBackground/renderBar run on the scrollbar
 * itself through owned callbacks and stay open. All arithmetic wraps uint32_t
 * except the first-vs-vis gate, which is signed. NULL ops or any NULL
 * callback returns EINVAL. Scrollbar construction/rendering internals are not
 * widget acceptance. */
struct oa_svg_list_scrollbar_ops {
    void *context;
    void *(*scrollbar)(void *ctx);
    int (*is_created)(void *ctx,void *sb);
    uint32_t (*flag)(void *ctx,void *sb);
    void (*render_background)(void *ctx,void *sb);
    void (*render_bar)(void *ctx,void *sb);
    void (*store_pos)(void *ctx,void *sb,uint32_t vis,uint32_t size,uint32_t first);
    void (*common_create)(void *ctx,void *sb,int created,uint32_t x,uint32_t y,uint32_t w,uint32_t h,void *parent);
};
int oa_svg_list_scrollbar_create(const struct oa_svg_list_scrollbar_ops *,
    int created,uint32_t x,uint32_t y,uint32_t w,uint32_t h,void *parent);
int oa_svg_list_scrollbar_set_position(const struct oa_svg_list_scrollbar_ops *,
    uint32_t vis,uint32_t size,uint32_t first);
#endif
