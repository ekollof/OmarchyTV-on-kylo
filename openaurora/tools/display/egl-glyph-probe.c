/* Explicit offscreen GPU glyph test. No window/framebuffer/input/HAL calls.
 * Native execution still requires the authorized idle hardware-test window. */
#include "openaurora/scene_gles.h"
#include "openaurora/font_pack.h"
#include "openaurora/font_box.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int error(oa_scene_gles *s)
{
    char text[160];oa_scene_gles_error(s,text,sizeof text);
    fprintf(stderr,"%s errno=%d\n",text,errno);return 1;
}
static int close_scene(oa_scene_gles **s)
{
    if(oa_scene_gles_close(s))return error(*s);
    return 0;
}
static int synthetic(void)
{
    oa_scene_gles *s=NULL;
    if(oa_scene_gles_open(&s,8,8)){int rc=error(s);(void)close_scene(&s);return rc;}
    uint8_t base[8*8*4]={0},out[sizeof base];
    const uint8_t mask[4]={255,0,128,255};
    struct oa_scene_glyph g={.x=2,.y=3,.width=2,.height=2,.coverage=mask,.coverage_size=4,
                            .rgba={255,0,0,255}};
    int rc=1;const char *stage="initial render";
#ifdef OA_GLYPH_SOFTWARE_TEST
    extern int oa_glyph_fail_program,oa_glyph_fail_texture;
    if(oa_scene_gles_render(s,base,sizeof base,0,1,out,sizeof out))goto failed;
    oa_glyph_fail_texture=1; /* FBO colour allocation fails before any upload. */
    if(oa_scene_gles_render_glyphs(s,NULL,0,&g,1,out,sizeof out)!=-1 ||
       oa_scene_gles_render(s,NULL,0,0,1,out,sizeof out))goto failed;
    if(oa_scene_gles_render_glyphs(s,NULL,0,NULL,0,out,sizeof out))goto failed;
    for(int failure=0;failure<2;++failure) {
        stage=failure?"glyph texture failure recovery":"glyph program failure recovery";
        if(failure)oa_glyph_fail_texture=1;else oa_glyph_fail_program=1;
        if(oa_scene_gles_render_glyphs(s,base,sizeof base,&g,1,out,sizeof out)!=-1)goto failed;
        if(oa_scene_gles_render(s,NULL,0,0,1,out,sizeof out))goto failed;
        for(size_t i=0;i<sizeof out;++i)if(out[i])goto failed;
    }
#endif
    stage="two-pass coverage";
    if(oa_scene_gles_render_glyphs(s,base,sizeof base,&g,1,out,sizeof out))goto failed;
    /* Readback is bottom-up, with premultiplied RGB. */
    for(int y=0;y<8;++y)for(int x=0;x<8;++x) {
        unsigned expected=x>=2 && x<4 && y>=3 && y<5?mask[(y-3)*2+x-2]:0;
        const uint8_t *p=out+((7-y)*8+x)*4;
        unsigned colour=(expected*expected+127)/255;
        if(abs((int)p[0]-(int)colour)>1 || p[1] || p[2] ||
           abs((int)p[3]-(int)expected)>1) {
            fprintf(stderr,"pixel%d,%d RGBA=%u,%u,%u,%u expected R%u A%u\n",
                    x,y,p[0],p[1],p[2],p[3],colour,expected);goto failed;
        }
    }
    /* A different mask must replace the previous texture; clipped U remaps
     * to its second column without altering the glyph's original dimensions. */
    const uint8_t second[4]={0,255,0,255};
    stage="clip and mask replacement";
    g.coverage=second;g.clipped=1;g.clip=(struct oa_font_clip){3,3,1,2};
    if(oa_scene_gles_render_glyphs(s,NULL,0,&g,1,out,sizeof out))goto failed;
    if(out[((7-3)*8+2)*4] || out[((7-3)*8+3)*4]!=255)goto failed;
    /* Whole-batch validation precedes any draw/readback. */
    uint8_t before[sizeof out];memcpy(before,out,sizeof out);
    stage="batch rejection";
    struct oa_scene_glyph bad[2]={g,g};bad[1].coverage_size=3;
    if(oa_scene_gles_render_glyphs(s,NULL,0,bad,2,out,sizeof out)!=-1 || errno!=EINVAL ||
       memcmp(before,out,sizeof out))goto failed;
    /* New API leaves the original texture/cache/program usable. */
    stage="legacy reuse";
    if(oa_scene_gles_render(s,NULL,0,0,1,out,sizeof out))goto failed;
    for(size_t i=0;i<sizeof out;++i)if(out[i])goto failed;
    /* Ordered overlap: alpha accumulates like the vendor ONE/ONE alpha blend. */
    g.coverage=mask;g.clipped=0;g.rgba[3]=128;
    stage="ordered overlap";
    struct oa_scene_glyph pair[2]={g,g};pair[1].rgba[0]=0;pair[1].rgba[2]=255;
    if(oa_scene_gles_render_glyphs(s,NULL,0,pair,2,out,sizeof out))goto failed;
    const uint8_t *p=out+((7-3)*8+2)*4;
    if(p[0]<62 || p[0]>65 || p[1] || p[2]<127 || p[2]>129 || p[3]!=255)goto failed;
    rc=0;
    puts("ok GPU glyphs: coverage/orientation, clip UVs, mask replacement, ordered blending, batch rejection and legacy RGBA reuse");
    goto done;
failed:
    fprintf(stderr,"failed stage: %s\n",stage);
    (void)error(s);
done:
    return close_scene(&s)?1:rc;
}
static int corpus(const char *index,const char *binary,const char *output)
{
    struct oa_font_pack pack={0};uint8_t *data=NULL;size_t size=0;
    struct oa_bitmap_font font;struct oa_font_box_layout layout;
    if(oa_font_pack_init(&pack,index,binary) || oa_font_pack_read(&pack,18,&data,&size) ||
       oa_bitmap_font_open(&font,data,size) ||
       oa_font_text_layout_box(&font,"Instellingen",583,90,4,0,&layout)) {
        free(data);oa_font_pack_free(&pack);return 1;
    }
    const char *text="Instellingen";struct oa_scene_glyph glyphs[12];
    int pen=108+layout.line.origin;
    for(unsigned i=0;i<12;++i) {
        unsigned id;struct oa_bitmap_glyph g;
        if(oa_bitmap_font_lookup(&font,(unsigned char)text[i],&id) ||
           oa_bitmap_font_glyph(&font,id,&g)){free(data);oa_font_pack_free(&pack);return 1;}
        glyphs[i]=(struct oa_scene_glyph){.x=pen+g.left,.y=5+layout.y+g.top,
            .width=g.right-g.left,.height=g.bottom-g.top,.coverage=g.coverage,
            .coverage_size=g.stride*(size_t)(g.bottom-g.top),.rgba={255,255,255,255}};
        pen+=g.advance;
    }
    size_t bytes=1920u*1080u*4u;
    uint8_t *base=calloc(bytes,1),*pixels=malloc(bytes);oa_scene_gles *s=NULL;
    int rc=1;
    if(!base || !pixels)goto done;
    if(oa_scene_gles_open(&s,1920,1080) ||
       oa_scene_gles_render_glyphs(s,base,bytes,glyphs,12,pixels,bytes)){(void)error(s);goto done;}
    FILE *out=fopen(output,"wb");if(!out)goto done;
    rc=0;
    for(unsigned y=0;y<1080;++y)
        if(fwrite(pixels+(1079-y)*1920u*4u,1,1920u*4u,out)!=1920u*4u){rc=1;break;}
    if(fclose(out))rc=1;
    if(!rc)puts("ok GPU title corpus: font18 retained origin23,5; top-down premultiplied RGBA1920x1080 (no display presentation)");
done:
    if(close_scene(&s))rc=1;
    free(base);free(pixels);free(data);oa_font_pack_free(&pack);return rc;
}
int main(int argc,char **argv)
{
    if(argc!=1 && argc!=4){fprintf(stderr,"usage: %s [FONT_INDEX FONT_BINARY OUTPUT_RGBA]\n",argv[0]);return 2;}
    if(synthetic())return 1;
    return argc==4?corpus(argv[1],argv[2],argv[3]):0;
}
