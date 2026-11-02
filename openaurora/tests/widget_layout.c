#include "openaurora/widget_layout.h"
#include "openaurora/svg_pack.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

struct bytes { uint8_t data[4096];size_t size; };
static void byte(struct bytes *b,unsigned n) { assert(b->size<sizeof b->data);b->data[b->size++]=(uint8_t)n; }
static void word(struct bytes *b,int n) { byte(b,(unsigned)n);byte(b,(unsigned)n>>8); }
static void string(struct bytes *b,const char *s) { do { byte(b,(unsigned char)*s); } while(*s++); }
static void group(struct bytes *b,const char *s) { byte(b,8);string(b,s); }
static void rect(struct bytes *b,const char *s,int x,int y,int w,int h)
{
    byte(b,1);string(b,s);word(b,x);word(b,y);word(b,w);word(b,h);word(b,1);byte(b,10);
    string(b,"none");string(b,"#123456");string(b,"");string(b,"");word(b,0);
}
static void equal(const struct oa_text_rect *a,int l,int t,int r,int b)
{ assert(a->left==l && a->top==t && a->right==r && a->bottom==b); }
static void synthetic(void)
{
    struct bytes b={0};group(&b,"rowItems");
    rect(&b,"before",13,25,4,5);
    rect(&b,"mRowM",10,20,10,30);
    group(&b,"nested");rect(&b,"after",13,25,4,5);byte(&b,9);
    rect(&b,"mRowM",100,200,1,1); /* duplicate never replaces the first cached mask */
    rect(&b,"last",14,27,4,5);byte(&b,9);
    group(&b,"rowItems");rect(&b,"other",1,2,3,4);byte(&b,9);
    struct oa_binsvg v;assert(!oa_binsvg_init(&v,b.data,b.size));
    struct oa_text_rect surface={100,200,150,260};
    struct oa_binsvg_rect r;
    assert(!oa_widget_group_rect(&v,"rowItems","mRowM",&surface,"before",&r));equal(&r.bounds,13,25,17,30);
    assert(!oa_widget_group_rect(&v,"rowItems","mRowM",&surface,"mRowM",&r));equal(&r.bounds,10,20,20,50);
    assert(!oa_widget_group_rect(&v,"rowItems","mRowM",&surface,"after",&r));equal(&r.bounds,115,205,135,210);
    assert(!strcmp(r.fill,"#123456") && r.opacity==10);
    assert(!oa_widget_group_rect(&v,"rowItems","mRowM",&surface,"last",&r));equal(&r.bounds,120,207,140,212);
    struct oa_binsvg_rect saved=r;
    assert(oa_widget_group_rect(&v,"rowItems","mRowM",&surface,"other",&r)==ENOENT);
    assert(!memcmp(&r,&saved,sizeof r));
    assert(!oa_widget_group_rect(&v,"rowItems","absent",&surface,"after",&r));equal(&r.bounds,13,25,17,30);
    struct oa_text_rect client={20,100,70,160},window;
    assert(!oa_widget_row_item_window(&v,&client,"after",&window));equal(&window,35,105,55,110);
    client=(struct oa_text_rect){INT_MAX,0,INT_MAX,60};
    struct oa_text_rect old=window;
    assert(oa_widget_row_item_window(&v,&client,"before",&window)==EOVERFLOW && !memcmp(&old,&window,sizeof old));
    client=(struct oa_text_rect){INT_MIN,0,INT_MAX,60};
    assert(oa_widget_row_item_window(&v,&client,"after",&window)==EOVERFLOW && !memcmp(&old,&window,sizeof old));
    assert(oa_widget_group_rect(&v,"","mRowM",&surface,"after",&r)==EINVAL);
    size_t at=0;struct oa_binsvg_element e;assert(!oa_binsvg_next(&v,&at,&e));
    saved=r;assert(oa_binsvg_rectangle(&v,e.offset,&r)==ENOTSUP && !memcmp(&saved,&r,sizeof r));
    assert(oa_binsvg_rectangle(&v,v.size,&r)==ENOENT);
    /* Animations in renderGroup have defective seek/closing behavior in
     * firmware. Explicitly reject rather than pretending this is supported. */
    b=(struct bytes){0};group(&b,"rowItems");byte(&b,10);string(&b,"");byte(&b,1);word(&b,0);
    for(unsigned i=0;i<3;++i)string(&b,"");
    word(&b,0);
    for(unsigned i=0;i<6;++i)string(&b,"");
    byte(&b,9);
    assert(!oa_binsvg_init(&v,b.data,b.size));
    assert(oa_widget_group_rect(&v,"rowItems","mRowM",&surface,"x",&r)==ENOTSUP);
    puts("ok widget windows: ordered mask discovery, pre-mask identity, first duplicate, nested/first group, offset/bounds, animation rejection");
}
static void corpus(const char *index,const char *binary)
{
    struct oa_svg_pack *pack=NULL;assert(!oa_svg_pack_load(&pack,index,binary));
    struct oa_binsvg v;assert(!oa_svg_pack_named(pack,"SettingItemsListBoxRowItems.svg",&v));
    /* Conditional composition: this explicit client is not a live object read.
     * Resource460 supplies every child/mask coordinate. */
    struct oa_text_rect client={20,186,769,256},r;
    assert(!oa_widget_row_item_window(&v,&client,"text1",&r));equal(&r,30,187,381,237);
    assert(!oa_widget_row_item_window(&v,&client,"labelLong",&r));equal(&r,30,187,480,237);
    assert(!oa_widget_row_item_window(&v,&client,"text2",&r));equal(&r,638,183,763,237);
    /* Advancing a row translates child windows; its geometry is not stretched
     * again by the parent origin. Negative local Y is preserved, no clipping. */
    client.top+=70;client.bottom+=70;
    assert(!oa_widget_row_item_window(&v,&client,"text2",&r));equal(&r,638,253,763,307);
    oa_svg_pack_free(pack);
    puts("ok widget windows: actual460 ordered rowItems/mRowM -> text1/labelLong/text2, explicit749x70 client and parent origin");
}
int main(int argc,char **argv)
{ synthetic();if(argc==3)corpus(argv[1],argv[2]);else assert(argc==1);return 0; }
