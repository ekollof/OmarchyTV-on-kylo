#include "openaurora/svg_list_rows.h"
#include "openaurora/svg_list_update.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
struct rfixture {
    uint32_t first,size,selected;void *rows[8];unsigned count;
    struct {unsigned id;uint32_t a;} trace[32];
};
static void remit(struct rfixture *f,unsigned id,uint32_t a)
{ assert(f->count<32);f->trace[f->count].id=id;f->trace[f->count].a=a;++f->count; }
enum {R_FIRST,R_SIZE,R_SEL,R_ROW};
static uint32_t rq_first(void *p) { struct rfixture *f=p;remit(f,R_FIRST,0);return f->first; }
static uint32_t rq_size(void *p) { struct rfixture *f=p;remit(f,R_SIZE,0);return f->size; }
static uint32_t rq_sel(void *p) { struct rfixture *f=p;remit(f,R_SEL,0);return f->selected; }
static uint32_t rq_slots(void *p) { (void)p;return 8; }
static void *rq_row(void *p,uint32_t rel) { struct rfixture *f=p;remit(f,R_ROW,rel);assert(rel<8);return f->rows[rel]; }
static void e_rstore(void *p,uint32_t v) { ((struct rfixture *)p)->selected=v; }
static void e_rimpl(void *p,uint32_t i,int fl) { (void)p;(void)i;(void)fl; }
static void rq_nofocus(void *row,int v) { (void)row;(void)v; }
static struct oa_svg_list_manager_ops rops_for(struct rfixture *f)
{ return (struct oa_svg_list_manager_ops){f,rq_first,rq_size,rq_sel,rq_row,rq_nofocus,e_rstore,e_rimpl,rq_slots}; }
struct ufixture { uint32_t total,aux,first;unsigned count;struct {unsigned id;uint32_t a;} trace[8]; };
static void uemit(struct ufixture *f,unsigned id,uint32_t a)
{ assert(f->count<8);f->trace[f->count].id=id;f->trace[f->count].a=a;++f->count; }
enum {U_TOTAL,U_STORE,U_FIRST,U_FULL,U_SCROLL,U_ANIM};
static uint32_t uq_total(void *p) { struct ufixture *f=p;uemit(f,U_TOTAL,0);return f->total; }
static void u_store(void *p,uint32_t v) { struct ufixture *f=p;uemit(f,U_STORE,v);f->aux=v; }
static uint32_t uq_first(void *p) { struct ufixture *f=p;uemit(f,U_FIRST,0);return f->first; }
static void u_full(void *p,uint32_t a,uint32_t b) { struct ufixture *f=p;uemit(f,U_FULL,a);(void)b; }
static void u_scroll(void *p) { uemit((struct ufixture *)p,U_SCROLL,0); }
static void u_anim(void *p) { uemit((struct ufixture *)p,U_ANIM,0); }
struct refresh_fixture { uint32_t first,total,slots,index,relative;unsigned calls;void *row; };
static uint32_t fq_first(void *p) { return ((struct refresh_fixture *)p)->first; }
static uint32_t fq_total(void *p) { return ((struct refresh_fixture *)p)->total; }
static uint32_t fq_slots(void *p) { return ((struct refresh_fixture *)p)->slots; }
static void *fq_row(void *p,uint32_t rel)
{ struct refresh_fixture *f=p;f->relative=rel;return f->row; }
static void fq_refresh(void *p,uint32_t index,void *row)
{ struct refresh_fixture *f=p;assert(row==f->row);f->index=index;++f->calls; }
int main(void)
{
    int error=42;struct rfixture f={0};
    struct oa_svg_list_manager_ops ops=rops_for(&f);
    struct oa_svg_list_manager_ops bad={0};
    assert(oa_svg_list_manager_size(&bad,&error)==0 && error==EINVAL);
    assert(oa_svg_list_manager_total_rows(&bad,&error)==0 && error==EINVAL);
    assert(oa_svg_list_manager_item_row(&bad,0,&error)==NULL && error==EINVAL);
    assert(oa_svg_list_manager_item_row(NULL,0,NULL)==NULL);
    /* Total is the slot count; last is first+size-1 wrapping. */
    assert(oa_svg_list_manager_total_rows(&ops,&error)==8 && !error);
    f.first=3;f.size=5;
    assert(oa_svg_list_manager_last_visible(&ops,NULL)==7);
    f.first=0xfffffffe;f.size=3;
    assert(oa_svg_list_manager_last_visible(&ops,NULL)==0);
    /* getItemRow bounds-checks against first and first+size. */
    for(unsigned i=0;i<8;++i)f.rows[i]=(void *)(uintptr_t)(0x50+i);
    f.first=2;f.size=3;
    assert(oa_svg_list_manager_item_row(&ops,1,NULL)==NULL);
    assert(oa_svg_list_manager_item_row(&ops,2,NULL)==(void *)0x50);
    assert(oa_svg_list_manager_item_row(&ops,4,NULL)==(void *)0x52);
    assert(oa_svg_list_manager_item_row(&ops,5,NULL)==NULL);
    assert(oa_svg_list_manager_item_row(&ops,0xffffffff,NULL)==NULL);
    /* updateEv stores the total then refreshes window, scrollbar, animation. */
    struct ufixture u={.total=9,.first=4};struct oa_svg_list_update_ops uops={&u,uq_total,u_store,uq_first,u_full,u_scroll,u_anim};
    assert(!oa_svg_list_update(&uops) && u.aux==9 && u.count==6);
    assert(u.trace[0].id==U_TOTAL && u.trace[1].id==U_STORE && u.trace[1].a==9);
    assert(u.trace[2].id==U_FIRST && u.trace[3].id==U_FULL && u.trace[3].a==4);
    assert(u.trace[4].id==U_SCROLL && u.trace[5].id==U_ANIM);
    assert(oa_svg_list_update(NULL)==EINVAL);
    struct oa_svg_list_update_ops ubad=uops;ubad.update_scrollbar=NULL;
    assert(oa_svg_list_update(&ubad)==EINVAL);
    /* Single-row refresh uses total+8 capped by slots, not visible size.
     * Overflowing first+count is an empty range even when rel<count. */
    struct refresh_fixture rf={.first=3,.total=9,.slots=4,.row=&f};
    struct oa_svg_list_refresh_ops refresh={&rf,fq_first,fq_total,fq_slots,fq_row,fq_refresh};
    assert(!oa_svg_list_refresh_row(&refresh,2) && !rf.calls);
    assert(!oa_svg_list_refresh_row(&refresh,6) && rf.calls==1 && rf.index==6 && rf.relative==3);
    assert(!oa_svg_list_refresh_row(&refresh,7) && rf.calls==1);
    rf.total=2;
    assert(!oa_svg_list_refresh_row(&refresh,5) && rf.calls==1);
    rf.first=UINT32_MAX-1;rf.total=3;
    assert(!oa_svg_list_refresh_row(&refresh,UINT32_MAX-1) && rf.calls==1);
    rf.first=0;rf.total=0;
    assert(!oa_svg_list_refresh_row(&refresh,0) && rf.calls==1);
    assert(oa_svg_list_refresh_row(NULL,0)==EINVAL);
    refresh.refresh=NULL;
    assert(oa_svg_list_refresh_row(&refresh,0)==EINVAL && rf.calls==1);
    puts("ok SVG list rows/update: total/last-visible/item-row bounds and full refresh order");
    return 0;
}
