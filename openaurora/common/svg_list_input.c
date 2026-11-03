#include "openaurora/svg_list_input.h"
#include <errno.h>
#include <stddef.h>
static int invoke(const struct oa_svg_list_ops *o,enum oa_svg_list_action action,int *handled)
{
    const void *message=(action==OA_SVG_LIST_FIRST || action==OA_SVG_LIST_LAST || action==OA_SVG_LIST_WINDOW)?o->message:NULL;
    *handled=o->run(o->context,action,message)!=0;return 0;
}
int oa_svg_list_key(const struct oa_svg_list_ops *o,uint32_t key,int *handled)
{
    if(!o || !o->query || !o->run || !handled)return EINVAL;
    if(key==0x20e)return invoke(o,OA_SVG_LIST_SELECT,handled);
    uint32_t horizontal=o->query(o->context,OA_SVG_LIST_HORIZONTAL);
    if((horizontal && key==0x20d) || (!horizontal && key==0x20b))return invoke(o,OA_SVG_LIST_DOWN,handled);
    if((horizontal && key==0x20c) || (!horizontal && key==0x20a))return invoke(o,OA_SVG_LIST_UP,handled);
    if(key==o->query(o->context,OA_SVG_LIST_PAGE_DOWN_KEY))return invoke(o,OA_SVG_LIST_PAGE_DOWN,handled);
    if(key==o->query(o->context,OA_SVG_LIST_PAGE_UP_KEY))return invoke(o,OA_SVG_LIST_PAGE_UP,handled);
    if(key==0x2db)return invoke(o,OA_SVG_LIST_FIRST,handled);
    if(key==0x2dc)return invoke(o,OA_SVG_LIST_LAST,handled);
    *handled=0;return 0;
}
int oa_svg_list_handle(const struct oa_svg_list_ops *o,uint32_t type,uint32_t key,int *handled)
{
    if(!o || !o->query || !o->run || !handled)return EINVAL;
    return type==0x1004?oa_svg_list_key(o,key,handled):invoke(o,OA_SVG_LIST_WINDOW,handled);
}
int oa_settings_row_value(const struct oa_settings_row_ops *o,uint32_t type,uint32_t *value)
{
    if(!o || !o->query || !value)return EINVAL;
    enum oa_settings_row_query query;
    switch(type) {
    case 2:case 3:query=OA_ROW_SELECTION_ID;break;
    case 9:case 10:case 11:case 12:query=OA_ROW_CACHE_PRIMARY;break;
    case 13:case 14:case 15:query=OA_ROW_NUMERIC;break;
    case 22:query=OA_ROW_CHECKED;break;
    case 23:case 24:case 25:query=OA_ROW_CACHE_SECONDARY;break;
    default:*value=0;return 0;
    }
    *value=o->query(o->context,query);return 0;
}
