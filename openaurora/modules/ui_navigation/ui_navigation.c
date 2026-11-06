/* Retained settings selection + enqueue-only route service. No display, input,
 * hardware, threads, callbacks or telemetry. Consumer dispatch is separate. */
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/launcher_route.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct state {
    uint32_t tab,open,count;
    int32_t item;
    uint32_t messages[OA_UI_NAVIGATION_CAPACITY];
};
struct ctx {
    const oa_host_api *api;
    struct state *state;
    int published,suspended;
    /* ABI 3 quiesce: the heap state is detached and freed on a worker. */
    pthread_t cleanup_thread;
    int cleanup_running;
    struct state *cleanup_state;
    unsigned cleanup_delay_ms;
    /* The host may retry reload after the first gate detached the live state;
     * keep the OAN2 handoff blob so save() still works before that retry. */
    unsigned char saved[16+4*OA_UI_NAVIGATION_CAPACITY];
    size_t saved_len;
};
static struct ctx *current;
static int available(void) { return current && current->published?0:ENODEV; }
static int writable(void)
{ int error=available();return error?error:current->suspended?EBUSY:0; }
static void copy_state(oa_ui_navigation_state *out)
{
    *out=(oa_ui_navigation_state){current->state->tab,current->state->open,
                                  current->state->count,current->state->item};
}
static int state(oa_ui_navigation_state *out)
{
    if(!out)return EINVAL;
    int error=available();if(error)return error;
    copy_state(out);return 0;
}
static int result(int32_t value)
{
    int error=writable();if(error)return error;
    struct oa_launcher_plan plan;
    error=oa_launcher_settings_plan(value,&plan);if(error)return error;
    if(current->state->count==OA_UI_NAVIGATION_CAPACITY)return ENOSPC;
    struct state next=*current->state;
    for(size_t i=0;i<plan.count;++i) {
        const struct oa_launcher_action *a=plan.actions+i;
        switch(a->kind) {
        case OA_LAUNCHER_SET_TAB:next.tab=(uint32_t)a->value;break;
        case OA_LAUNCHER_SET_ITEM:next.item=a->value;break;
        case OA_LAUNCHER_OPEN_SUBMENU:next.open=(uint32_t)a->value;break;
        case OA_LAUNCHER_PUSH_FRONT:
            if(next.count==OA_UI_NAVIGATION_CAPACITY)return ENOSPC;
            memmove(next.messages+1,next.messages,next.count*sizeof *next.messages);
            next.messages[0]=(uint32_t)a->value;++next.count;break;
        default:return EINVAL;
        }
    }
    *current->state=next;return 0;
}
static int operation(uint32_t value)
{ return result(oa_launcher_operation_result(value)); }
static int take(oa_ui_navigation_request *out)
{
    if(!out)return EINVAL;
    int error=writable();if(error)return error;
    if(!current->state->count)return EAGAIN;
    oa_ui_navigation_request request={0};request.navigation=current->state->messages[0];
    --current->state->count;
    memmove(current->state->messages,current->state->messages+1,
            current->state->count*sizeof *current->state->messages);
    copy_state(&request.selection);*out=request;return 0;
}
static int set_tab(int32_t value)
{ int error=writable();if(error)return error;current->state->tab=(uint32_t)value;return 0; }
static int set_item(int32_t value)
{ int error=writable();if(error)return error;current->state->item=value;return 0; }
static int set_open(uint8_t value)
{ int error=writable();if(error)return error;current->state->open=value;return 0; }
static const oa_ui_navigation_iface_v2 iface={{result,operation,state,take},set_tab,set_item,set_open};
_Static_assert(offsetof(oa_ui_navigation_iface_v2,base)==0,"v1 interface prefix");
static uint32_t get32(const unsigned char *p)
{ return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put32(unsigned char *p,uint32_t n)
{ for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(8*i)); }
static size_t state_save(const struct state *s,void *buf,size_t cap)
{
    size_t bytes=16+4*s->count;
    if(!buf || cap<bytes)return bytes;
    unsigned char *p=buf;memcpy(p,"OAN2",4);p[4]=(unsigned char)s->open;
    p[5]=(unsigned char)s->count;p[6]=p[7]=0;
    put32(p+8,s->tab);put32(p+12,(uint32_t)s->item);
    for(uint32_t i=0;i<s->count;++i)put32(p+16+4*i,s->messages[i]);
    return bytes;
}

static size_t m_save(void *v,void *buf,size_t cap)
{
    const struct ctx *c=v;
    if(c->state)return state_save(c->state,buf,cap);
    if(!c->saved_len)return 0;
    if(!buf || cap<c->saved_len)return c->saved_len;
    memcpy(buf,c->saved,c->saved_len);
    return c->saved_len;
}
static int state_restore(struct state *out,const unsigned char *p,size_t len)
{
    if(!p || len<16)return -1;
    int legacy=!memcmp(p,"OAN1",4);
    if((!legacy && memcmp(p,"OAN2",4)) || p[5]>OA_UI_NAVIGATION_CAPACITY ||
       p[6] || p[7] || len!=16u+4u*p[5] || (legacy && (p[4]>1 || get32(p+8)>4)))return -1;
    struct state next={0};next.open=p[4];next.count=p[5];next.tab=get32(p+8);
    uint32_t item=get32(p+12);
    next.item=(int32_t)((int64_t)item-(item&UINT32_C(0x80000000)?INT64_C(4294967296):0));
    for(uint32_t i=0;i<next.count;++i) {
        next.messages[i]=get32(p+16+4*i);
        if(next.messages[i]!=0x13)return -1;
    }
    *out=next;return 0;
}

static int m_restore(void *v,const void *buf,size_t len)
{
    struct ctx *c=v;
    if(c->published)return -1;
    if(!len)return 0;
    if(!c->state) {
        c->state=calloc(1,sizeof *c->state);
        if(!c->state)return -1;
    }
    if(state_restore(c->state,buf,len)) {
        free(c->state);c->state=NULL;
        return -1;
    }
    return 0;
}
static int m_init(const oa_host_api *api,void **out)
{
    if(!api || api->abi!=OA_ABI_VERSION || api->size<sizeof *api)return -1;
    struct ctx *c=calloc(1,sizeof *c);if(!c)return -1;
    c->state=calloc(1,sizeof *c->state);
    if(!c->state){free(c);return -1;}
    /* DemoApp ctor: tabfd8=0, itemfd0=0, open1259=0. */
    c->api=api;current=c;*out=c;return 0;
}
/* No host API calls after this returns. */
static void *cleanup_worker(void *v)
{
    struct ctx *c=v;
    if(c->cleanup_delay_ms) {
        const struct timespec pause={
            .tv_sec=c->cleanup_delay_ms/1000u,
            .tv_nsec=(long)(c->cleanup_delay_ms%1000u)*1000000L
        };
        (void)nanosleep(&pause,NULL);
    }
    struct state *state=c->cleanup_state;
    c->cleanup_state=NULL;
    free(state);
    return NULL;
}

static int m_start(void *v)
{
    struct ctx *c=v;
    if(c->published || c->cleanup_running) return -EBUSY;
    c->cleanup_running=0;
    long delay=atol(c->api->config_get(c->api->priv,
        "ui_navigation.cleanup_delay_ms","0"));
    c->cleanup_delay_ms=delay>0?(unsigned)delay:0;
    if(!c->state) {
        c->state=calloc(1,sizeof *c->state);
        if(!c->state)return -1;
        if(c->saved_len) (void)state_restore(c->state,c->saved,c->saved_len);
    }
    int rc=c->api->publish(c->api->priv,OA_SVC_UI_NAVIGATION,2,&iface.base);
    if(rc)return rc;
    c->published=1;c->suspended=0;return 0;
}

/* ABI 3 gate: unpublish ui.navigation synchronously and detach the heap
 * state, then free it synchronously or on a worker after
 * ui_navigation.cleanup_delay_ms. */
static int m_quiesce(void *v)
{
    struct ctx *c=v;
    if(!c)return 0;
    if(c->cleanup_running) {
        for(int i=0;i<10;i++) {
            int rc=pthread_tryjoin_np(c->cleanup_thread,NULL);
            if(rc==0){c->cleanup_running=0;return 0;}
            if(rc!=EBUSY){c->cleanup_running=0;return -EIO;}
            const struct timespec pause={.tv_nsec=5000000};
            (void)nanosleep(&pause,NULL);
        }
        return -EAGAIN;
    }
    int published=c->published;
    c->published=0;
    if(published)c->api->unpublish(c->api->priv,OA_SVC_UI_NAVIGATION);
    struct state *state=c->state;
    c->state=NULL;
    if(!state)return 0;
    c->saved_len=state_save(state,c->saved,sizeof c->saved);
    if(!c->cleanup_delay_ms){free(state);return 0;}
    c->cleanup_state=state;
    int rc=pthread_create(&c->cleanup_thread,NULL,cleanup_worker,c);
    if(rc){c->cleanup_state=NULL;free(state);return -EIO;}
    c->cleanup_running=1;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c=v;
    int published=c->published;c->published=0;
    if(published)c->api->unpublish(c->api->priv,OA_SVC_UI_NAVIGATION);
    if(c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread,NULL);
        c->cleanup_running=0;
    }
    free(c->cleanup_state);c->cleanup_state=NULL;
    free(c->state);c->state=NULL;
}

static void m_destroy(void *v)
{
    struct ctx *c=v;
    if(c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread,NULL);
        c->cleanup_running=0;
    }
    free(c->cleanup_state);
    free(c->state);
    if(current==c)current=NULL;
    free(c);
}

static int m_suspend(void *v) { ((struct ctx *)v)->suspended=1;return 0; }
static int m_resume(void *v) { ((struct ctx *)v)->suspended=0;return 0; }
static const oa_module module={
    .magic=OA_MODULE_MAGIC,.abi=OA_ABI_VERSION,.size=sizeof(oa_module),
    .name="ui_navigation",.version="0.2",.init=m_init,.start=m_start,.stop=m_stop,
    .destroy=m_destroy,.suspend=m_suspend,.resume=m_resume,.save=m_save,.restore=m_restore,
    .quiesce=m_quiesce,
};
OA_MODULE_EXPORT(module)
