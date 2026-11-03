#include "openaurora/svg_list_scroll.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
struct fixture {
    uint32_t size,total,flag,fields[6],visible,first;void *sb,*list;int created;
    unsigned count;struct {unsigned id;uint32_t a,b,c;} trace[16];
    uint32_t cx,cy,cw,ch;void *cparent;int ccreated;
    uint32_t pvis,psize,pfirst;int pset;unsigned visarg;
};
static void emit(struct fixture *f,unsigned id,uint32_t a,uint32_t b,uint32_t c)
{ assert(f->count<16);f->trace[f->count].id=id;f->trace[f->count].a=a;f->trace[f->count].b=b;f->trace[f->count].c=c;++f->count; }
enum {T_SIZE,T_TOTAL,T_FLAG,T_FIELD,T_CREATED,T_CREATE,T_VISIBLE,T_SETVIS,T_FIRST,T_POS};
static uint32_t q_size(void *p) { struct fixture *f=p;emit(f,T_SIZE,0,0,0);return f->size; }
static uint32_t q_total(void *p) { struct fixture *f=p;emit(f,T_TOTAL,0,0,0);return f->total; }
static uint32_t q_flag(void *p) { struct fixture *f=p;emit(f,T_FLAG,0,0,0);return f->flag; }
static uint32_t q_field(void *p,uint32_t off) { struct fixture *f=p;emit(f,T_FIELD,off,0,0);
    switch(off) { case 0x1f4:return f->fields[0];case 0x1fc:return f->fields[1];case 0x1f0:return f->fields[2];
    case 0x1f8:return f->fields[3];case 0x21c:return f->fields[4];default:return f->fields[5]; } }
static void *q_sb(void *p) { return ((struct fixture *)p)->sb; }
static void *q_list(void *p) { return ((struct fixture *)p)->list; }
static int q_visible(void *p) { struct fixture *f=p;emit(f,T_VISIBLE,0,0,0);return (int)f->visible; }
static uint32_t q_first(void *p) { struct fixture *f=p;emit(f,T_FIRST,0,0,0);return f->first; }
static int q_created(void *p,void *sb) { struct fixture *f=p;emit(f,T_CREATED,0,0,0);(void)sb;return f->created; }
static void e_create(void *p,void *sb,int created,uint32_t x,uint32_t y,uint32_t w,uint32_t h,void *parent)
{ struct fixture *f=p;emit(f,T_CREATE,x,y,w);f->cx=x;f->cy=y;f->cw=w;f->ch=h;f->cparent=parent;f->ccreated=created;(void)sb; }
static void e_setvis(void *p,void *sb,int v) { struct fixture *f=p;emit(f,T_SETVIS,(uint32_t)v,0,0);f->visarg=(unsigned)v;(void)sb; }
static void e_pos(void *p,void *sb,uint32_t vis,uint32_t size,uint32_t first)
{ struct fixture *f=p;emit(f,T_POS,vis,size,first);f->pvis=vis;f->psize=size;f->pfirst=first;f->pset=1;(void)sb; }
static struct oa_svg_list_scroll_ops ops_for(struct fixture *f)
{ return (struct oa_svg_list_scroll_ops){f,q_size,q_total,q_flag,q_field,q_sb,q_list,q_visible,q_first,q_created,e_create,e_setvis,e_pos}; }
int main(void)
{
    struct fixture f={0};struct oa_svg_list_scroll_ops ops=ops_for(&f);
    assert(oa_svg_list_update_scrollbar(NULL)==EINVAL);
    struct oa_svg_list_scroll_ops bad=ops;bad.scroll_create=NULL;
    assert(oa_svg_list_update_scrollbar(&bad)==EINVAL);
    /* Empty list with cleared flag hides an existing scrollbar. */
    static unsigned char sb[16],list[16];f=(struct fixture){.sb=sb,.list=list,.created=1};ops=ops_for(&f);
    assert(!oa_svg_list_update_scrollbar(&ops) && f.count==5);
    assert(f.trace[3].id==T_CREATED && f.trace[4].id==T_SETVIS && !f.visarg && !f.pset);
    /* Room to scroll builds the rect, shows and positions. */
    f=(struct fixture){.size=5,.total=9,.sb=sb,.list=list,.fields={3,7,11,13,17,19},.visible=1,.first=2};ops=ops_for(&f);
    assert(!oa_svg_list_update_scrollbar(&ops) && f.pset && f.pvis==9 && f.psize==5 && f.pfirst==2);
    assert(f.count==14 && f.ch==50 && f.cx==19 && f.cy==7 && f.cw==17 && f.cparent==list && !f.ccreated);
    assert(f.trace[9].id==T_CREATE && f.trace[11].id==T_SETVIS && f.visarg==1);
    assert(f.trace[12].id==T_FIRST && f.trace[13].id==T_POS);
    /* Created scrollbar skips construction but still positions. */
    f.created=1;f.count=0;f.pset=0;
    assert(!oa_svg_list_update_scrollbar(&ops) && f.pset && f.count==7);
    assert(f.trace[2].id==T_CREATED && f.trace[3].id==T_VISIBLE && f.trace[6].id==T_POS);
    /* Hidden list skips the show call. */
    f.created=0;f.count=0;f.pset=0;f.visible=0;
    assert(!oa_svg_list_update_scrollbar(&ops) && f.pset && f.count==13);
    /* Full list with cleared flag hides. */
    f=(struct fixture){.size=9,.total=5,.sb=sb,.list=list,.created=1};ops=ops_for(&f);
    assert(!oa_svg_list_update_scrollbar(&ops) && !f.pset && f.count==5 && !f.visarg);
    puts("ok SVG list scrollbar: create/hide gating, literal rect math, show and position");
    return 0;
}
