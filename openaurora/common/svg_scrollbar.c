#include "openaurora/svg_scrollbar.h"
#include <string.h>

/* ARM add/sub/mul keep the low32 bits before signed comparisons/division.
 * Use unsigned operations and preserve their bit representation, avoiding C
 * signed overflow. This is wraparound, not saturation or wider arithmetic. */
static int32_t signed_bits(uint32_t bits)
{ int32_t value;memcpy(&value,&bits,sizeof value);return value; }
static int32_t add32(int32_t a,int32_t b)
{ return signed_bits((uint32_t)a+(uint32_t)b); }
static int32_t sub32(int32_t a,int32_t b)
{ return signed_bits((uint32_t)a-(uint32_t)b); }
static int32_t mul32(int32_t a,int32_t b)
{ return signed_bits((uint32_t)a*(uint32_t)b); }
static int32_t divide32(int32_t a,int32_t b)
{
    /* Both geometry call sites have a nonzero denominator by branch order.
     * The shipped signed helper returns INT32_MIN for INT32_MIN/-1. */
    return a==INT32_MIN && b==-1?INT32_MIN:a/b;
}

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
        bar=divide32(mul32(height,visible),total);
        if(bar<s->min_bar)bar=s->min_bar;
        if(bar>=height)bar=height;
        s->bar_size=bar;
        if(total>add32(visible,first))
            pos=divide32(mul32(sub32(height,bar),first),sub32(total,visible));
        else
            pos=sub32(height,bar);
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
    if(!s->background_rendered && ops->render_background)
        ops->render_background(s);
    if(ops->render_bar)ops->render_bar(s);
    return 1;
}

/* SVGScrollBar::calculateMinBarSize (0x10c397c). The supplier image is cached
 * per SVGWindow state; without one the min stays unchanged. The body resets the min
 * only after that check, then adds the two moving-bar cap heights. */
void oa_svg_scrollbar_min_bar(struct oa_svg_scrollbar_skin *s,
    const struct oa_svg_scrollbar_skin_ops *ops)
{
    int32_t image,name,min;
    if(!s || !ops || s->widget_state>=4)return;
    image=s->cached_image[s->widget_state];
    if(!image) {
        image=ops->supplier_image(s,s->image_id[s->widget_state]);
        s->cached_image[s->widget_state]=image;
        if(!image)return;
    }
    min=0;
    name=ops->widget_name(s,0x20);
    ops->renderer_background(s,image,name);
    name=ops->widget_name(s,0x22);
    min=add32(min,ops->rect_height(s,name));
    name=ops->widget_name(s,0x21);
    ops->renderer_background(s,image,name);
    name=ops->widget_name(s,0x23);
    min=add32(min,ops->rect_height(s,name));
    s->min_bar=min;
}

static int32_t render_image(struct oa_svg_scrollbar_render *r,
    const struct oa_svg_scrollbar_render_ops *o)
{
    int32_t image=r->skin.cached_image[r->skin.widget_state];
    if(!image) {
        image=o->supplier_image(r,r->skin.image_id[r->skin.widget_state]);
        r->skin.cached_image[r->skin.widget_state]=image;
    }
    return image;
}

static void render_caps(struct oa_svg_scrollbar_render *r,
    const struct oa_svg_scrollbar_render_ops *o,int surface,int32_t image,
    const int32_t surface_rect[4],int top_name,int top_image,
    int bottom_name,int bottom_image,int *top_height,int *bottom_height)
{
    o->mask_align(r,3);
    o->mask_surface_rect(r,surface_rect);
    o->mask_name(r,top_name);
    o->mask_container(r);
    o->renderer_draw(r,surface,image,top_image,0);
    *top_height=o->mask_rect_height(r,top_name);
    o->mask_reset(r);
    o->mask_name(r,bottom_name);
    o->mask_align(r,4);
    o->renderer_draw(r,surface,image,bottom_image,0);
    *bottom_height=o->mask_rect_height(r,bottom_name);
}

static void render_row(struct oa_svg_scrollbar_render *r,
    const struct oa_svg_scrollbar_render_ops *o,int surface,int32_t image,
    const int32_t surface_rect[4],int top_height,int bottom_height,
    int row_name,int row_image)
{
    int32_t row[4];
    o->rect_new(r,row,0,top_height,surface_rect[2],
        sub32(surface_rect[3],bottom_height));
    o->vertical_surface_rect(r,row);
    o->vertical_name(r,row_name);
    o->vertical_container(r);
    o->renderer_draw(r,surface,image,row_image,1);
    (void)o->mask_rect_height(r,row_name);
}

void oa_svg_scrollbar_render_background(struct oa_svg_scrollbar_render *r,
    const struct oa_svg_scrollbar_render_ops *ops)
{
    int32_t image,top,bottom;
    int32_t rect[4],surface_rect[4];
    if(!r || !ops || r->skin.widget_state>=4)return;
    image=render_image(r,ops);
    if(!image)return;
    ops->window_rect(r,rect);
    ops->surface_resize(r,0,rect);
    ops->rect_new(r,surface_rect,0,0,sub32(rect[2],rect[0]),sub32(rect[3],rect[1]));
    render_caps(r,ops,0,image,surface_rect,15,3,24,12,&top,&bottom);
    render_row(r,ops,0,image,surface_rect,top,bottom,18,6);
    r->state.background_rendered=1;
}

void oa_svg_scrollbar_render_bar(struct oa_svg_scrollbar_render *r,
    const struct oa_svg_scrollbar_render_ops *ops)
{
    int32_t image,top=0,bottom=0;
    int32_t rect[4],bar_rect[4],surface_rect[4];
    if(!r || !ops || r->skin.widget_state>=4)return;
    image=render_image(r,ops);
    if(!image)return;
    /* The shipped body queries the rect once inside
     * calculateBarSizeAndPosition and again for the bar surface. */
    ops->window_rect(r,rect);
    oa_svg_scrollbar_calculate(&r->state,sub32(rect[3],rect[1]));
    ops->window_rect(r,rect);
    bar_rect[0]=rect[0];
    bar_rect[1]=add32(rect[1],r->state.bar_pos);
    bar_rect[2]=rect[2];
    bar_rect[3]=add32(bar_rect[1],r->state.bar_size);
    ops->surface_resize(r,1,bar_rect);
    ops->rect_new(r,surface_rect,0,0,sub32(rect[2],rect[0]),r->state.bar_size);
    if(r->state.bar_size==0) {
        ops->surface_clear(r,1);
        ops->surface_clear(r,0);
        r->state.background_rendered=0;
        return;
    }
    render_caps(r,ops,1,image,surface_rect,34,32,35,33,&top,&bottom);
    if(r->state.min_bar<r->state.bar_size)
        render_row(r,ops,1,image,surface_rect,top,bottom,30,27);
}
