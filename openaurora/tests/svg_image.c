#include "openaurora/svg_image.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

static unsigned count;
static unsigned char image_object[0x200];
static struct { uint32_t kind,id,flag,rect[4]; } ev[32];
static int sup_ok=1;
static uint32_t client[4]={1,2,7,8};
static uintptr_t last_res;
static int res_match;
static int clean_ok;

enum {E_ID,E_GETSUP,E_SUP,E_CLIENT,E_IMG,E_WD,E_CLEAN};

static void push(uint32_t kind,uint32_t id,uint32_t flag)
{
    assert(count<32);
    ev[count].kind=kind;ev[count].id=id;ev[count].flag=flag;
    memset(ev[count].rect,0,sizeof ev[count].rect);
    ++count;
}
static void sv_set_id(void *img,int32_t id) { (void)img;push(E_ID,(uint32_t)id,0); }
static void *sv_get_supplier(void *img) { (void)img;push(E_GETSUP,0,0);return (void *)0x1000; }
static int sv_supplier_get(void *sup,uint32_t id,void *out)
{
    (void)sup;push(E_SUP,id,(uint32_t)sup_ok);
    assert((uintptr_t)out%_Alignof(max_align_t)==0);
    last_res=(uintptr_t)out;
    for(unsigned i=0;i<OA_SVG_IMAGE_RESOURCE_BYTES;++i)((unsigned char *)out)[i]=(unsigned char)i;
    return sup_ok;
}
static void sv_get_client(void *parent,uint32_t out[4])
{
    (void)parent;push(E_CLIENT,1,0);memcpy(out,client,sizeof client);
}
static void sv_image_create(void *img,const void *res,const uint32_t rect[4],void *parent)
{
    (void)img;(void)parent;res_match=((uintptr_t)res==last_res);
    for(unsigned i=0;i<OA_SVG_IMAGE_RESOURCE_BYTES;++i)assert(((const unsigned char *)res)[i]==i);
    push(E_IMG,res_match,0);memcpy(ev[count-1].rect,rect,sizeof ev[count-1].rect);
}
static void sv_window_destroy(void *img) { (void)img;push(E_WD,0,0); }
static void sv_clean(void *img)
{
    clean_ok=(img==(unsigned char *)image_object+0x9c);push(E_CLEAN,clean_ok,0);
}

static struct oa_svg_image_ops ops(void)
{
    return (struct oa_svg_image_ops){sv_set_id,sv_get_supplier,sv_supplier_get,
        sv_get_client,sv_image_create,sv_window_destroy,sv_clean};
}

/* Second ops table: SVGImage::imageCreate. */
enum {S_PARENT,S_WRECT,S_IS,S_RESIZE,S_TARGET,S_RELEASE,S_DRAW,S_SCALED,
      S_CSURF,S_SUIS,S_CWIN};
static struct { uint32_t kind,a,b,c,d,e,f;uint32_t w[4]; } sev[32];
static unsigned sn;
static int s_created, s_b0;
static uint32_t s_fields[0x40];
static void spush(uint32_t kind)
{ assert(sn<32);memset(&sev[sn],0,sizeof sev[sn]);sev[sn].kind=kind;sn++; }
static void s_set_parent(void *img,void *parent)
{ (void)img;spush(S_PARENT);sev[sn-1].a=(uint32_t)(uintptr_t)parent; }
static void s_set_wrect(void *img,const uint32_t rect[4])
{ (void)img;spush(S_WRECT);memcpy(sev[sn-1].w,rect,16); }
static int s_is_created(void *img) { (void)img;spush(S_IS);return s_created; }
static void s_resize(void *img,const uint32_t rect[4])
{ (void)img;spush(S_RESIZE);memcpy(sev[sn-1].w,rect,16); }
static void *s_target(void *img) { (void)img;spush(S_TARGET);return (void *)0x7000; }
static void s_release(void *img,void *target)
{ (void)img;spush(S_RELEASE);sev[sn-1].a=(uint32_t)(uintptr_t)target; }
static void s_draw(void *img,const void *res,uint32_t x,uint32_t y,uint32_t flag,
    void *target,uint32_t tiling,int32_t f0,int32_t f1,uint32_t scaling)
{
    (void)img;spush(S_DRAW);
    sev[sn-1].a=flag;sev[sn-1].b=(uint32_t)(uintptr_t)target;
    sev[sn-1].c=tiling;sev[sn-1].d=(uint32_t)f0;sev[sn-1].e=(uint32_t)f1;
    sev[sn-1].f=scaling;
    assert((uint32_t)(uintptr_t)res==0x6000 && x==0 && y==0);
}
static void s_scaled(void *img,const void *res,uint32_t x,uint32_t y,void *target)
{ (void)img;spush(S_SCALED);sev[sn-1].a=(uint32_t)(uintptr_t)target;
  assert((uint32_t)(uintptr_t)res==0x6000 && x==0 && y==0); }
static void *s_csurf(void *img,const uint32_t rect[4],int a,int b)
{ (void)img;spush(S_CSURF);memcpy(sev[sn-1].w,rect,16);sev[sn-1].a=(uint32_t)a;sev[sn-1].b=(uint32_t)b;return (void *)0x8000; }
static void s_suis(void *img,void *surface)
{ (void)img;spush(S_SUIS);sev[sn-1].a=(uint32_t)(uintptr_t)surface; }
static void s_cwin(void *img) { (void)img;spush(S_CWIN); }
static uint32_t s_field(void *img,uint32_t off)
{ (void)img;assert(off>=0xa8 && off<=0xbc && !(off&3));return s_fields[off/4]; }
static struct oa_svg_image_surface_ops sops(void)
{ return (struct oa_svg_image_surface_ops){s_set_parent,s_set_wrect,s_is_created,
    s_resize,s_target,s_release,s_draw,s_scaled,s_csurf,s_suis,s_cwin,s_field}; }

int main(void)
{
    static const uint32_t rect[4]={10,20,30,40};
    struct oa_svg_image_ops o;
    void *img=image_object;

    /* create without a parent: setId, supplier lookup, imageCreate with the
     * rectangle unchanged and the resource token from the supplier. */
    count=0;sup_ok=1;o=ops();
    oa_svg_image_create(img,0x37,rect,NULL,&o);
    assert(count==4);
    assert(ev[0].kind==E_ID && ev[0].id==0x37);
    assert(ev[1].kind==E_GETSUP);
    assert(ev[2].kind==E_SUP && ev[2].id==0x37 && ev[2].flag==1);
    assert(ev[3].kind==E_IMG && ev[3].id==1);
    assert(ev[3].rect[0]==10 && ev[3].rect[1]==20 && ev[3].rect[2]==30 && ev[3].rect[3]==40);

    /* With a parent the client rect offsets x into words 0/2 and y into
     * words 1/3. */
    count=0;o=ops();
    oa_svg_image_create(img,7,rect,img,&o);
    assert(count==5);
    assert(ev[3].kind==E_CLIENT);
    assert(ev[4].kind==E_IMG);
    assert(ev[4].rect[0]==11 && ev[4].rect[1]==22 && ev[4].rect[2]==31 && ev[4].rect[3]==42);

    /* A missing resource skips imageCreate entirely. */
    count=0;sup_ok=0;o=ops();
    oa_svg_image_create(img,9,rect,NULL,&o);
    assert(count==3);
    assert(ev[2].kind==E_SUP && ev[2].flag==0);
    sup_ok=1;

    /* destroy: window destroy then clean UI surface. */
    count=0;o=ops();
    oa_svg_image_destroy(img,&o);
    assert(count==2);
    assert(ev[0].kind==E_WD);
    assert(ev[1].kind==E_CLEAN && ev[1].id==1);

    /* NULL guards do nothing. */
    count=0;o=ops();
    oa_svg_image_create(NULL,1,rect,NULL,&o);
    oa_svg_image_create(img,1,NULL,NULL,&o);
    oa_svg_image_destroy(NULL,&o);
    o.image_create=NULL;oa_svg_image_create(img,1,rect,NULL,&o);
    o=ops();o.supplier_get_resource=NULL;oa_svg_image_create(img,1,rect,NULL,&o);
    o=ops();o.get_client_rect=NULL;oa_svg_image_create(img,1,rect,img,&o);
    o=ops();o.set_clean_ui_surface=NULL;oa_svg_image_destroy(img,&o);
    assert(count==0);

    { const int32_t pt[2]={1,2};
      o=ops();o.set_id=NULL;oa_svg_image_create_point(img,1,pt,NULL,&o);
      o=ops();o.get_client_rect=NULL;oa_svg_image_create_point(img,1,pt,img,&o);
      assert(count==0); }

    /* imageCreate, created window: parent/window rect, surface resize, then a
     * full draw with the image's 0xa8/0xac/0xb4/0xb8/0xbc fields and the
     * target token released afterwards. */
    {
        static const uint32_t r[4]={10,20,30,40};
        struct oa_svg_image_surface_ops so=sops();
        sn=0;so.release_target=NULL;
        oa_svg_image_image_create((void *)0x1000,(void *)0x6000,r,NULL,&so);
        assert(sn==0);so=sops();
        sn=0;s_created=1;
        s_fields[0xa8/4]=1;s_fields[0xac/4]=2;s_fields[0xb0/4]=0;
        s_fields[0xb4/4]=0x3f800000;s_fields[0xb8/4]=0x40000000;s_fields[0xbc/4]=5;
        oa_svg_image_image_create((void *)0x1000,(void *)0x6000,r,(void *)0x2000,&so);
        assert(sn==7);
        assert(sev[0].kind==S_PARENT && sev[0].a==0x2000);
        assert(sev[1].kind==S_WRECT && sev[1].w[0]==10 && sev[1].w[3]==40);
        assert(sev[2].kind==S_IS);
        assert(sev[3].kind==S_RESIZE && sev[3].w[1]==20);
        assert(sev[4].kind==S_TARGET);
        assert(sev[5].kind==S_DRAW);
        assert(sev[5].a==1 && sev[5].b==0x7000 && sev[5].c==2);
        assert(sev[5].d==0x3f800000 && sev[5].e==0x40000000 && sev[5].f==5);
        assert(sev[6].kind==S_RELEASE && sev[6].a==0x7000);
        /* Uncreated with 0xb0 set: surface create/set, scaled draw, release,
         * then UIWindow::create. */
        sn=0;s_created=0;s_b0=1;s_fields[0xb0/4]=1;
        oa_svg_image_image_create((void *)0x1000,(void *)0x6000,r,NULL,&so);
        assert(sn==9);
        assert(sev[0].kind==S_PARENT && sev[0].a==0);
        assert(sev[3].kind==S_CSURF && sev[3].a==1 && sev[3].b==0);
        assert(sev[3].w[2]==30);
        assert(sev[4].kind==S_SUIS && sev[4].a==0x8000);
        assert(sev[5].kind==S_TARGET);
        assert(sev[6].kind==S_SCALED && sev[6].a==0x7000);
        assert(sev[7].kind==S_RELEASE && sev[7].a==0x7000);
        assert(sev[8].kind==S_CWIN);
        /* Uncreated without 0xb0: full draw instead of scaled, then create. */
        sn=0;s_created=0;s_fields[0xb0/4]=UINT32_C(0xffffff00); /* ldrb, not word truth */
        oa_svg_image_image_create((void *)0x1000,(void *)0x6000,r,NULL,&so);
        assert(sn==9);
        assert(sev[6].kind==S_DRAW && sev[6].a==1 && sev[6].c==2);
        assert(sev[7].kind==S_RELEASE && sev[7].a==0x7000);
        assert(sev[8].kind==S_CWIN);
    }
    /* Point overload: rectangle from the point; without a parent the
     * resource's size words at +8/+12 are added directly, with a parent the
     * client origin is added and both extents clamp to the client area. */
    {
        static const int32_t pt[2]={10,20};
        struct oa_svg_image_ops po=ops();
        count=0;sup_ok=1;
        oa_svg_image_create_point(img,0x21,pt,NULL,&po);
        assert(count==4);
        assert(ev[0].kind==E_ID && ev[0].id==0x21);
        assert(ev[3].kind==E_IMG && ev[3].id==1);
        assert(ev[3].rect[0]==10 && ev[3].rect[1]==20);
        assert(ev[3].rect[2]==10u+UINT32_C(0x0b0a0908));
        assert(ev[3].rect[3]==20u+UINT32_C(0x0f0e0d0c));
        count=0;
        oa_svg_image_create_point(img,0x22,pt,img,&po);
        assert(count==7);
        assert(ev[3].kind==E_CLIENT && ev[4].kind==E_CLIENT && ev[5].kind==E_CLIENT);
        assert(ev[6].kind==E_IMG);
        assert(ev[6].rect[0]==11 && ev[6].rect[1]==22);
        assert(ev[6].rect[2]==17 && ev[6].rect[3]==28);
        count=0;sup_ok=0;
        oa_svg_image_create_point(img,0x23,pt,NULL,&po);
        assert(count==3);
        sup_ok=1;
    }
    puts("ok svg image: create setId/supplier/resource gate and destroy order");
    return 0;
}
