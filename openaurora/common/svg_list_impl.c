#include "openaurora/svg_list_impl.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
static int64_t signed32(uint32_t value)
{ return value<=INT32_MAX?(int64_t)value:(int64_t)value-INT64_C(4294967296); }
static int ready(const struct oa_svg_list_impl_ops *o)
{
    if(!o || !o->cap || !o->first || !o->size || !o->selected_rel || !o->slots || !o->row_at ||
       !o->store_row || !o->owner || !o->store_first || !o->store_size || !o->store_selected ||
       !o->row_visible || !o->row_set_focus || !o->row_set_visible || !o->row_rect ||
       !o->row_set_position || !o->owner_empty_message || !o->owner_draw_row ||
       !o->owner_draw_empty_row || !o->owner_got_focus)return EINVAL;
    return 0;
}
int oa_svg_list_manager_rotate_rows(const struct oa_svg_list_impl_ops *o,int32_t delta)
{
    uint32_t n,fp,field;int32_t step;void *ctx;
    if(ready(o))return EINVAL;
    ctx=o->context;
    n=o->size(ctx);
    if((int64_t)delta<=0) {
        if(delta==0)return 0;
        if(0u-(uint32_t)delta>=n)return 0;
        fp=(uint32_t)delta+n;field=n-1u;step=-1;
    } else {
        if(n<=(uint32_t)delta)return 0;
        fp=n-(uint32_t)delta;field=0;step=1;
    }
    if(signed32(fp)<=0)return 0;
    for(uint32_t k=0;k<fp;++k) {
        uint32_t i=field,j=(uint32_t)((int64_t)delta+(int64_t)i);
        void *ri=o->row_at(ctx,i),*rj=o->row_at(ctx,j);
        uint32_t tmpa[4],tmpb[4];
        o->row_rect(ctx,ri,tmpa);
        o->store_row(ctx,i,rj);
        o->store_row(ctx,j,ri);
        o->row_rect(ctx,o->row_at(ctx,i),tmpb);
        o->row_set_position(ctx,ri,tmpb[0],tmpb[1]);
        o->row_set_position(ctx,o->row_at(ctx,i),tmpa[0],tmpa[1]);
        field=(uint32_t)((int64_t)field+step);
    }
    return 0;
}
static void draw_at(const struct oa_svg_list_impl_ops *o,void *ctx,uint32_t item,uint32_t rel)
{ o->owner_draw_row(ctx,o->owner(ctx),item,o->row_at(ctx,rel)); }
int oa_svg_list_manager_update_impl(const struct oa_svg_list_impl_ops *o,uint32_t index,int full)
{
    uint32_t slots,cap,start,sel,nvis,r5;
    void *ctx;
    if(ready(o))return EINVAL;
    ctx=o->context;
    slots=o->slots(ctx);
    cap=o->cap(ctx);
    if(cap<=slots)start=0;
    else {
        uint32_t t=index+slots;
        start=(cap>=t)?index:cap-slots;
    }
    sel=o->selected_rel(ctx);
    if(sel<slots)o->row_set_focus(ctx,o->row_at(ctx,sel),0);
    /* Wrapping subtract matches the shipped subs; gcc documents the same wrap
     * on conversion, mirroring the register value exactly. */
    if(!full)oa_svg_list_manager_rotate_rows(o,(int32_t)(start-o->first(ctx)));
    nvis=(cap<=slots)?cap:slots;
    r5=0;
    while(r5!=nvis) {
        if(o->size(ctx)<nvis) {
            void *rr=o->row_at(ctx,r5);
            if(!o->row_visible(ctx,rr))o->row_set_visible(ctx,o->row_at(ctx,r5),1);
        }
        {
            uint32_t first=o->first(ctx),item=start+r5;
            if(item<first)draw_at(o,ctx,item,r5);
            else if(item<first+o->size(ctx)) {
                if(full)draw_at(o,ctx,item,r5);
            } else draw_at(o,ctx,item,r5);
        }
        ++r5;
    }
    {
        uint32_t r9=r5;
        for(;;) {
            if(r9>=slots)break;
            if(r5<o->size(ctx)) {
                void *rr=o->row_at(ctx,r9);
                if(o->row_visible(ctx,rr))o->row_set_visible(ctx,o->row_at(ctx,r9),0);
            }
            ++r9;
        }
    }
    o->store_size(ctx,r5);
    o->store_first(ctx,start);
    if(r5==0) {
        void *own;
        o->store_selected(ctx,0);
        own=o->owner(ctx);
        if(o->owner_empty_message(ctx,own) && slots!=0) {
            o->row_set_visible(ctx,o->row_at(ctx,0),1);
            o->owner_draw_empty_row(ctx,o->owner(ctx),o->row_at(ctx,0));
        }
    } else {
        void *own;uint32_t s2=o->selected_rel(ctx);
        if(r5<=s2)o->store_selected(ctx,r5-1u);
        own=o->owner(ctx);
        if(o->owner_got_focus(ctx,own)) {
            uint32_t fs=o->selected_rel(ctx);
            o->row_set_focus(ctx,o->row_at(ctx,fs),1);
        }
    }
    return 0;
}
