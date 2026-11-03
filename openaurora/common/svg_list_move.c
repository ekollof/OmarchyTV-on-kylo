#include "openaurora/svg_list_move.h"
#include <errno.h>
#include <stddef.h>
static int ready(const struct oa_svg_list_move_ops *o)
{
    if(!o || !o->aux || !o->size || !o->selected_item || !o->first_visible || !o->focus_item ||
       !o->update_partial || !o->update_scrollbar || !o->start_animation)return EINVAL;
    return 0;
}
int oa_svg_list_move_window_top(const struct oa_svg_list_move_ops *o)
{
    if(ready(o))return EINVAL;
    o->update_partial(o->context,0);
    o->update_scrollbar(o->context);
    o->start_animation(o->context);return 0;
}
int oa_svg_list_move_window_bottom(const struct oa_svg_list_move_ops *o)
{
    uint32_t aux;
    if(ready(o))return EINVAL;
    aux=o->aux(o->context);
    o->update_partial(o->context,aux-o->size(o->context));
    o->update_scrollbar(o->context);
    o->start_animation(o->context);return 0;
}
int oa_svg_list_move_selection_to_top(const struct oa_svg_list_move_ops *o)
{
    if(ready(o))return EINVAL;
    o->focus_item(o->context,o->first_visible(o->context));return 0;
}
int oa_svg_list_move_selection_to_bottom(const struct oa_svg_list_move_ops *o)
{
    uint32_t first;
    if(ready(o))return EINVAL;
    first=o->first_visible(o->context);
    o->focus_item(o->context,first+o->size(o->context)-1u);return 0;
}
int oa_svg_list_move_window_up(const struct oa_svg_list_move_ops *o,uint32_t n)
{
    if(ready(o))return EINVAL;
    o->update_partial(o->context,o->first_visible(o->context)-n);
    o->update_scrollbar(o->context);return 0;
}
int oa_svg_list_move_window_down(const struct oa_svg_list_move_ops *o,uint32_t n)
{
    if(ready(o))return EINVAL;
    o->update_partial(o->context,o->first_visible(o->context)+n);
    o->update_scrollbar(o->context);return 0;
}
int oa_svg_list_move_selection_up(const struct oa_svg_list_move_ops *o,uint32_t n)
{
    if(ready(o))return EINVAL;
    o->focus_item(o->context,o->selected_item(o->context)-n);return 0;
}
int oa_svg_list_move_selection_down(const struct oa_svg_list_move_ops *o,uint32_t n)
{
    if(ready(o))return EINVAL;
    o->focus_item(o->context,o->selected_item(o->context)+n);return 0;
}
