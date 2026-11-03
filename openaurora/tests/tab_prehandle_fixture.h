#ifndef OA_TAB_PREHANDLE_FIXTURE_H
#define OA_TAB_PREHANDLE_FIXTURE_H
#include "openaurora/tab_prehandle.h"
#include <assert.h>
#include <stddef.h>
struct pre_fixture {
    uint32_t q[OA_TP_TAB_JUMP+1],child_result,base_result,group_result,image,jump_present,show_success,allow_focus;
    unsigned count;
    struct {enum oa_tab_pre_action action;uint32_t argument;const void *message;} trace[32];
};
static inline uint32_t pre_query(void *p,enum oa_tab_pre_query query)
{ return ((struct pre_fixture *)p)->q[query]; }
static inline uint32_t pre_run(void *p,enum oa_tab_pre_action action,uint32_t argument,const void *message)
{
    struct pre_fixture *f=p;assert(f->count<32);
    f->trace[f->count].action=action;f->trace[f->count].argument=argument;f->trace[f->count++].message=message;
    switch(action) {
    case OA_TP_SET_CONTENT_TAB:f->q[OA_TP_CONTENT_TAB]=argument;break;
    case OA_TP_UNFOCUS:f->q[OA_TP_FOCUSED]=0;f->q[OA_TP_BUTTONS_ACTIVE]=1;break;
    case OA_TP_HIDE:f->q[OA_TP_PRESENT]=0;break;
    case OA_TP_SHOW:f->q[OA_TP_PRESENT]=f->show_success;break;
    case OA_TP_FOCUS:f->q[OA_TP_FOCUSED]=f->q[OA_TP_PRESENT] && f->allow_focus;f->q[OA_TP_BUTTONS_ACTIVE]=!f->q[OA_TP_FOCUSED];break;
    case OA_TP_BUTTONS:f->q[OA_TP_BUTTONS_ACTIVE]=argument;break;
    case OA_TP_CONTENT_FOCUS:f->q[OA_TP_FOCUSED]=argument;break;
    case OA_TP_SET_ITEM:f->q[OA_TP_INDEX]=argument;break;
    case OA_TP_RIGHT_LEFT:f->q[OA_TP_PRESENT]=f->jump_present;break;
    case OA_TP_CONTENT_KEY:return f->child_result;
    case OA_TP_BASE_PREHANDLE:return f->base_result;
    case OA_TP_GROUP_KEY:return f->group_result;
    case OA_TP_REQUEST_IMAGE:return f->image;
    default:break;
    }
    return 0;
}
#endif
