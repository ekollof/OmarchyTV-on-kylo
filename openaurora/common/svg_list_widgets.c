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
int oa_setting_row_set_slide(const struct oa_svg_row_slide_ops *o,void *row,int value)
{
    uint32_t type,flagb;void *bar,*box;
    _Alignas(max_align_t) unsigned char textbuf[OA_SVG_ROW_TEXT_BYTES];
    if(!o || !row || !o->flag_a || !o->flag_b || !o->row_type || !o->set_progress ||
       !o->slide_bar || !o->is_created || !o->set_slider || !o->store_value || !o->text_box ||
       !o->text_format || !o->text_new || !o->text_append || !o->text_set || !o->text_free)return EINVAL;
    if(o->flag_a(row)) { o->set_progress(row,value);return 0; }
    flagb=o->flag_b(row);type=o->row_type(row);
    if(flagb) {
        if(type==UINT32_C(23)) { o->set_progress(row,value);return 0; }
    }
    bar=o->slide_bar(row,(type==UINT32_C(9) && !flagb)?UINT32_C(0x13c0):UINT32_C(0x1498));
    if(o->is_created(bar))o->set_slider(bar,value);
    o->store_value(row,value);
    box=o->text_box(row);
    if(o->is_created(box)) {
        o->text_new(textbuf,o->text_format(row));
        o->text_append(textbuf,value);
        o->text_set(box,textbuf);
        o->text_free(textbuf);
    }
    return 0;
}
