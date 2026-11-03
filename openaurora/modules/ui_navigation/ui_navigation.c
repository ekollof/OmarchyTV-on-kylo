/* Retained settings selection + enqueue-only route service. No display, input,
 * hardware, threads, callbacks or telemetry. Consumer dispatch is separate. */
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/launcher_route.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct state {
    uint32_t tab,open,count;
    int32_t item;
    uint32_t messages[OA_UI_NAVIGATION_CAPACITY];
};
struct ctx { const oa_host_api *api; struct state state; int published,suspended; };
static struct ctx *current;
static int available(void) { return current && current->published?0:ENODEV; }
static void copy_state(oa_ui_navigation_state *out)
{
    *out=(oa_ui_navigation_state){current->state.tab,current->state.open,
                                  current->state.count,current->state.item};
}
static int state(oa_ui_navigation_state *out)
{
    if(!out)return EINVAL;
    int error=available();if(error)return error;
    copy_state(out);return 0;
}
static int result(int32_t value)
{
    int error=available();if(error)return error;
    if(current->suspended)return EBUSY;
    struct oa_launcher_plan plan;
    error=oa_launcher_settings_plan(value,&plan);if(error)return error;
    if(current->state.count==OA_UI_NAVIGATION_CAPACITY)return ENOSPC;
    struct state next=current->state;
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
    current->state=next;return 0;
}
static int operation(uint32_t value)
{ return result(oa_launcher_operation_result(value)); }
static int take(oa_ui_navigation_request *out)
{
    if(!out)return EINVAL;
    int error=available();if(error)return error;
    if(current->suspended)return EBUSY;
    if(!current->state.count)return EAGAIN;
    oa_ui_navigation_request request={0};request.navigation=current->state.messages[0];
    --current->state.count;
    memmove(current->state.messages,current->state.messages+1,
            current->state.count*sizeof *current->state.messages);
    copy_state(&request.selection);*out=request;return 0;
}
static const oa_ui_navigation_iface iface={result,operation,state,take};
static uint32_t get32(const unsigned char *p)
{ return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put32(unsigned char *p,uint32_t n)
{ for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(8*i)); }
static size_t m_save(void *v,void *buf,size_t cap)
{
    const struct ctx *c=v;size_t bytes=16+4*c->state.count;
    if(!buf || cap<bytes)return bytes;
    unsigned char *p=buf;memcpy(p,"OAN1",4);p[4]=(unsigned char)c->state.open;
    p[5]=(unsigned char)c->state.count;p[6]=p[7]=0;
    put32(p+8,c->state.tab);put32(p+12,(uint32_t)c->state.item);
    for(uint32_t i=0;i<c->state.count;++i)put32(p+16+4*i,c->state.messages[i]);
    return bytes;
}
static int m_restore(void *v,const void *buf,size_t len)
{
    struct ctx *c=v;const unsigned char *p=buf;
    if(c->published)return -1;
    if(!len)return 0;
    if(!p || len<16 || memcmp(p,"OAN1",4) || p[4]>1 || p[5]>OA_UI_NAVIGATION_CAPACITY ||
       p[6] || p[7] || get32(p+8)>4 || len!=16u+4u*p[5])return -1;
    struct state next={0};next.open=p[4];next.count=p[5];next.tab=get32(p+8);
    uint32_t item=get32(p+12);
    next.item=(int32_t)((int64_t)item-(item&UINT32_C(0x80000000)?INT64_C(4294967296):0));
    for(uint32_t i=0;i<next.count;++i) {
        next.messages[i]=get32(p+16+4*i);
        if(next.messages[i]!=0x13)return -1;
    }
    c->state=next;return 0;
}
static int m_init(const oa_host_api *api,void **out)
{
    if(!api || api->abi!=OA_ABI_VERSION || api->size<sizeof *api)return -1;
    struct ctx *c=calloc(1,sizeof *c);if(!c)return -1;
    /* DemoApp ctor: tabfd8=0, itemfd0=0, open1259=0. */
    c->api=api;current=c;*out=c;return 0;
}
static int m_start(void *v)
{
    struct ctx *c=v;int rc=c->api->publish(c->api->priv,OA_SVC_UI_NAVIGATION,1,&iface);
    if(rc)return rc;
    c->published=1;c->suspended=0;return 0;
}
static void m_stop(void *v)
{
    struct ctx *c=v;int published=c->published;c->published=0;
    if(published)c->api->unpublish(c->api->priv,OA_SVC_UI_NAVIGATION);
}
static void m_destroy(void *v)
{ struct ctx *c=v;if(current==c)current=NULL;free(c); }
static int m_suspend(void *v) { ((struct ctx *)v)->suspended=1;return 0; }
static int m_resume(void *v) { ((struct ctx *)v)->suspended=0;return 0; }
static const oa_module module={
    .magic=OA_MODULE_MAGIC,.abi=OA_ABI_VERSION,.size=sizeof(oa_module),
    .name="ui_navigation",.version="0.1",.init=m_init,.start=m_start,.stop=m_stop,
    .destroy=m_destroy,.suspend=m_suspend,.resume=m_resume,.save=m_save,.restore=m_restore,
};
OA_MODULE_EXPORT(module)
