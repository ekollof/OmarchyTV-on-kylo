#ifndef OA_TESTS_SVG_IMAGE_NATIVE_H
#define OA_TESTS_SVG_IMAGE_NATIVE_H

#include "openaurora/svg_image.h"

/*
 * Differential for SVGImage::create (0x10c9638) and SVGImage::destroy
 * (0x10c8ef4). Both shipped bodies are copied verbatim to their own
 * addresses so their PC-relative calls resolve to stubs installed in place.
 * EASIRect::offset (0xd2af88) is copied verbatim as well, so the parent
 * client-rect offset is the shipped arithmetic. The image supplier's virtual
 * call goes through a fake object whose vtable slot 2 points at a stub.
 */
typedef void (*sv_create_fn)(void *,uint32_t,uint32_t,uint32_t);
typedef void (*sv_destroy_fn)(void *);
typedef uint32_t (*sv_ret3_fn)(void *,uint32_t,void *);

enum {SV_ID,SV_GETSUP,SV_SUP,SV_CLIENT,SV_IMG,SV_WD,SV_CLEAN};

struct sv_ev { uint32_t kind,a,b,w[4]; };
static struct sv_ev sv_evn[24],sv_evc[24],sv_cmp[24];
static struct sv_ev *sv_tp;static unsigned sv_tn;
static unsigned char *sv_img_native,*sv_img_ref;
static uint32_t sv_last_out;
static struct { uint32_t id,rect[4],client[4];int sup,parent; } sv_fix;

static void sv_emit(uint32_t kind,uint32_t a,uint32_t b,const uint32_t *w)
{
    assert(sv_tn<24);
    sv_tp[sv_tn].kind=kind;sv_tp[sv_tn].a=a;sv_tp[sv_tn].b=b;
    if(w)memcpy(sv_tp[sv_tn].w,w,16);else memset(sv_tp[sv_tn].w,0,16);
    ++sv_tn;
}
static void sv_stub3(void *mapped,uint32_t base,uint32_t address,sv_ret3_fn function)
{
    unsigned char *p=stub_destination(mapped,base,address,12);
    const uint16_t insn[]={0xf8df,0xc004,0x4760,0xbf00};
    assert(!(address&3));memcpy(p,insn,sizeof insn);
    uint32_t target;assert(sizeof function==sizeof target);
    memcpy(&target,&function,sizeof target);memcpy(p+8,&target,4);
}

/* Native-side stubs (called by the shipped bodies). */
static void svn_set_id(void *img,uint32_t id)
{ sv_emit(SV_ID,(uint32_t)((uintptr_t)img-(uintptr_t)sv_img_native),id,NULL); }
static uint32_t svn_get_sup(void *img)
{ (void)img;sv_emit(SV_GETSUP,0,0,NULL);return UINT32_C(0xe4c000); }
static uint32_t svn_sup_res(void *sup,uint32_t id,void *out)
{
    (void)sup;sv_emit(SV_SUP,id,(uint32_t)sv_fix.sup,NULL);
    memset(out,0x5a,16);sv_last_out=(uint32_t)(uintptr_t)out;
    return (uint32_t)sv_fix.sup;
}
static void svn_client(void *out,uint32_t parent)
{ (void)parent;sv_emit(SV_CLIENT,1,0,NULL);memcpy(out,sv_fix.client,16); }
static void svn_img_create(void *img,uint32_t res,uint32_t rectp,uint32_t parent)
{
    uint32_t w[4];(void)img;(void)parent;
    memcpy(w,(void *)(uintptr_t)rectp,16);
    sv_emit(SV_IMG,(uint32_t)(res==sv_last_out),0,w);
}
static void svn_wdestroy(void *img) { (void)img;sv_emit(SV_WD,0,0,NULL); }
static void svn_clean(void *inner)
{ sv_emit(SV_CLEAN,(uint32_t)((uintptr_t)inner==(uintptr_t)sv_img_native+0x9c),0,NULL); }

/* Owned-side callbacks. */
static void svc_set_id(void *img,int32_t id)
{ sv_emit(SV_ID,(uint32_t)((uintptr_t)img-(uintptr_t)sv_img_ref),(uint32_t)id,NULL); }
static void *svc_get_sup(void *img)
{ (void)img;sv_emit(SV_GETSUP,0,0,NULL);return (void *)(uintptr_t)UINT32_C(0xe4c000); }
static int svc_sup_res(void *sup,uint32_t id,void *out)
{
    (void)sup;sv_emit(SV_SUP,id,(uint32_t)sv_fix.sup,NULL);
    memset(out,0x5a,16);sv_last_out=(uint32_t)(uintptr_t)out;
    return sv_fix.sup;
}
static void svc_client(void *parent,uint32_t out[4])
{ (void)parent;sv_emit(SV_CLIENT,1,0,NULL);memcpy(out,sv_fix.client,16); }
static void svc_img_create(void *img,const void *res,const uint32_t rect[4],void *parent)
{
    (void)img;(void)parent;
    sv_emit(SV_IMG,(uint32_t)((uint32_t)(uintptr_t)res==sv_last_out),0,rect);
}
static void svc_wdestroy(void *img) { (void)img;sv_emit(SV_WD,0,0,NULL); }
static void svc_clean(void *surface)
{
    sv_emit(SV_CLEAN,(uint32_t)((uintptr_t)surface==(uintptr_t)sv_img_ref+0x9c),0,NULL);
}
static struct oa_svg_image_ops sv_ops(void)
{
    return (struct oa_svg_image_ops){svc_set_id,svc_get_sup,svc_sup_res,svc_client,
        svc_img_create,svc_wdestroy,svc_clean};
}

static void check_svg_image(const unsigned char *elf,size_t bytes)
{
    unsigned char *cb=page(0x10c8000,4096),*cc=page(0x10c9000,4096),*off=page(0xd2a000,4096);
    unsigned char *win=page(0x1152000,4096),*ctx=page(0xeed000,4096),*winx=page(0xeee000,4096);
    unsigned char *sup=page(0xe4c000,4096);
    size_t o;
    uint32_t guard=UINT32_C(0x51ab77e3),pointer=(uint32_t)(uintptr_t)&guard;
    {
        o=file_offset(elf,bytes,0x10c8ef4,60);
        assert(crc32(elf+o,60)==UINT32_C(0xcb48ce8f));
        memcpy(stub_destination(cb,0x10c8000,0x10c8ef4,60),elf+o,60);
        memcpy(stub_destination(cb,0x10c8000,0x10c8f2c,4),&pointer,4);
    }
    {
        o=file_offset(elf,bytes,0x10c9638,136);
        assert(crc32(elf+o,136)==UINT32_C(0xbdf31b8d));
        memcpy(stub_destination(cc,0x10c9000,0x10c9638,136),elf+o,136);
        memcpy(stub_destination(cc,0x10c9000,0x10c96bc,4),&pointer,4);
    }
    {
        o=file_offset(elf,bytes,0xd2af88,74);
        assert(crc32(elf+o,74)==UINT32_C(0xbc62c27a));
        memcpy(stub_destination(off,0xd2a000,0xd2af88,74),elf+o,74);
        memcpy(stub_destination(off,0xd2a000,0xd2afd4,4),&pointer,4);
    }
    pair_stub(win,0x1152000,0x1152a10,svn_set_id);
    pair_stub(win,0x1152000,0x1152974,svn_client);
    thumb_stub(ctx,0xeed000,0xeed678,(getter)svn_get_sup);
    blank_stub(ctx,0xeed000,0xeede94,svn_clean);
    blank_stub(winx,0xeee000,0xeeeba0,svn_wdestroy);
    quad_stub(cc,0x10c9000,0x10c9430,svn_img_create);
    sv_stub3(sup,0xe4c000,0xe4c100,svn_sup_res);
    {
        uint32_t vtable=UINT32_C(0xe4c010),slot=(uint32_t)(uintptr_t)0xe4c100|1u;
        memcpy(sup,&vtable,4);
        memcpy(sup+0x10+8,&slot,4);
    }
    executable(cb,4096);executable(cc,4096);executable(off,4096);
    executable(win,4096);executable(ctx,4096);executable(winx,4096);executable(sup,4096);
    sv_img_native=calloc(1,0x200);sv_img_ref=calloc(1,0x200);
    assert(sv_img_native && sv_img_ref);
    static const uint32_t rects[][4]={{10,20,30,40},{0,0,0,0}};
    static const uint32_t clients[4]={1,2,7,8};
    static const uint32_t ids[]={0,0x37};
    sv_create_fn cf;{ uint32_t entry=0x10c9639;memcpy(&cf,&entry,4); }
    sv_destroy_fn df;{ uint32_t entry=0x10c8ef5;memcpy(&df,&entry,4); }
    unsigned compared=0;
    for(unsigned s=0;s<2;++s)for(unsigned p=0;p<2;++p)
    for(unsigned i=0;i<2;++i)for(unsigned r=0;r<2;++r) {
        struct oa_svg_image_ops ops=sv_ops();
        void *parent=p?(void *)(uintptr_t)0x2222:NULL;
        sv_fix.id=ids[i];sv_fix.sup=(int)s;sv_fix.parent=(int)p;
        memcpy(sv_fix.rect,rects[r],16);memcpy(sv_fix.client,clients,16);
        sv_tp=sv_evn;sv_tn=0;sv_last_out=0;
        cf(sv_img_native,sv_fix.id,(uint32_t)(uintptr_t)sv_fix.rect,(uint32_t)(uintptr_t)parent);
        memcpy(sv_cmp,sv_evn,sv_tn*sizeof sv_evn[0]);unsigned n=sv_tn;
        sv_tp=sv_evc;sv_tn=0;sv_last_out=0;
        oa_svg_image_create(sv_img_ref,sv_fix.id,sv_fix.rect,parent,&ops);
        if(n!=sv_tn || memcmp(sv_cmp,sv_evc,n*sizeof sv_evn[0])) {
            fprintf(stderr,"svg image create mismatch sup=%u parent=%u id=%x rect=%u n=%u/%u\n",
                s,p,(unsigned)sv_fix.id,r,n,sv_tn);exit(1);
        }
        ++compared;
    }
    for(unsigned k=0;k<2;++k) {
        struct oa_svg_image_ops ops=sv_ops();
        sv_tp=sv_evn;sv_tn=0;df(sv_img_native);
        memcpy(sv_cmp,sv_evn,sv_tn*sizeof sv_evn[0]);unsigned n=sv_tn;
        sv_tp=sv_evc;sv_tn=0;oa_svg_image_destroy(sv_img_ref,&ops);
        if(n!=sv_tn || memcmp(sv_cmp,sv_evc,n*sizeof sv_evn[0])) {
            fprintf(stderr,"svg image destroy mismatch k=%u n=%u/%u\n",k,n,sv_tn);exit(1);
        }
        ++compared;
    }
    free(sv_img_native);free(sv_img_ref);
    assert(!munmap(cb,4096) && !munmap(cc,4096) && !munmap(off,4096));
    assert(!munmap(win,4096) && !munmap(ctx,4096) && !munmap(winx,4096) && !munmap(sup,4096));
    printf("ok offline shipped ARM svg image: %u native/C cases, create/supplier gate/rect offset and destroy order match\n",compared);
}

#endif
