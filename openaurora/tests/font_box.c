#include "openaurora/font_box.h"
#include "openaurora/font_pack.h"
#include "openaurora/svg_pack.h"
#include "openaurora/widget_layout.h"
#include "openaurora/text_origin.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t metrics[95*9],coverage[95],extremes[9]={0,0,3};
static struct oa_bitmap_font font={.pixels=coverage,.pixel_bytes=sizeof coverage,
    .metrics=metrics,.glyph_count=95,.extremes=extremes};
static void synthetic(void)
{
    for(unsigned i=0;i<95;++i) {
        metrics[i*9+2]=1;metrics[i*9+3]=1;metrics[i*9+4]=3;metrics[i*9+8]=(uint8_t)i;
        coverage[i]=255;
    }
    uint8_t storage[10*7*4+16];oa_canvas c={storage+8,10,7};
    const uint8_t ink[4]={231,42,99,255};
    for(unsigned a=1;a<=9;++a) {
        memset(storage,0x5a,sizeof storage);memset(c.pixels,0,10*7*4);
        struct oa_font_box_layout result;
        assert(!oa_font_text_draw_box(&c,&font,"A",a,0,ink,&result));
        const int xs[]={0,4,9},ys[]={0,2,4};int x=xs[(a-1)%3],y=ys[(a-1)/3];
        assert(result.line.origin==x && result.y==y);
        for(int py=0;py<7;++py)for(int px=0;px<10;++px) {
            const uint8_t zero[4]={0};
            assert(!memcmp(c.pixels+(py*10+px)*4,px==x&&py==y?ink:zero,4));
        }
        for(unsigned i=0;i<8;++i)assert(storage[i]==0x5a && storage[sizeof storage-1-i]==0x5a);
    }
    struct oa_font_box_layout r;
    assert(!oa_font_text_layout_box(&font,"A",10,7,0,0,&r) && r.line.origin==0 && r.y==2);
    assert(!oa_font_text_layout_box(&font,"A",10,7,UINT_MAX,0,&r) && r.horizontal==1 && r.vertical==3);
    c.width=7;memset(c.pixels,0,7*7*4);
    assert(!oa_font_text_draw_box(&c,&font,"ABCDE",5,0,ink,&r));
    assert(r.line.first==1 && r.line.count==3 && r.line.origin==0 && r.y==2);
    assert(c.pixels[(2*7)*4+3]==255 && c.pixels[(2*7+3)*4+3]==255 && c.pixels[(2*7+6)*4+3]==255);
    uint8_t before[sizeof storage];memcpy(before,storage,sizeof storage);
    struct oa_font_box_layout old=r;
    assert(oa_font_text_draw_box(&c,&font,"A\xff",4,0,ink,&r)==EILSEQ);
    assert(!memcmp(&old,&r,sizeof r) && !memcmp(before,storage,sizeof storage));
    extremes[2]=0;
    assert(oa_font_text_draw_box(&c,&font,"A",4,0,ink,&r)==EINVAL && !memcmp(&old,&r,sizeof r));
    extremes[2]=3;
    assert(oa_font_text_layout_box(&font,"A",0,7,4,0,&r)==EINVAL && !memcmp(&old,&r,sizeof r));
    unsigned h=99,v=99;assert(oa_text_box_justification(4,NULL,&v)==EINVAL && v==99);
    assert(!oa_text_box_justification(4,&h,&v) && h==1 && v==3);
    puts("ok font box:9 alignments with exact pixels/guards, defaults, center trim, atomic invalid text/font/dimensions");
}
static void corpus(const char *index,const char *binary,const char *font_index,const char *font_binary)
{
    struct oa_svg_pack *pack=NULL;assert(!oa_svg_pack_load(&pack,index,binary));
    struct oa_font_pack fonts={0};assert(!oa_font_pack_init(&fonts,font_index,font_binary));
    struct oa_binsvg rows;assert(!oa_svg_pack_named(pack,"SettingItemsListBoxRowItems.svg",&rows));
    const struct oa_text_rect parent={20,186,769,256};struct oa_text_rect child;
    assert(!oa_widget_row_item_window(&rows,&parent,"text1",&child));
    const char *names[]={"settingListTextboxUFA.svg","settingListTextboxFA.svg"};
    const unsigned ids[]={7,20};unsigned rendered=0;
    for(unsigned state=0;state<2;++state) {
        struct oa_binsvg resource;assert(!oa_svg_pack_named(pack,names[state],&resource));
        struct oa_widget_textbox box;assert(!oa_widget_single_line_textbox(&resource,&child,&box));
        unsigned char *data=NULL;size_t size=0;struct oa_bitmap_font f;
        assert(!oa_font_pack_read(&fonts,ids[state],&data,&size) && !oa_bitmap_font_open(&f,data,size));
        int w=box.text.bounds.right-box.text.bounds.left,h=box.text.bounds.bottom-box.text.bounds.top;
        size_t bytes=(size_t)w*(size_t)h*4;uint8_t *pixels=malloc(bytes);assert(pixels);
        oa_canvas c={pixels,w,h};const uint8_t ink[4]={255,255,255,255};
        struct oa_font_box_layout result;assert(!oa_canvas_clear(&c,0,0,0,0));
        assert(!oa_font_text_draw_box(&c,&f,"Modus",box.alignment,0,ink,&result));
        assert(result.y==(state?1:2) && result.line.origin==0 && result.line.count==5);
        const char *texts[]={"Modus","Natuurlijk","95"};
        for(unsigned t=0;t<3;++t)for(unsigned a=1;a<=9;++a) {
            assert(!oa_canvas_clear(&c,0,0,0,0));
            assert(!oa_font_text_draw_box(&c,&f,texts[t],a,0,ink,&result));++rendered;
        }
        free(pixels);free(data);
    }
    oa_font_pack_free(&fonts);oa_svg_pack_free(pack);
    assert(rendered==54);
    puts("ok font box: actual row/textbox surfaces -> font7/20 ->54 offscreen lines; local Y2/1, explicit parent and unit ratios");
}
int main(int argc,char **argv)
{ synthetic();if(argc==5)corpus(argv[1],argv[2],argv[3],argv[4]);else assert(argc==1);return 0; }
