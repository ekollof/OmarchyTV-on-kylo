#include "openaurora/settings_input.h"
#include <errno.h>
#include <stddef.h>
static enum oa_settings_route route(uint32_t type,uint32_t id)
{
    if(type!=0x1004)return OA_SD_BASE;
    switch(id) {
    case 0x22d:case 0x251:case 0x25b:case 0x210:case 0x255:return OA_SD_MENU;
    case 0x20d:case 0x20c:case 0x2da:return OA_SD_RIGHT_LEFT;
    case 0x20a:case 0x20b:case 0x2db:case 0x2dc:return OA_SD_UP_DOWN;
    case 0x20e:return OA_SD_SELECT;
    case 0x21f:case 0x28b:return OA_SD_QUIT;
    case 0x222:return OA_SD_MUTE;
    case 0x200:case 0x201:case 0x202:case 0x203:case 0x204:
    case 0x205:case 0x206:case 0x207:case 0x208:case 0x209:return OA_SD_DIGIT;
    case 0x230:case 0x24c:return OA_SD_GUIDE;
    default:return OA_SD_BASE;
    }
}
int oa_settings_dispatch(const struct oa_settings_dispatch_ops *o,uint32_t type,uint32_t id,int *handled)
{
    if(!o || !o->run || !handled)return EINVAL;
    enum oa_settings_route target=route(type,id);
    uint32_t result=o->run(o->context,target,target==OA_SD_MUTE?NULL:o->message);
    *handled=target==OA_SD_MUTE || result!=0;return 0;
}
static uint32_t q(const struct oa_settings_input_ops *o,enum oa_settings_query query,uint32_t arg)
{ return o->query(o->context,query,arg); }
static void act(const struct oa_settings_input_ops *o,enum oa_settings_action action,uint32_t arg,const void *message)
{ o->run(o->context,action,arg,message); }
static void list_key(const struct oa_settings_input_ops *o,uint32_t key)
{ act(o,OA_SI_LIST_KEY,key,o->message); }
static void update_description(const struct oa_settings_input_ops *o,uint32_t key)
{
    list_key(o,key);
    uint32_t index=q(o,OA_SI_ITEM_INDEX,q(o,OA_SI_SELECTED_ID,0));
    if(index<=INT32_MAX && q(o,OA_SI_ITEM_PRESENT,index)) {
        if(!q(o,OA_SI_DESCRIPTION_SENTINEL,index))act(o,OA_SI_ASSIGN_DESCRIPTION,index,NULL);
        if(q(o,OA_SI_TEXT_EMPTY,0))act(o,OA_SI_ASSIGN_LABEL,index,NULL);
    }
    act(o,OA_SI_ADD_MENU_INFO,0,NULL);act(o,OA_SI_UPDATE_SETTINGS,0,NULL);
}
int oa_settings_input(const struct oa_settings_input_ops *o,enum oa_settings_input_kind kind,uint32_t key,int *handled)
{
    if(!o || !o->query || !o->run || !handled || (unsigned)kind>OA_SI_MUTE)return EINVAL;
    switch(kind) {
    case OA_SI_UP_DOWN:
        if(q(o,OA_SI_ACTIVE_COUNT,0))act(o,OA_SI_NAVIGATE,key,o->message);
        else list_key(o,key);
        act(o,OA_SI_UPDATE_DISPLAY,0,NULL);break;
    case OA_SI_SELECT:
        list_key(o,key);act(o,OA_SI_UPDATE_SETTINGS,0,NULL);break;
    case OA_SI_DIGIT:update_description(o,key);break;
    case OA_SI_RIGHT_LEFT:
        if(!q(o,OA_SI_HORIZONTAL,0)) { update_description(o,key);break; }
        if(!q(o,OA_SI_ACTIVE_COUNT,0))list_key(o,key);
        else {
            uint32_t mirror=q(o,OA_SI_MIRROR,0),forward=mirror?0x20c:0x20d,back=mirror?0x20d:0x20c;
            if(key==forward)act(o,OA_SI_NAVIGATE,0x20b,NULL);
            else if(key==back)act(o,OA_SI_NAVIGATE,0x20a,NULL);
            else if(key==0x2db || key==0x2dc)act(o,OA_SI_NAVIGATE,key,NULL);
        }
        act(o,OA_SI_UPDATE_DISPLAY,0,NULL);break;
    case OA_SI_MUTE:act(o,OA_SI_TOGGLE_MUTE,0,NULL);break;
    }
    *handled=1;return 0;
}
