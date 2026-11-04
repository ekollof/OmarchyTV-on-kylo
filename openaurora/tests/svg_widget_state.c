#include "openaurora/svg_widget_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned calls;
static struct { uint32_t kind,a; } ev[8];
static int created;
static void push(uint32_t kind,uint32_t a)
{ assert(calls<8);ev[calls].kind=kind;ev[calls].a=a;calls++; }
static void b_focus(void *w,int f) { (void)w;push(1,(uint32_t)f); }
static int is_created(void *w) { (void)w;return created; }
static void draw_tb(void *w) { (void)w;push(2,0); }
static void slide_tb(void *w) { (void)w;push(3,0); }
static struct oa_svg_widget_state_ops ops(void)
{ return (struct oa_svg_widget_state_ops){b_focus,is_created,draw_tb,slide_tb}; }

static uint32_t state(const void *p) { uint32_t v;memcpy(&v,(const unsigned char *)p+0x6c,4);return v; }
static void set_state(void *p,uint32_t v) { memcpy((unsigned char *)p+0x6c,&v,4); }

int main(void)
{
    unsigned char win[0x300];
    struct oa_svg_widget_state_ops o=ops();
    static const uint32_t focus_on[4]={1,1,3,3},focus_off[4]={0,0,2,2};
    static const uint32_t act_off[4]={2,3,2,3},act_on[4]={0,1,0,1};
    memset(win,0,sizeof win);

    /* SVGWindow::setFocus qualification: 0->1/2->3 on, 1->0/3->2 off. */
    for(uint32_t s=0;s<4;++s) {
        set_state(win,s);calls=0;
        oa_svg_window_set_focus(win,1,&o);
        assert(state(win)==focus_on[s] && calls==1 && ev[0].kind==1 && ev[0].a==1);
        set_state(win,s);calls=0;
        oa_svg_window_set_focus(win,0,&o);
        assert(state(win)==focus_off[s] && calls==1 && ev[0].a==0);
    }
    /* SVGWindow::setActive: zero argument activates, non-zero deactivates. */
    for(uint32_t s=0;s<4;++s) {
        set_state(win,s);
        oa_svg_window_set_active(win,0);
        assert(state(win)==act_off[s]);
        set_state(win,s);
        oa_svg_window_set_active(win,1);
        assert(state(win)==act_on[s]);
    }
    /* SVGTextBox::setFocus redraws only on a state change with a created
     * window. */
    set_state(win,0);created=1;calls=0;
    oa_svg_text_box_set_focus(win,1,&o);
    assert(state(win)==1 && calls==3 && ev[0].kind==1 && ev[1].kind==2 && ev[2].kind==3);
    set_state(win,1);created=1;calls=0;
    oa_svg_text_box_set_focus(win,1,&o);
    assert(state(win)==1 && calls==1); /* no change: no redraw */
    set_state(win,0);created=0;calls=0;
    oa_svg_text_box_set_focus(win,1,&o);
    assert(state(win)==1 && calls==1); /* not created: no redraw */
    /* setElipses: mode 5 ignores switching on, off still stores. */
    memset(win,0,sizeof win);
    { uint32_t five=5;memcpy(win+0x220,&five,4); }
    win[0x1b1]=0;
    oa_svg_text_box_set_elipses(win,1);
    assert(win[0x1b1]==0);
    win[0x1b1]=1;
    oa_svg_text_box_set_elipses(win,0);
    assert(win[0x1b1]==0);
    { uint32_t nine=9;memcpy(win+0x220,&nine,4); }
    oa_svg_text_box_set_elipses(win,1);
    assert(win[0x1b1]==1);

    puts("ok svg widget state: focus/active qualification and textbox elipses mode");
    return 0;
}
