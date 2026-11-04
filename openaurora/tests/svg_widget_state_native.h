#ifndef OA_TESTS_SVG_WIDGET_STATE_NATIVE_H
#define OA_TESTS_SVG_WIDGET_STATE_NATIVE_H

#include "openaurora/svg_widget_state.h"

/* Differential for SVGWindow::setFocus/setActive and SVGTextBox::
 * setFocus/setElipses (all Thumb). Bodies are copied at their original
 * addresses; UIMessageTarget::setFocus, isCreated, drawTextBox and
 * performTextSlideAnimation are stubs. */
enum {WS_BASE,WS_CREATED,WS_DRAW,WS_SLIDE};
struct ws_ev { uint32_t kind,a; };
static struct ws_ev ws_evn[8],ws_evc[8],ws_cmp[8];
static struct ws_ev *ws_tp;static unsigned ws_tn;
static int ws_created;

static void ws_emit(uint32_t kind,uint32_t a)
{ assert(ws_tn<8);ws_tp[ws_tn].kind=kind;ws_tp[ws_tn].a=a;++ws_tn; }
static void wsn_base(void *w,uint32_t f) { (void)w;ws_emit(WS_BASE,f); }
static uint32_t wsn_created(void *w) { (void)w;ws_emit(WS_CREATED,0);return (uint32_t)ws_created; }
static void wsn_draw(void *w) { (void)w;ws_emit(WS_DRAW,0); }
static void wsn_slide(void *w) { (void)w;ws_emit(WS_SLIDE,0); }
static void wsc_base(void *w,int f) { (void)w;ws_emit(WS_BASE,(uint32_t)f); }
static int wsc_created(void *w) { (void)w;ws_emit(WS_CREATED,0);return ws_created; }
static void wsc_draw(void *w) { (void)w;ws_emit(WS_DRAW,0); }
static void wsc_slide(void *w) { (void)w;ws_emit(WS_SLIDE,0); }
static struct oa_svg_widget_state_ops ws_ops(void)
{ return (struct oa_svg_widget_state_ops){wsc_base,wsc_created,wsc_draw,wsc_slide}; }

static uint32_t ws_get32(const void *p,uint32_t off)
{ uint32_t v;memcpy(&v,(const unsigned char *)p+off,4);return v; }
static void ws_set32(void *p,uint32_t off,uint32_t v)
{ memcpy((unsigned char *)p+off,&v,4); }

static void check_svg_widget_state(const unsigned char *elf,size_t bytes)
{
    unsigned char *winp=page(0xeee000,4096),*tbp=page(0x10c8000,4096);
    unsigned char *elipp=page(0x10c6000,4096),*basep=page(0x1148000,4096);
    unsigned char *crep=page(0x1152000,4096),*slidep=page(0x10c7000,4096);
    uint32_t guard=UINT32_C(0x6fb1c2a9),pointer=(uint32_t)(uintptr_t)&guard;
    size_t o;
    struct { uint32_t addr,size,crc; } fr[]={
        {0xeeebd4,0x5c,UINT32_C(0x085d6c76)},{0xeee4ec,0x54,UINT32_C(0x7b1ef395)},
        {0x10c8a28,0x50,UINT32_C(0x117744b9)},{0x10c6f48,0x3c,UINT32_C(0x1606bd56)}
    };
    unsigned char *pages[]={winp,winp,tbp,elipp};
    uint32_t bases[]={0xeee000,0xeee000,0x10c8000,0x10c6000};
    for(unsigned i=0;i<4;++i) {
        o=file_offset(elf,bytes,fr[i].addr,fr[i].size);
        assert(crc32(elf+o,fr[i].size)==fr[i].crc);
        memcpy(stub_destination(pages[i],bases[i],fr[i].addr,fr[i].size),elf+o,fr[i].size);
    }
    memcpy(stub_destination(winp,0xeee000,0xeeec2c,4),&pointer,4);
    memcpy(stub_destination(winp,0xeee000,0xeee53c,4),&pointer,4);
    memcpy(stub_destination(tbp,0x10c8000,0x10c8a74,4),&pointer,4);
    memcpy(stub_destination(elipp,0x10c6000,0x10c6f80,4),&pointer,4);
    pair_stub(basep,0x1148000,0x1148f98,wsn_base);
    thumb_stub(crep,0x1152000,0x1152a40,(getter)wsn_created);
    blank_stub(tbp,0x10c8000,0x10c87e8,wsn_draw);
    blank_stub(slidep,0x10c7000,0x10c7768,wsn_slide);
    executable(winp,4096);executable(tbp,4096);executable(elipp,4096);
    executable(basep,4096);executable(crep,4096);executable(slidep,4096);
    unsigned char *nbuf=calloc(1,0x300),*cbuf=calloc(1,0x300);
    assert(nbuf && cbuf);
    void (*wfocus)(void *,uint32_t);{ uint32_t e=0xeeebd5;memcpy(&wfocus,&e,4); }
    void (*wactive)(void *,uint32_t);{ uint32_t e=0xeee4ed;memcpy(&wactive,&e,4); }
    void (*tbfocus)(void *,uint32_t);{ uint32_t e=0x10c8a29;memcpy(&tbfocus,&e,4); }
    void (*tbelip)(void *,uint32_t);{ uint32_t e=0x10c6f49;memcpy(&tbelip,&e,4); }
    unsigned compared=0;
    /* SVGWindow state machines. */
    for(unsigned k=0;k<2;++k)for(unsigned s=0;s<4;++s)for(unsigned a=0;a<2;++a) {
        struct oa_svg_widget_state_ops ops=ws_ops();
        memset(nbuf,0xaa,sizeof 0x300);memset(cbuf,0xaa,sizeof 0x300);
        ws_set32(nbuf,0x6c,s);ws_set32(cbuf,0x6c,s);
        ws_tp=ws_evn;ws_tn=0;
        if(k)wactive(nbuf,a);else wfocus(nbuf,a);
        memcpy(ws_cmp,ws_evn,ws_tn*sizeof ws_evn[0]);unsigned n=ws_tn;
        ws_tp=ws_evc;ws_tn=0;
        if(k)oa_svg_window_set_active(cbuf,(int)a);
        else oa_svg_window_set_focus(cbuf,(int)a,&ops);
        if(n!=ws_tn || memcmp(ws_cmp,ws_evc,n*sizeof ws_evn[0])) {
            fprintf(stderr,"widget state trace mismatch k=%u s=%u a=%u\n",k,s,a);exit(1);
        }
        if(ws_get32(nbuf,0x6c)!=ws_get32(cbuf,0x6c)) {
            fprintf(stderr,"widget state value mismatch k=%u s=%u a=%u %x/%x\n",
                k,s,a,ws_get32(nbuf,0x6c),ws_get32(cbuf,0x6c));exit(1);
        }
        ++compared;
    }
    /* SVGTextBox::setFocus with created and state-change gating. */
    for(unsigned s=0;s<4;++s)for(unsigned a=0;a<2;++a)for(unsigned cr=0;cr<2;++cr) {
        struct oa_svg_widget_state_ops ops=ws_ops();
        memset(nbuf,0xaa,sizeof 0x300);memset(cbuf,0xaa,sizeof 0x300);
        ws_set32(nbuf,0x6c,s);ws_set32(cbuf,0x6c,s);
        ws_created=(int)cr;
        ws_tp=ws_evn;ws_tn=0;tbfocus(nbuf,a);
        memcpy(ws_cmp,ws_evn,ws_tn*sizeof ws_evn[0]);unsigned n=ws_tn;
        ws_tp=ws_evc;ws_tn=0;oa_svg_text_box_set_focus(cbuf,(int)a,&ops);
        if(n!=ws_tn || memcmp(ws_cmp,ws_evc,n*sizeof ws_evn[0])) {
            fprintf(stderr,"textbox focus trace mismatch s=%u a=%u cr=%u\n",s,a,cr);exit(1);
        }
        if(ws_get32(nbuf,0x6c)!=ws_get32(cbuf,0x6c)) {
            fprintf(stderr,"textbox focus state mismatch s=%u a=%u cr=%u\n",s,a,cr);exit(1);
        }
        ++compared;
    }
    /* SVGTextBox::setElipses mode-5 gating. */
    for(unsigned mode=0;mode<2;++mode)for(unsigned a=0;a<2;++a) {
        memset(nbuf,0xaa,sizeof 0x300);memset(cbuf,0xaa,sizeof 0x300);
        uint32_t mv=mode?5:9;
        ws_set32(nbuf,0x220,mv);ws_set32(cbuf,0x220,mv);
        nbuf[0x1b1]=cbuf[0x1b1]=0x11;
        tbelip(nbuf,a);
        oa_svg_text_box_set_elipses(cbuf,(int)a);
        if(nbuf[0x1b1]!=cbuf[0x1b1]) {
            fprintf(stderr,"elipses mismatch mode=%u a=%u %u/%u\n",mode,a,nbuf[0x1b1],cbuf[0x1b1]);
            exit(1);
        }
        ++compared;
    }
    free(nbuf);free(cbuf);
    assert(!munmap(winp,4096) && !munmap(tbp,4096) && !munmap(elipp,4096));
    assert(!munmap(basep,4096) && !munmap(crep,4096) && !munmap(slidep,4096));
    printf("ok offline shipped ARM svg widget state: %u native/C cases, focus/active qualification, redraw gates and elipses mode match\n",compared);
}

#endif
