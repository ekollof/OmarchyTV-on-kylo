#include "openaurora/svg_list_anim.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct afixture {
    int enabled;uint32_t item;void *row;uint32_t rect[4],prev[4],cur[4];
    const char *stdname;uint32_t nslots;
    const char *names[4];const char *durs[4];int present[4];
    float started;int did_start;
    unsigned count;struct {unsigned id;uint32_t a,b;} trace[64];
};
static struct afixture *current_a;
enum {A_EN,A_SEL,A_ROW,A_RECT,A_RCUR,A_WPREV,A_WCUR,A_STD,A_COUNT,A_AT,A_NEW,A_LEN,A_SUB,A_EQ,A_FREE,A_START};
static void aemit(unsigned id,uint32_t a,uint32_t b)
{ assert(current_a->count<64);current_a->trace[current_a->count].id=id;current_a->trace[current_a->count].a=a;current_a->trace[current_a->count].b=b;++current_a->count; }
static int aq_enabled(void *l) { (void)l;aemit(A_EN,0,0);return current_a->enabled; }
static uint32_t aq_sel(void *m) { (void)m;aemit(A_SEL,0,0);return 7; }
static void *aq_row(void *m,uint32_t i) { (void)m;aemit(A_ROW,i,0);return current_a->row; }
static void aq_rect(void *r,uint32_t o[4]) { (void)r;memcpy(o,current_a->rect,16);aemit(A_RECT,0,0); }
static void aq_rcur(void *l,uint32_t o[4]) { (void)l;memcpy(o,current_a->cur,16);aemit(A_RCUR,0,0); }
static void aq_wprev(void *l,const uint32_t v[4]) { (void)l;memcpy(current_a->prev,v,16);aemit(A_WPREV,0,0); }
static void aq_wcur(void *l,const uint32_t v[4]) { (void)l;memcpy(current_a->cur,v,16);aemit(A_WCUR,0,0); }
static const char *aq_std(void *l,uint32_t id) { (void)l;aemit(A_STD,id,0);return current_a->stdname; }
static uint32_t aq_count(void *c) { (void)c;aemit(A_COUNT,0,0);return current_a->nslots; }
static void *aq_at(void *c,uint32_t i)
{ (void)c;aemit(A_AT,i,0);return (i<4 && current_a->present[i])?(void *)(uintptr_t)(0x500u+i):NULL; }
static const char *aq_name(void *a)
{ uintptr_t i=(uintptr_t)a-0x500u;assert(i<4);return current_a->names[i]; }
static const char *aq_text(void *a)
{ uintptr_t i=(uintptr_t)a-0x500u;assert(i<4);return current_a->durs[i]; }
static void *aq_ctrl(void *o) { (void)o;return (void *)(uintptr_t)0x77aa; }
static void *aq_owner(void *l) { (void)l;return (void *)(uintptr_t)0x99aa; }
static void *aq_container(void *l) { (void)l;return (void *)(uintptr_t)0x55aa; }
static void *aq_target(void *l) { (void)l;return (void *)(uintptr_t)0x33aa; }
static void aq_start(void *c,void *t,float v)
{ assert(c==(void *)(uintptr_t)0x77aa && t==(void *)(uintptr_t)0x33aa);aemit(A_START,0,0);current_a->started=v;current_a->did_start=1; }
static uint32_t aq_len(const char *s) { aemit(A_LEN,(uint32_t)strlen(s),0);return (uint32_t)strlen(s); }
static const char *aq_sub(const char *s,uint32_t st,uint32_t n)
{ (void)n;aemit(A_SUB,st,0);return s+st; }
static int aq_eq(const char *a,const char *b) { int r=!strcmp(a,b);aemit(A_EQ,(uint32_t)r,0);return r; }
static void aq_new(const char *s) { aemit(A_NEW,(uint32_t)strlen(s),0); }
static void aq_free(const char *t) { (void)t;aemit(A_FREE,0,0); }
static struct oa_svg_list_anim_ops aops_for(void)
{ return (struct oa_svg_list_anim_ops){NULL,aq_enabled,aq_sel,aq_row,aq_rect,aq_rcur,aq_wprev,aq_wcur,aq_std,aq_count,aq_at,aq_name,aq_text,aq_ctrl,aq_owner,aq_container,aq_target,aq_start,aq_new,aq_len,aq_sub,aq_eq,aq_free}; }
static unsigned tcount(struct afixture *f,unsigned id)
{ unsigned n=0;for(unsigned i=0;i<f->count;++i)if(f->trace[i].id==id)++n;return n; }
int main(void)
{
    struct afixture f={0};struct oa_svg_list_anim_ops ops=aops_for();current_a=&f;float started=-1.0f;
    struct oa_svg_list_anim_ops bad={0};
    assert(oa_svg_list_start_animation(NULL,NULL,NULL,&started)==EINVAL);
    assert(oa_svg_list_start_animation(&bad,NULL,NULL,&started)==EINVAL);
    assert(oa_svg_list_start_animation(&ops,NULL,NULL,&started)==EINVAL);
    /* Disabled animation runs nothing. */
    f.enabled=0;
    assert(!oa_svg_list_start_animation(&ops,(void *)1,(void *)2,&started) && f.count==1);
    assert(f.trace[0].id==A_EN && started==-1.0f);
    /* Null row runs only the gate and the fetch. */
    static unsigned char rowobj[8];f.enabled=1;f.row=NULL;f.count=0;
    assert(!oa_svg_list_start_animation(&ops,(void *)1,(void *)2,&started) && started==-1.0f);
    assert(f.count==3 && f.trace[2].id==A_ROW);
    /* Equal rectangle stops after the compare, with no snapshot write. */
    f.row=rowobj;f.rect[0]=1;f.rect[1]=2;f.rect[2]=3;f.rect[3]=4;
    f.cur[0]=1;f.cur[1]=2;f.cur[2]=3;f.cur[3]=4;f.count=0;f.prev[0]=9;
    assert(!oa_svg_list_start_animation(&ops,(void *)1,(void *)2,&started) && started==-1.0f);
    assert(f.count==5 && f.trace[4].id==A_RCUR && f.prev[0]==9);
    /* Changed rectangle snapshots, then an empty container starts nothing. */
    f.cur[0]=0;f.count=0;f.nslots=0;f.stdname="M";
    assert(!oa_svg_list_start_animation(&ops,(void *)1,(void *)2,&started) && started==-1.0f);
    assert(f.prev[0]==0 && f.prev[1]==2 && f.cur[0]==1 && f.cur[1]==2 && f.cur[2]==3 && f.cur[3]==4);
    assert(tcount(&f,A_STD)==1 && tcount(&f,A_COUNT)==1 && tcount(&f,A_AT)==0 && tcount(&f,A_FREE)==1);
    /* Single matching animation starts with atof*1000. */
    f.nslots=1;f.present[0]=1;f.names[0]="xM";f.durs[0]="2.5";f.count=0;f.did_start=0;
    f.rect[3]=9;
    assert(!oa_svg_list_start_animation(&ops,(void *)1,(void *)2,&started) && f.did_start);
    assert(started==2500.0f && tcount(&f,A_START)==1);
    /* First match wins; later entries never run. */
    f.nslots=3;f.present[0]=1;f.present[1]=1;f.present[2]=1;
    f.names[0]="yNo";f.durs[0]="100";f.names[1]="xM";f.durs[1]="0.5";f.names[2]="xM";f.durs[2]="7";
    f.count=0;f.did_start=0;started=-1.0f;f.rect[3]=10;
    assert(!oa_svg_list_start_animation(&ops,(void *)1,(void *)2,&started) && f.did_start);
    assert(started==500.0f && tcount(&f,A_AT)==2 && tcount(&f,A_START)==1);
    /* No match anywhere: every entry is probed, nothing starts. */
    f.names[1]="zNo";f.names[2]="wNo";f.count=0;f.did_start=0;started=-1.0f;f.rect[3]=11;
    assert(!oa_svg_list_start_animation(&ops,(void *)1,(void *)2,&started) && !f.did_start && started==-1.0f);
    assert(tcount(&f,A_AT)==3 && tcount(&f,A_START)==0);
    /* Null entries are skipped without any string work. */
    f.present[0]=0;f.count=0;f.rect[3]=12;
    assert(!oa_svg_list_start_animation(&ops,(void *)1,(void *)2,&started) && tcount(&f,A_NEW)==2);
    puts("ok SVG list anim: gates, rect snapshot, ordered search and first-match start");
    return 0;
}
