#include "openaurora/svg_list_rows.h"
#include <errno.h>
#include <stddef.h>
static int ready(const struct oa_svg_list_manager_ops *o)
{
    if(!o || !o->first || !o->size || !o->selected_rel || !o->row_at || !o->row_focus ||
       !o->store_selected || !o->update_impl || !o->slots)return EINVAL;
    return 0;
}
static int bad(int *error) { if(error)*error=EINVAL;return 0; }
uint32_t oa_svg_list_manager_size(const struct oa_svg_list_manager_ops *o,int *error)
{
    if(ready(o))return (uint32_t)bad(error);
    if(error)*error=0;
    return o->size(o->context);
}
uint32_t oa_svg_list_manager_selected_row(const struct oa_svg_list_manager_ops *o,int *error)
{
    if(ready(o))return (uint32_t)bad(error);
    if(error)*error=0;
    return o->selected_rel(o->context);
}
uint32_t oa_svg_list_manager_selected_item(const struct oa_svg_list_manager_ops *o,int *error)
{
    if(ready(o))return (uint32_t)bad(error);
    if(error)*error=0;
    return o->first(o->context)+o->selected_rel(o->context);
}
uint32_t oa_svg_list_manager_first_visible(const struct oa_svg_list_manager_ops *o,int *error)
{
    if(ready(o))return (uint32_t)bad(error);
    if(error)*error=0;
    return o->first(o->context);
}
int oa_svg_list_manager_focus_item(const struct oa_svg_list_manager_ops *o,uint32_t index)
{
    uint32_t count,first,rel,old;
    if(ready(o))return EINVAL;
    count=o->size(o->context);
    if(!count)return 0;
    first=o->first(o->context);
    rel=index-first;
    if(rel>=count)return 0;
    old=o->selected_rel(o->context);
    if(rel==old)return 0;
    o->row_focus(o->row_at(o->context,old),0);
    o->row_focus(o->row_at(o->context,rel),1);
    o->store_selected(o->context,rel);return 0;
}
int oa_svg_list_manager_update_partial(const struct oa_svg_list_manager_ops *o,uint32_t index)
{
    if(ready(o))return EINVAL;
    o->update_impl(o->context,index,0);return 0;
}
uint32_t oa_svg_list_manager_total_rows(const struct oa_svg_list_manager_ops *o,int *error)
{
    if(ready(o))return (uint32_t)bad(error);
    if(error)*error=0;
    return o->slots(o->context);
}
uint32_t oa_svg_list_manager_last_visible(const struct oa_svg_list_manager_ops *o,int *error)
{
    uint32_t first,size;
    if(ready(o))return (uint32_t)bad(error);
    if(error)*error=0;
    first=o->first(o->context);
    size=o->size(o->context);
    return first+size-1u;
}
void *oa_svg_list_manager_item_row(const struct oa_svg_list_manager_ops *o,uint32_t index,int *error)
{
    uint32_t first,rel,size;
    if(ready(o)) { if(error)*error=EINVAL;return NULL; }
    if(error)*error=0;
    first=o->first(o->context);
    if(index<first)return NULL;
    rel=index-first;
    size=o->size(o->context);
    if(rel>=size)return NULL;
    return o->row_at(o->context,rel);
}
