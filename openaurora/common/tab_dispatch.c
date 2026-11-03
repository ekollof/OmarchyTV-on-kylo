#include "openaurora/tab_dispatch.h"
#include <errno.h>
static enum oa_tab_dispatch_target target(uint32_t type,uint32_t id)
{
    if(type!=0x1004)return OA_TD_BASE;
    switch(id) {
    case 0x22d:case 0x251:case 0x2bb:case 0x25b:case 0x210:return OA_TD_BASE_MENU;
    case 0x20a:case 0x20b:return OA_TD_UP_DOWN;
    case 0x20d:case 0x20c:case 0x2c9:case 0x2c8:case 0x2da:return OA_TD_RIGHT_LEFT;
    case 0x220:case 0x221:return OA_TD_VOLUME;
    case 0x21f:case 0x28b:return OA_TD_QUIT_IGNORE;
    case 0x230:case 0x24c:return OA_TD_GUIDE;
    case 0x200:case 0x201:case 0x202:case 0x203:case 0x204:
    case 0x205:case 0x206:case 0x207:case 0x208:case 0x209:return OA_TD_DIGIT;
    case 0x2a7:return OA_TD_REMOVED;
    default:return OA_TD_BASE;
    }
}
int oa_tab_dispatch(const struct oa_tab_dispatch_ops *o,uint32_t type,uint32_t id,int *handled)
{
    if(!o || !o->run || !handled)return EINVAL;
    enum oa_tab_dispatch_target route=target(type,id);
    uint32_t result=o->run(o->context,route,o->message);
    *handled=route==OA_TD_BASE_MENU || result!=0;
    return 0;
}
int oa_tab_handle_key(const struct oa_tab_pre_ops *pre,const struct oa_tab_dispatch_ops *fallback,uint32_t key,int *handled)
{
    if(!pre || !pre->query || !pre->run || !fallback || !fallback->run || !handled ||
       pre->message!=fallback->message)return EINVAL;
    int consumed;
    int error=oa_tab_prehandle(pre,key,&consumed);
    if(error)return error;
    if(consumed) { *handled=1;return 0; }
    return oa_tab_dispatch(fallback,0x1004,key,handled);
}
