#include "openaurora/svg_list_scrollbar.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
struct fixture {
    void *sb;int created;uint32_t c8;
    unsigned count;struct {unsigned id;uint32_t a,b,c;} trace[8];
    uint32_t sx,sy,sw,sh,sp;void *sparent;int screated;
    uint32_t pvis,psize,pfirst;int pset;
};
static void emit(struct fixture *f,unsigned id,uint32_t a,uint32_t b,uint32_t c)
{ assert(f->count<8);f->trace[f->count].id=id;f->trace[f->count].a=a;f->trace[f->count].b=b;f->trace[f->count].c=c;++f->count; }
enum {T_CREATED,T_FLAG,T_BG,T_BAR,T_STORE,T_CREATE};
static void *q_sb(void *p) { return ((struct fixture *)p)->sb; }
static int q_created(void *p,void *sb) { struct fixture *f=p;emit(f,T_CREATED,0,0,0);(void)sb;return f->created; }
static uint32_t q_flag(void *p,void *sb) { struct fixture *f=p;emit(f,T_FLAG,0,0,0);(void)sb;return f->c8; }
static void e_bg(void *p,void *sb) { emit((struct fixture *)p,T_BG,0,0,0);(void)sb; }
static void e_bar(void *p,void *sb) { emit((struct fixture *)p,T_BAR,0,0,0);(void)sb; }
static void e_store(void *p,void *sb,uint32_t vis,uint32_t size,uint32_t first)
{ struct fixture *f=p;emit(f,T_STORE,vis,size,first);f->pvis=vis;f->psize=size;f->pfirst=first;f->pset=1;(void)sb; }
static void e_create(void *p,void *sb,int created,uint32_t x,uint32_t y,uint32_t w,uint32_t h,void *parent)
{ struct fixture *f=p;emit(f,T_CREATE,x,y,w);f->sx=x;f->sy=y;f->sw=w;f->sh=h;f->sparent=parent;f->screated=created;(void)sb; }
static struct oa_svg_list_scrollbar_ops ops_for(struct fixture *f)
{ return (struct oa_svg_list_scrollbar_ops){f,q_sb,q_created,q_flag,e_bg,e_bar,e_store,e_create}; }
int main(void)
{
    static unsigned char sb[16];struct fixture f={0};struct oa_svg_list_scrollbar_ops ops=ops_for(&f);
    assert(oa_svg_list_scrollbar_create(NULL,0,0,0,0,0,NULL)==EINVAL);
    struct oa_svg_list_scrollbar_ops bad=ops;bad.common_create=NULL;
    assert(oa_svg_list_scrollbar_create(&bad,0,0,0,0,0,NULL)==EINVAL);
    assert(oa_svg_list_scrollbar_set_position(&bad,0,0,0)==EINVAL);
    /* Create forwards everything unchanged. */
    f=(struct fixture){.sb=sb};ops=ops_for(&f);
    assert(!oa_svg_list_scrollbar_create(&ops,0,7,9,100,70,sb) && f.count==1);
    assert(f.trace[0].id==T_CREATE && f.sx==7 && f.sy==9 && f.sw==100 && f.sh==70 && f.sparent==sb && !f.screated);
    /* Uncreated bar stores nothing and renders nothing. */
    f=(struct fixture){.sb=sb,.created=0};ops=ops_for(&f);
    assert(!oa_svg_list_scrollbar_set_position(&ops,9,5,2) && !f.pset && f.count==1);
    /* Signed gate: first beyond vis stores and renders nothing. */
    f=(struct fixture){.sb=sb,.created=1};ops=ops_for(&f);
    assert(!oa_svg_list_scrollbar_set_position(&ops,2,9,5) && f.count==1 && f.trace[0].id==T_CREATED);
    /* Normal path stores, draws background when unset, then the bar. */
    f=(struct fixture){.sb=sb,.created=1,.c8=0};ops=ops_for(&f);
    assert(!oa_svg_list_scrollbar_set_position(&ops,9,5,2) && f.pset);
    assert(f.count==5 && f.trace[3].id==T_BG && f.trace[4].id==T_BAR);
    assert(f.pvis==9 && f.psize==5 && f.pfirst==2);
    /* Set flag skips the background but still draws the bar. */
    f.c8=1;f.count=0;f.pset=0;
    assert(!oa_svg_list_scrollbar_set_position(&ops,9,5,2) && f.pset && f.count==4);
    assert(f.trace[3].id==T_BAR);
    puts("ok SVG scrollbar body: create forward, signed gate, store, background/bar");
    return 0;
}
