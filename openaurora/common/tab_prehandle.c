#include "openaurora/tab_prehandle.h"
#include <errno.h>
#include <stddef.h>
static uint32_t q(const struct oa_tab_pre_ops *o,enum oa_tab_pre_query query)
{ return o->query(o->context,query); }
static uint32_t run(const struct oa_tab_pre_ops *o,enum oa_tab_pre_action action,uint32_t argument)
{ return o->run(o->context,action,argument,(action==OA_TP_CONTENT_KEY || action==OA_TP_BASE_PREHANDLE)?o->message:NULL); }
static int done(int *handled,uint32_t value) { *handled=value!=0;return 0; }
static int base(const struct oa_tab_pre_ops *o,uint32_t key,int *handled)
{ return done(handled,run(o,OA_TP_BASE_PREHANDLE,key)); }
int oa_tab_prehandle(const struct oa_tab_pre_ops *o,uint32_t key,int *handled)
{
    if(!o || !o->query || !o->run || !handled)return EINVAL;
    uint32_t vertical=q(o,OA_TP_VERTICAL_TABS);
    if((key==0x230 || key==0x24c) && q(o,OA_TP_IN_MEDIA)) {
        run(o,OA_TP_CLOSE_MODALS,0);run(o,OA_TP_POST_KEY,key);
        if(run(o,OA_TP_REQUEST_IMAGE,0))run(o,OA_TP_RELEASE_IMAGE,0);
        return done(handled,1);
    }
    if(key==0x25d || key==0x274) {
        run(o,OA_TP_POST_KEY,key);run(o,OA_TP_UNFOCUS,0);run(o,OA_TP_HIDE,0);run(o,OA_TP_END_DIALOG,0);
        return done(handled,1);
    }
    if(q(o,OA_TP_GROUP_HAS_FOCUS))run(o,OA_TP_SET_CONTENT_TAB,q(o,OA_TP_GROUP_ID));
    if(!q(o,OA_TP_PRESENT) || !q(o,OA_TP_FOCUSED)) {
        if(vertical && !q(o,OA_TP_PRESENT))return ENODATA;
        if(!vertical || q(o,OA_TP_FOCUSED)) {
            if(q(o,OA_TP_CIRCULAR)) {
                if(!q(o,OA_TP_PRESENT))return ENODATA;
                if(!q(o,OA_TP_FOCUSED)) {
                    if(key==0x20a) {
                        run(o,OA_TP_SHOW,0);if(!q(o,OA_TP_PRESENT))return ENODATA;
                        run(o,OA_TP_SET_ITEM,q(o,OA_TP_BIGGEST));run(o,OA_TP_FOCUS,0);
                    } else if(key==0x20b)run(o,OA_TP_SET_ITEM,q(o,OA_TP_SMALLEST));
                }
            }
        } else if(key==0x20d) {
            run(o,OA_TP_SET_ITEM,q(o,OA_TP_SMALLEST));run(o,OA_TP_FOCUS,0);
            return q(o,OA_TP_BUTTONS_ACTIVE)?done(handled,1):base(o,key,handled);
        } else if(key==0x20a || key==0x20b) {
            if(q(o,OA_TP_BUTTONS_ACTIVE)) {
                run(o,OA_TP_HIDE,0);
                uint32_t result=run(o,OA_TP_GROUP_KEY,key==0x20a?0x20c:0x20d);
                run(o,OA_TP_UPDATE_TITLE,0);run(o,OA_TP_SHOW,0);
                return done(handled,result);
            }
        } else if(key==0x20c)return done(handled,1);
        return base(o,key,handled);
    }
    if(key==0x20c || key==0x20d) {
        run(o,OA_TP_CONTENT_KEY,key);return done(handled,1);
    }
    if(key==0x22d || key==0x251) {
        run(o,OA_TP_CONTENT_KEY,key);run(o,OA_TP_UNFOCUS,0);run(o,OA_TP_HIDE,0);run(o,OA_TP_END_DIALOG,0);
        return key==0x22d?base(o,key,handled):done(handled,1);
    }
    if(key==0x210) {
        if(q(o,OA_TP_BACK_HANDLED))return done(handled,1);
        run(o,OA_TP_BUTTONS,1);run(o,OA_TP_SELECT_CHILD,q(o,OA_TP_CONTENT_TAB));
        run(o,OA_TP_CONTENT_FOCUS,0);run(o,OA_TP_DIALOG_FOCUS,1);return done(handled,1);
    }
    if(key==0x21f || key==0x28b) {
        run(o,OA_TP_UNFOCUS,0);run(o,OA_TP_HIDE,0);run(o,OA_TP_END_DIALOG,0x32);
        run(o,OA_TP_PUSH_FRONT_KEY,key);return done(handled,1);
    }
    if(key==0x20a || key==0x20b) {
        uint32_t current=q(o,OA_TP_INDEX),edge=q(o,key==0x20a?OA_TP_SMALLEST:OA_TP_BIGGEST);
        if(current==edge) {
            run(o,OA_TP_UNFOCUS,0);
            if(!q(o,OA_TP_TAB_JUMP))return done(handled,1);
            run(o,OA_TP_RIGHT_LEFT,key==0x20a?0x20c:0x20d);
            if(!vertical)return done(handled,1);
            if(q(o,OA_TP_PRESENT) && key==0x20a)run(o,OA_TP_SET_ITEM,q(o,OA_TP_BIGGEST));
            run(o,OA_TP_FOCUS,0);return done(handled,1);
        }
    }
    if(q(o,OA_TP_IN_MEDIA) && key==0x2a7) {
        run(o,OA_TP_UNFOCUS,0);return base(o,key,handled);
    }
    uint32_t result=run(o,OA_TP_CONTENT_KEY,key);
    return key==0x25b?base(o,key,handled):done(handled,result);
}
