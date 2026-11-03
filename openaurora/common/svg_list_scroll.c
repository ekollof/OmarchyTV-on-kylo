#include "openaurora/svg_list_scroll.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
static int64_t signed32(uint32_t value)
{ return value<=INT32_MAX?(int64_t)value:(int64_t)value-INT64_C(4294967296); }
static int ready(const struct oa_svg_list_scroll_ops *o)
{
    if(!o || !o->size || !o->total_items || !o->flag || !o->field || !o->scrollbar || !o->list_handle ||
       !o->list_visible || !o->first_visible || !o->scroll_created || !o->scroll_create ||
       !o->scroll_set_visible || !o->scroll_set_position)return EINVAL;
    return 0;
}
int oa_svg_list_update_scrollbar(const struct oa_svg_list_scroll_ops *o)
{
    void *sb;uint32_t size,total,flag=0;int create_path;
    if(ready(o))return EINVAL;
    sb=o->scrollbar(o->context);
    size=o->size(o->context);
    total=o->total_items(o->context);
    if(size!=0 && signed32(size)<signed32(total))create_path=1;
    else { flag=o->flag(o->context);create_path=flag!=0; }
    if(create_path) {
        if(!o->scroll_created(o->context,sb)) {
            uint32_t a=o->field(o->context,0x1f4),b=o->field(o->context,0x1fc);
            uint32_t c=o->field(o->context,0x1f0),d=o->field(o->context,0x1f8);
            uint32_t w=o->field(o->context,0x21c),x=o->field(o->context,0x214);
            o->scroll_create(o->context,sb,0,x,b,w,c*a+b-(a-d),o->list_handle(o->context));
        }
        if(o->list_visible(o->context))o->scroll_set_visible(o->context,sb,1);
        o->scroll_set_position(o->context,sb,total,size,o->first_visible(o->context));
    } else if(o->scroll_created(o->context,sb)) {
        o->scroll_set_visible(o->context,sb,(int)flag);
    }
    return 0;
}
