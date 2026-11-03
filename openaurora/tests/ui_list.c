#include "host.h"
#include "openaurora/events.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static char reply[1024];
static void put32(unsigned char *p,uint32_t value)
{ for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(value>>(8*i)); }
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
static const oa_ui_list_iface *service(oa_host *h)
{
    const oa_ui_list_iface *s=h->api.lookup(h,OA_SVC_UI_LIST,1);assert(s);return s;
}
/* first size selected total handled anim scroll partial focus group */
static void sense(oa_host *h,uint32_t first,uint32_t size,uint32_t selected,uint32_t total,
    int handled,uint32_t anim,uint32_t scroll,uint32_t partial,uint32_t focus,uint32_t group)
{
    oa_ui_list_state s;assert(!service(h)->state(&s));
    assert(s.first==first && s.size==size && s.selected==selected && s.total==total);
    assert(s.handled==handled && s.anim==anim && s.scroll==scroll);
    assert(s.partial==partial && s.focus==focus && s.group==group);
    h->api.release(h,OA_SVC_UI_LIST);
}
static void key(oa_host *h,const char *cmd,int handled)
{
    assert(!command(h,cmd));
    char expected[32];snprintf(expected,sizeof expected,"handled %d\n",handled);
    assert(!strcmp(reply,expected));
}
int main(int argc,char **argv)
{
    assert(argc==2);oa_host *h=calloc(1,sizeof *h);assert(h && !oa_host_init(h,argv[1],NULL));
    h->log_level=-1;
    failure(h,"list-state",ENODEV);
    failure(h,"list-key 521",ENODEV);
    oa_mod *m=oa_module_load(h,"ui_list");assert(m && !command(h,"start ui_list"));
    sense(h,0,5,0,9,-1,0,0,0,0,0);
    const char *bad[]={"list-state extra","list-key","list-key 4294967296",
        "list-key -1","list-key 12junk","list-key 12 extra","list-key --force 12",
        "list-state --force"};
    for(size_t i=0;i<sizeof bad/sizeof *bad;++i)failure(h,bad[i],EINVAL);
    /* down twice, up once: verified move+focus chain, animation each run. */
    key(h,"list-key 523",1);sense(h,0,5,1,9,1,1,0,0,2,0);
    key(h,"list-key 523",1);sense(h,0,5,2,9,1,2,0,0,4,0);
    key(h,"list-key 522",1);sense(h,0,5,1,9,1,3,0,0,6,0);
    /* select consumes with null parent, no animation, no movement. */
    key(h,"list-key 526",1);sense(h,0,5,1,9,1,3,0,0,6,0);
    /* last/first synthesize group indices through the owned group target. */
    key(h,"list-key 732",1);sense(h,4,5,4,9,1,3,0,0,6,1);
    key(h,"list-key 731",1);sense(h,0,5,0,9,1,3,0,0,6,2);
    /* unknown key: unhandled, no callbacks. */
    key(h,"list-key 4660",0);sense(h,0,5,0,9,0,3,0,0,6,2);
    /* page-down scrolls (4>4 false): window-down partial (size writeback5->9)
     * plus scrollbar/animation, selection unchanged. */
    key(h,"list-key 524",1);sense(h,0,9,0,9,1,4,1,1,6,2);
    /* page-up wraps to top: window-top partial/scroll/animation plus the
     * body's own animation; focus(0) is already selected so no focus calls. */
    key(h,"list-key 525",1);sense(h,0,9,0,9,1,6,2,2,6,2);
    /* suspend gates keys, state stays queryable; reload keeps suspend state. */
    assert(!oa_module_suspend_all(h));sense(h,0,9,0,9,1,6,2,2,6,2);
    failure(h,"list-key 523",EBUSY);
    uint32_t old=m->id;assert(!command(h,"reload ui_list"));
    m=oa_host_find_module(h,"ui_list");assert(m && m->id!=old);
    assert(m->state==OA_ST_SUSPENDED);failure(h,"list-state",ENODEV);
    assert(!oa_module_resume_all(h));sense(h,0,9,0,9,1,6,2,2,6,2);
    /* shutdown gates keys, queries still answer. */
    h->stop_requested=1;failure(h,"list-key 523",ESHUTDOWN);
    assert(!command(h,"list-state"));h->stop_requested=0;
    /* save/restore validation: exact OAL1 encoding, published guard. */
    unsigned char saved[64],probe[64];
    size_t n=m->desc->save(m->ctx,saved,sizeof saved);assert(n==44 && !memcmp(saved,"OAL1",4));
    assert(m->desc->save(m->ctx,NULL,0)==n);
    assert(m->desc->restore(m->ctx,saved,n)==-1);
    memcpy(probe,saved,n);probe[0]='X';assert(m->desc->restore(m->ctx,probe,n)==-1);
    memcpy(probe,saved,n);assert(m->desc->restore(m->ctx,probe,n-1)==-1);
    /* Exercise payload checks with the publication guard actually removed.
     * Invalid data must not partly mutate the stopped instance. */
    assert(!command(h,"stop ui_list"));
    const struct {unsigned offset;uint32_t value;} corrupt[]={
        {12,UINT32_MAX},{12,17},{16,UINT32_MAX},{8,UINT32_MAX},{4,8},{40,2},{40,0x80000000}
    };
    for(unsigned i=0;i<sizeof corrupt/sizeof *corrupt;++i) {
        memcpy(probe,saved,n);put32(probe+corrupt[i].offset,corrupt[i].value);
        if(m->desc->restore(m->ctx,probe,n)!=-1) {
            fprintf(stderr,"invalid ui.list snapshot accepted: offset=%u value=%x\n",corrupt[i].offset,corrupt[i].value);
            return 1;
        }
        unsigned char after[64];assert(m->desc->save(m->ctx,after,sizeof after)==n && !memcmp(after,saved,n));
    }
    memcpy(probe,saved,n);probe[0]='X';assert(m->desc->restore(m->ctx,probe,n)==-1);
    assert(m->desc->restore(m->ctx,saved,n-1)==-1);
    /* A larger total is legitimate; only the live window is capacity-bounded. */
    memcpy(probe,saved,n);put32(probe+4,100);put32(probe+8,95);put32(probe+12,5);put32(probe+16,4);
    assert(!m->desc->restore(m->ctx,probe,n));
    memcpy(probe,saved,n);put32(probe+4,0);put32(probe+8,0);put32(probe+12,0);put32(probe+16,0);
    assert(!m->desc->restore(m->ctx,probe,n));
    assert(!m->desc->restore(m->ctx,saved,n));
    assert(!command(h,"start ui_list"));
    key(h,"list-key 523",1);sense(h,0,9,1,9,1,7,2,2,8,2);
    assert(!command(h,"reload ui_list"));sense(h,0,9,1,9,1,7,2,2,8,2);
    /* unload removes the service; commands report ENODEV. */
    assert(!command(h,"unload ui_list"));
    failure(h,"list-state",ENODEV);failure(h,"list-key 523",ENODEV);
    close(h->selfpipe[0]);close(h->selfpipe[1]);pthread_mutex_destroy(&h->post_mu);free(h);
    puts("ok list: CLI domains/validation, verified key chain, suspend/resume, shutdown gate, OAL1 reload and unload");
    return 0;
}
