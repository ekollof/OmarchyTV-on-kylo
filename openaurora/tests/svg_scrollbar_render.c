#include "openaurora/svg_scrollbar.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { WINDOW, RESIZE, RECT_NEW, ALIGN, MASK_SURFACE, MASK_NAME,
       MASK_CONTAINER, DRAW, MASK_RECT, MASK_RESET, VERTICAL_RECT,
       VERTICAL_NAME, VERTICAL_CONTAINER, CLEAR, SUPPLIER };
struct ev { int kind,surface,image,id,transform; int rect[4]; };
static struct ev trace[64];static int n;
static int32_t supplier_value;
static int32_t top_height=20,bottom_height=16;

static void push(int kind,int surface,int image,int id,int transform,
    const int32_t *rect)
{
    assert(n<64);
    memset(&trace[n],0,sizeof trace[n]);
    trace[n].kind=kind;trace[n].surface=surface;trace[n].image=image;
    trace[n].id=id;trace[n].transform=transform;
    if(rect)memcpy(trace[n].rect,rect,16);
    ++n;
}
static int32_t op_supplier(struct oa_svg_scrollbar_render *r,int32_t id)
{ (void)r;push(SUPPLIER,0,0,id,0,NULL);return supplier_value; }
static void op_window(struct oa_svg_scrollbar_render *r,int32_t rect[4])
{
    (void)r;
    rect[0]=1750;rect[1]=180;rect[2]=1758;rect[3]=900;
    push(WINDOW,0,0,0,0,rect);
}
static void op_resize(struct oa_svg_scrollbar_render *r,int surface,const int32_t rect[4])
{ (void)r;push(RESIZE,surface,0,0,0,rect); }
static void op_clear(struct oa_svg_scrollbar_render *r,int surface)
{ (void)r;push(CLEAR,surface,0,0,0,NULL); }
static void op_rect(struct oa_svg_scrollbar_render *r,int32_t rect[4],int l,int t,int rt,int b)
{ (void)r;rect[0]=l;rect[1]=t;rect[2]=rt;rect[3]=b;push(RECT_NEW,0,0,0,0,rect); }
static void op_reset(struct oa_svg_scrollbar_render *r) { (void)r;push(MASK_RESET,0,0,0,0,NULL); }
static void op_align(struct oa_svg_scrollbar_render *r,int align)
{ (void)r;push(ALIGN,0,0,align,0,NULL); }
static void op_mask_surface(struct oa_svg_scrollbar_render *r,const int32_t rect[4])
{ (void)r;push(MASK_SURFACE,0,0,0,0,rect); }
static void op_mask_name(struct oa_svg_scrollbar_render *r,int32_t id)
{ (void)r;push(MASK_NAME,0,0,id,0,NULL); }
static void op_mask_container(struct oa_svg_scrollbar_render *r)
{ (void)r;push(MASK_CONTAINER,0,0,0,0,NULL); }
static void op_draw(struct oa_svg_scrollbar_render *r,int surface,int32_t image,
    int32_t id,int transform)
{ (void)r;push(DRAW,surface,image,id,transform,NULL); }
static int32_t op_rect_height(struct oa_svg_scrollbar_render *r,int32_t id)
{
    (void)r;push(MASK_RECT,0,0,id,0,NULL);
    if(id==15||id==34)return top_height;
    if(id==24||id==35)return bottom_height;
    return 0;
}
static void op_vertical_rect(struct oa_svg_scrollbar_render *r,const int32_t rect[4])
{ (void)r;push(VERTICAL_RECT,0,0,0,0,rect); }
static void op_vertical_name(struct oa_svg_scrollbar_render *r,int32_t id)
{ (void)r;push(VERTICAL_NAME,0,0,id,0,NULL); }
static void op_vertical_container(struct oa_svg_scrollbar_render *r)
{ (void)r;push(VERTICAL_CONTAINER,0,0,0,0,NULL); }
static const struct oa_svg_scrollbar_render_ops ops={
    op_supplier,op_window,op_resize,op_clear,op_rect,op_reset,op_align,
    op_mask_surface,op_mask_name,op_mask_container,op_draw,op_rect_height,
    op_vertical_rect,op_vertical_name,op_vertical_container};

static void setup(struct oa_svg_scrollbar_render *r)
{
    memset(r,0,sizeof *r);
    r->skin.cached_image[0]=0x7001;
    r->skin.image_id[0]=0x1234;
    r->skin.widget_state=0;
    r->state.min_bar=36;
}
static int rect_eq(const int32_t *a,const int32_t *b)
{ return !memcmp(a,b,16); }

int main(void)
{
    struct oa_svg_scrollbar_render r;
    /* Background: surface0 resized to the 1750,180,8x720 track; top cap 20,
     * bottom 16, row 0,20,8,704. */
    setup(&r);supplier_value=0x7001;n=0;
    oa_svg_scrollbar_render_background(&r,&ops);
    assert(r.state.background_rendered==1);
    assert(n==20);
    assert(trace[0].kind==WINDOW && rect_eq(trace[0].rect,(int32_t[4]){1750,180,1758,900}));
    assert(trace[1].kind==RESIZE && trace[1].surface==0 &&
           rect_eq(trace[1].rect,(int32_t[4]){1750,180,1758,900}));
    assert(trace[2].kind==RECT_NEW && rect_eq(trace[2].rect,(int32_t[4]){0,0,8,720}));
    assert(trace[3].kind==ALIGN && trace[3].id==3);
    assert(trace[4].kind==MASK_SURFACE && rect_eq(trace[4].rect,(int32_t[4]){0,0,8,720}));
    assert(trace[5].kind==MASK_NAME && trace[5].id==15);
    assert(trace[6].kind==MASK_CONTAINER);
    assert(trace[7].kind==DRAW && trace[7].surface==0 &&
           trace[7].image==0x7001 && trace[7].id==3 && trace[7].transform==0);
    assert(trace[8].kind==MASK_RECT && trace[8].id==15);
    assert(trace[9].kind==MASK_RESET);
    assert(trace[10].kind==MASK_NAME && trace[10].id==24);
    assert(trace[11].kind==ALIGN && trace[11].id==4);
    assert(trace[12].kind==DRAW && trace[12].id==12);
    assert(trace[13].kind==MASK_RECT && trace[13].id==24);
    assert(trace[14].kind==RECT_NEW &&
           rect_eq(trace[14].rect,(int32_t[4]){0,20,8,704}));
    assert(trace[15].kind==VERTICAL_RECT &&
           rect_eq(trace[15].rect,(int32_t[4]){0,20,8,704}));
    assert(trace[16].kind==VERTICAL_NAME && trace[16].id==18);
    assert(trace[17].kind==VERTICAL_CONTAINER);
    assert(trace[18].kind==DRAW && trace[18].id==6 && trace[18].transform==1);
    assert(trace[19].kind==MASK_RECT && trace[19].id==18);

    /* Bar: calculateBarSizeAndPosition gives 540 px for 20/15; surface1 is
     * resized to the bar rect; row 0,20,8,524. */
    setup(&r);supplier_value=0x7001;n=0;
    r.skin.cached_image[0]=0x7002;
    r.state.total=20;r.state.visible=15;r.state.first=0;r.state.min_bar=36;
    oa_svg_scrollbar_render_bar(&r,&ops);
    assert(r.state.bar_size==540 && r.state.bar_pos==0);
    assert(n==21);
    assert(trace[0].kind==WINDOW && trace[1].kind==WINDOW);
    assert(trace[2].kind==RESIZE && trace[2].surface==1 &&
           rect_eq(trace[2].rect,(int32_t[4]){1750,180,1758,720}));
    assert(trace[3].kind==RECT_NEW && rect_eq(trace[3].rect,(int32_t[4]){0,0,8,540}));
    assert(trace[4].kind==ALIGN && trace[4].id==3);
    assert(trace[5].kind==MASK_SURFACE);
    assert(trace[6].kind==MASK_NAME && trace[6].id==34);
    assert(trace[7].kind==MASK_CONTAINER);
    assert(trace[8].kind==DRAW && trace[8].surface==1 && trace[8].id==32);
    assert(trace[9].kind==MASK_RECT && trace[9].id==34);
    assert(trace[10].kind==MASK_RESET);
    assert(trace[11].kind==MASK_NAME && trace[11].id==35);
    assert(trace[12].kind==ALIGN && trace[12].id==4);
    assert(trace[13].kind==DRAW && trace[13].id==33);
    assert(trace[14].kind==MASK_RECT && trace[14].id==35);
    assert(trace[15].kind==RECT_NEW &&
           rect_eq(trace[15].rect,(int32_t[4]){0,20,8,524}));
    assert(trace[16].kind==VERTICAL_RECT &&
           rect_eq(trace[16].rect,(int32_t[4]){0,20,8,524}));
    assert(trace[17].kind==VERTICAL_NAME && trace[17].id==30);
    assert(trace[18].kind==VERTICAL_CONTAINER);
    assert(trace[19].kind==DRAW && trace[19].id==27 && trace[19].transform==1);
    assert(trace[20].kind==MASK_RECT && trace[20].id==30);

    /* Empty bar: clear both surfaces and clear the background flag. */
    setup(&r);supplier_value=0x7001;n=0;
    r.state.total=10;r.state.visible=15;r.state.first=0;r.state.min_bar=36;
    r.state.background_rendered=1;
    oa_svg_scrollbar_render_bar(&r,&ops);
    assert(r.state.bar_size==0 && r.state.background_rendered==0);
    assert(n==6);
    assert(trace[0].kind==WINDOW && trace[1].kind==WINDOW);
    assert(trace[2].kind==RESIZE && trace[2].surface==1 &&
           rect_eq(trace[2].rect,(int32_t[4]){1750,180,1758,180}));
    assert(trace[3].kind==RECT_NEW && rect_eq(trace[3].rect,(int32_t[4]){0,0,8,0}));
    assert(trace[4].kind==CLEAR && trace[4].surface==1);
    assert(trace[5].kind==CLEAR && trace[5].surface==0);

    /* Min >= bar size: no vertical row draw. */
    setup(&r);supplier_value=0x7001;n=0;
    r.skin.cached_image[0]=0x7002;
    r.state.total=20;r.state.visible=15;r.state.first=0;r.state.min_bar=600;
    oa_svg_scrollbar_render_bar(&r,&ops);
    assert(r.state.bar_size==600); /* min clamp raises 540 to 600 */
    assert(n==15);
    assert(trace[14].kind==MASK_RECT && trace[14].id==35);

    /* No supplier image: no further calls and the flag stays. */
    setup(&r);supplier_value=0;n=0;
    r.skin.cached_image[0]=0;r.state.background_rendered=0;
    oa_svg_scrollbar_render_background(&r,&ops);
    assert(n==1 && trace[0].kind==SUPPLIER && r.state.background_rendered==0);

    puts("ok svg scrollbar render: background/bar surface, mask caps and row geometry");
    return 0;
}
