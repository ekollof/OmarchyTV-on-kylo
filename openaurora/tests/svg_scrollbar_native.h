#ifndef OA_TESTS_SVG_SCROLLBAR_NATIVE_H
#define OA_TESTS_SVG_SCROLLBAR_NATIVE_H

#include "openaurora/svg_scrollbar.h"

/*
 * Differential for SVGScrollBar::calculateBarSizeAndPosition (0x10c3af0,
 * 0xbc bytes, CRC 0x37806af8) and setScrollPosition (0x10c4138, 0x70 bytes,
 * CRC 0x71cda0c0), both Thumb.
 *
 * The geometry body calls UIWindow::getWindowRect through vtable slot 0x28
 * (0x1152649); the fake object's vtable slot 10 writes {left,top,right,bottom}.
 * The division helper at 0x1285478 is stubbed with the same signed C division.
 * setScrollPosition's isCreated/renderBackground/renderBar calls are stubbed
 * and traced; the owned API takes the created result as an argument.
 */
static uint32_t sbg_guard=UINT32_C(0x4a71c3e5);
static int32_t sbg_rect[4];
static unsigned sbg_rect_calls;
struct sbg_ev { uint32_t kind,value; };
static struct sbg_ev sbg_evn[32],sbg_evc[32];
static struct sbg_ev *sbg_tp;static unsigned sbg_tn;
static int sbg_created;

static void sbg_emit(uint32_t kind,uint32_t value)
{
    assert(sbg_tn<32);
    sbg_tp[sbg_tn].kind=kind;sbg_tp[sbg_tn].value=value;++sbg_tn;
}
static int32_t sbg_div(int32_t numerator,int32_t divisor)
{
    assert(divisor!=0);
    return numerator/divisor;
}
static void sbg_get_rect(void *out,uint32_t self)
{
    (void)self;
    ++sbg_rect_calls;
    ((int32_t *)out)[0]=sbg_rect[0];
    ((int32_t *)out)[1]=sbg_rect[1];
    ((int32_t *)out)[2]=sbg_rect[2];
    ((int32_t *)out)[3]=sbg_rect[3];
}
typedef int32_t (*sbg_div_fn)(int32_t,int32_t);
static void sbg_div_stub(void *mapped,uint32_t base,uint32_t address,sbg_div_fn fn)
{
    unsigned char *p=stub_destination(mapped,base,address,8);
    const uint16_t insn[]={0x4b00,0x4718};
    assert(!(address&3));memcpy(p,insn,sizeof insn);
    uint32_t target;assert(sizeof fn==sizeof target);memcpy(&target,&fn,4);
    memcpy(p+4,&target,4);
}
static unsigned sbg_created_stub(void *p)
{ (void)p;sbg_emit(1,(uint32_t)sbg_created);return (unsigned)sbg_created; }
static void sbg_bg_stub(void *p) { (void)p;sbg_emit(2,0); }
static void sbg_bar_stub(void *p) { (void)p;sbg_emit(3,0); }
static void svc_bg(struct oa_svg_scrollbar_state *s) { (void)s;sbg_emit(2,0); }
static void svc_bar(struct oa_svg_scrollbar_state *s) { (void)s;sbg_emit(3,0); }
static struct oa_svg_scrollbar_ops sbg_ops(void)
{ return (struct oa_svg_scrollbar_ops){svc_bg,svc_bar}; }

static void sbg_read_state(const unsigned char *obj,struct oa_svg_scrollbar_state *s)
{
    memcpy(&s->bar_pos,obj+0xb0,4);
    memcpy(&s->bar_size,obj+0xb4,4);
    memcpy(&s->min_bar,obj+0xb8,4);
    memcpy(&s->total,obj+0xbc,4);
    memcpy(&s->visible,obj+0xc0,4);
    memcpy(&s->first,obj+0xc4,4);
    s->background_hidden=obj[0xc8];
}
static void sbg_write_state(unsigned char *obj,const struct oa_svg_scrollbar_state *s)
{
    memcpy(obj+0xb0,&s->bar_pos,4);
    memcpy(obj+0xb4,&s->bar_size,4);
    memcpy(obj+0xb8,&s->min_bar,4);
    memcpy(obj+0xbc,&s->total,4);
    memcpy(obj+0xc0,&s->visible,4);
    memcpy(obj+0xc4,&s->first,4);
    obj[0xc8]=s->background_hidden;
}

static void check_svg_scrollbar_geom(const unsigned char *elf,size_t bytes)
{
    unsigned char *calc=page(0x10c3000,4096),*setp=page(0x10c4000,4096);
    unsigned char *divp=page(0x1285000,4096),*win=page(0x1152000,4096);
    static uint32_t vtable[16];
    static const uint32_t guardp_dummy=0;
    size_t o;
    (void)guardp_dummy;
    o=file_offset(elf,bytes,0x10c3af0,0xbc);
    assert(crc32(elf+o,0xbc)==UINT32_C(0x37806af8));
    memcpy(stub_destination(calc,0x10c3000,0x10c3af0,0xbc),elf+o,0xbc);
    { uint32_t gp=(uint32_t)(uintptr_t)&sbg_guard;
      memcpy(stub_destination(calc,0x10c3000,0x10c3ba8,4),&gp,4); }
    o=file_offset(elf,bytes,0x10c4138,0x70);
    assert(crc32(elf+o,0x70)==UINT32_C(0x71cda0c0));
    memcpy(stub_destination(setp,0x10c4000,0x10c4138,0x70),elf+o,0x70);
    { uint32_t gp=(uint32_t)(uintptr_t)&sbg_guard;
      memcpy(stub_destination(setp,0x10c4000,0x10c41a4,4),&gp,4); }
    sbg_div_stub(divp,0x1285000,0x1285478,sbg_div);
    thumb_stub(win,0x1152000,0x1152a40,sbg_created_stub);
    blank_stub(calc,0x10c3000,0x10c3658,sbg_bg_stub);
    blank_stub(calc,0x10c3000,0x10c3bac,sbg_bar_stub);
    vtable[10]=(uint32_t)(uintptr_t)sbg_get_rect;
    executable(calc,4096);executable(setp,4096);executable(divp,4096);
    executable(win,4096);
    unsigned char *obj=calloc(1,0x100),*before=calloc(1,0x100);
    assert(obj && before);
    void (*calc_body)(void *);
    { uint32_t entry=0x10c3af1;memcpy(&calc_body,&entry,4); }
    int (*set_body)(void *,int32_t,int32_t,int32_t);
    { uint32_t entry=0x10c4139;memcpy(&set_body,&entry,4); }
    unsigned compared=0;
    static const int32_t totals[]={-5,0,1,10,20,100};
    static const int32_t visibles[]={-10,0,1,10,20};
    static const int32_t firsts[]={-5,0,1,5,10,15};
    static const int32_t mins[]={0,1,50,1000};
    static const int32_t heights[]={0,1,720,1080};
    for(unsigned a=0;a<sizeof totals/sizeof *totals;++a)
    for(unsigned b=0;b<sizeof visibles/sizeof *visibles;++b)
    for(unsigned c=0;c<sizeof firsts/sizeof *firsts;++c)
    for(unsigned d=0;d<sizeof mins/sizeof *mins;++d)
    for(unsigned e=0;e<sizeof heights/sizeof *heights;++e) {
        struct oa_svg_scrollbar_state state={
            .bar_pos=INT32_C(0x11110000)+(int32_t)a,
            .bar_size=INT32_C(0x22220000)+(int32_t)b,
            .min_bar=mins[d],.total=totals[a],.visible=visibles[b],
            .first=firsts[c],.background_hidden=(uint8_t)((a+b)&1)};
        memset(obj,0xa5,0x100);
        *(uint32_t *)obj=(uint32_t)(uintptr_t)vtable;
        sbg_write_state(obj,&state);
        memcpy(before,obj,0x100);
        sbg_rect[0]=1750;sbg_rect[1]=180;sbg_rect[2]=1758;
        sbg_rect[3]=180+heights[e];
        sbg_rect_calls=0;
        calc_body(obj);
        struct oa_svg_scrollbar_state native;
        sbg_read_state(obj,&native);
        assert(sbg_rect_calls==1);
        oa_svg_scrollbar_calculate(&state,heights[e]);
        if(native.bar_pos!=state.bar_pos || native.bar_size!=state.bar_size ||
           native.min_bar!=state.min_bar || native.total!=state.total ||
           native.visible!=state.visible || native.first!=state.first ||
           native.background_hidden!=state.background_hidden) {
            fprintf(stderr,"scrollbar calc mismatch t=%d v=%d f=%d m=%d h=%d nat=%d/%d ours=%d/%d\n",
                totals[a],visibles[b],firsts[c],mins[d],heights[e],
                native.bar_pos,native.bar_size,state.bar_pos,state.bar_size);
            exit(1);
        }
        for(unsigned i=0;i<0x100;++i)
            if((i<0xb0 || i>0xc8) && obj[i]!=before[i]) {
                fprintf(stderr,"scrollbar calc wrote outside fields at %x\n",i);
                exit(1);
            }
        ++compared;
    }
    static const int32_t st_totals[]={0,1,10,20};
    static const int32_t st_visibles[]={0,10,20};
    static const int32_t st_firsts[]={-1,0,5,10,21};
    for(unsigned cr=0;cr<2;++cr)
    for(unsigned a=0;a<sizeof st_totals/sizeof *st_totals;++a)
    for(unsigned b=0;b<sizeof st_visibles/sizeof *st_visibles;++b)
    for(unsigned c=0;c<sizeof st_firsts/sizeof *st_firsts;++c)
    for(unsigned bg=0;bg<2;++bg) {
        struct oa_svg_scrollbar_state state={
            .bar_pos=0x33,.bar_size=0x44,.min_bar=0x55,
            .total=7,.visible=8,.first=9,
            .background_hidden=(uint8_t)bg};
        int created=(int)cr;
        memset(obj,0xa5,0x100);
        *(uint32_t *)obj=(uint32_t)(uintptr_t)vtable;
        sbg_write_state(obj,&state);
        sbg_created=created;
        sbg_tp=sbg_evn;sbg_tn=0;
        int native_ret=set_body(obj,st_totals[a],st_visibles[b],st_firsts[c]);
        uint32_t native_created=0,native_bg=0,native_bar=0;
        for(unsigned i=0;i<sbg_tn;++i) {
            if(sbg_tp[i].kind==1)native_created=sbg_tp[i].value;
            else if(sbg_tp[i].kind==2)native_bg=1;
            else if(sbg_tp[i].kind==3)native_bar=1;
        }
        struct oa_svg_scrollbar_state native;
        sbg_read_state(obj,&native);
        struct oa_svg_scrollbar_ops ops=sbg_ops();
        sbg_tp=sbg_evc;sbg_tn=0;
        int ours_ret=oa_svg_scrollbar_set_position(&state,st_totals[a],
            st_visibles[b],st_firsts[c],created,&ops);
        uint32_t ours_bg=0,ours_bar=0;
        for(unsigned i=0;i<sbg_tn;++i) {
            if(sbg_tp[i].kind==2)ours_bg=1;
            else if(sbg_tp[i].kind==3)ours_bar=1;
        }
        if(native_ret!=ours_ret || native_created!=(uint32_t)created ||
           native_bg!=ours_bg || native_bar!=ours_bar ||
           native.bar_pos!=state.bar_pos || native.bar_size!=state.bar_size ||
           native.min_bar!=state.min_bar || native.total!=state.total ||
           native.visible!=state.visible || native.first!=state.first ||
           native.background_hidden!=state.background_hidden) {
            fprintf(stderr,"scrollbar setpos mismatch cr=%d t=%d v=%d f=%d bg=%d ret=%d/%d\n",
                created,st_totals[a],st_visibles[b],st_firsts[c],bg,
                native_ret,ours_ret);
            exit(1);
        }
        ++compared;
    }
    free(obj);free(before);
    assert(!munmap(calc,4096) && !munmap(setp,4096));
    assert(!munmap(divp,4096) && !munmap(win,4096));
    printf("ok offline shipped ARM svg scrollbar: %u native/C cases, 720px track geometry/state/render gates match\n",compared);
}

#endif
