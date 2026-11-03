#include "openaurora/settings_list_input.h"
#include <errno.h>
#include <stddef.h>
int oa_settings_list_dispatch(const struct oa_settings_list_dispatch_ops *o,uint32_t type,uint32_t id,int *handled)
{
    if(!o || !o->run || !handled)return EINVAL;
    enum oa_settings_list_route route=OA_SL_ROUTE_BASE;
    if(type==0x1004) {
        if(id==0x20d || id==0x20c || id==0x20e || id==0x2da)route=OA_SL_ROUTE_RIGHT_LEFT;
        else if(id>=0x200 && id<=0x209)route=OA_SL_ROUTE_DIGIT;
    }
    *handled=o->run(o->context,route,o->message)!=0;return 0;
}
static uint32_t q(const struct oa_settings_list_ops *o,enum oa_settings_list_query query,uint32_t arg)
{ return o->query(o->context,query,arg); }
int oa_settings_list_input(const struct oa_settings_list_ops *o,enum oa_settings_list_kind kind,uint32_t key,int *handled)
{
    if(!o || !o->query || !o->run || !handled || (unsigned)kind>OA_SL_DIGIT)return EINVAL;
    if(kind==OA_SL_RIGHT_LEFT && q(o,OA_SL_HORIZONTAL,0)) {
        const void *message=o->message;
        if(q(o,OA_SL_MIRROR,0) && (key==0x20c || key==0x20d)) { key=key==0x20c?0x20d:0x20c;message=NULL; }
        o->run(o->context,OA_SL_BASE_KEY,0,key,message);*handled=1;return 0;
    }
    uint32_t row=q(o,OA_SL_ROW,q(o,OA_SL_SELECTED,0));
    if(row) {
        o->run(o->context,OA_SL_ROW_KEY,row,key,o->message);
        if(kind==OA_SL_RIGHT_LEFT) {
            uint32_t record=q(o,OA_SL_RECORD,q(o,OA_SL_SELECTED,0));
            if(!record)return ENODATA;
            uint32_t value=q(o,OA_SL_ROW_VALUE,row);
            o->run(o->context,OA_SL_STORE_VALUE,record,value,NULL);
        }
    }
    *handled=kind==OA_SL_RIGHT_LEFT || row!=0;return 0;
}
