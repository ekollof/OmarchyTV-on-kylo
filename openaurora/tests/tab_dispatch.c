#include "openaurora/tab_dispatch.h"
#include "tab_prehandle_fixture.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
struct fixture { unsigned count;enum oa_tab_dispatch_target route;uint32_t result;const void *message; };
static uint32_t invoke(void *context,enum oa_tab_dispatch_target route,const void *message)
{ struct fixture *f=context;++f->count;f->route=route;f->message=message;return f->result; }
int main(void)
{
    int event=17,other=18,handled=42;
    struct fixture f={0};struct oa_tab_dispatch_ops o={&f,invoke,&event};
    assert(!oa_tab_dispatch(&o,0x1004,0x22d,&handled) && handled==1 && f.route==OA_TD_BASE_MENU && f.message==&event);
    assert(!oa_tab_dispatch(&o,0x1004,0x2c9,&handled) && !handled && f.route==OA_TD_RIGHT_LEFT);
    f.result=255;
    assert(!oa_tab_dispatch(&o,0x1004,0x209,&handled) && handled==1 && f.route==OA_TD_DIGIT);
    assert(!oa_tab_dispatch(&o,0x11004,0x209,&handled) && f.route==OA_TD_BASE);
    assert(!oa_tab_dispatch(&o,0x1004,0x10209,&handled) && f.route==OA_TD_BASE);
    assert(!oa_tab_dispatch(&o,0x1004,UINT32_MAX,&handled) && f.route==OA_TD_BASE);
    struct pre_fixture p={0};
    struct oa_tab_pre_ops pre={&p,pre_query,pre_run,&event};
    f.count=0;f.result=0;
    assert(!oa_tab_handle_key(&pre,&o,0x220,&handled) && !handled && f.count==1 && f.route==OA_TD_VOLUME);
    assert(p.count==1 && p.trace[0].action==OA_TP_BASE_PREHANDLE && p.trace[0].message==&event);
    p=(struct pre_fixture){.base_result=7};f.count=0;
    assert(!oa_tab_handle_key(&pre,&o,0x220,&handled) && handled==1 && f.count==0);
    p=(struct pre_fixture){0};
    assert(!oa_tab_handle_key(&pre,&o,0x25d,&handled) && handled==1 && f.count==0 && p.count==4);
    p=(struct pre_fixture){0};p.q[OA_TP_VERTICAL_TABS]=1;handled=42;
    assert(oa_tab_handle_key(&pre,&o,0x220,&handled)==ENODATA && handled==42 && !f.count);
    p=(struct pre_fixture){0};o.message=&other;
    assert(oa_tab_handle_key(&pre,&o,0x220,&handled)==EINVAL && handled==42 && !p.count && !f.count);
    o.message=&event;o.run=NULL;
    assert(oa_tab_handle_key(&pre,&o,0x220,&handled)==EINVAL && !p.count && !f.count);
    assert(oa_tab_dispatch(&o,0x1004,0x220,&handled)==EINVAL && handled==42);
    assert(oa_tab_dispatch(NULL,0,0,&handled)==EINVAL && handled==42);
    o.run=invoke;
    assert(oa_tab_dispatch(&o,0,0,NULL)==EINVAL && !f.count);
    assert(oa_tab_handle_key(NULL,&o,0,NULL)==EINVAL && !f.count);
    puts("ok tab dispatch: original event, full-width type/id, menu consumption, ordered pre/fallback and failure gates");
    return 0;
}
