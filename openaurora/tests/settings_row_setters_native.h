/* Offline ARM fragments: SettingItemsListBoxRow label/progress/range setters.
 * UString temporaries are controlled stubs; the shipped __aeabi_idiv body
 * runs for nonzero divisors. Only the three guard-pointer literals are
 * redirected. Harness UString objects are pointer slots: word 0 holds the
 * character pointer, matching the owned plain-string convention used for the
 * animation checks. */
#include "openaurora/settings_row_setters.h"
enum {SE_CREATED,SE_TEXT,SE_PERCENT,SE_RANGE,SE_CTOR,SE_APPEND,SE_DTOR,SE_ASSIGN};
struct srs_event { uint32_t kind,off,a,b;char text[16]; };
struct srs_result { struct srs_event events[32];unsigned count; };
struct srs_fixture {
    uint32_t type;int32_t lo,hi;int bar_created,box_created;
    struct srs_result *trace;unsigned char *row;
};
static struct srs_fixture *srs_native;
static _Alignas(uint32_t) unsigned char srs_row[0x3000],srs_refrow[0x3000];
static _Alignas(uint32_t) unsigned char srs_textbuf[16];
static void srs_emit(struct srs_result *r,uint32_t kind,uint32_t off,uint32_t a,uint32_t b,const char *text)
{
    struct srs_event *e;
    assert(r->count<32);e=&r->events[r->count++];
    e->kind=kind;e->off=off;e->a=a;e->b=b;
    if(text)snprintf(e->text,sizeof e->text,"%s",text);else e->text[0]=0;
}
static uint32_t srs_off(void *row,void *child)
{ assert((unsigned char *)child>=(unsigned char *)row && (unsigned char *)child<(unsigned char *)row+0x3000);
  return (uint32_t)((unsigned char *)child-(unsigned char *)row); }
static unsigned srs_created(void *child)
{ srs_emit(srs_native->trace,SE_CREATED,srs_off(srs_row,child),0,0,NULL);
  return (unsigned)(child==(void *)(srs_row+0x24e8)?srs_native->box_created:srs_native->bar_created); }
static void srs_settext(void *child,uint32_t text)
{ srs_emit(srs_native->trace,SE_TEXT,srs_off(srs_row,child),0,0,*(const char *const *)text); }
static void srs_ctor(void *buf,uint32_t fmt)
{ srs_emit(srs_native->trace,SE_CTOR,0,fmt,0,NULL);
  assert(*(const char *)fmt==0);*(const char **)buf=(const char *)srs_textbuf;srs_textbuf[0]=0; }
static void srs_assign(void *dst,uint32_t src)
{ const char *s=*(const char *const *)src;size_t n=strlen(s)+1;

  srs_emit(srs_native->trace,SE_ASSIGN,0,(uint32_t)n,0,NULL);
  assert(srs_off(srs_row,dst)==0x2dd8 && n<=32);memcpy(dst,s,n); }
static void srs_append(void *buf,uint32_t value)
{ srs_emit(srs_native->trace,SE_APPEND,0,value,0,NULL);
  snprintf((char *)srs_textbuf,sizeof srs_textbuf,"%d",(int)value);(void)buf; }
static unsigned srs_dtor(void *buf) { (void)buf;srs_emit(srs_native->trace,SE_DTOR,0,0,0,NULL);return 0; }
static void srs_percent(void *bar,uint32_t pct)
{ srs_emit(srs_native->trace,SE_PERCENT,srs_off(srs_row,bar),pct,0,NULL); }
static void srs_range(void *bar,uint32_t lo,uint32_t hi,uint32_t ignored)
{ (void)ignored;srs_emit(srs_native->trace,SE_RANGE,srs_off(srs_row,bar),lo,hi,NULL); }
/* Execute the actual divide, including its computed branches. Zero ranges
 * are excluded: their unconditional SIGFPE path is instruction-verified. */
static void shipped_divide(void *mapped,const unsigned char *elf,size_t bytes)
{
    size_t off=file_offset(elf,bytes,0x1285478,660);
    assert(crc32(elf+off,660)==UINT32_C(0xa99eabd1));
    memcpy(stub_destination(mapped,0x1285000,0x1285478,660),elf+off,660);
}
static struct srs_fixture *srs_ref;
static uint32_t srse_type(void *row) { (void)row;return srs_ref->type; }
static int32_t srse_min(void *row) { (void)row;return srs_ref->lo; }
static int32_t srse_max(void *row) { (void)row;return srs_ref->hi; }
static void *srse_child(void *row,uint32_t off) { (void)row;return srs_ref->row+off; }
static int srse_created(void *child)
{ srs_emit(srs_ref->trace,SE_CREATED,srs_off(srs_ref->row,child),0,0,NULL);
  return srs_off(srs_ref->row,child)==0x24e8?srs_ref->box_created:srs_ref->bar_created; }
static void srse_text(void *child,const char *text)
{ srs_emit(srs_ref->trace,SE_TEXT,srs_off(srs_ref->row,child),0,0,text); }
static void srse_percent(void *bar,int32_t pct)
{ srs_emit(srs_ref->trace,SE_PERCENT,srs_off(srs_ref->row,bar),(uint32_t)pct,0,NULL); }
static void srse_range(void *bar,int32_t lo,int32_t hi)
{ srs_emit(srs_ref->trace,SE_RANGE,srs_off(srs_ref->row,bar),(uint32_t)lo,(uint32_t)hi,NULL); }
static void srse_store_label(void *row,const char *text)
{ (void)row;size_t n=strlen(text)+1;assert(n<=32);memcpy(srs_ref->row+0x2dd8,text,n); }
static void srse_store_value(void *row,int32_t v) { (void)row;memcpy(srs_ref->row+0x2dfc,&v,4); }
static void srse_store_bounds(void *row,int32_t lo,int32_t hi)
{ (void)row;memcpy(srs_ref->row+0x2df0,&lo,4);memcpy(srs_ref->row+0x2df4,&hi,4); }
static struct oa_setting_row_setter_ops srs_ops(struct srs_fixture *f)
{
    return (struct oa_setting_row_setter_ops){f,srse_type,srse_min,srse_max,srse_child,srse_created,
        srse_text,srse_percent,srse_range,srse_store_label,srse_store_value,srse_store_bounds};
}
/* Native UString transport calls (ctor/append/dtor/assign) have no C-side
 * counterparts: the C implementation stores plain strings directly. They are
 * filtered here after asserting none leak into the compared stream; the
 * stored bytes are verified by the full row memcmp in each compare body. */
static void srs_compare_filtered(struct srs_result *vendor,struct srs_result *ref)
{
    unsigned rn=0;struct srs_event keep[32];
    for(unsigned i=0;i<ref->count;++i)keep[rn++]=ref->events[i];
    unsigned vn=0;struct srs_event vkeep[32];
    for(unsigned i=0;i<vendor->count;++i) {
        uint32_t k=vendor->events[i].kind;
        if(k==SE_CTOR || k==SE_APPEND || k==SE_DTOR || k==SE_ASSIGN)continue;
        vkeep[vn++]=vendor->events[i];
    }
    if(vn!=rn) {
        fprintf(stderr,"setters count native=%u ours=%u\n",vn,rn);
        for(unsigned i=0;i<vendor->count;++i)fprintf(stderr,"  native[%u]=%u/%x/%x/%x/%s\n",i,
            vendor->events[i].kind,vendor->events[i].off,vendor->events[i].a,
            vendor->events[i].b,vendor->events[i].text);
        for(unsigned i=0;i<rn;++i)fprintf(stderr,"  ours[%u]=%u/%x/%x/%x/%s\n",i,
            keep[i].kind,keep[i].off,keep[i].a,keep[i].b,keep[i].text);
        exit(1);
    }
    for(unsigned i=0;i<rn;++i) {
        struct srs_event *v=&vkeep[i],*o=&keep[i];
        if(v->kind!=o->kind || v->off!=o->off || v->a!=o->a || v->b!=o->b || strcmp(v->text,o->text)) {
            fprintf(stderr,"setters trace %u native=%u/%x/%x/%x/%s ours=%u/%x/%x/%x/%s\n",i,
                v->kind,v->off,v->a,v->b,v->text,o->kind,o->off,o->a,o->b,o->text);exit(1);
        }
    }
}
typedef void (*srs_label_fn)(void *,void *);
typedef void (*srs_progress_fn)(void *,int32_t);
typedef void (*srs_range_fn)(void *,int32_t,int32_t);
static void srs_setup_row(uint32_t type,int32_t lo,int32_t hi)
{
    memset(srs_row,0xa5,sizeof srs_row);memset(srs_refrow,0xa5,sizeof srs_refrow);
    memcpy(srs_row+0x2dec,&type,4);memcpy(srs_refrow+0x2dec,&type,4);
    memcpy(srs_row+0x2df0,&lo,4);memcpy(srs_refrow+0x2df0,&lo,4);
    memcpy(srs_row+0x2df4,&hi,4);memcpy(srs_refrow+0x2df4,&hi,4);
}
static const char srs_format_empty[]="";
static void check_settings_row_setters(const unsigned char *elf,size_t bytes)
{
    unsigned char *row=page(0xd51000,12288),*win=page(0x1152000,4096),*text=page(0x10c7000,4096);
    unsigned char *ustr=page(0xc86000,4096),*usop=page(0xc88000,4096),*bar=page(0x10b3000,4096);
    unsigned char *slider=page(0x10c4000,4096),*idiv=page(0x1285000,4096);
    const struct {uint32_t address,length,crc,literal;} fragments[]={
        {0xd52498,0x60,UINT32_C(0x1d395f7f),0xd524f8},
        {0xd52e40,0xb4,UINT32_C(0x9b0f3b13),0xd52ef4},
        {0xd53330,0x6e,UINT32_C(0x56d1fb77),0xd533a0},
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;
    for(unsigned i=0;i<3;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(stub_destination(row,0xd51000,fragments[i].address,fragments[i].length),elf+off,fragments[i].length);
        memcpy(stub_destination(row,0xd51000,fragments[i].literal,4),&pointer,4);
    }
    /* The progress body formats through the literal at 0xd52ef8, which points
     * at the shipped empty format string. Redirect it to an owned empty
     * string (cf. the slide check) instead of mapping vendor rodata. */
    { uint32_t fmt=(uint32_t)(uintptr_t)srs_format_empty;
      memcpy(stub_destination(row,0xd51000,0xd52ef8,4),&fmt,4); }
    thumb_stub(win,0x1152000,0x1152a40,srs_created);
    pair_stub(text,0x10c7000,0x10c7828,srs_settext);
    pair_stub(ustr,0xc86000,0xc86744,srs_ctor);
    pair_stub(ustr,0xc86000,0xc86b00,srs_assign);
    thumb_stub(ustr,0xc86000,0xc86554,srs_dtor);
    pair_stub(usop,0xc88000,0xc8865c,srs_append);
    pair_stub(bar,0x10b3000,0x10b3310,srs_percent);
    quad_stub(slider,0x10c4000,0x10c4974,srs_range);
    shipped_divide(idiv,elf,bytes);
    executable(row,12288);executable(win,4096);executable(text,4096);executable(ustr,4096);
    executable(usop,4096);executable(bar,4096);executable(slider,4096);executable(idiv,4096);
    unsigned compared=0;
    srs_label_fn label=0;srs_progress_fn progress=0;srs_range_fn range=0;
    { uint32_t entry=0xd52499;memcpy(&label,&entry,4); }
    { uint32_t entry=0xd52e41;memcpy(&progress,&entry,4); }
    { uint32_t entry=0xd53331;memcpy(&range,&entry,4); }
    {
        const uint32_t types[]={0,1,2,9,0x17,23,27,UINT32_MAX};
        const char *labels[]={"","Volume","Modus","123456789012345"};
        for(unsigned a=0;a<8;++a)for(unsigned c=0;c<2;++c)for(unsigned l=0;l<4;++l) {
            struct srs_result native={0},ref={0};
            struct srs_fixture n={types[a],0,0,(int)c,0,&native,srs_row},r=n;
            unsigned char wrapper[8];const char *sp=labels[l];memset(wrapper,0,sizeof wrapper);memcpy(wrapper,&sp,sizeof sp);
            r.trace=&ref;r.row=srs_refrow;
            struct oa_setting_row_setter_ops ops=srs_ops(&r);
            srs_setup_row(types[a],0,0);
            srs_native=&n;{ srs_label_fn volatile call=label;call(srs_row,wrapper); }
            srs_ref=&r;assert(!oa_setting_row_set_label(srs_refrow,&ops,labels[l]));
            srs_compare_filtered(&native,&ref);
            if(memcmp(srs_row,srs_refrow,sizeof srs_row)) {
                fprintf(stderr,"label row mismatch type=%x created=%u\n",types[a],c);exit(1);
            }
            ++compared;
        }
    }
    {
        const uint32_t types[]={0,9,0x17,23};
        const struct {int32_t v,lo,hi;} cases[]={
            {50,0,100},{0,0,100},{100,0,100},{150,0,100},{-50,0,100},{30,-100,100},
            {0,100,0},{7,0,0},{0,5,5},{-7,5,5},{INT32_MAX,0,100},{INT32_MIN,0,100},
            {0x20000001,1,0},{INT32_MAX,INT32_MIN,INT32_MAX},
        };
        for(unsigned a=0;a<4;++a)for(unsigned b=0;b<2;++b)for(unsigned x=0;x<2;++x)
        for(unsigned k=0;k<sizeof cases/sizeof *cases;++k) {
            if(cases[k].lo==cases[k].hi)continue;
            struct srs_result native={0},ref={0};
            struct srs_fixture n={types[a],cases[k].lo,cases[k].hi,(int)b,(int)x,&native,srs_row},r=n;
            r.trace=&ref;r.row=srs_refrow;
            struct oa_setting_row_setter_ops ops=srs_ops(&r);
            srs_setup_row(types[a],cases[k].lo,cases[k].hi);
            srs_native=&n;{ srs_progress_fn volatile call=progress;call(srs_row,cases[k].v); }
            srs_ref=&r;assert(!oa_setting_row_set_progress(srs_refrow,&ops,cases[k].v));
            srs_compare_filtered(&native,&ref);
            if(memcmp(srs_row,srs_refrow,sizeof srs_row)) {
                fprintf(stderr,"progress row mismatch type=%x v=%d lo=%d hi=%d\n",
                    types[a],cases[k].v,cases[k].lo,cases[k].hi);exit(1);
            }
            ++compared;
        }
    }
    {
        const uint32_t types[]={0,9,23};
        const struct {int32_t lo,hi;} cases[]={
            {0,100},{100,0},{5,5},{0,0},{-100,-50},{INT32_MIN,INT32_MAX},
        };
        for(unsigned a=0;a<3;++a)for(unsigned c=0;c<2;++c)
        for(unsigned k=0;k<sizeof cases/sizeof *cases;++k) {
            struct srs_result native={0},ref={0};
            struct srs_fixture n={types[a],0,0,(int)c,0,&native,srs_row},r=n;
            r.trace=&ref;r.row=srs_refrow;
            struct oa_setting_row_setter_ops ops=srs_ops(&r);
            srs_setup_row(types[a],0,0);
            srs_native=&n;{ srs_range_fn volatile call=range;call(srs_row,cases[k].lo,cases[k].hi); }
            srs_ref=&r;assert(!oa_setting_row_set_bar_range(srs_refrow,&ops,cases[k].lo,cases[k].hi));
            srs_compare_filtered(&native,&ref);
            if(memcmp(srs_row,srs_refrow,sizeof srs_row)) {
                fprintf(stderr,"range row mismatch type=%x lo=%d hi=%d\n",types[a],cases[k].lo,cases[k].hi);exit(1);
            }
            ++compared;
        }
    }
    srs_native=NULL;srs_ref=NULL;
    assert(!munmap(row,12288) && !munmap(win,4096) && !munmap(text,4096) && !munmap(ustr,4096));
    assert(!munmap(usop,4096) && !munmap(bar,4096) && !munmap(slider,4096) && !munmap(idiv,4096));
    printf("ok offline shipped ARM settings row setters: %u native/C comparisons, child select/created gates/stores match\n",compared);
}
