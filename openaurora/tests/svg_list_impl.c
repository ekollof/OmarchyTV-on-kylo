#include "openaurora/svg_list_impl.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
struct fixture {
    uint32_t cap,first,size,selected,slots;
    void *rows[16];uint32_t rect[16][4];unsigned char vis[16];
    int empty_message,got_focus;unsigned count;
    struct {unsigned id;uint32_t a,b;} trace[64];
};
static void emit(struct fixture *f,unsigned id,uint32_t a,uint32_t b)
{ assert(f->count<64);f->trace[f->count].id=id;f->trace[f->count].a=a;f->trace[f->count].b=b;++f->count; }
enum {T_FOCUS,T_SHOW,T_RECT,T_POS,T_DRAW,T_DROW};
static uint32_t q_cap(void *p) { return ((struct fixture *)p)->cap; }
static uint32_t q_first(void *p) { return ((struct fixture *)p)->first; }
static uint32_t q_size(void *p) { return ((struct fixture *)p)->size; }
static uint32_t q_sel(void *p) { return ((struct fixture *)p)->selected; }
static uint32_t q_slots(void *p) { return ((struct fixture *)p)->slots; }
static void *q_row(void *p,uint32_t rel) { struct fixture *f=p;assert(rel<16);return f->rows[rel]; }
static void e_srow(void *p,uint32_t rel,void *row) { struct fixture *f=p;assert(rel<16);f->rows[rel]=row; }
static void *q_owner(void *p) { (void)p;return (void *)(uintptr_t)0x77aa; }
static void e_sfirst(void *p,uint32_t v) { ((struct fixture *)p)->first=v; }
static void e_ssize(void *p,uint32_t v) { ((struct fixture *)p)->size=v; }
static void e_ssel(void *p,uint32_t v) { ((struct fixture *)p)->selected=v; }
static int q_vis(void *p,void *row) { struct fixture *f=p;return f->vis[(uintptr_t)row]; }
static void e_focus(void *p,void *row,int v) { emit((struct fixture *)p,T_FOCUS,(uint32_t)(uintptr_t)row,(uint32_t)v); }
static void e_show(void *p,void *row,int v) { emit((struct fixture *)p,T_SHOW,(uint32_t)(uintptr_t)row,(uint32_t)v); }
static void e_rect(void *p,void *row,uint32_t r[4]) { struct fixture *f=p;memcpy(r,f->rect[(uintptr_t)row],16);emit(f,T_RECT,(uint32_t)(uintptr_t)row,0); }
static void e_pos(void *p,void *row,uint32_t x,uint32_t y) { emit((struct fixture *)p,T_POS,(uint32_t)(uintptr_t)row,x|(y<<10)); }
static int q_empty(void *p,void *o) { struct fixture *f=p;(void)o;return f->empty_message; }
static void e_draw(void *p,void *o,uint32_t i,void *row) { emit((struct fixture *)p,T_DRAW,i,(uint32_t)(uintptr_t)row);(void)o; }
static void e_drow(void *p,void *o,void *row) { emit((struct fixture *)p,T_DROW,(uint32_t)(uintptr_t)row,0);(void)o; }
static int q_got(void *p,void *o) { struct fixture *f=p;(void)o;return f->got_focus; }
static struct oa_svg_list_impl_ops ops_for(struct fixture *f)
{ return (struct oa_svg_list_impl_ops){f,q_cap,q_first,q_size,q_sel,q_slots,q_row,e_srow,q_owner,
    e_sfirst,e_ssize,e_ssel,q_vis,e_focus,e_show,e_rect,e_pos,q_empty,e_draw,e_drow,q_got}; }
static void init_rows(struct fixture *f,unsigned n)
{ for(unsigned i=0;i<n;++i) { f->rows[i]=(void *)(uintptr_t)i;f->rect[i][0]=i;f->rect[i][1]=10*i;f->rect[i][2]=100;f->rect[i][3]=70;f->vis[i]=1; } }
int main(void)
{
    struct fixture f={0};struct oa_svg_list_impl_ops ops=ops_for(&f);
    assert(oa_svg_list_manager_update_impl(NULL,0,0)==EINVAL);
    struct oa_svg_list_impl_ops bad=ops;bad.row_at=NULL;
    assert(oa_svg_list_manager_rotate_rows(&bad,1)==EINVAL);
    assert(oa_svg_list_manager_update_impl(&bad,0,0)==EINVAL);
    /* Rotate +1 swaps pairs from the front with full position exchange. */
    f=(struct fixture){.size=5,.slots=5};ops=ops_for(&f);init_rows(&f,8);
    assert(!oa_svg_list_manager_rotate_rows(&ops,1) && f.count==16);
    assert(f.rows[0]==(void *)1 && f.rows[1]==(void *)2 && f.rows[4]==(void *)0);
    /* Rotate by zero or out-of-range delta does nothing. */
    f.count=0;
    assert(!oa_svg_list_manager_rotate_rows(&ops,0) && !f.count);
    assert(!oa_svg_list_manager_rotate_rows(&ops,5) && !f.count);
    assert(!oa_svg_list_manager_rotate_rows(&ops,-5) && !f.count);
    /* Negative delta rotates from the back. */
    for(unsigned i=0;i<5;++i)f.rows[i]=(void *)(uintptr_t)i;
    f.count=0;
    assert(!oa_svg_list_manager_rotate_rows(&ops,-1) && f.count==16);
    assert(f.rows[0]==(void *)4 && f.rows[3]==(void *)2 && f.rows[4]==(void *)3);
    /* Partial update with a start shift redraws only out-of-window rows. */
    f=(struct fixture){.cap=9,.first=0,.size=5,.selected=2,.slots=5,.empty_message=0,.got_focus=0};
    ops=ops_for(&f);init_rows(&f,8);f.count=0;
    assert(!oa_svg_list_manager_update_impl(&ops,3,0) && f.first==3 && f.size==5 && f.selected==2);
    assert(f.count==12 && f.rows[0]==(void *)3);
    /* Full update redraws everything in the window. */
    for(unsigned i=0;i<5;++i)f.rows[i]=(void *)(uintptr_t)i;
    f.count=0;
    assert(!oa_svg_list_manager_update_impl(&ops,3,1) && f.count==6);
    /* Empty count draws the empty message path. */
    f=(struct fixture){.cap=0,.first=0,.size=0,.selected=0,.slots=5,.empty_message=1,.got_focus=0};
    ops=ops_for(&f);init_rows(&f,8);f.count=0;
    assert(!oa_svg_list_manager_update_impl(&ops,0,0) && f.selected==0);
    assert(f.count==3 && f.trace[0].id==T_FOCUS && f.trace[2].id==T_DROW);
    /* Shrinking count hides trailing visible rows. */
    f=(struct fixture){.cap=3,.first=0,.size=9,.selected=0,.slots=5,.empty_message=0,.got_focus=0};
    ops=ops_for(&f);init_rows(&f,8);f.count=0;
    assert(!oa_svg_list_manager_update_impl(&ops,0,0) && f.size==3 && f.first==0);
    assert(f.count==3 && f.trace[1].id==T_SHOW && f.trace[2].id==T_SHOW);
    /* Clamp plus focus on gotFocus. */
    f=(struct fixture){.cap=5,.first=0,.size=5,.selected=7,.slots=5,.empty_message=0,.got_focus=1};
    ops=ops_for(&f);init_rows(&f,8);f.count=0;
    assert(!oa_svg_list_manager_update_impl(&ops,0,0) && f.selected==4);
    assert(f.count==1 && f.trace[0].id==T_FOCUS && f.trace[0].a==4 && f.trace[0].b==1);
    puts("ok SVG list impl: rotate pairs/position swap, partial/full redraw, hide, empty, clamp/focus");
    return 0;
}
