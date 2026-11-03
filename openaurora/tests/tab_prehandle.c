#include "tab_prehandle_fixture.h"
#include <errno.h>
#include <stdio.h>
static void actions(const struct pre_fixture *f,const enum oa_tab_pre_action *wanted,unsigned count)
{ assert(f->count==count);for(unsigned i=0;i<count;++i)assert(f->trace[i].action==wanted[i]); }
int main(void)
{
    struct pre_fixture f={0};uint32_t original=0x251;int handled=-1;
    struct oa_tab_pre_ops ops={&f,pre_query,pre_run,&original};
    f.q[OA_TP_PRESENT]=1;f.q[OA_TP_FOCUSED]=1;f.child_result=0;
    assert(!oa_tab_prehandle(&ops,0x20c,&handled) && handled==1);
    assert(f.count==1 && f.trace[0].action==OA_TP_CONTENT_KEY && f.trace[0].message==&original);
    f.count=0;f.q[OA_TP_CONTENT_TAB]=4;
    assert(!oa_tab_prehandle(&ops,0x210,&handled) && handled==1);
    const enum oa_tab_pre_action back[]={OA_TP_BUTTONS,OA_TP_SELECT_CHILD,OA_TP_CONTENT_FOCUS,OA_TP_DIALOG_FOCUS};actions(&f,back,4);
    assert(f.trace[1].argument==4 && !f.q[OA_TP_FOCUSED]);
    f=(struct pre_fixture){0};f.q[OA_TP_PRESENT]=1;f.q[OA_TP_FOCUSED]=1;
    assert(!oa_tab_prehandle(&ops,0x22d,&handled) && !handled);
    const enum oa_tab_pre_action menu[]={OA_TP_CONTENT_KEY,OA_TP_UNFOCUS,OA_TP_HIDE,OA_TP_END_DIALOG,OA_TP_BASE_PREHANDLE};actions(&f,menu,5);
    assert(f.trace[4].message==&original);
    f=(struct pre_fixture){0};f.q[OA_TP_IN_MEDIA]=1;f.image=1;
    assert(!oa_tab_prehandle(&ops,0x230,&handled) && handled);
    const enum oa_tab_pre_action guide[]={OA_TP_CLOSE_MODALS,OA_TP_POST_KEY,OA_TP_REQUEST_IMAGE,OA_TP_RELEASE_IMAGE};actions(&f,guide,4);
    for(unsigned i=0;i<f.count;++i)assert(!f.trace[i].message);
    f=(struct pre_fixture){0};f.q[OA_TP_PRESENT]=1;f.q[OA_TP_FOCUSED]=1;
    f.q[OA_TP_TAB_JUMP]=1;f.q[OA_TP_VERTICAL_TABS]=1;f.q[OA_TP_BIGGEST]=7;f.jump_present=1;f.allow_focus=1;
    assert(!oa_tab_prehandle(&ops,0x20a,&handled) && handled);
    const enum oa_tab_pre_action edge[]={OA_TP_UNFOCUS,OA_TP_RIGHT_LEFT,OA_TP_SET_ITEM,OA_TP_FOCUS};actions(&f,edge,4);
    assert(f.trace[1].argument==0x20c && f.trace[2].argument==7 && !f.trace[1].message);
    f=(struct pre_fixture){.allow_focus=1};f.q[OA_TP_VERTICAL_TABS]=1;f.q[OA_TP_PRESENT]=1;
    assert(!oa_tab_prehandle(&ops,0x20d,&handled) && !handled);
    const enum oa_tab_pre_action enter[]={OA_TP_SET_ITEM,OA_TP_FOCUS,OA_TP_BASE_PREHANDLE};actions(&f,enter,3);
    f=(struct pre_fixture){0};f.q[OA_TP_VERTICAL_TABS]=1;handled=42;
    assert(oa_tab_prehandle(&ops,0x20d,&handled)==ENODATA && handled==42 && !f.count);
    f=(struct pre_fixture){0};f.q[OA_TP_PRESENT]=1;f.q[OA_TP_CIRCULAR]=1;
    assert(oa_tab_prehandle(&ops,0x20a,&handled)==ENODATA && handled==42);
    assert(f.count==1 && f.trace[0].action==OA_TP_SHOW); /* failed show already ran */
    f.count=0;
    assert(oa_tab_prehandle(NULL,0,&handled)==EINVAL && handled==42);
    ops.run=NULL;assert(oa_tab_prehandle(&ops,0,&handled)==EINVAL && !f.count);
    ops.run=pre_run;ops.query=NULL;assert(oa_tab_prehandle(&ops,0,&handled)==EINVAL && !f.count);
    ops.query=pre_query;assert(oa_tab_prehandle(&ops,0,NULL)==EINVAL && !f.count);
    puts("ok tab pre-handler: focused forwarding, back/menu ordering, media guide, edge jump, original/synthetic identity and missing-child safety");
    return 0;
}
