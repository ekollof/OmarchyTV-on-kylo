#ifndef OA_TESTS_SVG_SCROLLBAR_RENDER_NATIVE_H
#define OA_TESTS_SVG_SCROLLBAR_RENDER_NATIVE_H

#include "openaurora/svg_scrollbar.h"

/*
 * Differential for SVGScrollBar::renderBackground (0x10c3658) and renderBar
 * (0x10c3bac), Thumb. The range 0x10c3658..0x10c3f08 is copied verbatim at its
 * original address so renderBar's calculateBarSizeAndPosition call resolves
 * in place. The stack-guard pools at 0x10c3970/0x10c3aec/0x10c3efc are
 * redirected. External calls are stubbed and traced; the same trace is
 * produced by the owned ops.
 */
enum { SBR_WINDOW,SBR_RESIZE,SBR_RECT_NEW,SBR_ALIGN,SBR_MASK_SURFACE,
       SBR_MASK_NAME,SBR_MASK_CONTAINER,SBR_DRAW,SBR_MASK_RECT,SBR_RESET,
       SBR_VERTICAL_RECT,SBR_VERTICAL_NAME,SBR_VERTICAL_CONTAINER,SBR_CLEAR,
       SBR_SUPPLIER };
struct sbr_ev { uint32_t kind,surface,image,id,transform;int32_t rect[4]; };
static struct sbr_ev sbr_evn[64],sbr_evc[64],sbr_cmp[64];
static struct sbr_ev *sbr_tp;static unsigned sbr_tn;
static uint32_t sbr_guard=UINT32_C(0x5e21b7c9);
static int32_t sbr_window[4]={1750,180,1758,900};
static int32_t sbr_supplier_value;
static int32_t sbr_top_height=20,sbr_bottom_height=16;
static uint32_t sbr_render_base;
static int32_t sbr_last_rect_height;
static uint32_t sbr_transform_kind;
static uint32_t sbr_supplier_vtable[4],sbr_supplier_obj[1];
static uint32_t sbr_renderer_vtable[8],sbr_renderer_obj[1];
static uint32_t sbr_element[16];

static void sbr_emit(uint32_t kind,uint32_t surface,uint32_t image,uint32_t id,
    uint32_t transform,const int32_t *rect)
{
    assert(sbr_tn<64);
    memset(&sbr_tp[sbr_tn],0,sizeof sbr_tp[sbr_tn]);
    sbr_tp[sbr_tn].kind=kind;sbr_tp[sbr_tn].surface=surface;
    sbr_tp[sbr_tn].image=image;sbr_tp[sbr_tn].id=id;
    sbr_tp[sbr_tn].transform=transform;
    if(rect)memcpy(sbr_tp[sbr_tn].rect,rect,16);
    ++sbr_tn;
}
static uint32_t sbr_surface_index(void *surface)
{
    uint32_t p=(uint32_t)(uintptr_t)surface;
    if(p==sbr_render_base+0x98)return 0;
    if(p==sbr_render_base+0xa4)return 1;
    assert(0);return 2;
}
static uint32_t sbr_name_id(void *name) { return ((uint32_t *)name)[0]; }

static uint32_t sbr_bits_window(void (*f)(void*,void*))
{ uint32_t v;assert(sizeof f==4);memcpy(&v,&f,4);return v; }
static uint32_t sbr_bits_supplier(int32_t (*f)(void*,uint32_t))
{ uint32_t v;assert(sizeof f==4);memcpy(&v,&f,4);return v; }
static uint32_t sbr_bits_draw(void (*f)(void*,void*,int32_t,void*,void*,void*))
{ uint32_t v;assert(sizeof f==4);memcpy(&v,&f,4);return v; }
static void sbr_window_rect(void *out,void *self)
{
    (void)self;
    memcpy(out,sbr_window,16);
    sbr_emit(SBR_WINDOW,0,0,0,0,sbr_window);
}
static int32_t sbr_supplier(void *self,uint32_t image_id)
{
    (void)self;sbr_emit(SBR_SUPPLIER,0,0,image_id,0,NULL);
    return sbr_supplier_value;
}
static void sbr_draw(void *self,void *surface,int32_t image,void *name,
    void *container,void *transform)
{
    (void)self;(void)container;(void)transform;
    sbr_emit(SBR_DRAW,sbr_surface_index(surface),(uint32_t)image,
        sbr_name_id(name),sbr_transform_kind,NULL);
}
static void sbr_name(void *out,uint32_t self,uint32_t id)
{
    (void)self;((uint32_t *)out)[0]=id;((uint32_t *)out)[1]=0;
}
static int32_t sbr_div(int32_t numerator,int32_t divisor)
{ assert(divisor!=0);return numerator/divisor; }
typedef int32_t (*sbr_div_fn)(int32_t,int32_t);
static void sbr_div_install(void *mapped,uint32_t base,uint32_t address,
    sbr_div_fn fn)
{
    unsigned char *p=stub_destination(mapped,base,address,8);
    const uint16_t insn[]={0x4b00,0x4718};
    assert(!(address&3));memcpy(p,insn,sizeof insn);
    uint32_t target;assert(sizeof fn==sizeof target);memcpy(&target,&fn,4);
    memcpy(p+4,&target,4);
}
typedef void (*sbr_rect_fn5)(void *,int32_t,int32_t,int32_t,int32_t);
static void sbr_rect_install(void *mapped,uint32_t base,uint32_t address,
    sbr_rect_fn5 fn)
{
    unsigned char *p=stub_destination(mapped,base,address,16);
    const uint16_t insn[]={0xb40f,0x4b02,0x469c,0xbc0f,0x4760,0x46c0};
    assert(!(address&3));memcpy(p,insn,sizeof insn);
    uint32_t target;assert(sizeof fn==sizeof target);memcpy(&target,&fn,4);
    memcpy(p+12,&target,4);
}
static void sbr_rect_new(void *out,int32_t l,int32_t t,int32_t r,int32_t b)
{
    ((int32_t *)out)[0]=l;((int32_t *)out)[1]=t;
    ((int32_t *)out)[2]=r;((int32_t *)out)[3]=b;
    sbr_emit(SBR_RECT_NEW,0,0,0,0,(int32_t *)out);
}
static void sbr_mask_reset(void *mask)
{ (void)mask;sbr_emit(SBR_RESET,0,0,0,0,NULL); }
static void sbr_mask_align(void *mask,int32_t align)
{ (void)mask;sbr_emit(SBR_ALIGN,0,0,(uint32_t)align,0,NULL); }
static void sbr_mask_surface(void *mask,void *rect)
{ (void)mask;sbr_emit(SBR_MASK_SURFACE,0,0,0,0,(int32_t *)rect); }
static void sbr_mask_name(void *mask,void *name)
{ (void)mask;sbr_emit(SBR_MASK_NAME,0,0,sbr_name_id(name),0,NULL); }
static void sbr_mask_container(void *mask,void *container)
{ (void)mask;(void)container;sbr_transform_kind=0;sbr_emit(SBR_MASK_CONTAINER,0,0,0,0,NULL); }
static void sbr_vertical_name(void *vertical,void *name)
{ (void)vertical;sbr_emit(SBR_VERTICAL_NAME,0,0,sbr_name_id(name),0,NULL); }
static void sbr_vertical_container(void *vertical,void *container)
{ (void)vertical;(void)container;sbr_transform_kind=1;sbr_emit(SBR_VERTICAL_CONTAINER,0,0,0,0,NULL); }
static void sbr_vertical_rect(void *vertical,void *rect)
{ (void)vertical;sbr_emit(SBR_VERTICAL_RECT,0,0,0,0,(int32_t *)rect); }
static void sbr_mask_get_rect(void *out,void *mask)
{
    (void)mask;
    int32_t r[4]={0,0,8,sbr_last_rect_height};
    memcpy(out,r,16);
}
static void *sbr_get_rect(void *container,void *name)
{
    (void)container;
    uint32_t id=sbr_name_id(name);
    sbr_emit(SBR_MASK_RECT,0,0,id,0,NULL);
    if(id==15||id==34)sbr_last_rect_height=sbr_top_height;
    else if(id==24||id==35)sbr_last_rect_height=sbr_bottom_height;
    else sbr_last_rect_height=0;
    return sbr_element;
}
static void sbr_resize(void *surface,void *rect)
{
    sbr_emit(SBR_RESIZE,sbr_surface_index(surface),0,0,0,(int32_t *)rect);
}
static void sbr_clear(void *surface)
{ sbr_emit(SBR_CLEAR,sbr_surface_index(surface),0,0,0,NULL); }
static unsigned sbr_get_supplier(void *ctx)
{ (void)ctx;return (unsigned)(uintptr_t)sbr_supplier_obj; }
static unsigned sbr_get_renderer(void *ctx)
{ (void)ctx;return (unsigned)(uintptr_t)sbr_renderer_obj; }
static void sbr_blank(void *p) { (void)p; }

/* Exact harness-signature wrappers. */
static void sbr_mask_container_fn(void *a,uint32_t b)
{ sbr_mask_container(a,(void *)(uintptr_t)b); }
static void sbr_mask_align_fn(void *a,uint32_t b)
{ sbr_mask_align(a,(int32_t)b); }
static void sbr_mask_name_fn(void *a,uint32_t b)
{ sbr_mask_name(a,(void *)(uintptr_t)b); }
static void sbr_mask_surface_fn(void *a,uint32_t b)
{ sbr_mask_surface(a,(void *)(uintptr_t)b); }
static void sbr_mask_get_rect_fn(void *a,uint32_t b)
{ sbr_mask_get_rect(a,(void *)(uintptr_t)b); }
static void sbr_vertical_container_fn(void *a,uint32_t b)
{ sbr_vertical_container(a,(void *)(uintptr_t)b); }
static void sbr_vertical_name_fn(void *a,uint32_t b)
{ sbr_vertical_name(a,(void *)(uintptr_t)b); }
static void sbr_vertical_rect_fn(void *a,uint32_t b)
{ sbr_vertical_rect(a,(void *)(uintptr_t)b); }
static void sbr_resize_fn(void *a,uint32_t b)
{ sbr_resize(a,(void *)(uintptr_t)b); }
static void sbr_get_rect_fn(void *a,uint32_t b)
{ (void)sbr_get_rect(a,(void *)(uintptr_t)b); }
static unsigned sbr_clear_fn(void *a)
{ sbr_clear(a);return 0; }

/* Owned ops. */
static int32_t sbro_supplier(struct oa_svg_scrollbar_render *r,int32_t id)
{ (void)r;sbr_emit(SBR_SUPPLIER,0,0,id,0,NULL);return sbr_supplier_value; }
static void sbro_window(struct oa_svg_scrollbar_render *r,int32_t rect[4])
{ (void)r;memcpy(rect,sbr_window,16);sbr_emit(SBR_WINDOW,0,0,0,0,rect); }
static void sbro_resize(struct oa_svg_scrollbar_render *r,int surface,
    const int32_t rect[4])
{ (void)r;sbr_emit(SBR_RESIZE,(uint32_t)surface,0,0,0,rect); }
static void sbro_clear(struct oa_svg_scrollbar_render *r,int surface)
{ (void)r;sbr_emit(SBR_CLEAR,(uint32_t)surface,0,0,0,NULL); }
static void sbro_rect(struct oa_svg_scrollbar_render *r,int32_t rect[4],
    int32_t l,int32_t t,int32_t rt,int32_t b)
{
    (void)r;rect[0]=l;rect[1]=t;rect[2]=rt;rect[3]=b;
    sbr_emit(SBR_RECT_NEW,0,0,0,0,rect);
}
static void sbro_reset(struct oa_svg_scrollbar_render *r)
{ (void)r;sbr_emit(SBR_RESET,0,0,0,0,NULL); }
static void sbro_align(struct oa_svg_scrollbar_render *r,int align)
{ (void)r;sbr_emit(SBR_ALIGN,0,0,(uint32_t)align,0,NULL); }
static void sbro_mask_surface(struct oa_svg_scrollbar_render *r,
    const int32_t rect[4])
{ (void)r;sbr_emit(SBR_MASK_SURFACE,0,0,0,0,rect); }
static void sbro_mask_name(struct oa_svg_scrollbar_render *r,int32_t id)
{ (void)r;sbr_emit(SBR_MASK_NAME,0,0,id,0,NULL); }
static void sbro_mask_container(struct oa_svg_scrollbar_render *r)
{ (void)r;sbr_emit(SBR_MASK_CONTAINER,0,0,0,0,NULL); }
static void sbro_draw(struct oa_svg_scrollbar_render *r,int surface,
    int32_t image,int32_t id,int transform)
{ (void)r;sbr_emit(SBR_DRAW,(uint32_t)surface,image,id,(uint32_t)transform,NULL); }
static int32_t sbro_rect_height(struct oa_svg_scrollbar_render *r,int32_t id)
{
    (void)r;sbr_emit(SBR_MASK_RECT,0,0,id,0,NULL);
    if(id==15||id==34)return sbr_top_height;
    if(id==24||id==35)return sbr_bottom_height;
    return 0;
}
static void sbro_vertical_rect(struct oa_svg_scrollbar_render *r,
    const int32_t rect[4])
{ (void)r;sbr_emit(SBR_VERTICAL_RECT,0,0,0,0,rect); }
static void sbro_vertical_name(struct oa_svg_scrollbar_render *r,int32_t id)
{ (void)r;sbr_emit(SBR_VERTICAL_NAME,0,0,id,0,NULL); }
static void sbro_vertical_container(struct oa_svg_scrollbar_render *r)
{ (void)r;sbr_emit(SBR_VERTICAL_CONTAINER,0,0,0,0,NULL); }
static struct oa_svg_scrollbar_render_ops sbr_ops(void)
{
    return (struct oa_svg_scrollbar_render_ops){
        sbro_supplier,sbro_window,sbro_resize,sbro_clear,sbro_rect,sbro_reset,
        sbro_align,sbro_mask_surface,sbro_mask_name,sbro_mask_container,
        sbro_draw,sbro_rect_height,sbro_vertical_rect,sbro_vertical_name,
        sbro_vertical_container};
}

static void check_svg_scrollbar_render(const unsigned char *elf,size_t bytes)
{
    unsigned char *body=page(0x10c3000,4096),*ctx=page(0xeed000,4096);
    unsigned char *mask=page(0xeec000,4096),*namep=page(0xeee000,4096);
    unsigned char *rect=page(0x9a7000,4096),*ustr=page(0xc86000,4096);
    unsigned char *divp=page(0x1285000,4096);
    size_t o=file_offset(elf,bytes,0x10c3658,0x8b0);
    memcpy(stub_destination(body,0x10c3000,0x10c3658,0x8b0),elf+o,0x8b0);
    { uint32_t gp=(uint32_t)(uintptr_t)&sbr_guard;
      memcpy(stub_destination(body,0x10c3000,0x10c3970,4),&gp,4);
      memcpy(stub_destination(body,0x10c3000,0x10c3aec,4),&gp,4);
      memcpy(stub_destination(body,0x10c3000,0x10c3ba8,4),&gp,4);
      memcpy(stub_destination(body,0x10c3000,0x10c3efc,4),&gp,4); }
    /* Window rect through the fake object's vtable slot 0x28. */
    sbr_render_base=(uint32_t)(uintptr_t)calloc(1,0x100);
    assert(sbr_render_base);
    static uint32_t vtable[16];
    vtable[10]=sbr_bits_window(sbr_window_rect);
    *(uint32_t *)(uintptr_t)sbr_render_base=(uint32_t)(uintptr_t)vtable;
    sbr_supplier_vtable[2]=sbr_bits_supplier(sbr_supplier);
    sbr_supplier_obj[0]=(uint32_t)(uintptr_t)sbr_supplier_vtable;
    sbr_renderer_vtable[3]=sbr_bits_draw(sbr_draw);
    sbr_renderer_obj[0]=(uint32_t)(uintptr_t)sbr_renderer_vtable;
    thumb_stub(ctx,0xeed000,0xeed5e8,sbr_get_supplier);
    thumb_stub(ctx,0xeed000,0xeed618,sbr_get_renderer);
    pair_stub(ctx,0xeed000,0xeedf38,sbr_resize_fn);
    thumb_stub(ctx,0xeed000,0xeedec8,sbr_clear_fn);
    blank_stub(ctx,0xeed000,0xeed280,sbr_blank);
    pair_stub(ctx,0xeed000,0xeed2b8,sbr_get_rect_fn);
    blank_stub(ctx,0xeed000,0xeed374,sbr_blank);
    blank_stub(mask,0xeec000,0xeec2e8,sbr_blank);
    blank_stub(mask,0xeec000,0xeec354,sbr_mask_reset);
    pair_stub(mask,0xeec000,0xeec390,sbr_mask_container_fn);
    pair_stub(mask,0xeec000,0xeec3c0,sbr_mask_align_fn);
    pair_stub(mask,0xeec000,0xeec3f0,sbr_mask_name_fn);
    pair_stub(mask,0xeec000,0xeec4ec,sbr_mask_surface_fn);
    pair_stub(mask,0xeec000,0xeec580,sbr_mask_get_rect_fn);
    blank_stub(mask,0xeec000,0xeeced4,sbr_blank);
    pair_stub(mask,0xeec000,0xeecf50,sbr_vertical_container_fn);
    pair_stub(mask,0xeec000,0xeecfbc,sbr_vertical_name_fn);
    pair_stub(ctx,0xeed000,0xeed030,sbr_vertical_rect_fn);
    tri_stub(namep,0xeee000,0xeee798,sbr_name);
    sbr_rect_install(rect,0x9a7000,0x9a7fcc,sbr_rect_new);
    sbr_div_install(divp,0x1285000,0x1285478,sbr_div);
    blank_stub(ustr,0xc86000,0xc86554,sbr_blank);
    executable(body,4096);executable(ctx,4096);executable(mask,4096);
    executable(divp,4096);
    executable(namep,4096);executable(rect,4096);executable(ustr,4096);
    void (*bg_body)(void *);
    { uint32_t entry=0x10c3659;memcpy(&bg_body,&entry,4); }
    void (*bar_body)(void *);
    { uint32_t entry=0x10c3bad;memcpy(&bar_body,&entry,4); }
    unsigned compared=0;
    for(unsigned mode=0;mode<8;++mode) {
        struct oa_svg_scrollbar_render nr,cr;
        memset(&nr,0,sizeof nr);memset(&cr,0,sizeof cr);
        nr.skin.cached_image[0]=mode==1||mode==5?0:0x7002;
        nr.skin.image_id[0]=0x1234;
        nr.state.min_bar=mode==6?600:36;
        nr.state.total=mode==3||mode==4||mode==5||mode==6?10:20;
        nr.state.visible=mode==3||mode==4||mode==5||mode==6?15:15;
        nr.state.first=mode==4?5:0;
        nr.state.background_rendered=(uint8_t)(mode!=3);
        cr=nr;
        sbr_supplier_value=mode==0?0:0x7001;
        sbr_render_base=(uint32_t)(uintptr_t)calloc(1,0x100);
        assert(sbr_render_base);
        *(uint32_t *)(uintptr_t)sbr_render_base=(uint32_t)(uintptr_t)vtable;
        *(uint32_t *)(uintptr_t)(sbr_render_base+0x68)=0x1234;
        *(uint32_t *)(uintptr_t)(sbr_render_base+0x6c)=0;
        *(uint32_t *)(uintptr_t)(sbr_render_base+0x74)=nr.skin.cached_image[0];
        *(uint32_t *)(uintptr_t)(sbr_render_base+0x84)=nr.skin.image_id[0];
        memcpy((void *)(uintptr_t)(sbr_render_base+0xb0),&nr.state,sizeof nr.state);
        sbr_tp=sbr_evn;sbr_tn=0;
        if(mode<=1)bg_body((void *)(uintptr_t)sbr_render_base);
        else bar_body((void *)(uintptr_t)sbr_render_base);
        unsigned native_n=sbr_tn;
        memcpy(sbr_cmp,sbr_evn,native_n*sizeof sbr_evn[0]);
        uint32_t native_flag=*(uint8_t *)(uintptr_t)(sbr_render_base+0xc8);
        uint32_t native_cache=*(uint32_t *)(uintptr_t)(sbr_render_base+0x74);
        struct oa_svg_scrollbar_render_ops ops=sbr_ops();
        sbr_tp=sbr_evc;sbr_tn=0;
        if(mode<=1)oa_svg_scrollbar_render_background(&cr,&ops);
        else oa_svg_scrollbar_render_bar(&cr,&ops);
        if(native_n!=sbr_tn ||
           memcmp(sbr_cmp,sbr_evc,native_n*sizeof sbr_evc[0]) ||
           native_flag!=cr.state.background_rendered ||
           native_cache!=(uint32_t)cr.skin.cached_image[0]) {
            fprintf(stderr,"scrollbar render mismatch mode=%u n=%u/%u flag=%u/%u cache=%x/%x\n",
                mode,native_n,sbr_tn,native_flag,cr.state.background_rendered,
                native_cache,cr.skin.cached_image[0]);
            for(unsigned i=0;i<native_n||i<sbr_tn;++i)
                fprintf(stderr,"  [%u] nat k=%u s=%u i=%u id=%u t=%u | ours k=%u s=%u i=%u id=%u t=%u\n",
                    i,i<native_n?sbr_cmp[i].kind:99,i<native_n?sbr_cmp[i].surface:99,
                    i<native_n?sbr_cmp[i].image:99,i<native_n?sbr_cmp[i].id:99,
                    i<native_n?sbr_cmp[i].transform:99,
                    i<sbr_tn?sbr_evc[i].kind:99,i<sbr_tn?sbr_evc[i].surface:99,
                    i<sbr_tn?sbr_evc[i].image:99,i<sbr_tn?sbr_evc[i].id:99,
                    i<sbr_tn?sbr_evc[i].transform:99);
            exit(1);
        }
        ++compared;
    }
    assert(!munmap(body,4096) && !munmap(ctx,4096) && !munmap(mask,4096));
    assert(!munmap(namep,4096) && !munmap(rect,4096) && !munmap(ustr,4096));
    assert(!munmap(divp,4096));
    printf("ok offline shipped ARM svg scrollbar render: %u native/C cases, surfaces/masks/caps/row match\n",compared);
}

#endif
