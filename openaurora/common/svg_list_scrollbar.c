#include "openaurora/svg_list_scrollbar.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
static int64_t signed32(uint32_t value)
{ return value<=INT32_MAX?(int64_t)value:(int64_t)value-INT64_C(4294967296); }
static int ready(const struct oa_svg_list_scrollbar_ops *o)
{
    if(!o || !o->scrollbar || !o->is_created || !o->flag || !o->render_background ||
       !o->render_bar || !o->store_pos || !o->common_create)return EINVAL;
    return 0;
}
int oa_svg_list_scrollbar_create(const struct oa_svg_list_scrollbar_ops *o,
    int created,uint32_t x,uint32_t y,uint32_t w,uint32_t h,void *parent)
{
    void *sb;
    if(ready(o))return EINVAL;
    sb=o->scrollbar(o->context);
    o->common_create(o->context,sb,created,x,y,w,h,parent);
    return 0;
}
int oa_svg_list_scrollbar_set_position(const struct oa_svg_list_scrollbar_ops *o,
    uint32_t vis,uint32_t size,uint32_t first)
{
    void *sb;
    if(ready(o))return EINVAL;
    sb=o->scrollbar(o->context);
    if(!o->is_created(o->context,sb))return 0;
    if(signed32(first)>signed32(vis))return 0;
    o->store_pos(o->context,sb,vis,size,first);
    if(!o->flag(o->context,sb))o->render_background(o->context,sb);
    o->render_bar(o->context,sb);
    return 0;
}
