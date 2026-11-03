#include "openaurora/svg_list_move.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
struct fixture {
    uint32_t aux,size,item,first;unsigned count;struct {unsigned id;uint32_t a;} trace[6];
};
static void emit(struct fixture *f,unsigned id,uint32_t a)
{ assert(f->count<6);f->trace[f->count].id=id;f->trace[f->count].a=a;++f->count; }
enum {T_SIZE,T_ITEM,T_FIRST,T_FOCUS,T_PARTIAL,T_SCROLL,T_ANIM};
static uint32_t q_aux(void *p) { return ((struct fixture *)p)->aux; }
static uint32_t q_size(void *p) { struct fixture *f=p;emit(f,T_SIZE,0);return f->size; }
static uint32_t q_item(void *p) { struct fixture *f=p;emit(f,T_ITEM,0);return f->item; }
static uint32_t q_first(void *p) { struct fixture *f=p;emit(f,T_FIRST,0);return f->first; }
static void e_focus(void *p,uint32_t i) { emit(p,T_FOCUS,i); }
static void e_partial(void *p,uint32_t i) { emit(p,T_PARTIAL,i); }
static void e_scroll(void *p) { emit(p,T_SCROLL,0); }
static void e_anim(void *p) { emit(p,T_ANIM,0); }
static struct oa_svg_list_move_ops ops_for(struct fixture *f)
{ return (struct oa_svg_list_move_ops){f,q_aux,q_size,q_item,q_first,e_focus,e_partial,e_scroll,e_anim}; }
int main(void)
{
    struct fixture f={0};struct oa_svg_list_move_ops ops=ops_for(&f);
    assert(oa_svg_list_move_window_top(NULL)==EINVAL);
    struct oa_svg_list_move_ops bad=ops;bad.update_partial=NULL;
    assert(oa_svg_list_move_window_top(&bad)==EINVAL);
    bad=ops;bad.focus_item=NULL;
    assert(oa_svg_list_move_selection_up(&bad,1)==EINVAL);
    /* Top posts index 0 then scrollbar and animation. */
    f=(struct fixture){0};ops=ops_for(&f);
    assert(!oa_svg_list_move_window_top(&ops) && f.count==3);
    assert(f.trace[0].id==T_PARTIAL && !f.trace[0].a && f.trace[1].id==T_SCROLL && f.trace[2].id==T_ANIM);
    /* Bottom posts aux-size with wrapping arithmetic. */
    f=(struct fixture){.aux=9,.size=5};ops=ops_for(&f);
    assert(!oa_svg_list_move_window_bottom(&ops) && f.count==4);
    assert(f.trace[1].id==T_PARTIAL && f.trace[1].a==4 && f.trace[3].id==T_ANIM);
    f=(struct fixture){.aux=2,.size=5};ops=ops_for(&f);
    assert(!oa_svg_list_move_window_bottom(&ops) && f.count==4 && f.trace[1].a==UINT32_C(0xfffffffd));
    /* Window up/down shift first-visible by n without animating. */
    f=(struct fixture){.first=7};ops=ops_for(&f);
    assert(!oa_svg_list_move_window_up(&ops,2) && f.count==3);
    assert(f.trace[0].id==T_FIRST && f.trace[1].id==T_PARTIAL && f.trace[1].a==5 && f.trace[2].id==T_SCROLL);
    f=(struct fixture){.first=7};ops=ops_for(&f);
    assert(!oa_svg_list_move_window_down(&ops,2) && f.count==3 && f.trace[1].a==9);
    /* Selection moves focus by selected-item offset; to-top/bottom use first-visible. */
    f=(struct fixture){.item=7};ops=ops_for(&f);
    assert(!oa_svg_list_move_selection_up(&ops,2) && f.count==2 && f.trace[1].a==5);
    f=(struct fixture){.item=7};ops=ops_for(&f);
    assert(!oa_svg_list_move_selection_down(&ops,2) && f.count==2 && f.trace[1].a==9);
    f=(struct fixture){.first=3};ops=ops_for(&f);
    assert(!oa_svg_list_move_selection_to_top(&ops) && f.count==2 && f.trace[1].a==3);
    f=(struct fixture){.first=3,.size=5};ops=ops_for(&f);
    assert(!oa_svg_list_move_selection_to_bottom(&ops) && f.count==3);
    assert(f.trace[2].id==T_FOCUS && f.trace[2].a==7);
    puts("ok SVG list moves: eight primitives, wrap arithmetic, no animation on shifts");
    return 0;
}
