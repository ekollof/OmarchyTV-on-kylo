#include "openaurora/svg_list_widgets.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
static int bad(int *error) { if(error)*error=EINVAL;return 0; }
uint32_t oa_setting_row_numeric(const struct oa_svg_row_text_ops *o,int *error)
{
    void *entry;
    if(!o || !o->text_entry || !o->get_numeric)return (uint32_t)bad(error);
    if(error)*error=0;
    entry=o->text_entry(o->context);
    return o->get_numeric(entry);
}
uint32_t oa_setting_list_selected_id(const struct oa_svg_list_select_ops *o,int *error)
{
    void *table,*entry;uint32_t item;
    if(!o || !o->is_created || !o->selected_item || !o->id_table || !o->id_entry || !o->entry_id) {
        if(error)*error=EINVAL;
        return UINT32_C(0xffffffff);
    }
    if(error)*error=0;
    if(!o->is_created(o->context))return UINT32_C(0xffffffff);
    item=o->selected_item(o->context);
    if(item+UINT32_C(1)==UINT32_C(0))return UINT32_C(0xffffffff);
    table=o->id_table(o->context);
    entry=o->id_entry(table,item);
    return o->entry_id(entry);
}
