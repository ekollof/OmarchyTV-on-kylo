#include "openaurora/svg_scrollbar.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int supplier_value;
static int rect23_present;
static unsigned trace_count;
struct skin_ev { int kind,id,value; };
static struct skin_ev trace[16];

static int32_t op_supplier(struct oa_svg_scrollbar_skin *s,int32_t image_id)
{
    (void)s;trace[trace_count++]=(struct skin_ev){1,0,image_id};
    return supplier_value;
}
static int32_t op_name(struct oa_svg_scrollbar_skin *s,int32_t widget_id)
{
    (void)s;trace[trace_count++]=(struct skin_ev){2,widget_id,0};
    return widget_id;
}
static void op_bg(struct oa_svg_scrollbar_skin *s,int32_t image,int32_t name)
{
    (void)s;trace[trace_count++]=(struct skin_ev){3,name,image};
}
static int32_t op_rect(struct oa_svg_scrollbar_skin *s,int32_t name)
{
    (void)s;trace[trace_count++]=(struct skin_ev){4,name,0};
    if(name==0x22)return 20;    /* bar_mTopM outer cap */
    if(name==0x23)return rect23_present?16:0; /* bar_mBotM outer cap */
    return 0;
}
static const struct oa_svg_scrollbar_skin_ops ops={
    op_supplier,op_name,op_bg,op_rect};

static void reset(void)
{
    supplier_value=0x7001;rect23_present=1;trace_count=0;
}
int main(void)
{
    struct oa_svg_scrollbar_skin s;
    /* Fresh image: supplier, then name/bg 0x20, name/rect 0x22,
     * name/bg 0x21, name/rect 0x23. Caps 20+16 -> 36. */
    reset();
    s=(struct oa_svg_scrollbar_skin){.cached_image={0,0},.image_id={0x1234,0x5678},
        .min_bar=99,.widget_state=0};
    oa_svg_scrollbar_min_bar(&s,&ops);
    assert(s.min_bar==36 && s.cached_image[0]==0x7001);
    assert(trace_count==9);
    assert(trace[0].kind==1 && trace[0].value==0x1234);
    assert(trace[1].kind==2 && trace[1].id==0x20);
    assert(trace[2].kind==3 && trace[2].id==0x20 && trace[2].value==0x7001);
    assert(trace[3].kind==2 && trace[3].id==0x22);
    assert(trace[4].kind==4 && trace[4].id==0x22);
    assert(trace[5].kind==2 && trace[5].id==0x21);
    assert(trace[6].kind==3 && trace[6].id==0x21);
    assert(trace[7].kind==2 && trace[7].id==0x23);
    assert(trace[8].kind==4 && trace[8].id==0x23);
    assert(trace_count==9);

    /* Cached image: no supplier call. */
    reset();trace_count=0;
    s=(struct oa_svg_scrollbar_skin){.cached_image={0x7003,0},.image_id={0x1234,0},
        .min_bar=99,.widget_state=0};
    oa_svg_scrollbar_min_bar(&s,&ops);
    assert(s.min_bar==36 && trace[0].kind==2 && trace[0].id==0x20);
    assert(trace_count==8);

    /* No supplier image: min is left unchanged. */
    reset();supplier_value=0;trace_count=0;
    s=(struct oa_svg_scrollbar_skin){.cached_image={0,0},.image_id={0x1234,0},
        .min_bar=99,.widget_state=0};
    oa_svg_scrollbar_min_bar(&s,&ops);
    assert(s.min_bar==99 && s.cached_image[0]==0 && trace_count==1);
    assert(trace[0].kind==1);

    /* Orientation 1 uses the second cache/image slot. */
    reset();trace_count=0;
    s=(struct oa_svg_scrollbar_skin){.cached_image={0xAAAA,0},.image_id={0x1234,0x5678},
        .min_bar=99,.widget_state=1};
    oa_svg_scrollbar_min_bar(&s,&ops);
    assert(s.min_bar==36 && s.cached_image[1]==0x7001);
    assert(trace[0].kind==1 && trace[0].value==0x5678);

    /* Missing bottom cap pointer contributes 0. */
    reset();rect23_present=0;trace_count=0;
    s=(struct oa_svg_scrollbar_skin){.cached_image={0x7003,0},.image_id={0,0},
        .min_bar=99,.widget_state=0};
    oa_svg_scrollbar_min_bar(&s,&ops);
    assert(s.min_bar==20);
    assert(trace_count==8 && trace[6].kind==2 && trace[6].id==0x23);

    s.widget_state=3;s.image_id[3]=0x9abc;s.cached_image[3]=0;trace_count=0;
    oa_svg_scrollbar_min_bar(&s,&ops);
    assert(s.min_bar==20 && s.cached_image[3]==0x7001);
    s.widget_state=4;trace_count=0;
    oa_svg_scrollbar_min_bar(&s,&ops);assert(trace_count==0);
    puts("ok svg scrollbar skin: 36px cap minimum, four widget states, invalid-state rejection and trace order");
    return 0;
}
