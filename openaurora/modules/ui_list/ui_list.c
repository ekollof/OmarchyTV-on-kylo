/* Owned SVG-list selection service. No display, input, hardware, threads,
 * asynchronous callbacks or telemetry. Drives the firmware-verified key/body/move/row
 * layers over owned state words; animation/scrollbar/full-update targets and
 * the +0x48 group virtual are owned counting stubs (construction/rendering/
 * animation internals stay open). */
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/svg_list_input.h"
#include "openaurora/svg_list_bodies.h"
#include "openaurora/svg_list_move.h"
#include "openaurora/svg_list_rows.h"
#include "openaurora/svg_list_impl.h"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Owned widget fields: vertical layout with non-colliding page aliases.
 * Per-widget key fields come from widget instances the module does not own;
 * these fixed values only route the verified router. Select0x20e/down0x20b/
 * up0x20a/first0x2db/last0x2dc are the router's fixed codes. */
#define LIST_PAGE_DOWN_KEY UINT32_C(0x20c)
#define LIST_PAGE_UP_KEY UINT32_C(0x20d)
#define LIST_SLOTS UINT32_C(16)
#define LIST_SAVE_MAGIC "OAL1"
#define LIST_SAVE_WORDS 10u
#define LIST_SAVE_BYTES (4u+4u*LIST_SAVE_WORDS)

struct words { uint32_t aux,first,size,selected; };
struct state {
    struct words w;
    void *handles[16];
    int handled; /* -1 before any key */
    uint32_t anim,scroll,partial,focus,group,group_target;
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
     * keep the OAL1 handoff blob so save() still works before that retry. */
    unsigned char saved[LIST_SAVE_BYTES];
    int have_saved;
};
static struct ctx *current;
static void state_defaults(struct state *s);

static int available(void) { return current && current->published?0:ENODEV; }
static int writable(void)
{ int error=available();return error?error:current->suspended?EBUSY:0; }

/* ---- verified-layer callbacks (context is struct ctx *) ---- */
static uint32_t box_aux(void *p) { return ((struct ctx *)p)->state->w.aux; }
static uint32_t box_size(void *p) { return ((struct ctx *)p)->state->w.size; }
static uint32_t box_row(void *p) { return ((struct ctx *)p)->state->w.selected; }
static uint32_t box_item(void *p)
{ struct ctx *c=p;return c->state->w.first+c->state->w.selected; }
static uint32_t box_first(void *p) { return ((struct ctx *)p)->state->w.first; }
static uint32_t box_slots(void *p) { (void)p;return LIST_SLOTS; }
static void *box_row_at(void *p,uint32_t rel)
{ struct ctx *c=p;return rel<16?c->state->handles[rel]:NULL; }
static void box_row_focus(void *row,int focused) { (void)row;(void)focused;++current->state->focus; }
static void box_store_selected(void *p,uint32_t rel) { ((struct ctx *)p)->state->w.selected=rel; }
static void box_update_impl(void *p,uint32_t index,int full)
{ (void)p;(void)index;(void)full;++current->state->partial; }
static struct oa_svg_list_manager_ops manager_ops(struct ctx *c)
{
    return (struct oa_svg_list_manager_ops){c,box_first,box_size,box_row,box_row_at,
        box_row_focus,box_store_selected,box_update_impl,box_slots};
}
static void box_focus_item(void *p,uint32_t index)
{
    struct ctx *c=p;struct oa_svg_list_manager_ops ops=manager_ops(c);
    (void)oa_svg_list_manager_focus_item(&ops,index);
}
static uint32_t move_aux(void *p) { return box_aux(p); }
static uint32_t move_size(void *p) { return box_size(p); }
static uint32_t move_item(void *p) { return box_item(p); }
static uint32_t move_first(void *p) { return box_first(p); }
static void move_focus(void *p,uint32_t index) { box_focus_item(p,index); }
/* Owned inert render/owner callbacks (construction/rendering/animation and
 * the full-update draw targets stay open). updatePartial runs the verified
 * updateImpl so window moves reposition first/size through its writeback. */
static int impl_row_visible(void *row) { (void)row;return 1; }
static void impl_row_focus(void *row,int focused) { (void)row;(void)focused; }
static void impl_row_show(void *row,int visible) { (void)row;(void)visible; }
static void impl_row_rect(void *row,uint32_t rect[4])
{ (void)row;rect[0]=rect[1]=rect[2]=rect[3]=0; }
static void impl_row_position(void *row,uint32_t x,uint32_t y) { (void)row;(void)x;(void)y; }
static void *impl_owner(void *p) { return p; }
static void impl_store_first(void *p,uint32_t v) { ((struct ctx *)p)->state->w.first=v; }
static void impl_store_size(void *p,uint32_t v) { ((struct ctx *)p)->state->w.size=v; }
static void impl_store_selected(void *p,uint32_t v) { ((struct ctx *)p)->state->w.selected=v; }
static void impl_store_row(void *p,uint32_t rel,void *row)
{ struct ctx *c=p;if(rel<16)c->state->handles[rel]=row; }
static int impl_empty(void *p,void *owner) { (void)p;(void)owner;return 0; }
static void impl_draw(void *p,void *owner,uint32_t index,void *row)
{ (void)p;(void)owner;(void)index;(void)row; }
static void impl_draw_empty(void *p,void *owner,void *row) { (void)p;(void)owner;(void)row; }
static int impl_got_focus(void *p,void *owner) { (void)p;(void)owner;return 0; }
static uint32_t impl_cap(void *p) { return ((struct ctx *)p)->state->w.aux; }
static uint32_t impl_first(void *p) { return box_first(p); }
static uint32_t impl_size(void *p) { return box_size(p); }
static uint32_t impl_sel(void *p) { return box_row(p); }
static struct oa_svg_list_impl_ops impl_ops(struct ctx *c)
{
    return (struct oa_svg_list_impl_ops){c,impl_cap,impl_first,impl_size,impl_sel,box_slots,
        box_row_at,impl_store_row,impl_owner,impl_store_first,impl_store_size,impl_store_selected,
        impl_row_visible,impl_row_focus,impl_row_show,impl_row_rect,impl_row_position,
        impl_empty,impl_draw,impl_draw_empty,impl_got_focus};
}
static void move_partial(void *p,uint32_t index)
{
    struct ctx *c=p;++c->state->partial;
    struct oa_svg_list_impl_ops ops=impl_ops(c);
    (void)oa_svg_list_manager_update_impl(&ops,index,0);
}
static void move_scrollbar(void *p) { ++((struct ctx *)p)->state->scroll; }
static void move_animate(void *p) { ++((struct ctx *)p)->state->anim; }
static struct oa_svg_list_move_ops move_ops(struct ctx *c)
{
    return (struct oa_svg_list_move_ops){c,move_aux,move_size,move_item,move_first,
        move_focus,move_partial,move_scrollbar,move_animate};
}
static void box_move_selection_up(void *p,uint32_t n)
{ struct ctx *c=p;struct oa_svg_list_move_ops ops=move_ops(c);
    (void)oa_svg_list_move_selection_up(&ops,n); }
static void box_move_selection_down(void *p,uint32_t n)
{ struct ctx *c=p;struct oa_svg_list_move_ops ops=move_ops(c);
    (void)oa_svg_list_move_selection_down(&ops,n); }
static void box_move_window_up(void *p,uint32_t n)
{ struct ctx *c=p;struct oa_svg_list_move_ops ops=move_ops(c);
    (void)oa_svg_list_move_window_up(&ops,n); }
static void box_move_window_down(void *p,uint32_t n)
{ struct ctx *c=p;struct oa_svg_list_move_ops ops=move_ops(c);
    (void)oa_svg_list_move_window_down(&ops,n); }
static void box_move_window_top(void *p)
{ struct ctx *c=p;struct oa_svg_list_move_ops ops=move_ops(c);
    (void)oa_svg_list_move_window_top(&ops); }
static void box_move_window_bottom(void *p)
{ struct ctx *c=p;struct oa_svg_list_move_ops ops=move_ops(c);
    (void)oa_svg_list_move_window_bottom(&ops); }
static void box_selection_to_top(void *p)
{ struct ctx *c=p;struct oa_svg_list_move_ops ops=move_ops(c);
    (void)oa_svg_list_move_selection_to_top(&ops); }
static void box_selection_to_bottom(void *p)
{ struct ctx *c=p;struct oa_svg_list_move_ops ops=move_ops(c);
    (void)oa_svg_list_move_selection_to_bottom(&ops); }
static void box_animate(void *p) { move_animate(p); }
static void *box_parent(void *p) { (void)p;return NULL; }
static uint32_t box_item_id(void *p) { return box_item(p); }
static uint32_t box_window_id(void *p) { (void)p;return UINT32_C(0); }
static uint32_t box_notify(void *p,void *parent,uint32_t item,uint32_t wid)
{ (void)p;(void)parent;(void)item;(void)wid;return 0; }
/* Owned +0x48 group-target behavior (the group virtual stays open): go to
 * first group selects item0; go to last group selects aux-1 clamped into
 * the window with wrapping arithmetic. Records the synthesized index. */
static void box_go_group(void *p,uint32_t index)
{
    struct ctx *c=p;++c->state->group;c->state->group_target=index;
    if(index==0) { c->state->w.first=0;c->state->w.selected=0; }
    else if(index==c->state->w.aux-1u && c->state->w.aux!=0) {
        uint32_t size=c->state->w.size;
        c->state->w.first=c->state->w.aux-size;c->state->w.selected=size-1u;
    }
}
static struct oa_svg_list_box_ops box_ops(struct ctx *c)
{
    return (struct oa_svg_list_box_ops){c,box_aux,box_size,box_row,box_item,box_first,
        box_focus_item,box_move_selection_up,box_move_selection_down,box_move_window_up,
        box_move_window_down,box_move_window_top,box_move_window_bottom,box_selection_to_top,
        box_selection_to_bottom,box_animate,box_parent,box_item_id,box_window_id,box_notify,
        box_go_group};
}
static uint32_t router_query(void *p,enum oa_svg_list_query query)
{
    (void)p;
    switch(query) {
    case OA_SVG_LIST_HORIZONTAL:return 0;
    case OA_SVG_LIST_PAGE_DOWN_KEY:return LIST_PAGE_DOWN_KEY;
    case OA_SVG_LIST_PAGE_UP_KEY:return LIST_PAGE_UP_KEY;
    }
    return 0;
}
static uint32_t router_run(void *p,enum oa_svg_list_action action,const void *message)
{
    struct ctx *c=p;struct oa_svg_list_box_ops ops=box_ops(c);
    int handled=0;
    switch(action) {
    case OA_SVG_LIST_SELECT:(void)oa_svg_list_select_body(&ops,&handled);break;
    case OA_SVG_LIST_DOWN:(void)oa_svg_list_down_body(&ops,&handled);break;
    case OA_SVG_LIST_UP:(void)oa_svg_list_up_body(&ops,&handled);break;
    case OA_SVG_LIST_PAGE_DOWN:(void)oa_svg_list_page_down_body(&ops,&handled);break;
    case OA_SVG_LIST_PAGE_UP:(void)oa_svg_list_page_up_body(&ops,&handled);break;
    case OA_SVG_LIST_FIRST:(void)oa_svg_list_first_body(&ops,&handled);break;
    case OA_SVG_LIST_LAST:(void)oa_svg_list_last_body(&ops,&handled);break;
    case OA_SVG_LIST_WINDOW:handled=0;break;
    }
    (void)message;return (uint32_t)(handled!=0);
}

/* ---- service ---- */
static int list_state(oa_ui_list_state *out)
{
    if(!out)return EINVAL;
    int error=available();if(error)return error;
    const struct state *s=current->state;
    *out=(oa_ui_list_state){s->w.first,s->w.size,s->w.selected,s->w.aux,
        s->handled,s->anim,s->scroll,s->partial,s->focus,s->group};
    return 0;
}
static int list_key(uint32_t code,int *handled)
{
    if(!handled)return EINVAL;
    int error=writable();if(error)return error;
    struct ctx *c=current;
    struct oa_svg_list_ops ops={c,router_query,router_run,NULL};
    int h=0;error=oa_svg_list_key(&ops,code,&h);if(error)return error;
    c->state->handled=h;*handled=h;return 0;
}
static const oa_ui_list_iface iface={list_state,list_key};
static uint32_t get32(const unsigned char *p)
{ return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put32(unsigned char *p,uint32_t n)
{ for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(8*i)); }
static size_t state_save(const struct state *s,void *buf,size_t cap)
{
    size_t bytes=LIST_SAVE_BYTES;
    if(!buf || cap<bytes)return bytes;
    unsigned char *p=buf;memcpy(p,LIST_SAVE_MAGIC,4);
    put32(p+4,s->w.aux);put32(p+8,s->w.first);put32(p+12,s->w.size);
    put32(p+16,s->w.selected);put32(p+20,s->anim);put32(p+24,s->scroll);
    put32(p+28,s->partial);put32(p+32,s->focus);put32(p+36,s->group);
    put32(p+40,(uint32_t)s->handled);
    return bytes;
}

static size_t m_save(void *v,void *buf,size_t cap)
{
    const struct ctx *c=v;
    if(c->state)return state_save(c->state,buf,cap);
    if(!c->have_saved)return 0;
    if(!buf || cap<sizeof c->saved)return sizeof c->saved;
    memcpy(buf,c->saved,sizeof c->saved);
    return sizeof c->saved;
}
static int m_restore(void *v,const void *buf,size_t len)
{
    struct ctx *c=v;const unsigned char *p=buf;
    if(c->published)return -1;
    if(!len)return 0;
    if(!p || len!=4+4*LIST_SAVE_WORDS || memcmp(p,LIST_SAVE_MAGIC,4))return -1;
    struct words w={get32(p+4),get32(p+8),get32(p+12),get32(p+16)};
    uint32_t handled=get32(p+40);
    /* OAL1 is our owned state format, not a vendor ABI. Reject bad geometry
     * before mutating anything or allowing callback loops to trust its size.
     * Total count may exceed the visible handle capacity (virtualized lists). */
    if(w.size>LIST_SLOTS || w.first>w.aux || w.size>w.aux-w.first ||
       (w.aux?(!w.size || w.selected>=w.size):w.selected!=0) ||
       (handled!=UINT32_MAX && handled!=0 && handled!=1))return -1;
    if(!c->state) {
        c->state=calloc(1,sizeof *c->state);
        if(!c->state)return -1;
        state_defaults(c->state);
    }
    c->state->w=w;c->state->anim=get32(p+20);c->state->scroll=get32(p+24);
    c->state->partial=get32(p+28);c->state->focus=get32(p+32);c->state->group=get32(p+36);
    c->state->handled=handled==UINT32_MAX?-1:(int)handled;
    c->state->group_target=0;return 0;
}
static void state_defaults(struct state *s)
{
    memset(s,0,sizeof *s);
    /* Owned fixture geometry (not firmware): 9-item list, 5-row window. */
    s->w.aux=9;s->w.size=5;s->handled=-1;
    for(unsigned i=0;i<16;++i)s->handles[i]=(void *)(uintptr_t)(0x1000u+i);
}

static int m_init(const oa_host_api *api,void **out)
{
    if(!api || api->abi!=OA_ABI_VERSION || api->size<sizeof *api)return -1;
    struct ctx *c=calloc(1,sizeof *c);if(!c)return -1;
    c->state=calloc(1,sizeof *c->state);
    if(!c->state){free(c);return -1;}
    state_defaults(c->state);
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
        "ui_list.cleanup_delay_ms","0"));
    c->cleanup_delay_ms=delay>0?(unsigned)delay:0;
    if(!c->state) {
        /* A failed start/resume quiesces after the publish collision. Rebuild
         * the OAL1 snapshot on the retry instead of silently starting empty. */
        int restored=c->have_saved && m_restore(c,c->saved,sizeof c->saved)==0;
        if(!restored) {
            c->state=calloc(1,sizeof *c->state);
            if(!c->state)return -1;
            state_defaults(c->state);
        }
    }
    int rc=c->api->publish(c->api->priv,OA_SVC_UI_LIST,1,&iface);
    if(rc)return rc;
    c->published=1;c->suspended=0;return 0;
}

/* ABI 3 gate: unpublish ui.list synchronously and detach the heap state, then
 * free it synchronously or on a worker after ui_list.cleanup_delay_ms. */
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
    if(published)c->api->unpublish(c->api->priv,OA_SVC_UI_LIST);
    struct state *state=c->state;
    c->state=NULL;
    if(!state)return 0;
    state_save(state,c->saved,sizeof c->saved);
    c->have_saved=1;
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
    if(published)c->api->unpublish(c->api->priv,OA_SVC_UI_LIST);
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
    .name="ui_list",.version="0.1",.init=m_init,.start=m_start,.stop=m_stop,
    .destroy=m_destroy,.suspend=m_suspend,.resume=m_resume,.save=m_save,.restore=m_restore,
    .quiesce=m_quiesce,
};
OA_MODULE_EXPORT(module)
