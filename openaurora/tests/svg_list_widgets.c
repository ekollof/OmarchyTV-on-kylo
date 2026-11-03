#include "openaurora/svg_list_widgets.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
struct nfixture { void *row,*entry;uint32_t value;unsigned count;struct {unsigned id;uint32_t a;} trace[4]; };
static struct nfixture *current_n;
static void nemit(unsigned id,uint32_t a)
{ assert(current_n->count<4);current_n->trace[current_n->count].id=id;current_n->trace[current_n->count].a=a;++current_n->count; }
enum {N_ENTRY,N_GET};
static void *nq_entry(void *row) { nemit(N_ENTRY,(uint32_t)(uintptr_t)row);return current_n->entry; }
static uint32_t nq_get(void *entry) { nemit(N_GET,(uint32_t)(uintptr_t)entry);return current_n->value; }
struct sfixture {
    int created;uint32_t selected;void *table;void *entries[4];uint32_t ids[4];
    unsigned count;struct {unsigned id;uint32_t a;} trace[8];
};
static struct sfixture *current_s;
static void semit(unsigned id,uint32_t a)
{ assert(current_s->count<8);current_s->trace[current_s->count].id=id;current_s->trace[current_s->count].a=a;++current_s->count; }
enum {S_CREATED,S_SEL,S_TABLE,S_ENTRY,S_ID};
static int sq_created(void *list) { semit(S_CREATED,(uint32_t)(uintptr_t)list);return current_s->created; }
static uint32_t sq_sel(void *list) { semit(S_SEL,(uint32_t)(uintptr_t)list);return current_s->selected; }
static void *sq_table(void *list) { semit(S_TABLE,(uint32_t)(uintptr_t)list);return current_s->table; }
static void *sq_entry(void *table,uint32_t i)
{ semit(S_ENTRY,i);(void)table;assert(i<4);return current_s->entries[i]; }
static uint32_t sq_id(void *entry)
{
    for(unsigned i=0;i<4;++i)if(current_s->entries[i]==entry) { semit(S_ID,i);return current_s->ids[i]; }
    assert(0);return 0;
}
struct lfixture {
    uint32_t fa,fb,type;void *row;int bar_on,box_on;int32_t stored;
    unsigned count;struct {unsigned id;uint32_t a,b;} trace[16];
};
static struct lfixture *current_l;
enum {L_PROGRESS,L_BAR,L_CREATED,L_SLIDER,L_STORE,L_BOX,L_FORMAT,L_NEW,L_APPEND,L_SET,L_FREE};
static void lemit(unsigned id,uint32_t a,uint32_t b)
{ assert(current_l->count<16);current_l->trace[current_l->count].id=id;current_l->trace[current_l->count].a=a;current_l->trace[current_l->count].b=b;++current_l->count; }
static uint32_t lq_fa(void *row) { (void)row;return current_l->fa; }
static uint32_t lq_fb(void *row) { (void)row;return current_l->fb; }
static uint32_t lq_type(void *row) { (void)row;return current_l->type; }
static void le_progress(void *row,int v) { lemit(L_PROGRESS,(uint32_t)(uintptr_t)row,(uint32_t)v); }
static void *le_bar(void *row,uint32_t off) { lemit(L_BAR,(uint32_t)(uintptr_t)row,off);return (unsigned char *)row+off; }
static int le_created(void *h)
{
    lemit(L_CREATED,(uint32_t)(uintptr_t)h,0);
    uintptr_t base=(uintptr_t)current_l->row;
    if((uintptr_t)h==base+0x1498u || (uintptr_t)h==base+0x13c0u)return current_l->bar_on;
    return current_l->box_on;
}
static void le_slider(void *h,int v) { lemit(L_SLIDER,(uint32_t)(uintptr_t)h,(uint32_t)v); }
static void le_store(void *row,int v) { lemit(L_STORE,(uint32_t)(uintptr_t)row,(uint32_t)v);current_l->stored=v; }
static void *le_box(void *row) { lemit(L_BOX,(uint32_t)(uintptr_t)row,0);return (unsigned char *)row+0x1ca8; }
static const char *le_format(void *row) { (void)row;lemit(L_FORMAT,0,0);return ""; }
static void le_new(void *buf,const char *fmt) { assert((uintptr_t)buf%_Alignof(max_align_t)==0);lemit(L_NEW,(uint32_t)(uintptr_t)buf,(uint32_t)(uintptr_t)fmt); }
static void le_append(void *buf,int v) { lemit(L_APPEND,(uint32_t)(uintptr_t)buf,(uint32_t)v); }
static void le_set(void *box,const void *text) { lemit(L_SET,(uint32_t)(uintptr_t)box,(uint32_t)(uintptr_t)text); }
static void le_free(void *buf) { lemit(L_FREE,(uint32_t)(uintptr_t)buf,0); }
static struct oa_svg_row_slide_ops lops_for(struct lfixture *f)
{ return (struct oa_svg_row_slide_ops){f,lq_fa,lq_fb,lq_type,le_progress,le_bar,le_created,le_slider,le_store,le_box,le_format,le_new,le_append,le_set,le_free}; }
struct check_fixture { void *row,*box;int created;bool cached;unsigned stage; };
static void *cq_box(void *p,void *row)
{ struct check_fixture *f=p;assert(row==f->row && f->stage++==0);return f->box; }
static int cq_created(void *p,void *box)
{ struct check_fixture *f=p;assert(box==f->box && f->stage++==1);return f->created; }
static void cq_set(void *p,void *box,bool v)
{ struct check_fixture *f=p;assert(box==f->box && f->stage++==2);assert(v!=f->cached); }
static void cq_store(void *p,void *row,bool v)
{ struct check_fixture *f=p;assert(row==f->row && f->stage++==3);f->cached=v; }
int main(void)
{
    int error=42;struct nfixture nf={0};
    /* EINVAL paths run no callbacks. */
    assert(!oa_setting_row_numeric(NULL,&error) && error==EINVAL);
    struct oa_svg_row_text_ops nbad={0};
    current_n=&nf;
    assert(!oa_setting_row_numeric(&nbad,&error) && error==EINVAL && !nf.count);
    assert(oa_setting_list_selected_id(NULL,&error)==UINT32_C(0xffffffff) && error==EINVAL);
    struct oa_svg_list_select_ops sbad={0};
    assert(oa_setting_list_selected_id(&sbad,&error)==UINT32_C(0xffffffff) && error==EINVAL);
    /* Numeric forwards the embedded entry handle unchanged. */
    static unsigned char entryobj[8];nf.entry=entryobj;nf.value=0x1234abcd;
    static unsigned char rowobj[8];nf.row=rowobj;
    struct oa_svg_row_text_ops nops={&nf,nq_entry,nq_get};
    assert(oa_setting_row_numeric(&nops,&error)==0x1234abcd && !error);
    assert(nf.count==2 && nf.trace[0].id==N_ENTRY);
    assert(nf.trace[1].id==N_GET && nf.trace[1].a==(uint32_t)(uintptr_t)entryobj);
    /* Not created short-circuits before any other query. */
    static unsigned char listobj[8];struct sfixture sf={0};
    for(unsigned i=0;i<4;++i)sf.entries[i]=(void *)(uintptr_t)(0x3000u+i);
    struct oa_svg_list_select_ops sops={&sf,sq_created,sq_sel,sq_table,sq_entry,sq_id};
    current_s=&sf;
    sf.created=0;sf.selected=2;
    assert(oa_setting_list_selected_id(&sops,&error)==UINT32_C(0xffffffff) && !error && sf.count==1);
    /* Selected -1 (wrapping) short-circuits before the table walk. */
    sf.created=1;sf.selected=UINT32_MAX;sf.count=0;
    assert(oa_setting_list_selected_id(&sops,&error)==UINT32_C(0xffffffff) && !error && sf.count==2);
    /* Normal path walks table[index] and reads its first word. */
    static unsigned char tableobj[16];sf.table=tableobj;
    sf.selected=2;sf.ids[2]=0x77;sf.count=0;
    assert(oa_setting_list_selected_id(&sops,&error)==0x77 && !error && sf.count==5);
    assert(sf.trace[2].id==S_TABLE && sf.trace[3].id==S_ENTRY && sf.trace[3].a==2);
    assert(sf.trace[4].id==S_ID && sf.trace[4].a==2);
    (void)listobj;
    /* --- setSlideValue paths: progress vs slider+text --- */
    {
        static unsigned char slrow[0x3000];struct lfixture lf={0};
        struct oa_svg_row_slide_ops lops=lops_for(&lf);current_l=&lf;lf.row=slrow;
        /* EINVAL paths run no callbacks. */
        struct oa_svg_row_slide_ops lbad={0};
        assert(oa_setting_row_set_slide(NULL,slrow,5)==EINVAL);
        assert(oa_setting_row_set_slide(&lbad,slrow,5)==EINVAL);
        assert(oa_setting_row_set_slide(&lops,NULL,5)==EINVAL);
        assert(!lf.count);
        /* flagA routes to progress only. */
        lf.fa=1;lf.fb=1;lf.type=23;
        assert(!oa_setting_row_set_slide(&lops,slrow,-7) && lf.count==1);
        assert(lf.trace[0].id==L_PROGRESS && lf.trace[0].b==(uint32_t)(uint32_t)-7);
        /* flagB with type23 routes to progress only. */
        lf.fa=0;lf.count=0;
        assert(!oa_setting_row_set_slide(&lops,slrow,9) && lf.count==1 && lf.trace[0].id==L_PROGRESS);
        /* Slider path: bar, create gate, slider, store, box, text dance. */
        lf.fb=1;lf.type=7;lf.bar_on=1;lf.box_on=1;lf.count=0;
        assert(!oa_setting_row_set_slide(&lops,slrow,9) && lf.stored==9);
        assert(lf.count==11);
        assert(lf.trace[0].id==L_BAR && lf.trace[0].b==0x1498);
        assert(lf.trace[1].id==L_CREATED && lf.trace[2].id==L_SLIDER && lf.trace[2].b==9);
        assert(lf.trace[3].id==L_STORE && lf.trace[3].b==9);
        assert(lf.trace[4].id==L_BOX && lf.trace[5].id==L_CREATED);
        assert(lf.trace[6].id==L_FORMAT && lf.trace[7].id==L_NEW && lf.trace[8].id==L_APPEND && lf.trace[8].b==9);
        assert(lf.trace[9].id==L_SET && lf.trace[10].id==L_FREE);
        /* Uncreated bar skips the slider; uncreated box skips the text. */
        lf.bar_on=0;lf.count=0;
        assert(!oa_setting_row_set_slide(&lops,slrow,3) && lf.stored==3 && lf.count==10);
        lf.bar_on=1;lf.box_on=0;lf.count=0;
        assert(!oa_setting_row_set_slide(&lops,slrow,4) && lf.stored==4 && lf.count==6);
        assert(lf.trace[4].id==L_BOX && lf.trace[5].id==L_CREATED);
        /* Clear flagB with type9 selects the 0x13c0 bar; other types keep 0x1498. */
        lf.box_on=1;lf.fb=0;lf.type=9;lf.count=0;
        assert(!oa_setting_row_set_slide(&lops,slrow,6) && lf.trace[0].id==L_BAR && lf.trace[0].b==0x13c0);
        lf.type=8;lf.count=0;
        assert(!oa_setting_row_set_slide(&lops,slrow,6) && lf.trace[0].id==L_BAR && lf.trace[0].b==0x1498);
    }
    struct check_fixture cf={.row=rowobj,.box=entryobj,.cached=true};
    struct oa_svg_row_check_ops cops={&cf,cq_box,cq_created,cq_set,cq_store};
    assert(!oa_setting_row_set_checked(&cops,rowobj,false) && cf.stage==2 && cf.cached);
    cf.stage=0;cf.created=-1;
    assert(!oa_setting_row_set_checked(&cops,rowobj,false) && cf.stage==4 && !cf.cached);
    cf.stage=0;
    assert(!oa_setting_row_set_checked(&cops,rowobj,true) && cf.stage==4 && cf.cached);
    cf.stage=0;
    assert(oa_setting_row_set_checked(NULL,rowobj,false)==EINVAL);
    assert(oa_setting_row_set_checked(&cops,NULL,false)==EINVAL);
    cops.store_checked=NULL;
    assert(oa_setting_row_set_checked(&cops,rowobj,false)==EINVAL && !cf.stage);
    puts("ok SVG list widgets: numeric forward, created/none gates, table-walk id, slide and checked paths");
    return 0;
}
