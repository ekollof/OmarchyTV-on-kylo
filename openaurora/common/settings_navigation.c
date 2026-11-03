#include "openaurora/settings_navigation.h"
#include <errno.h>
static int64_t signed32(uint32_t value)
{ return value<=INT32_MAX?(int64_t)value:(int64_t)value-INT64_C(4294967296); }
static uint32_t q(const struct oa_settings_nav_ops *o,enum oa_settings_nav_query query,uint32_t arg)
{ return o->query(o->context,query,arg); }
static void act(const struct oa_settings_nav_ops *o,enum oa_settings_nav_action action,uint32_t arg)
{ o->run(o->context,action,arg); }
int oa_settings_navigate(const struct oa_settings_nav_ops *o,uint32_t key)
{
    if(!o || !o->query || !o->run)return EINVAL;
    uint32_t count=q(o,OA_SN_COUNT,0);
    if(count>UINT32_C(0x1fffffff))return EOVERFLOW;
    if(!count)return 0;
    uint32_t cursor=q(o,OA_SN_CURSOR,0),next=cursor;int write=0;
    if(count>1) {
        if(key==0x20b) { next=cursor==count-1?0:cursor+UINT32_C(1);write=1; }
        else if(key==0x20a) { next=cursor?cursor-UINT32_C(1):count-1;write=1; }
        else if(key==0x2db && cursor) { next=0;write=1; }
        else if(key==0x2dc && cursor!=count-1) { next=count-1;write=1; }
    }
    if(write)act(o,OA_SN_SET_CURSOR,next);
    if(next>=count)return ERANGE;
    uint32_t index=q(o,OA_SN_ITEM_INDEX,q(o,OA_SN_ACTIVE_ID,next));
    uint32_t biggest=q(o,OA_SN_BIGGEST_VISIBLE,0),rows=q(o,OA_SN_ROWS,0);
    uint32_t top=biggest-rows+UINT32_C(1);
    if(signed32(index)>signed32(biggest) || signed32(index)<signed32(top)) {
        if(signed32(index)<signed32(rows) || key!=0x20b)act(o,OA_SN_SET_ITEM,index);
        else {
            act(o,OA_SN_SET_ITEM,index-rows+UINT32_C(1));
            act(o,OA_SN_SET_ROW,rows-UINT32_C(1));
        }
    } else act(o,OA_SN_SET_ROW,index-top);
    return 0;
}
