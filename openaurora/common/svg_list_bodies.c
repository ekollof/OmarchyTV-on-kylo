#include "openaurora/svg_list_bodies.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
static int64_t signed32(uint32_t value)
{ return value<=INT32_MAX?(int64_t)value:(int64_t)value-INT64_C(4294967296); }
static int ready(const struct oa_svg_list_box_ops *o,int *handled)
{
    if(!o || !handled || !o->aux || !o->size || !o->selected_row || !o->selected_item ||
       !o->first_visible || !o->focus_item || !o->move_selection_up || !o->move_selection_down ||
       !o->move_window_up || !o->move_window_down || !o->move_window_top || !o->move_window_bottom ||
       !o->selection_to_top || !o->selection_to_bottom || !o->start_animation || !o->parent ||
       !o->selected_item_id || !o->window_id || !o->notify || !o->go_group)return EINVAL;
    return 0;
}
int oa_svg_list_select_body(const struct oa_svg_list_box_ops *o,int *handled)
{
    if(ready(o,handled))return EINVAL;
    if(!o->aux(o->context) || !o->size(o->context)) { *handled=0;return 0; }
    void *parent=o->parent(o->context);
    if(parent) {
        uint32_t item=o->selected_item_id(o->context),wid=o->window_id(o->context);
        o->notify(o->context,parent,item,wid);
    }
    *handled=1;return 0;
}
int oa_svg_list_down_body(const struct oa_svg_list_box_ops *o,int *handled)
{
    if(ready(o,handled))return EINVAL;
    uint32_t aux=o->aux(o->context);
    if(!aux) { *handled=0;return 0; }
    if(!o->size(o->context)) { *handled=0;return 0; }
    if(o->selected_row(o->context)!=o->size(o->context)-1u) {
        o->move_selection_down(o->context,1);
    } else {
        uint32_t first=o->first_visible(o->context),end=first+o->size(o->context);
        if(end!=aux) {
            o->move_window_down(o->context,1);
        } else {
            if(end!=o->size(o->context))o->move_window_top(o->context);
            o->selection_to_top(o->context);
        }
    }
    o->start_animation(o->context);*handled=1;return 0;
}
int oa_svg_list_up_body(const struct oa_svg_list_box_ops *o,int *handled)
{
    if(ready(o,handled))return EINVAL;
    uint32_t aux=o->aux(o->context);
    if(!aux) { *handled=0;return 0; }
    if(!o->size(o->context)) { *handled=0;return 0; }
    if(o->selected_row(o->context)) {
        o->move_selection_up(o->context,1);
    } else if(o->first_visible(o->context)) {
        o->move_window_up(o->context,1);
    } else if(aux!=o->size(o->context)) {
        o->move_window_bottom(o->context);o->selection_to_bottom(o->context);
    } else {
        o->selection_to_bottom(o->context);
    }
    o->start_animation(o->context);*handled=1;return 0;
}
int oa_svg_list_page_down_body(const struct oa_svg_list_box_ops *o,int *handled)
{
    if(ready(o,handled))return EINVAL;
    uint32_t aux=o->aux(o->context);
    if(!aux) { *handled=0;return 0; }
    if(!o->size(o->context)) { *handled=0;return 0; }
    uint32_t size=o->size(o->context);
    uint32_t selected=o->selected_item(o->context),first=o->first_visible(o->context);
    if(signed32(first+size-1u)>signed32(aux-size)) {
        uint32_t index=selected+size-1u;
        o->move_window_bottom(o->context);
        if(signed32(index)>=signed32(aux))index=aux-1u;
        o->focus_item(o->context,index);
    } else {
        o->move_window_down(o->context,size);
    }
    o->start_animation(o->context);*handled=1;return 0;
}
int oa_svg_list_page_up_body(const struct oa_svg_list_box_ops *o,int *handled)
{
    if(ready(o,handled))return EINVAL;
    if(!o->aux(o->context)) { *handled=0;return 0; }
    if(!o->size(o->context)) { *handled=0;return 0; }
    uint32_t size=o->size(o->context);
    uint32_t selected=o->selected_item(o->context),first=o->first_visible(o->context);
    if(signed32(first-1u-size)>0) {
        o->move_window_up(o->context,size);
    } else {
        int64_t back=signed32(selected-1u-size);
        o->move_window_top(o->context);
        o->focus_item(o->context,back<0?0u:(uint32_t)back);
    }
    o->start_animation(o->context);*handled=1;return 0;
}
int oa_svg_list_first_body(const struct oa_svg_list_box_ops *o,int *handled)
{
    if(ready(o,handled))return EINVAL;
    if(!o->aux(o->context) || !o->size(o->context)) { *handled=0;return 0; }
    if(o->selected_item(o->context))o->go_group(o->context,0);
    *handled=1;return 0;
}
int oa_svg_list_last_body(const struct oa_svg_list_box_ops *o,int *handled)
{
    if(ready(o,handled))return EINVAL;
    uint32_t aux=o->aux(o->context);
    if(!aux || !o->size(o->context)) { *handled=0;return 0; }
    o->go_group(o->context,aux-1u);*handled=1;return 0;
}
