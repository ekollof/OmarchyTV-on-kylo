#include "host.h"
#include "openaurora/events.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static char reply[512];
static const oa_ui_navigation_iface *removing;
static unsigned removed_checks;
static void removed(const void *ev,void *ud)
{
    (void)ud;const oa_service_removed_event *event=ev;
    if(strcmp(event->name,OA_SVC_UI_NAVIGATION) || !removing)return;
    oa_ui_navigation_state state;
    oa_ui_navigation_request request;
    assert(removing->state(&state)==ENODEV && removing->result(108)==ENODEV &&
           removing->take(&request)==ENODEV);
    const oa_ui_navigation_iface_v2 *v2=(const void *)removing;
    assert(v2->set_tab(1)==ENODEV && v2->set_item(1)==ENODEV && v2->set_open_submenu(1)==ENODEV);
    removing=NULL;++removed_checks;
}
static int command(oa_host *h,const char *text)
{
    char line[256];snprintf(line,sizeof line,"%s",text);
    return oa_control_command(h,line,reply,sizeof reply);
}
static void failure(oa_host *h,const char *text,int error)
{
    assert(command(h,text)==-1);char expected[64];
    snprintf(expected,sizeof expected,"failed errno %d\n",error);assert(!strcmp(reply,expected));
}
static const oa_ui_navigation_iface *service(oa_host *h)
{
    const oa_ui_navigation_iface *s=h->api.lookup(h,OA_SVC_UI_NAVIGATION,1);assert(s);return s;
}
static const oa_ui_navigation_iface_v2 *service2(oa_host *h)
{ const oa_ui_navigation_iface_v2 *s=h->api.lookup(h,OA_SVC_UI_NAVIGATION,2);assert(s);return s; }
static void selection(oa_host *h,uint32_t tab,int32_t item,uint32_t open,uint32_t count)
{
    oa_ui_navigation_state state;
    assert(!service(h)->state(&state));
    assert(state.tab==tab && state.item==item && state.open_submenu==open && state.pending==count);
    h->api.release(h,OA_SVC_UI_NAVIGATION);
}
int main(int argc,char **argv)
{
    assert(argc==2);oa_host *h=calloc(1,sizeof *h);assert(h && !oa_host_init(h,argv[1],NULL));
    h->log_level=-1;
    uint32_t removed_sub=h->api.subscribe(h,OA_TOPIC_SERVICE_REMOVED,removed,NULL);assert(removed_sub);
    failure(h,"nav-state",ENODEV);
    failure(h,"nav-set-tab 0",ENODEV);
    int legacy_peer=42;assert(!h->api.publish(h,OA_SVC_UI_NAVIGATION,1,&legacy_peer));
    failure(h,"nav-set-tab 0",ENOTSUP);failure(h,"nav-set-item 0",ENOTSUP);failure(h,"nav-set-submenu 1",ENOTSUP);
    assert(!h->api.unpublish(h,OA_SVC_UI_NAVIGATION));
    oa_mod *m=oa_module_load(h,"ui_navigation");assert(m && !command(h,"start ui_navigation"));
    selection(h,0,0,0,0);
    failure(h,"nav-take",EAGAIN);
    const char *bad[]={"nav-result","nav-operation","nav-state extra","nav-take extra",
        "nav-operation -1","nav-operation 4294967296","nav-result 2147483648",
        "nav-result -2147483649","nav-result +108","nav-result 108junk",
        "nav-result 108 extra","nav-state --force","nav-result --force 108",
        "nav-set-tab","nav-set-item","nav-set-submenu","nav-set-tab 2147483648",
        "nav-set-tab -2147483649","nav-set-item 2147483648","nav-set-item -2147483649",
        "nav-set-submenu 256","nav-set-submenu -1","nav-set-submenu +1",
        "nav-set-tab +1","nav-set-item 0 junk","nav-set-tab --force 0","nav-set-submenu 0junk"};
    for(size_t i=0;i<sizeof bad/sizeof *bad;++i)failure(h,bad[i],EINVAL);
    failure(h,"nav-result -1",ENOTSUP);failure(h,"nav-operation 30",ENOTSUP);
    failure(h,"nav-operation 5",ENOTSUP);failure(h,"nav-result 166",ENOTSUP);
    selection(h,0,0,0,0);
    assert(!command(h,"nav-operation 2") && !strcmp(reply,"queued\n"));selection(h,0,0,0,1);
    assert(!command(h,"nav-result 109"));selection(h,0,109,1,2);
    assert(!command(h,"nav-operation 6"));selection(h,2,109,1,3);
    const oa_ui_navigation_iface *s=service(h);
    oa_ui_navigation_request request;
    assert(!s->take(&request) && request.navigation==0x13 && request.selection.tab==2 &&
           request.selection.item==109 && request.selection.open_submenu==1 && request.selection.pending==2);
    h->api.release(h,OA_SVC_UI_NAVIGATION);
    assert(!command(h,"nav-take") && !strcmp(reply,"navigation 19 tab 2 item 109 submenu 1 pending 1\n"));
    removing=service(h);h->api.release(h,OA_SVC_UI_NAVIGATION);
    uint32_t old=m->id;assert(!command(h,"reload ui_navigation") && removed_checks==1);
    m=oa_host_find_module(h,"ui_navigation");assert(m && m->id!=old);selection(h,2,109,1,1);
    assert(!command(h,"nav-take"));
    puts("ok navigation: CLI domains/validation, copied current selection at take, retained fields and pending reload");

    const oa_ui_navigation_iface_v2 *v2=service2(h);
    assert(!v2->set_tab(-1));selection(h,UINT32_MAX,109,1,0);
    assert(!v2->set_item(INT32_MIN));selection(h,UINT32_MAX,INT32_MIN,1,0);
    assert(!v2->set_open_submenu(255));selection(h,UINT32_MAX,INT32_MIN,255,0);
    h->api.release(h,OA_SVC_UI_NAVIGATION);
    assert(!command(h,"nav-operation 6"));selection(h,2,INT32_MIN,255,1);
    assert(!command(h,"nav-set-tab -2147483648") && !strcmp(reply,"ok\n"));
    assert(!command(h,"nav-set-item 2147483647") && !command(h,"reload ui_navigation"));
    m=oa_host_find_module(h,"ui_navigation");assert(m);selection(h,UINT32_C(0x80000000),INT32_MAX,255,1);
    assert(!command(h,"nav-take") && !strcmp(reply,"navigation 19 tab 2147483648 item 2147483647 submenu 255 pending 0\n"));
    assert(!command(h,"nav-set-submenu 0"));selection(h,UINT32_C(0x80000000),INT32_MAX,0,0);
    assert(!command(h,"nav-set-submenu 1"));
    puts("ok navigation v2: independent setters preserve queue, signed32/raw byte domains, v1 prefix and OAN2 reload/take");

    for(unsigned i=0;i<OA_UI_NAVIGATION_CAPACITY;++i)assert(!command(h,"nav-result 109"));
    failure(h,"nav-operation 3",ENOSPC);selection(h,0,109,1,OA_UI_NAVIGATION_CAPACITY);
    assert(!command(h,"nav-set-item -1"));selection(h,0,-1,1,OA_UI_NAVIGATION_CAPACITY);
    for(unsigned i=0;i<OA_UI_NAVIGATION_CAPACITY;++i)assert(!command(h,"nav-take"));
    failure(h,"nav-take",EAGAIN);
    assert(!command(h,"nav-operation 2"));selection(h,0,0,1,1); /* no open reset */
    h->stop_requested=1;failure(h,"nav-operation 3",ESHUTDOWN);failure(h,"nav-take",ESHUTDOWN);
    failure(h,"nav-set-tab 0",ESHUTDOWN);failure(h,"nav-set-item 0",ESHUTDOWN);failure(h,"nav-set-submenu 0",ESHUTDOWN);
    assert(!command(h,"nav-state"));h->stop_requested=0;
    assert(!oa_module_suspend_all(h));selection(h,0,0,1,1);
    failure(h,"nav-operation 3",EBUSY);failure(h,"nav-take",EBUSY);
    failure(h,"nav-set-tab 0",EBUSY);failure(h,"nav-set-item 0",EBUSY);failure(h,"nav-set-submenu 0",EBUSY);
    assert(!command(h,"reload ui_navigation"));m=oa_host_find_module(h,"ui_navigation");
    assert(m && m->state==OA_ST_SUSPENDED);failure(h,"nav-state",ENODEV);
    int peer=42;assert(!h->api.publish(h,OA_SVC_UI_NAVIGATION,1,&peer));
    assert(oa_module_resume_all(h)==-1 && m->state==OA_ST_SUSPENDED);
    assert(h->api.lookup(h,OA_SVC_UI_NAVIGATION,1)==&peer);
    assert(!h->api.unpublish(h,OA_SVC_UI_NAVIGATION) && !oa_module_resume_all(h));
    selection(h,0,0,1,1);
    puts("ok navigation: atomic full queue, shutdown gate, standby retention, suspended reload and publication-failure retry");

    oa_mod *consumer=oa_module_load(h,"ticker");assert(consumer);
    h->cur_module=consumer->id;s=service(h);h->cur_module=0;
    assert(command(h,"stop ui_navigation")==-1 && command(h,"unload ui_navigation")==-1 &&
           command(h,"reload ui_navigation")==-1);
    oa_ui_navigation_state state;assert(!s->state(&state) && state.pending==1);
    h->cur_module=consumer->id;h->api.release(h,OA_SVC_UI_NAVIGATION);h->cur_module=0;
    assert(!oa_module_unload(h,"ticker",0) && !command(h,"stop ui_navigation"));
    unsigned char saved[128],bad_blob[128],unchanged[128];
    size_t n=m->desc->save(m->ctx,saved,sizeof saved);assert(n==20 && !memcmp(saved,"OAN2",4));
    memset(unchanged,0xee,sizeof unchanged);
    assert(m->desc->save(m->ctx,unchanged,n-1)==n);
    for(size_t i=0;i<sizeof unchanged;++i)assert(unchanged[i]==0xee);
    const unsigned offsets[]={0,3,5,6,7,16};
    const unsigned char values[]={0,'3',17,1,1,0};
    for(size_t i=0;i<sizeof offsets/sizeof *offsets;++i) {
        memcpy(bad_blob,saved,n);bad_blob[offsets[i]]=values[i];
        assert(m->desc->restore(m->ctx,bad_blob,n)==-1);
        assert(m->desc->save(m->ctx,unchanged,sizeof unchanged)==n && !memcmp(saved,unchanged,n));
    }
    assert(m->desc->restore(m->ctx,saved,n-1)==-1 && m->desc->restore(m->ctx,saved,n+1)==-1);
    assert(m->desc->restore(m->ctx,NULL,n)==-1 && !m->desc->restore(m->ctx,NULL,0));
    memcpy(bad_blob,saved,n);memcpy(bad_blob,"OAN1",4);
    assert(!m->desc->restore(m->ctx,bad_blob,n)); /* prior version still loads */
    bad_blob[4]=2;assert(m->desc->restore(m->ctx,bad_blob,n)==-1);
    bad_blob[4]=1;bad_blob[8]=5;assert(m->desc->restore(m->ctx,bad_blob,n)==-1);
    const uint32_t bits[]={0,1,4,5,UINT32_C(0x80000000),INT32_MAX,UINT32_MAX};
    unsigned roundtrips=0;
    for(size_t i=0;i<sizeof bits/sizeof *bits;++i)for(unsigned raw=0;raw<256;++raw) {
        memcpy(bad_blob,saved,n);bad_blob[4]=(unsigned char)raw;
        for(unsigned j=0;j<4;++j) {
            bad_blob[8+j]=(unsigned char)(bits[i]>>(8*j));
            bad_blob[12+j]=(unsigned char)(~bits[i]>>(8*j));
        }
        assert(!m->desc->restore(m->ctx,bad_blob,n));
        assert(m->desc->save(m->ctx,unchanged,sizeof unchanged)==n && !memcmp(bad_blob,unchanged,n));
        ++roundtrips;
    }
    assert(roundtrips==1792);
    memcpy(bad_blob,saved,n);bad_blob[12]=0x85;bad_blob[13]=bad_blob[14]=bad_blob[15]=0xff;
    assert(!m->desc->restore(m->ctx,bad_blob,n) && !command(h,"start ui_navigation"));
    selection(h,0,-123,1,1);
    assert(m->desc->restore(m->ctx,saved,n)==-1);selection(h,0,-123,1,1);
    assert(!command(h,"nav-operation 6"));selection(h,2,-123,1,2);
    removing=service(h);h->api.release(h,OA_SVC_UI_NAVIGATION);
    assert(!command(h,"unload ui_navigation") && removed_checks==2);failure(h,"nav-state",ENODEV);
    h->api.unsubscribe(h,removed_sub);
    close(h->selfpipe[0]);close(h->selfpipe[1]);pthread_mutex_destroy(&h->post_mu);free(h);
    puts("ok navigation: holds, OAN1 migration/OAN2 validation and1792 raw-state roundtrips, signed retained row and cleanup");
    return 0;
}
