#include "openaurora/svg_widget_state.h"
#include <string.h>

#define STATE_OFF 0x6cu
#define ELIPSES_OFF 0x1b1u
#define MODE_OFF 0x220u

static uint32_t get32(const void *p,uint32_t off)
{ uint32_t v;memcpy(&v,(const unsigned char *)p+off,4);return v; }
static void set32(void *p,uint32_t off,uint32_t v)
{ memcpy((unsigned char *)p+off,&v,4); }

void oa_svg_window_set_focus(void *window,int focus,
    const struct oa_svg_widget_state_ops *o)
{
    uint32_t state;
    if(!window || !o)return;
    o->base_set_focus(window,focus);
    state=get32(window,STATE_OFF);
    if(focus) {
        if(state==0)state=1;
        else if(state==2)state=3;
    } else {
        if(state==1)state=0;
        else if(state==3)state=2;
    }
    set32(window,STATE_OFF,state);
}

/* The shipped argument is inverted relative to the state: a zero argument
 * activates (0->2, 1->3) and a non-zero argument deactivates (2->0, 3->1). */
void oa_svg_window_set_active(void *window,int active)
{
    uint32_t state;
    if(!window)return;
    state=get32(window,STATE_OFF);
    if(!active) {
        if(state==0)state=2;
        else if(state==1)state=3;
    } else {
        if(state==2)state=0;
        else if(state==3)state=1;
    }
    set32(window,STATE_OFF,state);
}

void oa_svg_text_box_set_focus(void *text_box,int focus,
    const struct oa_svg_widget_state_ops *o)
{
    uint32_t before;
    if(!text_box || !o)return;
    before=get32(text_box,STATE_OFF);
    oa_svg_window_set_focus(text_box,focus,o);
    if(before!=get32(text_box,STATE_OFF) && o->is_created(text_box)) {
        o->draw_text_box(text_box);
        o->perform_text_slide(text_box);
    }
}

/* Mode 5 ignores switching elipses on; switching it off still stores. */
void oa_svg_text_box_set_elipses(void *text_box,int on)
{
    if(!text_box)return;
    if(on && get32(text_box,MODE_OFF)==5)return;
    ((unsigned char *)text_box)[ELIPSES_OFF]=(unsigned char)(on?1:0);
}
