#include "openaurora/svg_list_update.h"
#include <errno.h>
#include <stddef.h>
int oa_svg_list_update(const struct oa_svg_list_update_ops *o)
{
    uint32_t total,first;
    if(!o || !o->total_items || !o->store_count || !o->first_visible ||
       !o->update_full || !o->update_scrollbar || !o->start_animation)return EINVAL;
    total=o->total_items(o->context);
    o->store_count(o->context,total);
    first=o->first_visible(o->context);
    o->update_full(o->context,first,total);
    o->update_scrollbar(o->context);
    o->start_animation(o->context);
    return 0;
}
