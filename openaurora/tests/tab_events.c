#include "openaurora/tab_events.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
struct fixture { uint32_t open,buttons,present;unsigned count;enum oa_tab_action actions[8];uint32_t arguments[8];const void *messages[8]; };
static uint32_t query(void *v,enum oa_tab_query q)
{
    struct fixture *f=v;
    switch(q){case OA_TAB_OPEN_SUBMENU:return f->open;case OA_TAB_BUTTONS_ACTIVE:return f->buttons;case OA_TAB_CONTENT_PRESENT:return f->present;}
    assert(0);return 0;
}
static void action(void *v,enum oa_tab_action a,uint32_t argument,const void *message)
{
    struct fixture *f=v;assert(f->count<8);
    f->actions[f->count]=a;f->messages[f->count]=message;f->arguments[f->count++]=argument;
    if(a==OA_TAB_UPDATE_ACTIVE_LIST){assert(f->open);f->buttons=1;f->present=0;}
    if(a==OA_TAB_SET_OPEN_SUBMENU)f->open=argument;
    if(a==OA_TAB_HIDE_CONTENT)f->buttons=0;
}
int main(void)
{
    uint32_t message=0x20d;
    struct fixture f={.open=255,.buttons=0,.present=1};struct oa_tab_event_ops ops={&f,query,action,&message};
    assert(!oa_tab_event_up_down(&ops,0x20b) && f.count==5 && !f.open);
    const enum oa_tab_action expected[]={OA_TAB_SET_ITEM,OA_TAB_UPDATE_ACTIVE_LIST,OA_TAB_SET_OPEN_SUBMENU,OA_TAB_SHOW_CONTENT,OA_TAB_FOCUS_CONTENT};
    for(unsigned i=0;i<5;++i)assert(f.actions[i]==expected[i] && !f.arguments[i] && !f.messages[i]);
    f=(struct fixture){.buttons=1,.present=1};assert(!oa_tab_event_up_down(&ops,0x20b) && f.count==1 && f.actions[0]==OA_TAB_FOCUS_CONTENT);
    f=(struct fixture){.buttons=1};assert(!oa_tab_event_up_down(&ops,0x20a) && !f.count);
    assert(!oa_tab_event_right_left(&ops,UINT32_MAX) && f.count==4);
    assert(f.actions[0]==OA_TAB_HIDE_CONTENT && f.actions[1]==OA_TAB_GROUP_KEY && f.arguments[1]==UINT32_MAX &&
           f.actions[2]==OA_TAB_UPDATE_TITLE && f.actions[3]==OA_TAB_SHOW_CONTENT);
    assert(!f.messages[0] && f.messages[1]==&message && !f.messages[2] && !f.messages[3]);
    f=(struct fixture){0};assert(!oa_tab_event_right_left(&ops,0x20d) && !f.count);
    assert(!oa_tab_event_up_down(&ops,0x20b) && !f.count);
    assert(oa_tab_event_up_down(NULL,0)==EINVAL && oa_tab_event_right_left(NULL,0)==EINVAL);
    ops.action=NULL;assert(oa_tab_event_up_down(&ops,0)==EINVAL && oa_tab_event_right_left(&ops,0)==EINVAL && !f.count);
    ops.action=action;ops.query=NULL;
    assert(oa_tab_event_up_down(&ops,0)==EINVAL && oa_tab_event_right_left(&ops,0)==EINVAL && !f.count);
    puts("ok tab events: exact reset/update ordering, dynamic state, existing/missing content, gate changes, full ICE codes and invalid callbacks");
    return 0;
}
