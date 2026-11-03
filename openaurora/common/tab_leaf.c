#include "openaurora/tab_leaf.h"
#include <errno.h>
#include <stddef.h>
static uint32_t q(const struct oa_tab_leaf_ops *o,enum oa_tab_leaf_query query)
{ return o->query(o->context,query); }
static void act(const struct oa_tab_leaf_ops *o,enum oa_tab_leaf_action action,uint32_t argument)
{ o->run(o->context,action,argument,action==OA_TL_CONTENT_KEY?o->message:NULL); }
int oa_tab_leaf(const struct oa_tab_leaf_ops *o,enum oa_tab_leaf_kind kind,uint32_t key,int *handled)
{
    if(!o || !o->query || !o->run || !handled || (unsigned)kind>OA_TL_REMOVED)return EINVAL;
    int consumed=1;
    switch(kind) {
    case OA_TL_VOLUME:
        if(!q(o,OA_TL_CONTENT_PRESENT))return ENODATA;
        if(q(o,OA_TL_CONTENT_SHOWING))act(o,OA_TL_CONTENT_KEY,key);
        break;
    case OA_TL_DIGIT: {
        uint32_t bits=q(o,OA_TL_DIGITS)*UINT32_C(10)+key-UINT32_C(0x200);
        int64_t value=bits;
        if(bits>INT32_MAX)value-=INT64_C(4294967296);
        uint32_t digits=(uint32_t)(value%10000);
        act(o,OA_TL_SET_DIGITS,digits);
        if(digits==0x450)act(o,OA_TL_SVG_END_DIALOG,0xd1);
        else if(digits==0x1275)act(o,OA_TL_SVG_END_DIALOG,0xb5);
        break;
    }
    case OA_TL_MENU:
        if(key==0x22d || key==0x25b || key==0x251)act(o,OA_TL_RESET_PICTURE_INDEX,0);
        act(o,OA_TL_END_DIALOG,key==0x210?0x29a:0);
        break;
    case OA_TL_QUIT:
        act(o,OA_TL_END_DIALOG,0);act(o,OA_TL_PUSH_KEY,key);
        break;
    case OA_TL_GUIDE:
        act(o,OA_TL_END_DIALOG,0);act(o,OA_TL_PUSH_NAVIGATION,0xb);
        break;
    case OA_TL_REMOVED:
        consumed=q(o,OA_TL_IN_MEDIA)!=0;
        if(consumed) { act(o,OA_TL_END_DIALOG,0);act(o,OA_TL_PUSH_KEY,0x2a7); }
        break;
    }
    *handled=consumed;
    return 0;
}
