#ifndef OA_TESTS_SVG_SCROLLBAR_SKIN_NATIVE_H
#define OA_TESTS_SVG_SCROLLBAR_SKIN_NATIVE_H

#include "openaurora/svg_scrollbar.h"

/*
 * Differential for SVGScrollBar::calculateMinBarSize (0x10c397c, 0x174 bytes,
 * CRC 0xd9fdda32), Thumb.
 *
 * The body caches the supplier image at +0x74+orientation*4 (image id at
 * +0x84+orientation*4), then resolves standard names 0x20/0x22 and 0x21/0x23
 * and adds the two moving-bar cap heights. The native stubs provide a fake
 * SVG supplier/renderer, getWidgetStandardName, RectangleContainer and
 * UString destructor; the owned ops trace the same supplier/name/background/
 * rect sequence.
 */
static uint32_t skin_guard=UINT32_C(0x7ac31e45);
struct skin_ev { uint32_t kind,id,value; };
static struct skin_ev skin_evn[16],skin_evc[16];
static struct skin_ev *skin_tp;static unsigned skin_tn;
static int32_t skin_supplier_value;
static int skin_rect23_present;

static void skin_emit(uint32_t kind,uint32_t id,uint32_t value)
{
    assert(skin_tn<16);
    skin_tp[skin_tn].kind=kind;skin_tp[skin_tn].id=id;
    skin_tp[skin_tn].value=value;++skin_tn;
}
static uint32_t skin_supplier_vtable[4];
static uint32_t skin_supplier_obj[1];
static uint32_t skin_renderer_vtable[8];
static uint32_t skin_renderer_obj[1];
static uint32_t skin_rect22[8],skin_rect23[8];

static int32_t skin_supplier_stub(void *self,uint32_t image_id)
{
    (void)self;
    skin_emit(1,0,image_id);
    return skin_supplier_value;
}
static void skin_renderer_stub(void *self,void *image,void *name,
    void *container,uint32_t zero)
{
    (void)self;(void)image;(void)container;(void)zero;
    skin_emit(3,((uint32_t *)name)[0],0);
}
static void skin_name_stub(void *out,uint32_t self,uint32_t id)
{
    (void)self;
    ((uint32_t *)out)[0]=id;((uint32_t *)out)[1]=0;
    skin_emit(2,id,0);
}
typedef uint32_t (*skin_rect_fn)(void *,uint32_t);
static void skin_rect_install(void *mapped,uint32_t base,uint32_t address,
    skin_rect_fn fn)
{
    unsigned char *p=stub_destination(mapped,base,address,8);
    const uint16_t insn[]={0x4b00,0x4718};
    assert(!(address&3));memcpy(p,insn,sizeof insn);
    uint32_t target;assert(sizeof fn==sizeof target);memcpy(&target,&fn,4);
    memcpy(p+4,&target,4);
}
static uint32_t skin_rect_stub(void *container,uint32_t namep)
{
    (void)container;
    uint32_t id=((uint32_t *)(uintptr_t)namep)[0];
    skin_emit(4,id,0);
    if(id==0x22)return (uint32_t)(uintptr_t)skin_rect22;
    if(id==0x23 && skin_rect23_present)return (uint32_t)(uintptr_t)skin_rect23;
    return 0;
}
static unsigned skin_get_supplier(void *ctx)
{ (void)ctx;return (unsigned)(uintptr_t)skin_supplier_obj; }
static unsigned skin_get_renderer(void *ctx)
{ (void)ctx;return (unsigned)(uintptr_t)skin_renderer_obj; }
static void skin_blank(void *p) { (void)p; }
static int32_t skinc_supplier(struct oa_svg_scrollbar_skin *s,int32_t image_id)
{ (void)s;skin_emit(1,0,image_id);return skin_supplier_value; }
static int32_t skinc_name(struct oa_svg_scrollbar_skin *s,int32_t id)
{ (void)s;skin_emit(2,id,0);return id; }
static void skinc_bg(struct oa_svg_scrollbar_skin *s,int32_t image,int32_t name)
{ (void)s;(void)image;skin_emit(3,name,0); }
static int32_t skinc_rect(struct oa_svg_scrollbar_skin *s,int32_t name)
{
    (void)s;skin_emit(4,name,0);
    if(name==0x22)return 20;
    if(name==0x23 && skin_rect23_present)return 16;
    return 0;
}
static struct oa_svg_scrollbar_skin_ops skin_ops(void)
{
    return (struct oa_svg_scrollbar_skin_ops){skinc_supplier,skinc_name,
        skinc_bg,skinc_rect};
}

static void check_svg_scrollbar_skin(const unsigned char *elf,size_t bytes)
{
    unsigned char *body=page(0x10c3000,4096),*ctx=page(0xeed000,4096);
    unsigned char *namep=page(0xeee000,4096),*ustr=page(0xc86000,4096);
    size_t o=file_offset(elf,bytes,0x10c397c,0x174);
    assert(crc32(elf+o,0x174)==UINT32_C(0xd9fdda32));
    memcpy(stub_destination(body,0x10c3000,0x10c397c,0x174),elf+o,0x174);
    { uint32_t gp=(uint32_t)(uintptr_t)&skin_guard;
      memcpy(stub_destination(body,0x10c3000,0x10c3aec,4),&gp,4); }
    thumb_stub(ctx,0xeed000,0xeed5e8,skin_get_supplier);
    thumb_stub(ctx,0xeed000,0xeed618,skin_get_renderer);
    blank_stub(ctx,0xeed000,0xeed280,skin_blank);
    skin_rect_install(ctx,0xeed000,0xeed2b8,skin_rect_stub);
    blank_stub(ctx,0xeed000,0xeed374,skin_blank);
    tri_stub(namep,0xeee000,0xeee798,skin_name_stub);
    blank_stub(ustr,0xc86000,0xc86554,skin_blank);
    skin_supplier_vtable[2]=(uint32_t)(uintptr_t)skin_supplier_stub;
    skin_supplier_obj[0]=(uint32_t)(uintptr_t)skin_supplier_vtable;
    skin_renderer_vtable[5]=(uint32_t)(uintptr_t)skin_renderer_stub;
    skin_renderer_obj[0]=(uint32_t)(uintptr_t)skin_renderer_vtable;
    memset(skin_rect22,0,sizeof skin_rect22);
    memset(skin_rect23,0,sizeof skin_rect23);
    skin_rect22[5]=20; /* +0x14 bottom, +0x0c top=0 */
    skin_rect23[5]=16;
    executable(body,4096);executable(ctx,4096);executable(namep,4096);
    executable(ustr,4096);
    unsigned char *obj=calloc(1,0x100);
    assert(obj);
    void (*min_body)(void *);
    { uint32_t entry=0x10c397d;memcpy(&min_body,&entry,4); }
    unsigned compared=0;
    for(unsigned orient=0;orient<2;++orient)
    for(unsigned cache=0;cache<2;++cache)
    for(unsigned supplier=0;supplier<2;++supplier)
    for(unsigned rect23=0;rect23<2;++rect23) {
        struct oa_svg_scrollbar_skin state=(struct oa_svg_scrollbar_skin){
            .cached_image={0xAAAA,0xBBBB},
            .image_id={0x1234,0x5678},
            .min_bar=99,.orientation=(uint8_t)orient};
        state.cached_image[orient]=cache?0x7003:0;
        skin_supplier_value=supplier?0x7001:0;
        skin_rect23_present=(int)rect23;
        memset(obj,0xa5,0x100);
        *(uint32_t *)(obj+0x68)=0x12345678;
        *(uint32_t *)(obj+0x6c)=(uint32_t)orient;
        *(uint32_t *)(obj+0x74+orient*4)=state.cached_image[orient];
        *(uint32_t *)(obj+0x84+orient*4)=state.image_id[orient];
        *(uint32_t *)(obj+0xb8)=99;
        skin_tp=skin_evn;skin_tn=0;
        min_body(obj);
        uint32_t native_min=*(uint32_t *)(obj+0xb8);
        uint32_t native_cache=*(uint32_t *)(obj+0x74+orient*4);
        struct skin_ev *native_trace=skin_evn;unsigned native_n=skin_tn;
        struct oa_svg_scrollbar_skin_ops ops=skin_ops();
        skin_tp=skin_evc;skin_tn=0;
        oa_svg_scrollbar_min_bar(&state,&ops);
        if(native_min!=(uint32_t)state.min_bar ||
           native_cache!=(uint32_t)state.cached_image[orient] ||
           native_n!=skin_tn ||
           memcmp(native_trace,skin_evc,native_n*sizeof skin_evn[0])) {
            fprintf(stderr,"scrollbar skin mismatch orient=%u cache=%u sup=%u rect23=%u n=%u/%u min=%u/%d\n",
                orient,cache,supplier,rect23,native_n,skin_tn,
                native_min,state.min_bar);
            exit(1);
        }
        ++compared;
    }
    free(obj);
    assert(!munmap(body,4096) && !munmap(ctx,4096));
    assert(!munmap(namep,4096) && !munmap(ustr,4096));
    printf("ok offline shipped ARM svg scrollbar skin: %u native/C cases, cap minimum/cache/orientation match\n",compared);
}

#endif
