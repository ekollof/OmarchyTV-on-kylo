#include "openaurora/font_draw.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    unsigned char metrics[9] = {255,255,1,1,2,0,0,0,0};
    unsigned char coverage[4] = {0,85,170,255};
    struct oa_bitmap_font f = {.pixels=coverage, .pixel_bytes=4,
                              .metrics=metrics, .glyph_count=1};
    unsigned char pixels[64], before[64];
    oa_canvas c = {pixels,4,4};
    const uint8_t color[4] = {230,40,90,255};
    assert(!oa_canvas_clear(&c,0,0,0,0));
    assert(!oa_bitmap_draw_glyph(&c,&f,0,1,1,color,NULL));
    assert(!memcmp(pixels,(uint8_t[]){0,0,0,0,230,40,90,85},8));
    assert(!memcmp(pixels+16,(uint8_t[]){230,40,90,170,230,40,90,255},8));
    /* One clipped bottom-right pixel; signed bearings start the glyph at0,0. */
    assert(!oa_canvas_clear(&c,0,0,0,0));
    struct oa_font_clip clip = {1,1,1,1};
    assert(!oa_bitmap_draw_glyph(&c,&f,0,1,1,color,&clip));
    for (unsigned i=0;i<16;++i) assert(pixels[i*4+3] == (i==5 ? 255 : 0));
    memcpy(before,pixels,sizeof pixels);
    assert(!oa_bitmap_draw_glyph(&c,&f,0,INT_MAX,INT_MAX,color,NULL));
    assert(!oa_bitmap_draw_glyph(&c,&f,0,INT_MIN,INT_MIN,color,NULL));
    clip.width=INT_MAX; clip.x=INT_MAX;
    assert(!oa_bitmap_draw_glyph(&c,&f,0,1,1,color,&clip));
    assert(!memcmp(before,pixels,sizeof pixels));
    assert(oa_bitmap_draw_glyph(&c,&f,1,1,1,color,NULL)==EINVAL);
    const struct oa_bitmap_font empty = {0};
    assert(oa_bitmap_draw_glyph(&c,&empty,0,1,1,color,NULL)==EINVAL);
    metrics[8]=4;
    assert(oa_bitmap_draw_glyph(&c,&f,0,1,1,color,NULL)==EINVAL);
    assert(!memcmp(before,pixels,sizeof pixels)); metrics[8]=0;
    coverage[3]=1;
    const uint8_t faint[4]={230,40,90,1};
    assert(!oa_canvas_clear(&c,0,0,0,0));
    memcpy(pixels+20,(uint8_t[]){19,71,143,0},4);
    assert(!oa_bitmap_draw_glyph(&c,&f,0,1,1,faint,NULL));
    assert(!memcmp(pixels+20,(uint8_t[]){0,0,0,0},4));
    /* Exhaust every coverage/color-alpha pair against integer color math,
     * including sub-byte effective alpha and translucent destination. */
    for (unsigned a=0;a<256;++a) for (unsigned v=0;v<256;++v) {
        uint8_t src[4]={230,40,90,(uint8_t)a};
        assert(!oa_canvas_clear(&c,19,71,143,153));
        coverage[3]=(uint8_t)v;
        assert(!oa_bitmap_draw_glyph(&c,&f,0,1,1,src,NULL));
        unsigned effective=a*v/255, weight=effective*255+153*(255-effective);
        uint8_t *p=pixels+20;
        assert(p[3]==weight/255);
        const unsigned bg[3]={19,71,143};
        for (unsigned k=0;k<3;++k)
            assert(p[k]==(src[k]*effective*255+bg[k]*153*(255-effective))/weight);
    }
    puts("ok glyph draw: signed bearings, clipping/extremes, bounds, all alpha/coverage pairs");
    return 0;
}
