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

    puts("ok svg image: create setId/supplier/resource gate and destroy order");
    return 0;
}
