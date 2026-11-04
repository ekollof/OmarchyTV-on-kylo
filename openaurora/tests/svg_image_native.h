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

enum {SV_ID,SV_GETSUP,SV_SUP,SV_CLIENT,SV_IMG,SV_WD,SV_CLEAN,
      SV_PW,SV_WR,SV_IC,SV_RES,SV_TH,SV_REL,SV_DRAW,SV_SCALED,SV_CS,SV_SU,SV_CW};

struct sv_ev { uint32_t kind,a,b,w[4]; };
static struct sv_ev sv_evn[24],sv_evc[24],sv_cmp[24];
static struct sv_ev *sv_tp;static unsigned sv_tn;
static unsigned char *sv_img_native,*sv_img_ref;
static uint32_t sv_last_out;
static uint32_t sv_token;
static int sv_created,sv_parent;
static uint32_t sv_fields[0x40];
static struct { uint32_t id,rect[4],client[4];int sup,parent; } sv_fix;

static void sv_emit(uint32_t kind,uint32_t a,uint32_t b,const uint32_t *w)
{
    assert(sv_tn<24);
    sv_tp[sv_tn].kind=kind;sv_tp[sv_tn].a=a;sv_tp[sv_tn].b=b;
    if(w)memcpy(sv_tp[sv_tn].w,w,16);else memset(sv_tp[sv_tn].w,0,16);
    ++sv_tn;
}
static void sv_patch_bl(unsigned char *body,uint32_t base,uint32_t at,uint32_t target)
{
    int32_t off=(int32_t)(target-(at+4));
    uint32_t s=((uint32_t)off>>24)&1u,i1=((uint32_t)off>>23)&1u,i2=((uint32_t)off>>22)&1u;
    uint32_t imm10=((uint32_t)off>>12)&0x3ffu,imm11=((uint32_t)off>>1)&0x7ffu;
    uint32_t j1=(~(i1^s))&1u,j2=(~(i2^s))&1u;
    uint16_t hw1=(uint16_t)(0xf000u|(s<<10)|imm10);
    uint16_t hw2=(uint16_t)(0xd000u|(j1<<13)|(j2<<11)|imm11);
    unsigned char *p=stub_destination(body,base,at,4);
    memcpy(p,&hw1,2);memcpy(p+2,&hw2,2);
}
static void sv_stub12(void *mapped,uint32_t base,uint32_t address,uint32_t target)
{
    unsigned char *p=stub_destination(mapped,base,address,12);
    const uint16_t insn[]={0xf8df,0xc004,0x4760,0xbf00};
    assert(!(address&3));memcpy(p,insn,sizeof insn);memcpy(p+8,&target,4);
}
static uint32_t sv_bits(const void *f,size_t n)
{ uint32_t v=0;assert(n==sizeof v);memcpy(&v,f,n);return v; }
static void sv_stub3(void *mapped,uint32_t base,uint32_t address,sv_ret3_fn function)
{ sv_stub12(mapped,base,address,sv_bits(&function,sizeof function)); }

/* Native-side stubs (called by the shipped bodies). */
static void svn_set_id(void *img,uint32_t id)
{ sv_emit(SV_ID,(uint32_t)((uintptr_t)img-(uintptr_t)sv_img_native),id,NULL); }
static uint32_t svn_get_sup(void *img)
{ (void)img;sv_emit(SV_GETSUP,0,0,NULL);return UINT32_C(0xe4c000); }
static uint32_t svn_sup_res(void *sup,uint32_t id,void *out)
{
    (void)sup;sv_emit(SV_SUP,id,(uint32_t)sv_fix.sup,NULL);
    for(unsigned i=0;i<32;++i)((unsigned char *)out)[i]=(unsigned char)i;
    sv_last_out=(uint32_t)(uintptr_t)out;
    return (uint32_t)sv_fix.sup;
}
static void svn_client(void *out,uint32_t parent)
{ (void)parent;sv_emit(SV_CLIENT,1,0,NULL);memcpy(out,sv_fix.client,16); }
static void svn_img_create(void *img,uint32_t res,uint32_t rectp,uint32_t parent)
{
    uint32_t w[4];(void)img;(void)parent;
    memcpy(w,(void *)(uintptr_t)rectp,16);
    for(unsigned i=0;i<32;++i)assert(((const unsigned char *)(uintptr_t)res)[i]==i);
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
    for(unsigned i=0;i<32;++i)((unsigned char *)out)[i]=(unsigned char)i;
    sv_last_out=(uint32_t)(uintptr_t)out;
    return sv_fix.sup;
}
static void svc_client(void *parent,uint32_t out[4])
{ (void)parent;sv_emit(SV_CLIENT,1,0,NULL);memcpy(out,sv_fix.client,16); }
static void svc_img_create(void *img,const void *res,const uint32_t rect[4],void *parent)
{
    (void)img;(void)parent;
    for(unsigned i=0;i<32;++i)assert(((const unsigned char *)res)[i]==i);
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

/* imageCreate stubs (1-10 argument preserving trampolines). */
typedef uint32_t (*sv_ret4_fn)(void *,uint32_t,uint32_t,uint32_t);
typedef void (*sv_draw_fn)(void *,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,float,float,uint32_t);
typedef void (*sv_scaled_fn)(void *,uint32_t,uint32_t,uint32_t,uint32_t);
static uint32_t sv_bits_draw(sv_draw_fn f) { uint32_t v;assert(sizeof f==sizeof v);memcpy(&v,&f,4);return v; }
static uint32_t sv_bits_scaled(sv_scaled_fn f) { uint32_t v;assert(sizeof f==sizeof v);memcpy(&v,&f,4);return v; }
static uint32_t sv_bits_ret4(sv_ret4_fn f) { uint32_t v;assert(sizeof f==sizeof v);memcpy(&v,&f,4);return v; }
static void svn_pw(void *img,uint32_t parent)
{ sv_emit(SV_PW,(uint32_t)((uintptr_t)img-(uintptr_t)sv_img_native),parent,NULL); }
static void svn_wr(void *img,uint32_t rectp)
{ uint32_t w[4];(void)img;memcpy(w,(void *)(uintptr_t)rectp,16);sv_emit(SV_WR,0,0,w); }
static uint32_t svn_ic(void *img) { (void)img;sv_emit(SV_IC,0,0,NULL);return (uint32_t)sv_created; }
static void svn_resize(void *img,uint32_t rectp)
{ uint32_t w[4];(void)img;memcpy(w,(void *)(uintptr_t)rectp,16);sv_emit(SV_RES,0,0,w); }
static void svn_target(void *out,uint32_t self)
{
    (void)self;sv_token=UINT32_C(0x7000);
    ((uint32_t *)out)[0]=sv_token;((uint32_t *)out)[1]=0;
    sv_emit(SV_TH,0,0,NULL);
}
static void svn_release(void *ctrl) { (void)ctrl;sv_emit(SV_REL,sv_token,0,NULL); }
static void svn_draw(void *img,uint32_t res,uint32_t x,uint32_t y,uint32_t flag,
    uint32_t target,uint32_t tiling,float f0,float f1,uint32_t scaling)
{
    uint32_t b0,b1,w[4];
    (void)img;memcpy(&b0,&f0,4);memcpy(&b1,&f1,4);
    assert(res==UINT32_C(0x6000) && !x && !y);
    { uint32_t tok=0;memcpy(&tok,(void *)(uintptr_t)target,4);target=tok; }
    w[0]=flag;w[1]=target;w[2]=tiling;w[3]=scaling;
    sv_emit(SV_DRAW,b0,b1,w);
}
static void svn_scaled(void *img,uint32_t res,uint32_t x,uint32_t y,uint32_t target)
{ uint32_t tok=0;(void)img;assert(res==UINT32_C(0x6000) && !x && !y);
  memcpy(&tok,(void *)(uintptr_t)target,4);sv_emit(SV_SCALED,tok,0,NULL); }
static uint32_t svn_csurf(void *img,uint32_t rectp,uint32_t a,uint32_t b)
{ uint32_t w[4];(void)img;memcpy(w,(void *)(uintptr_t)rectp,16);sv_emit(SV_CS,a,b,w);return UINT32_C(0x8000); }
static void svn_suis(void *img,uint32_t surface)
{ (void)img;sv_emit(SV_SU,surface,0,NULL); }
static void svn_cwin(void *img) { (void)img;sv_emit(SV_CW,0,0,NULL); }

/* Owned-side imageCreate callbacks. */
static void svc_pw(void *img,void *parent)
{ sv_emit(SV_PW,(uint32_t)((uintptr_t)img-(uintptr_t)sv_img_ref),(uint32_t)(uintptr_t)parent,NULL); }
static void svc_wr(void *img,const uint32_t rect[4])
{ (void)img;sv_emit(SV_WR,0,0,rect); }
static int svc_ic(void *img) { (void)img;sv_emit(SV_IC,0,0,NULL);return sv_created; }
static void svc_resize(void *img,const uint32_t rect[4])
{ (void)img;sv_emit(SV_RES,0,0,rect); }
static void *svc_target(void *img) { (void)img;sv_token=UINT32_C(0x7000);sv_emit(SV_TH,0,0,NULL);return (void *)(uintptr_t)sv_token; }
static void svc_release(void *img,void *target)
{ (void)img;sv_emit(SV_REL,(uint32_t)(uintptr_t)target,0,NULL); }
static void svc_draw(void *img,const void *res,uint32_t x,uint32_t y,uint32_t flag,
    void *target,uint32_t tiling,int32_t f0,int32_t f1,uint32_t scaling)
{
    uint32_t w[4];(void)img;
    assert((uint32_t)(uintptr_t)res==UINT32_C(0x6000) && !x && !y);
    w[0]=flag;w[1]=(uint32_t)(uintptr_t)target;w[2]=tiling;w[3]=scaling;
    sv_emit(SV_DRAW,(uint32_t)f0,(uint32_t)f1,w);
}
static void svc_scaled(void *img,const void *res,uint32_t x,uint32_t y,void *target)
{ (void)img;assert((uint32_t)(uintptr_t)res==UINT32_C(0x6000) && !x && !y);
  sv_emit(SV_SCALED,(uint32_t)(uintptr_t)target,0,NULL); }
static void *svc_csurf(void *img,const uint32_t rect[4],int a,int b)
{ (void)img;sv_emit(SV_CS,(uint32_t)a,(uint32_t)b,rect);return (void *)(uintptr_t)UINT32_C(0x8000); }
static void svc_suis(void *img,void *surface)
{ (void)img;sv_emit(SV_SU,(uint32_t)(uintptr_t)surface,0,NULL); }
static void svc_cwin(void *img) { (void)img;sv_emit(SV_CW,0,0,NULL); }
static uint32_t svc_field(void *img,uint32_t off)
{ (void)img;assert(off>=0xa8 && off<=0xbc && !(off&3));return sv_fields[off/4]; }
static struct oa_svg_image_surface_ops sv_sops(void)
{
    return (struct oa_svg_image_surface_ops){svc_pw,svc_wr,svc_ic,svc_resize,
        svc_target,svc_release,svc_draw,svc_scaled,svc_csurf,svc_suis,svc_cwin,
        svc_field};
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
    static const uint32_t rects[][4]={{10,20,30,40},{0,0,0,0},{UINT32_MAX,UINT32_MAX,UINT32_MAX-1,UINT32_MAX-2}};
    static const uint32_t clients[4]={1,2,7,8};
    static const uint32_t ids[]={0,0x37,UINT32_MAX};
    sv_create_fn cf;{ uint32_t entry=0x10c9639;memcpy(&cf,&entry,4); }
    sv_destroy_fn df;{ uint32_t entry=0x10c8ef5;memcpy(&df,&entry,4); }
    unsigned compared=0;
    for(unsigned s=0;s<3;++s)for(unsigned p=0;p<2;++p)
    for(unsigned i=0;i<3;++i)for(unsigned r=0;r<3;++r) {
        struct oa_svg_image_ops ops=sv_ops();
        void *parent=p?(void *)(uintptr_t)0x2222:NULL;
        sv_fix.id=ids[i];sv_fix.sup=s==2?-1:(int)s;sv_fix.parent=(int)p;
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
    /* imageCreate (0x10c9430): copy the body over the create stub and run a
     * created/uncreated, 0xb0, field-set and parent grid. */
    {
        unsigned char *dtor=page(0x95a000,4096);
        assert(!mprotect(cc,4096,PROT_READ|PROT_WRITE));
        size_t o2=file_offset(elf,bytes,0x10c9430,288);
        assert(crc32(elf+o2,288)==UINT32_C(0x5c029f6e));
        memcpy(stub_destination(cc,0x10c9000,0x10c9430,288),elf+o2,288);
        memcpy(stub_destination(cc,0x10c9000,0x10c9550,4),&pointer,4);
        pair_stub(cc,0x10c9000,0x10c9580,svn_target);
        sv_patch_bl(cc,0x10c9000,0x10c9480,0x10c9580);
        sv_patch_bl(cc,0x10c9000,0x10c94e2,0x10c9580);
        sv_patch_bl(cc,0x10c9000,0x10c94fe,0x10c9580);
        executable(cc,4096);
        assert(!mprotect(win,4096,PROT_READ|PROT_WRITE));
        assert(!mprotect(ctx,4096,PROT_READ|PROT_WRITE));
        assert(!mprotect(winx,4096,PROT_READ|PROT_WRITE));
        pair_stub(win,0x1152000,0x1152870,svn_pw);
        thumb_stub(win,0x1152000,0x1152a40,(getter)svn_ic);
        sv_stub12(win,0x1152000,0x1152ce8,sv_bits_ret4(svn_csurf));
        blank_stub(win,0x1152000,0x1152e5c,svn_cwin);
        pair_stub(ctx,0xeed000,0xeede60,svn_suis);
        pair_stub(ctx,0xeed000,0xeedf38,svn_resize);
        pair_stub(winx,0xeee000,0xeeeb6c,svn_wr);
        sv_stub12(winx,0xeee000,0xeee384,sv_bits_draw(svn_draw));
        sv_stub12(winx,0xeee000,0xeee430,sv_bits_scaled(svn_scaled));
        blank_stub(dtor,0x95a000,0x95a990,svn_release);
        executable(dtor,4096);executable(win,4096);executable(ctx,4096);executable(winx,4096);
        static const uint32_t r[4]={10,20,30,40};
        static const uint32_t fs[2][5]={{1,2,3,4,5},{0,7,8,9,10}};
        void (*icf)(void *,uint32_t,uint32_t,uint32_t);
        { uint32_t entry=0x10c9431;memcpy(&icf,&entry,4); }
        for(unsigned c=0;c<2;++c)for(unsigned b0=0;b0<2;++b0)
        for(unsigned f=0;f<2;++f)for(unsigned par=0;par<2;++par) {
            struct oa_svg_image_surface_ops so=sv_sops();
            void *parent=par?(void *)(uintptr_t)0x2222:NULL;
            sv_created=(int)c;sv_parent=(int)par;
            sv_fields[0xa8/4]=fs[f][0];sv_fields[0xac/4]=fs[f][1];
            sv_fields[0xb0/4]=b0?1:0;sv_fields[0xb4/4]=fs[f][2];
            sv_fields[0xb8/4]=fs[f][3];sv_fields[0xbc/4]=fs[f][4];
            for(unsigned k=0;k<0x40;++k) {
                if(k>=0xa8/4 && k<=0xbc/4) {
                    memcpy(sv_img_native+k*4,&sv_fields[k],4);
                    memcpy(sv_img_ref+k*4,&sv_fields[k],4);
                }
            }
            sv_tp=sv_evn;sv_tn=0;sv_token=0;
            icf(sv_img_native,0x6000,(uint32_t)(uintptr_t)r,(uint32_t)(uintptr_t)parent);
            memcpy(sv_cmp,sv_evn,sv_tn*sizeof sv_evn[0]);unsigned n=sv_tn;
            sv_tp=sv_evc;sv_tn=0;sv_token=0;
            oa_svg_image_image_create(sv_img_ref,(void *)(uintptr_t)0x6000,r,parent,&so);
            if(n!=sv_tn || memcmp(sv_cmp,sv_evc,n*sizeof sv_evn[0])) {
                fprintf(stderr,"imageCreate mismatch c=%u b0=%u f=%u par=%u n=%u/%u\n",c,b0,f,par,n,sv_tn);
                for(unsigned k=0;k<(n>sv_tn?n:sv_tn);++k) {
                    fprintf(stderr,"  [%u] nat k=%u a=%x b=%x w=%x/%x/%x/%x  ours k=%u a=%x b=%x w=%x/%x/%x/%x\n",
                        k,k<n?sv_cmp[k].kind:0xffff,k<n?sv_cmp[k].a:0,k<n?sv_cmp[k].b:0,
                        k<n?sv_cmp[k].w[0]:0,k<n?sv_cmp[k].w[1]:0,k<n?sv_cmp[k].w[2]:0,k<n?sv_cmp[k].w[3]:0,
                        k<sv_tn?sv_evc[k].kind:0xffff,k<sv_tn?sv_evc[k].a:0,k<sv_tn?sv_evc[k].b:0,
                        k<sv_tn?sv_evc[k].w[0]:0,k<sv_tn?sv_evc[k].w[1]:0,k<sv_tn?sv_evc[k].w[2]:0,k<sv_tn?sv_evc[k].w[3]:0);
                }
                exit(1);
            }
            ++compared;
        }
        assert(!munmap(dtor,4096));
    }
    /* Point overload (0x10c9554): rectangle from a point, client offset and
     * size clamping, then imageCreate. EASIRect::EASIRect() is copied too. */
    {
        unsigned char *ef=page(0x9a7000,4096);
        size_t eo=file_offset(elf,bytes,0x9a7f94,52);
        assert(crc32(elf+eo,52)==UINT32_C(0x2a2b3d80));
        memcpy(stub_destination(ef,0x9a7000,0x9a7f94,52),elf+eo,52);
        memcpy(stub_destination(ef,0x9a7000,0x9a7fc8,4),&pointer,4);
        executable(ef,4096);
        assert(!mprotect(cc,4096,PROT_READ|PROT_WRITE));
        size_t po=file_offset(elf,bytes,0x10c9554,228);
        assert(crc32(elf+po,228)==UINT32_C(0x09f01190));
        memcpy(stub_destination(cc,0x10c9000,0x10c9554,228),elf+po,228);
        memcpy(stub_destination(cc,0x10c9000,0x10c9634,4),&pointer,4);
        quad_stub(cc,0x10c9000,0x10c9430,svn_img_create);
        executable(cc,4096);
        static const int32_t pts[2][2]={{10,20},{-5,7}};
        void (*pf)(void *,uint32_t,uint32_t,uint32_t);
        { uint32_t entry=0x10c9555;memcpy(&pf,&entry,4); }
        for(unsigned sup=0;sup<2;++sup)for(unsigned par=0;par<2;++par)
        for(unsigned pv=0;pv<2;++pv) {
            struct oa_svg_image_ops po=sv_ops();
            void *parent=par?(void *)(uintptr_t)0x2222:NULL;
            sv_fix.sup=(int)sup;sv_fix.parent=(int)par;
            memcpy(sv_fix.client,clients,16);
            sv_tp=sv_evn;sv_tn=0;sv_last_out=0;
            pf(sv_img_native,0x21,(uint32_t)(uintptr_t)pts[pv],(uint32_t)(uintptr_t)parent);
            memcpy(sv_cmp,sv_evn,sv_tn*sizeof sv_evn[0]);unsigned n=sv_tn;
            sv_tp=sv_evc;sv_tn=0;sv_last_out=0;
            oa_svg_image_create_point(sv_img_ref,0x21,pts[pv],parent,&po);
            if(n!=sv_tn || memcmp(sv_cmp,sv_evc,n*sizeof sv_evn[0])) {
                fprintf(stderr,"point create mismatch sup=%u par=%u pv=%u n=%u/%u\n",sup,par,pv,n,sv_tn);
                exit(1);
            }
            ++compared;
        }
        assert(!munmap(ef,4096));
    }
    free(sv_img_native);free(sv_img_ref);
    assert(!munmap(cb,4096) && !munmap(cc,4096) && !munmap(off,4096));
    assert(!munmap(win,4096) && !munmap(ctx,4096) && !munmap(winx,4096) && !munmap(sup,4096));
    printf("ok offline shipped ARM svg image: %u native/C cases, create/imageCreate/point/supplier gate/rect offset and destroy order match\n",compared);
}

#endif
