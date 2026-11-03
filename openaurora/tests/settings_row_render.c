#include "openaurora/settings_row_render.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
struct rrow {
    uint32_t type;uint32_t words[36];uint32_t sel_img,sel_id,num,op,hi,imgid,scale,slo,shi,sval;int flag,checked,rebuild;const char *label,*blabel,*vtext;
    void *rects[11];uint32_t rwords[11][4];int32_t pt[2];
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
enum {R_CTBOX,R_CSLIDER,R_SVAL,R_SFOCUS,R_BASE,R_GET,R_IS,R_NAME,R_FOCUS,R_ELIP,R_FONT,R_POINT,R_RECT,R_TEXT,R_CTEXT,R_CBTN,R_ALIGN,R_CHECK,R_CCB,R_SELID,R_CSEL,R_SELIMG,R_EMODE,R_MAXCH,R_CAUTO,R_NUM,R_CTE,R_STORE,R_DESTROY,R_RESETXML,R_GLYPH,R_HL,R_SUBHL,R_SHADOW,R_VIS,R_SCALE,R_CIMG,R_DRAWTOP};
static void q_base(void *r) { (void)r;emit(R_BASE,0,0,0,NULL); }
static void *q_cont(void *r) { (void)r;return (void *)0x1000; }
static void *q_rect(void *c,const char *k)
{ (void)c;emit(R_GET,0,0,0,k);
  for(int i=0;i<11;++i)if(!strcmp(k,((const char*[]){"alexa","text1","labelLong","button","checkbox","selectionbox","image1","image2","banner","slidebar","text2"})[i]))return t->rects[i];
  assert(0);return NULL; }
static void q_words(void *r,uint32_t out[4])
{ for(int i=0;i<11;++i)if(r==t->rects[i]) { memcpy(out,t->rwords[i],16);return; }assert(0); }
static void wset(struct rrow *r,uint32_t off,uint32_t v) { assert(off>=0x2e4c && off<=0x2ed8);r->words[(off-0x2e4c)/4]=v; }
static uint32_t q_type(void *r) { (void)r;return t->type; }
static uint32_t q_word(void *r,uint32_t o)
{ (void)r;
  if(o==0x2e54)return t->sel_img;
  if(o==0x2de8)return t->sel_id;
  if(o==0x2e00)return t->num;
  if(o==0x2dd4)return t->op;
  if(o==0x2df0)return t->slo;
  if(o==0x2df4)return t->shi;
  if(o==0x2df8)return t->sval;
  assert(o>=0x2e4c && o<=0x2ed8 && !(o&3));return t->words[(o-0x2e4c)/4]; }
static int q_flag(void *r,uint32_t o)
{ (void)r;assert(o==0x2e45||o==0x2e44||o==0x2e49);
  return o==0x2e44?t->checked:(o==0x2e49?t->rebuild:t->flag); }
static const char *q_label(void *r) { (void)r;return t->label; }
static const char *q_blabel(void *r) { (void)r;return t->blabel; }
static const char *q_vtext(void *r) { (void)r;return t->vtext; }
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
static void q_cbtn(void *b,uint32_t icon,const char *l,const uint32_t r[4],void *par)
{ (void)r;(void)par;emit(R_CBTN,(uint32_t)(uintptr_t)b-0x2000,icon,0,l); }
static void q_align(void *c,uint32_t a) { emit(R_ALIGN,(uint32_t)(uintptr_t)c-0x2000,a,0,NULL); }
static void q_checked(void *c,uint32_t f) { emit(R_CHECK,(uint32_t)(uintptr_t)c-0x2000,f,0,NULL); }
static void q_ccb(void *c,uint32_t z,uint32_t a,uint32_t b,const char *l,const uint32_t r[4],void *par)
{ (void)par;(void)r;emit(R_CCB,(uint32_t)(uintptr_t)c-0x2000,z,a?(b<<16)|a:0,l); }
static void q_selid(void *c,uint32_t id) { emit(R_SELID,(uint32_t)(uintptr_t)c-0x2000,id,0,NULL); }
static void q_csel(void *c,uint32_t z,uint32_t a,uint32_t b,const char *l,const uint32_t r[4],void *par)
{ (void)par;(void)r;emit(R_CSEL,(uint32_t)(uintptr_t)c-0x2000,z,a?(b<<16)|a:0,l); }
static void q_selimg(void *r,uint32_t id) { emit(R_SELIMG,(uint32_t)(uintptr_t)r-0x2000,id,0,NULL); }
static void q_emode(void *c,uint32_t m) { emit(R_EMODE,(uint32_t)(uintptr_t)c-0x2000,m,0,NULL); }
static void q_maxch(void *c,uint32_t n) { emit(R_MAXCH,(uint32_t)(uintptr_t)c-0x2000,n,0,NULL); }
static void q_cauto(void *c,int on) { emit(R_CAUTO,(uint32_t)(uintptr_t)c-0x2000,(uint32_t)on,0,NULL); }
static void q_num(void *c,uint32_t v) { emit(R_NUM,(uint32_t)(uintptr_t)c-0x2000,v,0,NULL); }
static void q_store(void *r,uint32_t o,uint32_t v)
{ (void)r;assert(o>=0x2e4c && o<=0x2ed8 && !(o&3));t->words[(o-0x2e4c)/4]=v;emit(R_STORE,o,v,0,NULL); }
static void q_destroy(void *c) { emit(R_DESTROY,(uint32_t)(uintptr_t)c-0x2000,0,0,NULL); }
static void q_reset(void *c) { emit(R_RESETXML,(uint32_t)(uintptr_t)c-0x2000,0,0,NULL); }
static uint32_t q_gid(uint32_t op,uint32_t st) { return (op<<8)|st; }
static void q_glyph(void *c,uint32_t id,uint32_t st) { emit(R_GLYPH,(uint32_t)(uintptr_t)c-0x2000,id,st,NULL); }
static void q_hl(void *c,uint32_t id) { emit(R_HL,(uint32_t)(uintptr_t)c-0x2000,id,0,NULL); }
static void q_subhl(void *c,uint32_t id) { emit(R_SUBHL,(uint32_t)(uintptr_t)c-0x2000,id,0,NULL); }
static void q_shadow(void *c,uint32_t id) { emit(R_SHADOW,(uint32_t)(uintptr_t)c-0x2000,id,0,NULL); }
static void q_vis(void *c,int on) { emit(R_VIS,(uint32_t)(uintptr_t)c-0x2000,(uint32_t)on,0,NULL); }
static void q_scale(void *c,uint32_t ty) { emit(R_SCALE,(uint32_t)(uintptr_t)c-0x2000,ty,0,NULL); }
static void q_cimg(void *c,uint32_t id,const uint32_t r[4],void *par)
{ (void)par;(void)r;emit(R_CIMG,(uint32_t)(uintptr_t)c-0x2000,id,0,NULL); }
static void q_drawtop(void *c,int on) { emit(R_DRAWTOP,(uint32_t)(uintptr_t)c-0x2000,(uint32_t)on,0,NULL); }
static void q_ctbox(void *c,uint32_t z,const char *l,const uint32_t r[4],uint32_t ex,void *par)
{ (void)par;(void)r;emit(R_CTBOX,(uint32_t)(uintptr_t)c-0x2000,z,ex,l); }
static void q_cslider(void *c,uint32_t z,int32_t lo,int32_t hi,const uint32_t r[4],void *par)
{ (void)par;(void)r;emit(R_CSLIDER,(uint32_t)(uintptr_t)c-0x2000,(uint32_t)lo,(uint32_t)hi,NULL);(void)z; }
static void q_sval(void *c,int32_t v) { emit(R_SVAL,(uint32_t)(uintptr_t)c-0x2000,(uint32_t)v,0,NULL); }
static void q_sfocus(void *c,int on) { emit(R_SFOCUS,(uint32_t)(uintptr_t)c-0x2000,(uint32_t)on,0,NULL); }
static void q_cte(void *c,uint32_t z,uint32_t a,uint32_t b,const char *l,const uint32_t r[4],void *par)
{ (void)par;(void)r;emit(R_CTE,(uint32_t)(uintptr_t)c-0x2000,z,a?(b<<16)|a:0,l); }
static struct oa_setting_row_render_ops ops(void)
{
    return (struct oa_setting_row_render_ops){t,q_base,q_cont,q_rect,q_words,q_type,q_word,
        q_flag,q_label,q_blabel,q_vtext,q_child,q_is,q_name,q_focus,q_elipes,q_font,q_point,
        q_srect,q_text,q_ctext,q_cbtn,q_align,q_checked,q_ccb,q_selid,q_csel,q_selimg,q_emode,q_maxch,q_cauto,q_num,q_cte,
        q_store,q_destroy,q_reset,q_gid,q_glyph,q_hl,q_subhl,q_shadow,q_vis,q_scale,q_cimg,q_drawtop,
        q_ctbox,q_cslider,q_sval,q_sfocus};
}
static void reset(struct rrow *r)
{
    memset(r,0,sizeof *r);t=r;r->type=UINT32_MAX;r->label="";r->blabel="";r->vtext="";
    for(int i=0;i<11;++i) { r->rects[i]=(void *)(uintptr_t)(0x3000+i*0x20); }
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
    /* Types other than the implemented set stay open, after the shared tail. */
    reset(&r);o=ops();r.type=10;
    assert(oa_setting_row_render(&r,&o)==ENOTSUP && r.count==13);
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
    reset(&r);o=ops();r.type=0;r.rects[0]=NULL;r.words[(0x2e90-0x2e4c)/4]=0xaa;
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
    r.words[(0x2e90-0x2e4c)/4]=0xbb;
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
    /* Type 0x1a: shared tail uses 0x2e8c for all four name slots (no setFont),
     * then the case body drives the 0x684 button with 0x2ed0/0x2ecc/0x2ed8/
     * 0x2ed4 and creates it from the "button" rectangle. */
    reset(&r);o=ops();r.type=0x1a;r.blabel="PB";
    for(int i=0;i<4;++i)r.rwords[3][i]=(uint32_t)(100+i);
    for(int k=0;k<20;++k)wset(&r,(uint32_t)(0x2e8c+4*k),(uint32_t)(0x200+k));
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==19);
    /* tail: is_created(0x104), four 0x2e8c names, focus/elipses, create */
    assert(r.trace[4].kind==R_IS && r.trace[4].off==0x104);
    assert(r.trace[5].kind==R_NAME && r.trace[5].b==0x200);
    assert(r.trace[8].kind==R_NAME && r.trace[8].b==0x200);
    assert(r.trace[11].kind==R_CTEXT);
    /* case body: button lookup, is_created(0x684), four 0x2edx names, create */
    assert(r.trace[12].kind==R_GET && !strcmp(r.trace[12].text,"button"));
    assert(r.trace[13].kind==R_IS && r.trace[13].off==0x684);
    assert(r.trace[14].kind==R_NAME && r.trace[14].b==0x211);
    assert(r.trace[17].kind==R_NAME && r.trace[17].b==0x212);
    assert(r.trace[18].kind==R_CBTN && !strcmp(r.trace[18].text,"PB"));
    /* Type 0x1a created button: tail then setWindowRect/setText (no names). */
    reset(&r);o=ops();r.type=0x1a;r.created=1;r.blabel="PB";
    for(int i=0;i<4;++i)r.rwords[3][i]=(uint32_t)(100+i);
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==11);
    assert(r.trace[8].kind==R_IS && r.trace[8].off==0x684);
    assert(r.trace[10].kind==R_TEXT && !strcmp(r.trace[10].text,"PB"));
    /* Type 0x16: shared tail (0x26d names) then the checkbox case body with
     * aligned empty-label create and the 0x2e44 checked byte. */
    reset(&r);o=ops();r.type=0x16;
    for(int i=0;i<4;++i)r.rwords[4][i]=(uint32_t)(200+i);
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==22);
    assert(r.trace[13].kind==R_GET && !strcmp(r.trace[13].text,"checkbox"));
    assert(r.trace[14].kind==R_IS && r.trace[14].off==0x27a8);
    assert(r.trace[15].kind==R_NAME && r.trace[15].a==1 && r.trace[15].b==9);
    assert(r.trace[18].kind==R_NAME && r.trace[18].a==2 && r.trace[18].b==12);
    assert(r.trace[19].kind==R_ALIGN && r.trace[19].a==5);
    assert(r.trace[20].kind==R_CCB && r.trace[20].a==0 && !strcmp(r.trace[20].text,""));
    assert(r.trace[21].kind==R_CHECK && r.trace[21].a==0);
    /* Type 0x16 with the checked byte set reports 1. */
    reset(&r);o=ops();r.type=0x16;r.checked=1;
    assert(!oa_setting_row_render(&r,&o));
    assert(r.trace[21].kind==R_CHECK && r.trace[21].a==1);
    /* Type 2: shared tail then the selection-box case body (0x26b names +
     * font, slot order 1/3/0/2, empty-label create). */
    reset(&r);o=ops();r.type=2;
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==21);
    assert(r.trace[13].kind==R_GET && !strcmp(r.trace[13].text,"selectionbox"));
    assert(r.trace[14].kind==R_IS && r.trace[14].off==0x1014);
    assert(r.trace[15].kind==R_NAME && r.trace[15].a==1 && r.trace[15].b==0x26b);
    assert(r.trace[16].kind==R_NAME && r.trace[16].a==3 && r.trace[16].b==0x26b);
    assert(r.trace[17].kind==R_NAME && r.trace[17].a==0 && r.trace[17].b==0x26b);
    assert(r.trace[19].kind==R_FONT && r.trace[19].a==0x1e);
    assert(r.trace[20].kind==R_CSEL && r.trace[20].a==0 && !strcmp(r.trace[20].text,""));
    /* Type 2 created: rect math + setSelectedItemWithId([0x2de8]). */
    reset(&r);o=ops();r.type=2;r.created=1;r.sel_id=0x55;
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==11);
    assert(r.trace[8].kind==R_IS && r.trace[8].off==0x1014);
    assert(r.trace[9].kind==R_RECT);
    assert(r.trace[10].kind==R_SELID && r.trace[10].a==0x55);
    /* Type 3: row image id first, then 0x2e9c/0x2ea0/0x2ea4/0x2ea8 names. */
    reset(&r);o=ops();r.type=3;r.sel_img=0x77;
    for(int k=0;k<20;++k)wset(&r,(uint32_t)(0x2e8c+4*k),(uint32_t)(0x300+k));
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==21);
    assert(r.trace[13].kind==R_GET && !strcmp(r.trace[13].text,"selectionbox"));
    assert(r.trace[14].kind==R_SELIMG && r.trace[14].a==0x77);
    assert(r.trace[15].kind==R_IS && r.trace[15].off==0x1014);
    assert(r.trace[16].kind==R_NAME && r.trace[16].a==1 && r.trace[16].b==0x305);
    assert(r.trace[17].kind==R_NAME && r.trace[17].a==3 && r.trace[17].b==0x307);
    assert(r.trace[18].kind==R_NAME && r.trace[18].a==0 && r.trace[18].b==0x304);
    assert(r.trace[19].kind==R_NAME && r.trace[19].a==2 && r.trace[19].b==0x306);
    assert(r.trace[20].kind==R_CSEL);
    /* Type 0xd: shared tail then the text-entry case body. Uncreated writes
     * 0x2ebc/0x2ec0/0x2ec4/0x2ec8 names in slots 0/1/2/3 order 1/0/3/2,
     * creates from an empty label, then mode0/max4/cursor-auto. */
    reset(&r);o=ops();r.type=0xd;
    for(int k=0;k<20;++k)wset(&r,(uint32_t)(0x2e8c+4*k),(uint32_t)(0x400+k));
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==23);
    assert(r.trace[13].kind==R_GET && !strcmp(r.trace[13].text,"selectionbox"));
    assert(r.trace[14].kind==R_IS && r.trace[14].off==0x18d8);
    assert(r.trace[15].kind==R_NAME && r.trace[15].a==1 && r.trace[15].b==0x40d);
    assert(r.trace[16].kind==R_NAME && r.trace[16].a==0 && r.trace[16].b==0x40c);
    assert(r.trace[17].kind==R_NAME && r.trace[17].a==3 && r.trace[17].b==0x40f);
    assert(r.trace[18].kind==R_NAME && r.trace[18].a==2 && r.trace[18].b==0x40e);
    assert(r.trace[19].kind==R_CTE && r.trace[19].a==0 && !strcmp(r.trace[19].text,""));
    assert(r.trace[20].kind==R_EMODE && r.trace[20].a==0);
    assert(r.trace[21].kind==R_MAXCH && r.trace[21].a==4);
    assert(r.trace[22].kind==R_CAUTO && r.trace[22].a==1);
    /* Type 0xd created: setNumericContents from row[0x2e00] then the setters. */
    reset(&r);o=ops();r.type=0xd;r.created=1;r.num=0x99;
    assert(!oa_setting_row_render(&r,&o));
    assert(r.trace[8].kind==R_IS && r.trace[8].off==0x18d8);
    assert(r.trace[9].kind==R_RECT);
    assert(r.trace[10].kind==R_NUM && r.trace[10].a==0x99);
    assert(r.trace[11].kind==R_EMODE && r.trace[11].a==0);
    assert(r.trace[13].kind==R_CAUTO && r.trace[13].a==1);
    /* Type 0xe/0xf are identical: mode 0xa, max 5, numeric contents. */
    for(unsigned t=0;t<2;++t) {
        reset(&r);o=ops();r.type=t?0xf:0xe;r.num=0x21;
        for(int k=0;k<20;++k)wset(&r,(uint32_t)(0x2e8c+4*k),(uint32_t)(0x500+k));
        assert(!oa_setting_row_render(&r,&o));
        assert(r.count==23);
        assert(r.trace[15].kind==R_NAME && r.trace[15].a==1 && r.trace[15].b==0x50d);
        assert(r.trace[19].kind==R_CTE && r.trace[19].a==0);
        assert(r.trace[20].kind==R_EMODE && r.trace[20].a==0xa);
        assert(r.trace[21].kind==R_MAXCH && r.trace[21].a==5);
        assert(r.trace[22].kind==R_CAUTO && r.trace[22].a==1);
        reset(&r);o=ops();r.type=t?0xf:0xe;r.created=1;r.num=0x21;
        assert(!oa_setting_row_render(&r,&o));
        assert(r.trace[10].kind==R_NUM && r.trace[10].a==0x21);
        assert(r.trace[11].kind==R_EMODE && r.trace[11].a==0xa);
        assert(r.trace[12].kind==R_MAXCH && r.trace[12].a==5);
    }
    /* Type 0x10: mode 1, max 10, created path uses the value text (0x2e04). */
    reset(&r);o=ops();r.type=0x10;
    for(int k=0;k<20;++k)wset(&r,(uint32_t)(0x2e8c+4*k),(uint32_t)(0x600+k));
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==23);
    assert(r.trace[20].kind==R_EMODE && r.trace[20].a==1);
    assert(r.trace[21].kind==R_MAXCH && r.trace[21].a==10);
    assert(r.trace[22].kind==R_CAUTO && r.trace[22].a==1);
    reset(&r);o=ops();r.type=0x10;r.created=1;r.vtext="VX";
    assert(!oa_setting_row_render(&r,&o));
    assert(r.trace[9].kind==R_RECT);
    assert(r.trace[10].kind==R_TEXT && !strcmp(r.trace[10].text,"VX"));
    assert(r.trace[11].kind==R_EMODE && r.trace[11].a==1);
    assert(r.trace[12].kind==R_MAXCH && r.trace[12].a==10);
    /* Type 5 launcher button: destroy/reinit/4 glyphs/highlight/shadow, then
     * the two images and setDrawOnTop. */
    reset(&r);o=ops();r.type=5;
    wset(&r,0x2e6c,0x701);wset(&r,0x2e70,0x702);wset(&r,0x2e74,0x703);wset(&r,0x2e78,0x704);
    r.op=0x42;wset(&r,0x2e68,0x43);
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==40);
    assert(r.trace[13].kind==R_GET && !strcmp(r.trace[13].text,"button"));
    assert(r.trace[14].kind==R_GET && !strcmp(r.trace[14].text,"image1"));
    assert(r.trace[15].kind==R_GET && !strcmp(r.trace[15].text,"image2"));
    assert(r.trace[16].kind==R_IS && r.trace[16].off==0x684);
    assert(r.trace[17].kind==R_IS && r.trace[17].off==0x684);
    assert(r.trace[18].kind==R_NAME && r.trace[18].a==1 && r.trace[18].b==0x702);
    assert(r.trace[22].kind==R_GLYPH && r.trace[22].a==0x4201 && r.trace[22].b==1);
    assert(r.trace[25].kind==R_GLYPH && r.trace[25].b==3);
    assert(r.trace[26].kind==R_HL && r.trace[26].a==0x43);
    assert(r.trace[27].kind==R_SUBHL && r.trace[27].a==0x237);
    assert(r.trace[28].kind==R_ELIP && r.trace[28].a==1);
    assert(r.trace[29].kind==R_SHADOW && r.trace[29].a==0x94);
    assert(r.trace[30].kind==R_CBTN && r.trace[30].a==0x42 && !strcmp(r.trace[30].text,""));
    assert(r.trace[31].kind==R_STORE && r.trace[31].off==0x2e4c && r.trace[31].a==0x92);
    assert(r.trace[32].kind==R_STORE && r.trace[32].off==0x2e50 && r.trace[32].a==0x93);
    assert(r.trace[33].kind==R_IS && r.trace[33].off==0x2b28);
    assert(r.trace[34].kind==R_SCALE && r.trace[34].a==0);
    assert(r.trace[35].kind==R_CIMG && r.trace[35].a==0x92);
    assert(r.trace[36].kind==R_IS && r.trace[36].off==0x2c0c);
    assert(r.trace[37].kind==R_SCALE && r.trace[37].a==0);
    assert(r.trace[38].kind==R_CIMG && r.trace[38].a==0x93);
    assert(r.trace[39].kind==R_DRAWTOP && r.trace[39].a==1);
    /* Type 6 uses the second name set; created path refreshes only. */
    reset(&r);o=ops();r.type=6;
    wset(&r,0x2e7c,0x801);wset(&r,0x2e80,0x802);wset(&r,0x2e84,0x803);wset(&r,0x2e88,0x804);
    assert(!oa_setting_row_render(&r,&o));
    assert(r.trace[18].kind==R_NAME && r.trace[18].a==1 && r.trace[18].b==0x802);
    reset(&r);o=ops();r.type=6;r.created=1;
    assert(!oa_setting_row_render(&r,&o));
    assert(r.trace[5].kind==R_RECT);
    assert(r.trace[6].kind==R_TEXT);
    /* Type 7 banner button: no destroy/images, type-5 names, 4 glyphs,
     * elipses 1, shadow 0x94, icon create, then setDrawOnTop. */
    reset(&r);o=ops();r.type=7;
    wset(&r,0x2e6c,0x901);wset(&r,0x2e70,0x902);wset(&r,0x2e74,0x903);wset(&r,0x2e78,0x904);
    r.op=0x11;
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==27);
    assert(r.trace[13].kind==R_GET && !strcmp(r.trace[13].text,"banner"));
    assert(r.trace[14].kind==R_IS && r.trace[14].off==0x684);
    assert(r.trace[15].kind==R_NAME && r.trace[15].a==1 && r.trace[15].b==0x902);
    assert(r.trace[16].kind==R_NAME && r.trace[16].a==3 && r.trace[16].b==0x904);
    assert(r.trace[19].kind==R_GLYPH && r.trace[19].a==0x1101 && r.trace[19].b==1);
    assert(r.trace[23].kind==R_ELIP && r.trace[23].a==1);
    assert(r.trace[24].kind==R_SHADOW && r.trace[24].a==0x94);
    assert(r.trace[25].kind==R_CBTN && r.trace[25].a==0x11);
    assert(r.trace[26].kind==R_DRAWTOP && r.trace[26].a==1);
    /* Type 7 created refreshes with setWindowRect only (no setText). */
    reset(&r);o=ops();r.type=7;r.created=1;
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==11);
    assert(r.trace[8].kind==R_IS && r.trace[8].off==0x684);
    assert(r.trace[9].kind==R_RECT);
    assert(r.trace[10].kind==R_DRAWTOP);
    /* Type 8 plain button: names 1/0/3/2 from 0x2eb0/0x2eac/0x2eb8/0x2eb4,
     * icon-0 create, created refresh with setText. */
    reset(&r);o=ops();r.type=8;
    wset(&r,0x2eac,0xa01);wset(&r,0x2eb0,0xa02);wset(&r,0x2eb4,0xa03);wset(&r,0x2eb8,0xa04);
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==20);
    assert(r.trace[13].kind==R_GET && !strcmp(r.trace[13].text,"button"));
    assert(r.trace[14].kind==R_IS && r.trace[14].off==0x684);
    assert(r.trace[15].kind==R_NAME && r.trace[15].a==1 && r.trace[15].b==0xa02);
    assert(r.trace[16].kind==R_NAME && r.trace[16].a==0 && r.trace[16].b==0xa01);
    assert(r.trace[17].kind==R_NAME && r.trace[17].a==3 && r.trace[17].b==0xa04);
    assert(r.trace[18].kind==R_NAME && r.trace[18].a==2 && r.trace[18].b==0xa03);
    assert(r.trace[19].kind==R_CBTN && r.trace[19].a==0);
    reset(&r);o=ops();r.type=8;r.created=1;
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==11);
    assert(r.trace[8].kind==R_IS && r.trace[8].off==0x684);
    assert(r.trace[9].kind==R_RECT);
    assert(r.trace[10].kind==R_TEXT);
    /* Type 9 slider row: text box (0x1ca8) + slider (0x13c0). */
    reset(&r);o=ops();r.type=9;r.slo=10;r.shi=90;r.sval=42;
    assert(!oa_setting_row_render(&r,&o));
    assert(r.count==33);
    assert(r.trace[13].kind==R_GET && !strcmp(r.trace[13].text,"slidebar"));
    assert(r.trace[14].kind==R_GET && !strcmp(r.trace[14].text,"text2"));
    assert(r.trace[15].kind==R_IS && r.trace[15].off==0x1ca8);
    assert(r.trace[16].kind==R_NAME && r.trace[16].a==1 && r.trace[16].b==0x26d);
    assert(r.trace[20].kind==R_FONT && r.trace[20].a==0x1e);
    assert(r.trace[23].kind==R_CTBOX && r.trace[23].a==0 && r.trace[23].b==1);
    assert(r.trace[24].kind==R_TEXT && !strcmp(r.trace[24].text,"42"));
    assert(r.trace[25].kind==R_IS && r.trace[25].off==0x13c0);
    assert(r.trace[26].kind==R_NAME && r.trace[26].a==1 && r.trace[26].b==0x26c);
    assert(r.trace[30].kind==R_SFOCUS && r.trace[30].a==0);
    assert(r.trace[31].kind==R_CSLIDER && r.trace[31].a==10 && r.trace[31].b==90);
    assert(r.trace[32].kind==R_SVAL && r.trace[32].a==42);
    /* Invalid ops/args change nothing. */
    o=ops();o.set_name=NULL;
    assert(oa_setting_row_render(&r,&o)==EINVAL && r.count==33);
    assert(oa_setting_row_render(NULL,&o)==EINVAL);
    assert(oa_setting_row_render(&r,NULL)==EINVAL);
    puts("ok settings row render: 0/1/4 paths, shared tail, gates and open types");
    return 0;
}
