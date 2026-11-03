#include "openaurora/settings_row_type.h"
#include <errno.h>
#include <stddef.h>
static const uint16_t standard[]={0x684,0x13c0,0x1498,0x1ca8,0x1014,0x18d8,0x27a8,0x2cf0,0x2b28,0x2c0c,0x1570,0x1724,0x24e8,0x1f68,0x2228};
static const uint16_t slide[]={0x684,0x13c0,0x1498,0x1ca8,0x18d8,0x1014,0x27a8,0x2cf0,0x2b28,0x2c0c,0x1570,0x1724,0x24e8,0x1f68,0x2228};
static const uint16_t range[]={0x684,0x13c0,0x1498,0x1ca8,0x1f68,0x2228,0x18d8,0x1014,0x27a8,0x2cf0,0x2b28,0x2c0c,0x1570,0x1724,0x24e8};
static const uint16_t images[]={0x684,0x13c0,0x1498,0x1f68,0x2228,0x1ca8,0x18d8,0x1014,0x27a8,0x2cf0,0x2b28,0x2c0c,0x1570,0x1724,0x24e8};
static const uint16_t progress[]={0x684,0x13c0,0x1498,0x1f68,0x2228,0x18d8,0x1014,0x27a8,0x2cf0,0x2b28,0x2c0c,0x1570,0x1724,0x24e8};
static void child(const struct oa_setting_row_type_ops *o,uint32_t offset,int visible)
{
    enum oa_setting_child_kind kind=offset==0x1014?OA_SETTING_SELECTION:
        offset==0x18d8?OA_SETTING_TEXT_ENTRY:offset==0x27a8?OA_SETTING_CHECKBOX:OA_SETTING_WINDOW;
    o->set_visible(o->context,kind,offset,visible);
}
static int shown(uint32_t type,uint32_t offset)
{
    switch(type) {
    case 2:return offset==0x1014;
    case 3:return offset==0x1014 || offset==0x2cf0;
    case 4:case 5:case 6:case 7:case 8:case 26:return offset==0x684;
    case 9:return offset==0x13c0 || offset==0x1ca8;
    case 10:return offset==0x1498 || offset==0x1ca8;
    case 11:return offset==0x1498 || offset==0x2b28 || offset==0x2c0c;
    case 12:return offset==0x1498 || offset==0x1ca8 || offset==0x1f68 || offset==0x2228;
    case 13:case 14:case 15:case 16:case 17:case 18:case 19:case 20:case 21:return offset==0x18d8;
    case 22:return offset==0x27a8;
    case 23:return offset==0x1570 || offset==0x24e8;
    case 24:return offset==0x1724 || offset==0x24e8;
    case 25:return offset==0x2b28 || offset==0x2c0c || offset==0x1724;
    default:return 0;
    }
}
int oa_setting_row_set_type(struct oa_setting_row_type_state *state,const struct oa_setting_row_type_ops *o,uint32_t type)
{
    if(!state || !o || !o->render_items || !o->is_visible || !o->set_visible)return EINVAL;
    if(state->type!=type) { state->type=type;state->rebuild=1;o->render_items(o->context); }
    else state->rebuild=0;
    if(!o->is_visible(o->context))return 0;
    child(o,0x3c4,type==1);child(o,0x104,type!=1);
    if(type>26)return 0;
    const uint16_t *order=standard;size_t n=sizeof standard/sizeof *standard;
    if(type==9 || type==23 || type==25)order=slide;
    else if(type==10 || type==12)order=range;
    else if(type==11)order=images;
    else if(type==24) { order=progress;n=sizeof progress/sizeof *progress; }
    for(size_t i=0;i<n;++i)child(o,order[i],shown(type,order[i]));
    return 0;
}
