#include "openaurora/tab_events.h"
#include <errno.h>
#include <stddef.h>
static void act(const struct oa_tab_event_ops *o,enum oa_tab_action action,uint32_t argument)
{ o->action(o->context,action,argument,action==OA_TAB_GROUP_KEY?o->message:NULL); }
int oa_tab_event_up_down(const struct oa_tab_event_ops *o,uint32_t key)
{
    if(!o || !o->query || !o->action)return EINVAL;
    if(o->query(o->context,OA_TAB_OPEN_SUBMENU)) {
        act(o,OA_TAB_SET_ITEM,0);
        act(o,OA_TAB_UPDATE_ACTIVE_LIST,0);
        act(o,OA_TAB_SET_OPEN_SUBMENU,0);
    }
    if(o->query(o->context,OA_TAB_BUTTONS_ACTIVE) && key==0x20b) {
        if(!o->query(o->context,OA_TAB_CONTENT_PRESENT))
            act(o,OA_TAB_SHOW_CONTENT,0);
        act(o,OA_TAB_FOCUS_CONTENT,0);
    }
    return 0;
}
int oa_tab_event_right_left(const struct oa_tab_event_ops *o,uint32_t key)
{
    if(!o || !o->query || !o->action)return EINVAL;
    if(o->query(o->context,OA_TAB_BUTTONS_ACTIVE)) {
        act(o,OA_TAB_HIDE_CONTENT,0);
        act(o,OA_TAB_GROUP_KEY,key);
        act(o,OA_TAB_UPDATE_TITLE,0);
        act(o,OA_TAB_SHOW_CONTENT,0);
    }
    return 0;
}
