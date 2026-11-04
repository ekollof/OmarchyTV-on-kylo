#include "openaurora/svg_scrollbar.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned calls;
static struct { unsigned kind; } trace[8];
static void on_bg(struct oa_svg_scrollbar_state *s) { (void)s;trace[calls++].kind=1; }
static void on_bar(struct oa_svg_scrollbar_state *s) { (void)s;trace[calls++].kind=2; }
static const struct oa_svg_scrollbar_ops ops={on_bg,on_bar};

static void calc(struct oa_svg_scrollbar_state *s,int32_t total,int32_t visible,
    int32_t first,int32_t min,int32_t height,int32_t want_pos,int32_t want_size)
{
    *s=(struct oa_svg_scrollbar_state){.min_bar=min,.total=total,.visible=visible,
        .first=first,.bar_pos=0x1111,.bar_size=0x2222};
    oa_svg_scrollbar_calculate(s,height);
    if(s->bar_pos!=want_pos || s->bar_size!=want_size)
        fprintf(stderr,"calc total=%d visible=%d first=%d min=%d h=%d -> pos=%d size=%d want %d/%d\n",
            total,visible,first,min,height,s->bar_pos,s->bar_size,want_pos,want_size);
    assert(s->bar_pos==want_pos && s->bar_size==want_size);
}

int main(void)
{
    struct oa_svg_scrollbar_state s;
    /* Apps-grid track: 1750,180,8x720 -> height 720. */
    calc(&s,0,0,0,0,720,0,0);
    calc(&s,10,10,0,0,720,0,0);
    calc(&s,10,20,0,0,720,0,0);
    calc(&s,20,10,0,0,720,0,360);
    calc(&s,20,10,5,0,720,180,360);
    calc(&s,20,10,10,0,720,360,360);
    calc(&s,20,10,15,0,720,360,360);
    calc(&s,20,10,0,50,720,0,360);
    calc(&s,100,1,0,50,720,0,50);
    calc(&s,100,1,99,50,720,670,50);
    calc(&s,20,10,0,1000,720,0,0);     /* clamped to height then hidden */
    calc(&s,101,100,0,0,720,0,712);
    calc(&s,1,0,0,0,720,0,0);
    calc(&s,-5,0,0,0,720,0,0);
    calc(&s,-5,-10,0,0,720,0x1111,0x2222); /* negative branch keeps both */
    calc(&s,20,10,0,0,0,0,0);
    calc(&s,20,10,0,1000,0,0,0);

    /* setScrollPosition: not created and first > total leave the fields. */
    struct oa_svg_scrollbar_state saved;
    s=(struct oa_svg_scrollbar_state){.total=7,.visible=8,.first=9,
        .bar_pos=0x33,.bar_size=0x44,.background_hidden=0};
    saved=s;calls=0;
    assert(!oa_svg_scrollbar_set_position(&s,20,10,0,0,&ops));
    assert(!memcmp(&s,&saved,sizeof s) && calls==0);
    assert(!oa_svg_scrollbar_set_position(&s,20,10,21,1,&ops));
    assert(!memcmp(&s,&saved,sizeof s) && calls==0);
    calls=0;
    assert(oa_svg_scrollbar_set_position(&s,20,10,3,1,&ops));
    assert(s.total==20 && s.visible==10 && s.first==3);
    assert(calls==2 && trace[0].kind==1 && trace[1].kind==2);
    s.background_hidden=1;calls=0;
    assert(oa_svg_scrollbar_set_position(&s,30,12,4,1,&ops));
    assert(calls==1 && trace[0].kind==2);
    oa_svg_scrollbar_calculate(&s,720);
    assert(s.total==30 && s.visible==12 && s.first==4);
    assert(s.bar_size==288 && s.bar_pos==(720-288)*4/(30-12));

    puts("ok svg scrollbar: position state, 720px apps track geometry and render gates");
    return 0;
}
