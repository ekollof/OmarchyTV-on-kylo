#include "openaurora/svg_list_rows.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
struct fixture {
    uint32_t first,size,selected;uint32_t rows[8];unsigned count;
    struct {unsigned id;uint32_t a;int b;} trace[8];uint32_t impl_index;int impl_full;
};
static void emit(struct fixture *f,unsigned id,uint32_t a,int b)
{ assert(f->count<8);f->trace[f->count].id=id;f->trace[f->count].a=a;f->trace[f->count].b=b;++f->count; }
enum {T_FIRST,T_SIZE,T_SEL,T_ROW,T_FOCUS,T_STORE,T_IMPL};
static uint32_t q_first(void *p) { struct fixture *f=p;emit(f,T_FIRST,0,0);return f->first; }
static uint32_t q_size(void *p) { struct fixture *f=p;emit(f,T_SIZE,0,0);return f->size; }
static uint32_t q_sel(void *p) { struct fixture *f=p;emit(f,T_SEL,0,0);return f->selected; }
static void *q_row(void *p,uint32_t rel) { struct fixture *f=p;emit(f,T_ROW,rel,0);assert(rel<8);return (void *)(uintptr_t)(0x1000+rel); }
static struct fixture *current;
static void row_focus_rec(void *row,int focused)
{ emit(current,T_FOCUS,(uint32_t)(uintptr_t)row,focused); }
static void e_store(void *p,uint32_t rel) { struct fixture *f=p;emit(f,T_STORE,rel,0);f->selected=rel; }
static void e_impl(void *p,uint32_t index,int full) { struct fixture *f=p;emit(f,T_IMPL,index,full);f->impl_index=index;f->impl_full=full; }
static struct oa_svg_list_manager_ops ops_for(struct fixture *f)
{ return (struct oa_svg_list_manager_ops){f,q_first,q_size,q_sel,q_row,row_focus_rec,e_store,e_impl}; }
int main(void)
{
    int error=42;struct fixture f={0};struct oa_svg_list_manager_ops ops=ops_for(&f);current=&f;
    assert(!oa_svg_list_manager_size(NULL,&error) && error==EINVAL);
    assert(oa_svg_list_manager_focus_item(NULL,0)==EINVAL);
    struct oa_svg_list_manager_ops bad=ops;bad.row_focus=NULL;
    assert(oa_svg_list_manager_focus_item(&bad,0)==EINVAL);
    assert(oa_svg_list_manager_update_partial(&bad,0)==EINVAL);
    /* Getters read raw words; selected item adds first and selected row. */
    f=(struct fixture){.first=3,.size=9,.selected=2};ops=ops_for(&f);current=&f;
    assert(oa_svg_list_manager_size(&ops,&error)==9 && !error);
    assert(oa_svg_list_manager_selected_row(&ops,NULL)==2);
    assert(oa_svg_list_manager_selected_item(&ops,&error)==5 && !error);
    assert(oa_svg_list_manager_first_visible(&ops,&error)==3 && !error);
    /* Focus with zero count or out-of-range index runs nothing. */
    f=(struct fixture){.first=3,.size=0,.selected=2};ops=ops_for(&f);current=&f;
    assert(!oa_svg_list_manager_focus_item(&ops,4) && f.count==1 && f.selected==2);
    f.size=9;f.count=0;
    assert(!oa_svg_list_manager_focus_item(&ops,30) && f.count==2 && f.selected==2);
    /* Already-selected index runs queries but no row calls or store. */
    f=(struct fixture){.first=3,.size=9,.selected=2};ops=ops_for(&f);current=&f;
    assert(!oa_svg_list_manager_focus_item(&ops,5) && f.count==3 && f.selected==2);
    /* New index unfocuses the old row, focuses the new one and stores rel. */
    f=(struct fixture){.first=3,.size=9,.selected=2};ops=ops_for(&f);current=&f;
    assert(!oa_svg_list_manager_focus_item(&ops,6) && f.selected==3);
    assert(f.count==8 && f.trace[3].id==T_ROW && f.trace[3].a==2);
    assert(f.trace[4].id==T_FOCUS && f.trace[4].a==0x1002 && !f.trace[4].b);
    assert(f.trace[5].id==T_ROW && f.trace[5].a==3);
    assert(f.trace[6].id==T_FOCUS && f.trace[6].a==0x1003 && f.trace[6].b==1);
    assert(f.trace[7].id==T_STORE && f.trace[7].a==3);
    /* Partial forwards to updateImpl with full=false. */
    f=(struct fixture){0};ops=ops_for(&f);current=&f;
    assert(!oa_svg_list_manager_update_partial(&ops,7) && f.count==1);
    assert(f.trace[0].id==T_IMPL && f.trace[0].a==7 && !f.trace[0].b && f.impl_full==0);
    puts("ok SVG list rows: manager field layout, focus switch/store and partial forward");
    return 0;
}
