#include "openaurora/settings_row_render.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
struct rrow {
    uint32_t type;uint32_t words[16];int flag;const char *label,*blabel;
    void *rects[4];uint32_t rwords[4][4];int32_t pt[2];
    int created;unsigned count;struct {uint32_t kind,off,a,b;char text[24];} trace[48];
};
static struct rrow *t;
static void emit(uint32_t kind,uint32_t off,uint32_t a,uint32_t b,const char *text)
{
    assert(t->count<48);t->trace[t->count].kind=kind;t->trace[t->count].off=off;
    t->trace[t->count].a=a;t->trace[t->count].b=b;
    if(text) { assert(strlen(text)<sizeof t->trace[0].text);strcpy(t->trace[t->count].text,text); }
    else t->trace[t->count].text[0]=0;
    ++t->count;
}
enum {R_BASE,R_GET,R_IS,R_NAME,R_FOCUS,R_ELIP,R_FONT,R_POINT,R_RECT,R_TEXT,R_CTEXT,R_CBTN};
static void q_base(void *r) { (void)r;emit(R_BASE,0,0,0,NULL); }
static void *q_cont(void *r) { (void)r;return (void *)0x1000; }
static void *q_rect(void *c,const char *k)
{ (void)c;emit(R_GET,0,0,0,k);for(int i=0;i<4;++i)if(!strcmp(k,((const char*[]){"alexa","text1","labelLong","button"})[i]))return t->rects[i];assert(0);return NULL; }
static void q_words(void *r,uint32_t out[4])
{ for(int i=0;i<4;++i)if(r==t->rects[i]) { memcpy(out,t->rwords[i],16);return; }assert(0); }
static uint32_t q_type(void *r) { (void)r;return t->type; }
static uint32_t q_word(void *r,uint32_t o) { (void)r;assert(o>=0x2e8c && o<=0x2eb8 && !(o&3));return t->words[(o-0x2e8c)/4]; }
static int q_flag(void *r,uint32_t o) { (void)r;assert(o==0x2e45);return t->flag; }
static const char *q_label(void *r) { (void)r;return t->label; }
static const char *q_blabel(void *r) { (void)r;return t->blabel; }
static void *q_child(void *r,uint32_t o) { (void)r;return (void *)(uintptr_t)(0x2000+o); }
static int q_is(void *c) { (void)c;emit(R_IS,(uint32_t)(uintptr_t)c-0x2000,0,0,NULL);return t->created; }
static void q_name(void *c,uint32_t s,uint32_t id) { emit(R_NAME,(uint32_t)(uintptr_t)c-0x2000,s,id,NULL); }
static void q_focus(void *c,int on) { emit(R_FOCUS,(uint32_t)(uintptr_t)c-0x2000,(uint32_t)on,0,NULL); }
static void q_elipes(void *c,int on) { emit(R_ELIP,(uint32_t)(uintptr_t)c-0x2000,(uint32_t)on,0,NULL); }
static void q_font(void *c,uint32_t id) { emit(R_FONT,(uint32_t)(uintptr_t)c-0x2000,id,0,NULL); }
static void q_point(void *r,int32_t p[2]) { (void)r;p[0]=t->pt[0];p[1]=t->pt[1]; }
static void q_srect(void *c,const uint32_t r[4])
{ char b[24];snprintf(b,sizeof b,"%x/%x/%x/%x",r[0],r[1],r[2],r[3]);emit(R_RECT,(uint32_t)(uintptr_t)c-0x2000,0,0,b); }
static void q_text(void *c,const char *s) { emit(R_TEXT,(uint32_t)(uintptr_t)c-0x2000,0,0,s); }
static void q_ctext(void *c,uint32_t z,const char *l,const uint32_t r[4],uint32_t x,void *par)
{
    (void)par;(void)r;emit(R_CTEXT,(uint32_t)(uintptr_t)c-0x2000,x,z,l);
}
static void q_cbtn(void *b,const char *l,const uint32_t r[4],void *par)
{ char t2[24];snprintf(t2,sizeof t2,"%s",l);emit(R_CBTN,(uint32_t)(uintptr_t)b-0x2000,0,0,t2);(void)r;(void)par; }
static struct oa_setting_row_render_ops ops(void)
{
    return (struct oa_setting_row_render_ops){t,q_base,q_cont,q_rect,q_words,q_type,q_word,
        q_flag,q_label,q_blabel,q_child,q_is,q_name,q_focus,q_elipes,q_font,q_point,
        q_srect,q_text,q_ctext,q_cbtn};
}
static void reset(struct rrow *r)
{
    memset(r,0,sizeof *r);t=r;r->type=UINT32_MAX;r->label="";r->blabel="";
    for(int i=0;i<4;++i) { r->rects[i]=(void *)(uintptr_t)(0x3000+i*0x20); }
}
int main(void)
{
    struct rrow r;struct oa_setting_row_render_ops o;
    /* Immediate default: no callbacks at all. */
    reset(&r);o=ops();r.type=0x1b;
    assert(!oa_setting_row_render(&r,&o) && r.count==0);
    /* Out-of-range switch default: prologue plus shared tail, then default. */
    reset(&r);o=ops();r.type=0x100;
    assert(!oa_setting_row_render(&r,&o) && r.count==13);
    assert(r.trace[0].kind==R_BASE && r.trace[1].kind==R_GET && !strcmp(r.trace[1].text,"alexa"));
    assert(!strcmp(r.trace[2].text,"text1") && !strcmp(r.trace[3].text,"labelLong"));
    assert(r.trace[4].kind==R_IS && r.trace[4].off==0x104);
    assert(r.trace[12].kind==R_CTEXT && !strcmp(r.trace[12].text,""));
    /* Type 2..26 stay open, after the shared tail where it runs. */
    reset(&r);o=ops();r.type=2;
    assert(oa_setting_row_render(&r,&o)==ENOTSUP && r.count==13);
    /* Type 0x1a: open before the tail. */
    reset(&r);o=ops();r.type=0x1a;
    assert(oa_setting_row_render(&r,&o)==ENOTSUP && r.count==4);
    /* Type 0 with missing key2: prologue then out. */
    reset(&r);o=ops();r.type=0;r.rects[1]=NULL;
    assert(!oa_setting_row_render(&r,&o) && r.count==4);
    /* Type 1 created: full label path with rect math. */
    reset(&r);o=ops();r.type=1;r.created=1;
    for(int i=0;i<4;++i)r.rwords[1][i]=(uint32_t)(10+i);
    r.pt[0]=5;r.pt[1]=-3;r.label="V";r.blabel="B";
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==7);
    assert(r.trace[4].kind==R_IS && r.trace[4].off==0x3c4);
    assert(r.trace[5].kind==R_RECT && !strcmp(r.trace[5].text,"f/8/11/a"));
    assert(r.trace[6].kind==R_TEXT && !strcmp(r.trace[6].text,"V"));
    /* Type 1 not created: names, focus, create with type extra. */
    reset(&r);o=ops();r.type=1;r.label="V";
    for(int i=0;i<4;++i)r.rwords[1][i]=(uint32_t)(10+i);
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==12);
    assert(r.trace[5].kind==R_NAME && r.trace[5].a==1 && r.trace[5].b==0x1ec);
    assert(r.trace[9].kind==R_FOCUS && r.trace[9].a==0);
    assert(r.trace[11].kind==R_CTEXT && !strcmp(r.trace[11].text,"V") && r.trace[11].a==1 && r.trace[11].b==0);
    /* Shared 0x104 tail via type 0, created. */
    reset(&r);o=ops();r.type=0;r.created=1;
    for(int i=0;i<4;++i)r.rwords[1][i]=(uint32_t)i;
    assert(!oa_setting_row_render(&r,&o));
    assert(r.trace[4].kind==R_IS && r.trace[4].off==0x104);
    assert(r.trace[6].kind==R_TEXT && !strcmp(r.trace[6].text,""));
    /* Shared tail not created, key1 missing: 0x2e90 names + stale-label create. */
    reset(&r);o=ops();r.type=0;r.rects[0]=NULL;r.words[(0x2e90-0x2e8c)/4]=0xaa;
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==12);
    assert(r.trace[5].kind==R_NAME && r.trace[5].b==0xaa);
    assert(r.trace[11].kind==R_CTEXT && !strcmp(r.trace[11].text,"") && r.trace[11].b==0);
    /* Type 4 created button path (shared tail runs first when key2 present). */
    reset(&r);o=ops();r.type=4;r.created=1;r.blabel="PB";
    for(int i=0;i<4;++i)r.rwords[3][i]=(uint32_t)(100+i);
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==11);
    assert(r.trace[4].kind==R_IS && r.trace[4].off==0x104);
    assert(r.trace[8].kind==R_IS && r.trace[8].off==0x684);
    assert(r.trace[10].kind==R_TEXT && !strcmp(r.trace[10].text,"PB"));
    /* Type 4 not created, key1 missing: shared tail, then button create. */
    reset(&r);o=ops();r.type=4;r.rects[0]=NULL;r.blabel="PB";
    for(int i=0;i<4;++i)r.rwords[3][i]=(uint32_t)(100+i);
    r.words[(0x2e90-0x2e8c)/4]=0xbb;
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==19);
    assert(r.trace[5].kind==R_NAME && r.trace[5].b==0xbb);
    assert(r.trace[18].kind==R_CBTN && !strcmp(r.trace[18].text,"PB"));
    /* Type 4 reconstruct: flag set with key3 present runs the shared
     * tail, then falls through to the button body. */
    reset(&r);o=ops();r.type=4;r.created=1;r.flag=1;r.blabel="PB";
    for(int i=0;i<4;++i)r.rwords[3][i]=(uint32_t)(100+i);
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==11);
    assert(r.trace[4].kind==R_IS && r.trace[4].off==0x104);
    assert(r.trace[8].kind==R_IS && r.trace[8].off==0x684);
    assert(r.trace[10].kind==R_TEXT && !strcmp(r.trace[10].text,"PB"));
    /* Invalid ops/args change nothing. */
    o=ops();o.set_name=NULL;
    assert(oa_setting_row_render(&r,&o)==EINVAL && r.count==11);
    assert(oa_setting_row_render(NULL,&o)==EINVAL);
    assert(oa_setting_row_render(&r,NULL)==EINVAL);
    puts("ok settings row render: 0/1/4 paths, shared tail, gates and open types");
    return 0;
}
