#include "openaurora/widget_layout.h"
#include "openaurora/font_pack.h"
#include "openaurora/font_bitmap.h"
#include "openaurora/font_finder.h"
#include "openaurora/svg_pack.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct bytes { uint8_t data[8192];size_t size; };
static void byte(struct bytes *b,unsigned n) { assert(b->size<sizeof b->data);b->data[b->size++]=(uint8_t)n; }
static void word(struct bytes *b,int n) { byte(b,(unsigned)n);byte(b,(unsigned)n>>8); }
static void string(struct bytes *b,const char *s) { do { byte(b,(unsigned char)*s); } while(*s++); }
static void group(struct bytes *b,const char *s) { byte(b,8);string(b,s); }
static void rect(struct bytes *b,const char *s,int x,int y,int w,int h)
{
    byte(b,1);string(b,s);word(b,x);word(b,y);word(b,w);word(b,h);word(b,1);byte(b,10);
    string(b,"none");string(b,"#123456");string(b,"");string(b,"");word(b,0);
}
static void fixture(struct bytes *b,unsigned align,unsigned font,int mask_height)
{
    static const char *const names[]={"fontRegular","fontBold","fontLight","fontItalic","fontBoldItalic","fontLightItalic"};
    *b=(struct bytes){0};
    /* These caps must not reduce the full-height one-line row surface. */
    group(b,"gTopL");rect(b,"mTopL",0,0,1,14);byte(b,9);
    group(b,"gBotL");rect(b,"mBotL",0,0,1,19);byte(b,9);
    group(b,"gRowL");rect(b,"mRowL",1,9,1,30);
    if(align&1)rect(b,"alignment",1,9,1,30);
    byte(b,9);
    group(b,"gRowR");rect(b,"mRowR",55,10,1,30);
    if(align&2)rect(b,"alignment",55,10,1,30);
    byte(b,9);
    group(b,"gRowM");rect(b,"mRowM",9,8,40,mask_height);rect(b,"textPos",9,3,40,40);
    if(align&4)rect(b,"alignment",9,8,1,30);
    byte(b,9);
    for(unsigned i=0;i<6;++i)if(font&(1u<<i))rect(b,names[i],0,0,1,1);
}
static void equal(const struct oa_text_rect *a,int l,int t,int r,int b)
{ assert(a->left==l && a->top==t && a->right==r && a->bottom==b); }
static void synthetic(void)
{
    struct bytes b;struct oa_binsvg v;
    const struct oa_text_rect window={20,100,122,150};
    struct oa_widget_textbox result;
    for(unsigned align=0;align<8;++align)for(unsigned font=0;font<6;++font) {
        fixture(&b,align,1u<<font,30);assert(!oa_binsvg_init(&v,b.data,b.size));
        assert(!oa_widget_single_line_textbox(&v,&window,&result));
        equal(&result.text.bounds,21,95,121,161); /* H50, not50-14-19 */
        assert(result.alignment==((align&1)?4:(align&2)?6:(align&4)?5:4));
        assert(result.font_type==font && !strcmp(result.text.fill,"#123456"));
    }
    fixture(&b,0,3,30);assert(!oa_binsvg_init(&v,b.data,b.size));
    assert(!oa_widget_single_line_textbox(&v,&window,&result) && result.font_type==0);
    struct oa_widget_textbox saved=result;
    fixture(&b,0,0,0);assert(!oa_binsvg_init(&v,b.data,b.size));
    assert(oa_widget_single_line_textbox(&v,&window,&result)==EDOM && !memcmp(&saved,&result,sizeof saved));
    const struct oa_text_rect huge={INT_MIN,0,INT_MAX,50};
    assert(oa_widget_single_line_textbox(&v,&huge,&result)==EOVERFLOW && !memcmp(&saved,&result,sizeof saved));
    fixture(&b,0,0,30);assert(!oa_binsvg_init(&v,b.data,b.size));
    const struct oa_text_rect bottom={20,INT_MAX-50,122,INT_MAX};
    assert(oa_widget_single_line_textbox(&v,&bottom,&result)==EOVERFLOW && !memcmp(&saved,&result,sizeof saved));
    const uint8_t empty_group[]={8,'x',0,9};assert(!oa_binsvg_init(&v,empty_group,sizeof empty_group));
    assert(oa_widget_single_line_textbox(&v,&window,&result)==ENOENT && !memcmp(&saved,&result,sizeof saved));
    b=(struct bytes){0};group(&b,"gRowM");rect(&b,"textPos",9,3,40,40);rect(&b,"mRowM",9,8,40,30);byte(&b,9);
    assert(!oa_binsvg_init(&v,b.data,b.size));
    assert(oa_widget_single_line_textbox(&v,&window,&result)==ENOTSUP && !memcmp(&saved,&result,sizeof saved));
    puts("ok textbox: full-height slices exclude caps, 48 alignment/type cases, regular priority, raw metadata and atomic errors");
}
static void corpus(const char *index,const char *binary,const char *font_index,const char *font_binary)
{
    struct oa_svg_pack *pack=NULL;assert(!oa_svg_pack_load(&pack,index,binary));
    struct oa_binsvg row,ufa,fa;
    assert(!oa_svg_pack_named(pack,"SettingItemsListBoxRowItems.svg",&row));
    assert(!oa_svg_pack_named(pack,"settingListTextboxUFA.svg",&ufa));
    assert(!oa_svg_pack_named(pack,"settingListTextboxFA.svg",&fa));
    const struct oa_text_rect parent={20,186,769,256};
    struct oa_text_rect child;assert(!oa_widget_row_item_window(&row,&parent,"text1",&child));
    equal(&child,30,187,381,237);
    struct oa_widget_textbox a,b;
    assert(!oa_widget_single_line_textbox(&ufa,&child,&a));equal(&a.text.bounds,31,182,380,248);
    assert(!oa_widget_single_line_textbox(&fa,&child,&b));equal(&b.text.bounds,31,181,380,251);
    assert(a.alignment==4 && b.alignment==4 && a.font_type==0 && b.font_type==1);
    assert(!strcmp(a.text.stroke,"#FFFFFF") && !strcmp(b.text.stroke,"#7E000A"));
    assert(!strcmp(a.text.fill,"none") && !strcmp(b.text.fill,"none"));
    struct oa_font_pack fonts={0};assert(!oa_font_pack_init(&fonts,font_index,font_binary));
    struct oa_font_finder finder={0};
    static const unsigned ids[2][10]={{18,17,16,7,6,5,4,3,2,1},{22,21,20,14,13,12,11,10,9,8}};
    for(unsigned type=0;type<2;++type)for(unsigned i=0;i<10;++i) {
        unsigned char *data=NULL;size_t size=0;struct oa_bitmap_font f;int height;
        assert(!oa_font_pack_read(&fonts,ids[type][i],&data,&size));
        assert(!oa_bitmap_font_open(&f,data,size) && !oa_bitmap_font_height(&f,&height));
        assert(!oa_font_finder_add(&finder,ids[type][i],height,type));free(data);
    }
    unsigned selected;
    assert(!oa_font_finder_select(&finder,a.text.bounds.bottom-a.text.bounds.top,1,a.font_type,&selected) && selected==7);
    assert(!oa_font_finder_select(&finder,b.text.bounds.bottom-b.text.bounds.top,1,b.font_type,&selected) && selected==20);
    oa_font_pack_free(&fonts);oa_svg_pack_free(pack);
    puts("ok textbox: actual460->489/488 text surfaces height66/70, regular/bold font7/20 via20 live startup font metrics (explicit parent)");
}
int main(int argc,char **argv)
{ synthetic();if(argc==5)corpus(argv[1],argv[2],argv[3],argv[4]);else assert(argc==1);return 0; }
