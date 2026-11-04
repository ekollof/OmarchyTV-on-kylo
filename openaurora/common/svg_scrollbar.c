#include "openaurora/svg_scrollbar.h"

/* SVGScrollBar::calculateBarSizeAndPosition (0x10c3af0). The branch order and
 * the final "full-height bar hides itself" rule follow the shipped body. */
void oa_svg_scrollbar_calculate(struct oa_svg_scrollbar_state *s,int32_t height)
{
    int32_t total,visible,first,bar,pos;
    if(!s)return;
    total=s->total;visible=s->visible;first=s->first;
    if(total==0) {
        s->bar_pos=0;
        s->bar_size=height;
    } else if(total<=visible) {
        s->bar_pos=0;
        s->bar_size=0;
    } else if(total<=0) {
        /* Negative totals preserve the previous bar fields; only the final
         * full-height rule runs, exactly as shipped. */
    } else {
        bar=height*visible/total;
        if(bar<s->min_bar)bar=s->min_bar;
        if(bar>=height)bar=height;
        s->bar_size=bar;
        if(total>visible+first)
            pos=(height-bar)*first/(total-visible);
        else
            pos=height-bar;
        s->bar_pos=pos;
    }
    if(s->bar_size==height)s->bar_size=0;
}

/* SVGScrollBar::setScrollPosition (0x10c4138). */
int oa_svg_scrollbar_set_position(struct oa_svg_scrollbar_state *s,
    int32_t total,int32_t visible,int32_t first,int created,
    const struct oa_svg_scrollbar_ops *ops)
{
    if(!s || !ops)return 0;
    if(!created)return 0;
    if(first>total)return 0;
    s->total=total;
    s->visible=visible;
    s->first=first;
    if(!s->background_hidden && ops->render_background)
        ops->render_background(s);
    if(ops->render_bar)ops->render_bar(s);
    return 1;
}

/* SVGScrollBar::calculateMinBarSize (0x10c397c). The supplier image is cached
 * per orientation; without one the min stays unchanged. The body resets the min
 * only after that check, then adds the two moving-bar cap heights. */
void oa_svg_scrollbar_min_bar(struct oa_svg_scrollbar_skin *s,
    const struct oa_svg_scrollbar_skin_ops *ops)
{
    int32_t image,name,min;
    if(!s || !ops)return;
    image=s->cached_image[s->orientation];
    if(!image) {
        image=ops->supplier_image(s,s->image_id[s->orientation]);
        s->cached_image[s->orientation]=image;
        if(!image)return;
    }
    min=0;
    name=ops->widget_name(s,0x20);
    ops->renderer_background(s,image,name);
    name=ops->widget_name(s,0x22);
    min+=ops->rect_height(s,name);
    name=ops->widget_name(s,0x21);
    ops->renderer_background(s,image,name);
    name=ops->widget_name(s,0x23);
    min+=ops->rect_height(s,name);
    s->min_bar=min;
}
