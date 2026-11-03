#include "openaurora/svg_list_bodies.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
struct fixture {
    uint32_t aux,size,row,item,first,item_id,wid;void *parent;uint32_t notified_parent,notified_item,notified_wid,grouped;
    unsigned count;struct {unsigned id;uint32_t a,b;} trace[12];
};
static void emit(struct fixture *f,unsigned id,uint32_t a,uint32_t b)
{ assert(f->count<12);f->trace[f->count].id=id;f->trace[f->count].a=a;f->trace[f->count].b=b;++f->count; }
enum {T_SIZE,T_ROW,T_ITEM,T_FIRST,T_FOCUS,T_SEL_UP,T_SEL_DOWN,T_WIN_UP,T_WIN_DOWN,T_WIN_TOP,T_WIN_BOTTOM,T_TO_TOP,T_TO_BOTTOM,T_ANIM,T_PARENT,T_ITEM_ID,T_WID,T_NOTIFY,T_GROUP};
static uint32_t q_aux(void *p) { return ((struct fixture *)p)->aux; }
static uint32_t q_size(void *p) { struct fixture *f=p;emit(f,T_SIZE,0,0);return f->size; }
static uint32_t q_row(void *p) { struct fixture *f=p;emit(f,T_ROW,0,0);return f->row; }
static uint32_t q_item(void *p) { struct fixture *f=p;emit(f,T_ITEM,0,0);return f->item; }
static uint32_t q_first(void *p) { struct fixture *f=p;emit(f,T_FIRST,0,0);return f->first; }
static void e_focus(void *p,uint32_t i) { emit(p,T_FOCUS,i,0); }
static void e_sel_up(void *p,uint32_t n) { emit(p,T_SEL_UP,n,0); }
static void e_sel_down(void *p,uint32_t n) { emit(p,T_SEL_DOWN,n,0); }
static void e_win_up(void *p,uint32_t n) { emit(p,T_WIN_UP,n,0); }
static void e_win_down(void *p,uint32_t n) { emit(p,T_WIN_DOWN,n,0); }
static void e_win_top(void *p) { emit(p,T_WIN_TOP,0,0); }
static void e_win_bottom(void *p) { emit(p,T_WIN_BOTTOM,0,0); }
static void e_to_top(void *p) { emit(p,T_TO_TOP,0,0); }
static void e_to_bottom(void *p) { emit(p,T_TO_BOTTOM,0,0); }
static void e_anim(void *p) { emit(p,T_ANIM,0,0); }
static void *e_parent(void *p) { struct fixture *f=p;emit(f,T_PARENT,0,0);return f->parent; }
static uint32_t q_item_id(void *p) { struct fixture *f=p;emit(f,T_ITEM_ID,0,0);return f->item_id; }
static uint32_t q_wid(void *p) { struct fixture *f=p;emit(f,T_WID,0,0);return f->wid; }
static uint32_t e_notify(void *p,void *parent,uint32_t item,uint32_t wid)
{ struct fixture *f=p;emit(f,T_NOTIFY,item,wid);f->notified_parent=(uint32_t)(uintptr_t)parent;f->notified_item=item;f->notified_wid=wid;return 0; }
static void e_group(void *p,uint32_t index) { struct fixture *f=p;emit(f,T_GROUP,index,0);f->grouped=index; }
static struct oa_svg_list_box_ops ops_for(struct fixture *f)
{
    return (struct oa_svg_list_box_ops){f,q_aux,q_size,q_row,q_item,q_first,e_focus,e_sel_up,e_sel_down,
        e_win_up,e_win_down,e_win_top,e_win_bottom,e_to_top,e_to_bottom,e_anim,e_parent,q_item_id,q_wid,e_notify,e_group};
}
int main(void)
{
    int handled=42;struct fixture f={0};struct oa_svg_list_box_ops ops=ops_for(&f);
    /* EINVAL leaves handled untouched. */
    assert(oa_svg_list_select_body(NULL,&handled)==EINVAL && handled==42);
    assert(oa_svg_list_down_body(&ops,NULL)==EINVAL);
    struct oa_svg_list_box_ops bad=ops;bad.size=NULL;
    assert(oa_svg_list_up_body(&bad,&handled)==EINVAL && handled==42);
    bad=ops;bad.go_group=NULL;
    assert(oa_svg_list_first_body(&bad,&handled)==EINVAL && handled==42);
    /* Unhandled: zero aux/size runs nothing. */
    f=(struct fixture){0};ops=ops_for(&f);
    assert(!oa_svg_list_down_body(&ops,&handled) && !handled && !f.count);
    f.aux=4;f.size=0;
    assert(!oa_svg_list_up_body(&ops,&handled) && !handled && f.count==1 && f.trace[0].id==T_SIZE);
    /* Down: middle moves selection; last with room scrolls window. */
    f=(struct fixture){.aux=5,.size=5,.row=2};ops=ops_for(&f);
    assert(!oa_svg_list_down_body(&ops,&handled) && handled==1 && f.count==5);
    assert(f.trace[3].id==T_SEL_DOWN && f.trace[3].a==1 && f.trace[4].id==T_ANIM);
    f=(struct fixture){.aux=5,.size=5,.row=4,.first=1};ops=ops_for(&f);
    assert(!oa_svg_list_down_body(&ops,&handled) && handled==1 && f.count==7);
    assert(f.trace[5].id==T_WIN_DOWN && f.trace[5].a==1 && f.trace[6].id==T_ANIM);
    /* Down at end wraps: rechecked size, window top (when not already there). */
    f=(struct fixture){.aux=7,.size=5,.row=4,.first=2};ops=ops_for(&f);
    assert(!oa_svg_list_down_body(&ops,&handled) && handled==1 && f.count==9);
    assert(f.trace[4].id==T_SIZE && f.trace[5].id==T_SIZE && f.trace[6].id==T_WIN_TOP);
    assert(f.trace[7].id==T_TO_TOP && f.trace[8].id==T_ANIM);
    /* Down wrap already at top skips the window move (third size read decides). */
    f=(struct fixture){.aux=5,.size=5,.row=4,.first=0};ops=ops_for(&f);
    assert(!oa_svg_list_down_body(&ops,&handled) && handled==1 && f.count==8);
    assert(f.trace[4].id==T_SIZE && f.trace[5].id==T_SIZE && f.trace[6].id==T_TO_TOP && f.trace[7].id==T_ANIM);
    /* Up at top with equal counts moves selection to bottom of window. */
    f=(struct fixture){.aux=5,.size=5,.row=0,.first=0};ops=ops_for(&f);
    assert(!oa_svg_list_up_body(&ops,&handled) && handled==1 && f.count==6);
    assert(f.trace[4].id==T_TO_BOTTOM && f.trace[5].id==T_ANIM);
    /* Up at top with room wraps the window to the bottom. */
    f=(struct fixture){.aux=9,.size=5,.row=0,.first=0};ops=ops_for(&f);
    assert(!oa_svg_list_up_body(&ops,&handled) && handled==1 && f.count==7);
    assert(f.trace[4].id==T_WIN_BOTTOM && f.trace[5].id==T_TO_BOTTOM && f.trace[6].id==T_ANIM);
    /* Page down past the last-page start wraps and clamps the focus index. */
    f=(struct fixture){.aux=9,.size=5,.item=7,.first=4};ops=ops_for(&f);
    assert(!oa_svg_list_page_down_body(&ops,&handled) && handled==1 && f.count==7);
    assert(f.trace[4].id==T_WIN_BOTTOM && f.trace[5].id==T_FOCUS && f.trace[5].a==8 && f.trace[6].id==T_ANIM);
    /* Page up near the top wraps and clamps at zero. */
    f=(struct fixture){.aux=9,.size=5,.item=1,.first=1};ops=ops_for(&f);
    assert(!oa_svg_list_page_up_body(&ops,&handled) && handled==1);
    assert(f.trace[4].id==T_WIN_TOP && f.trace[5].id==T_FOCUS && f.trace[5].a==0);
    /* Page down with room scrolls by the window size. */
    f=(struct fixture){.aux=20,.size=5,.item=2,.first=0};ops=ops_for(&f);
    assert(!oa_svg_list_page_down_body(&ops,&handled) && handled==1 && f.count==6);
    assert(f.trace[4].id==T_WIN_DOWN && f.trace[4].a==5 && f.trace[5].id==T_ANIM);
    /* Select notifies then consumes; null parent still consumes. */
    static unsigned char parent_obj[16];f=(struct fixture){.aux=4,.size=4,.item_id=7,.wid=9,.parent=parent_obj};ops=ops_for(&f);
    assert(!oa_svg_list_select_body(&ops,&handled) && handled==1 && f.count==5);
    assert(f.trace[1].id==T_PARENT && f.trace[2].id==T_ITEM_ID && f.trace[3].id==T_WID);
    assert(f.trace[4].id==T_NOTIFY && f.notified_item==7 && f.notified_wid==9 && f.notified_parent==(uint32_t)(uintptr_t)parent_obj);
    f.parent=NULL;
    assert(!oa_svg_list_select_body(&ops,&handled) && handled==1 && f.count==7);
    /* First/last synthesize group indices. */
    f=(struct fixture){.aux=9,.size=9,.item=3};ops=ops_for(&f);
    assert(!oa_svg_list_first_body(&ops,&handled) && handled==1 && f.count==3 && f.grouped==0);
    f.item=0;f.count=0;
    assert(!oa_svg_list_first_body(&ops,&handled) && handled==1 && f.count==2);
    f=(struct fixture){.aux=9,.size=9};ops=ops_for(&f);
    assert(!oa_svg_list_last_body(&ops,&handled) && handled==1 && f.count==2 && f.grouped==8);
    puts("ok SVG list bodies: seven key paths, signed page arithmetic, wrap/clamp, notify and group indices");
    return 0;
}
