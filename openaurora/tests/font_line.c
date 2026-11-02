#include "openaurora/font_text.h"
#include "openaurora/font_pack.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char metrics[95*9], coverage[95*4];
static struct oa_bitmap_font font={.pixels=coverage,.pixel_bytes=sizeof coverage,
                                  .metrics=metrics,.glyph_count=95};
static void init_font(void)
{
    memset(metrics,0,sizeof metrics); memset(coverage,255,sizeof coverage);
    for (unsigned i=0;i<95;++i) {
        unsigned char *p=metrics+i*9;
        p[2]=1; p[3]=1; p[4]=3; p[7]=(unsigned char)((i*4)>>8); p[8]=(unsigned char)(i*4);
    }
}

static int corpus(const char *index, const char *path)
{
    struct oa_font_pack pack;
    assert(!oa_font_pack_init(&pack,index,path));
    uint8_t *pixels=malloc(512*128*4);
    assert(pixels);
    oa_canvas canvas={pixels,512,128};
    const uint8_t white[4]={255,255,255,255};
    for (unsigned id=7;id<=16;id+=9) {
        unsigned char *data;
        size_t size;
        struct oa_bitmap_font f;
        assert(!oa_font_pack_read(&pack,id,&data,&size));
        assert(!oa_bitmap_font_open(&f,data,size));
        const char *texts[] = {"Modus","Natuurlijk","95","50","Energiebesparing"};
        for (unsigned t=0;t<5;++t) for (unsigned a=1;a<=3;++a) {
            assert(!oa_canvas_clear(&canvas,0,0,0,0));
            struct oa_font_line_style s={.x=10,.y=5,.left=10,.right=502,.alignment=a};
            struct oa_font_line_layout result;
            assert(!oa_font_text_draw_line(&canvas,&f,texts[t],&s,white,NULL,&result));
            assert(result.count==strlen(texts[t]) && !result.first);
            assert(result.reverse==(a==2));
            if (a==2) {
                unsigned slot;
                struct oa_bitmap_glyph last;
                assert(!oa_bitmap_font_lookup(&f,(unsigned char)texts[t][strlen(texts[t])-1],&slot));
                assert(!oa_bitmap_font_glyph(&f,slot,&last));
                assert(result.origin==502-last.right);
            }
        }
        free(data);
    }
    free(pixels); oa_font_pack_free(&pack);
    puts("ok font line: real font7/16 start/end/center render 30 lines");
    return 0;
}

int main(int argc, char **argv)
{
    init_font();
    struct oa_font_line_style s={.x=0,.y=0,.left=0,.right=10,.spacing=1,.alignment=1};
    struct oa_font_line_layout result;
    assert(!oa_font_text_layout_line(&font,"AB",&s,&result));
    assert(result.origin==0 && result.physical_width==5 && result.count==2);
    s.alignment=2;
    assert(!oa_font_text_layout_line(&font,"AB",&s,&result) && result.origin==9 && result.reverse);
    s.alignment=3;
    assert(!oa_font_text_layout_line(&font,"AB",&s,&result) && result.origin==2);
    /* Negative first bearing affects physical width and the fitting center. */
    metrics[('A'-32)*9+1]=255;
    assert(!oa_font_text_layout_line(&font,"AB",&s,&result));
    assert(result.physical_width==6 && result.origin==3);
    init_font();
    s=(struct oa_font_line_style){.x=33,.y=0,.left=0,.right=7,.spacing=1,.alignment=3};
    assert(!oa_font_text_layout_line(&font,"ABCDEF",&s,&result));
    assert(result.first==2 && result.count==2 && result.origin==1 && result.physical_width==5);
    metrics[('C'-32)*9+1]=255;
    assert(!oa_font_text_layout_line(&font,"ABCDEF",&s,&result));
    assert(result.first==2 && result.count==2 && result.origin==0 && result.physical_width==6);
    init_font();
    s.right=0; s.x=12;
    assert(!oa_font_text_layout_line(&font,"AB",&s,&result));
    assert(!result.first && result.count==2 && result.origin==12); /* no slice fits */
    uint8_t pixels[16*4]={0}, before[sizeof pixels];
    oa_canvas canvas={pixels,16,1};
    const uint8_t color[4]={230,40,90,255};
    s=(struct oa_font_line_style){.x=33,.left=0,.right=7,.spacing=1,.alignment=3};
    coverage[('C'-32)*4]=70; coverage[('D'-32)*4]=80;
    assert(!oa_font_text_draw_line(&canvas,&font,"ABCDEF",&s,color,NULL,&result));
    assert(pixels[1*4+3]==70 && pixels[5*4+3]==80 && !pixels[0*4+3]);
    assert(!oa_canvas_clear(&canvas,0,0,0,0));
    init_font();
    s=(struct oa_font_line_style){.left=0,.right=1,.alignment=2};
    metrics[('A'-32)*9+4]=metrics[('B'-32)*9+4]=0;
    coverage[('A'-32)*4]=1; coverage[('B'-32)*4]=9;
    assert(!oa_canvas_clear(&canvas,19,71,143,153));
    assert(!oa_font_text_draw_line(&canvas,&font,"AB",&s,color,NULL,&result));
    assert(!memcmp(pixels,(uint8_t[]){32,68,138,156},4)); /* B then A, same position */
    s.alignment=1;
    assert(!oa_canvas_clear(&canvas,19,71,143,153));
    assert(!oa_font_text_draw_line(&canvas,&font,"AB",&s,color,NULL,&result));
    assert(!memcmp(pixels,(uint8_t[]){32,68,139,156},4)); /* reverse would differ */
    init_font();
    memcpy(before,pixels,sizeof pixels);
    struct oa_font_line_layout saved=result;
    assert(oa_font_text_draw_line(&canvas,&font,"AB\xff",&s,color,NULL,&result)==EILSEQ);
    assert(!memcmp(pixels,before,sizeof pixels) && !memcmp(&result,&saved,sizeof result));
    s.x=INT_MAX;
    assert(oa_font_text_draw_line(&canvas,&font,"AB",&s,color,NULL,&result)==EOVERFLOW);
    assert(!memcmp(pixels,before,sizeof pixels));
    s.alignment=4;
    assert(oa_font_text_layout_line(&font,"AB",&s,&result)==ENOTSUP);
    s.alignment=1; s.x=0;
    assert(!oa_font_text_layout_line(&font,"",&s,&result) && !result.count);
    puts("ok font line: ink width, center bearing/trim, end paint order, atomic failures");
    return argc==3 ? corpus(argv[1],argv[2]) : argc!=1;
}
