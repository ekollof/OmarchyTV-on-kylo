/* Offline ARM fragment: SettingItemsListBoxRow::renderItems for row types
 * 0, 1 and 4, plus the immediate 0x1b default and the switch default for any
 * other type outside 2..26. UString temporaries and every widget/rectangle
 * call are controlled stubs, not vendor implementations; the EASIRect copy
 * (0xd51d00) and offset (0xd2af88) bodies run verbatim from the dump. Only
 * the five literal slots in range are redirected (guard plus four key
 * pointers aimed at owned strings). Switch cases 2..26 are never executed.
 * Harness row buffers carry the label/word/rect state both sides read. */
#include "openaurora/settings_row_render.h"
typedef void (*tri_fn)(void *,uint32_t,uint32_t);
static uint32_t tri_bits(tri_fn f) { uint32_t v;_Static_assert(sizeof f==sizeof v,"fn width");memcpy(&v,&f,sizeof v);return v; }
static void tri_stub(void *mapped,uint32_t base,uint32_t address,tri_fn function)
{
    unsigned char *p=stub_destination(mapped,base,address,8);const uint16_t insn[]={0x4b00,0x4718};
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target=tri_bits(function);memcpy(p+4,&target,4);
}
typedef void *(*rectget_fn)(void *,void *);
static uint32_t rectget_bits(rectget_fn f) { uint32_t v;_Static_assert(sizeof f==sizeof v,"fn width");memcpy(&v,&f,sizeof v);return v; }
static void rectget_stub(void *mapped,uint32_t base,uint32_t address,rectget_fn function)
{
    unsigned char *p=stub_destination(mapped,base,address,8);const uint16_t insn[]={0x4b00,0x4718};
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target=rectget_bits(function);memcpy(p+4,&target,4);
}
typedef void (*blank_fn)(void *);
static uint32_t blank_bits(blank_fn f) { uint32_t v;_Static_assert(sizeof f==sizeof v,"fn width");memcpy(&v,&f,sizeof v);return v; }
typedef void (*pair2_fn)(void *,void *);
static uint32_t pair2_bits(pair2_fn f) { uint32_t v;_Static_assert(sizeof f==sizeof v,"fn width");memcpy(&v,&f,sizeof v);return v; }
static void pair2_stub(void *mapped,uint32_t base,uint32_t address,pair2_fn function)
{
    unsigned char *p=stub_destination(mapped,base,address,8);const uint16_t insn[]={0x4b00,0x4718};
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target=pair2_bits(function);memcpy(p+4,&target,4);
}
typedef void (*point_fn)(void *,void *);
static uint32_t point_bits(point_fn f) { uint32_t v;_Static_assert(sizeof f==sizeof v,"fn width");memcpy(&v,&f,sizeof v);return v; }
static void blank_stub(void *mapped,uint32_t base,uint32_t address,blank_fn function)
{
    unsigned char *p=stub_destination(mapped,base,address,8);const uint16_t insn[]={0x4b00,0x4718};
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target=blank_bits(function);memcpy(p+4,&target,4);
}
/* Preserving trampoline for calls with stack arguments (create takes five
 * or six). ldr r3 would clobber the fourth argument, so the target address
 * travels in the scratch ip register instead; r0-r3 and the stack words
 * reach the callee untouched. Thumb-2 wide load, Thumb bx. */
typedef void (*six_fn)(void *,uint32_t,void *,void *,uint32_t,void *);
static uint32_t six_bits(six_fn f) { uint32_t v;_Static_assert(sizeof f==sizeof v,"fn width");memcpy(&v,&f,sizeof v);return v; }
static void six_stub(void *mapped,uint32_t base,uint32_t address,six_fn function)
{
    unsigned char *p=stub_destination(mapped,base,address,12);
    const uint16_t insn[]={0xf8df,0xc004,0x4760,0xbf00};
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target=six_bits(function);memcpy(p+8,&target,4);
}
typedef void (*seven_fn)(void *,uint32_t,uint32_t,uint32_t,void *,void *,void *);
static uint32_t seven_bits(seven_fn f) { uint32_t v;_Static_assert(sizeof f==sizeof v,"fn width");memcpy(&v,&f,sizeof v);return v; }
static void seven_stub(void *mapped,uint32_t base,uint32_t address,seven_fn function)
{
    unsigned char *p=stub_destination(mapped,base,address,12);
    const uint16_t insn[]={0xf8df,0xc004,0x4760,0xbf00};
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target=seven_bits(function);memcpy(p+8,&target,4);
}
typedef void (*five_fn)(void *,uint32_t,void *,void *,void *);
static uint32_t five_bits(five_fn f) { uint32_t v;_Static_assert(sizeof f==sizeof v,"fn width");memcpy(&v,&f,sizeof v);return v; }
static void five_stub(void *mapped,uint32_t base,uint32_t address,five_fn function)
{
    unsigned char *p=stub_destination(mapped,base,address,12);
    const uint16_t insn[]={0xf8df,0xc004,0x4760,0xbf00};
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target=five_bits(function);memcpy(p+8,&target,4);
}
enum {RR_BASE,RR_GET,RR_IS,RR_NAME,RR_FOCUS,RR_ELIP,RR_FONT,RR_RECT,RR_TEXT,RR_CTEXT,RR_CBTN,RR_ALIGN,RR_CHECK,RR_CCB,RR_SELID,RR_CSEL,RR_SELIMG};
struct rr_event { uint32_t kind,off,a,b;uint32_t w[4];char text[24]; };
struct rr_result { struct rr_event events[64];unsigned count; };
struct rr_fixture {
    uint32_t type;int present[6];int created,checked;int32_t pt[2];
    uint32_t words[20];uint32_t sel_img,sel_id;int flag;const char *l1,*l2;uint32_t rwords[6][4];
    struct rr_result *trace;unsigned char *row;
};
static const char rr_empty[]="";
static struct rr_fixture *rr_native;
static _Alignas(uint32_t) unsigned char rr_row[0x3000],rr_refrow[0x3000];
static uint32_t rr_vtable[16];
static void rr_emit(struct rr_result *r,uint32_t kind,uint32_t off,uint32_t a,uint32_t b,
    const uint32_t *w,const char *text)
{
    struct rr_event *e;assert(r->count<64);e=&r->events[r->count++];
    e->kind=kind;e->off=off;e->a=a;e->b=b;
    if(w)memcpy(e->w,w,16);else memset(e->w,0,16);
    if(text)snprintf(e->text,sizeof e->text,"%s",text);else e->text[0]=0;
}
static uint32_t rr_off(void *row,void *child)
{ assert((unsigned char *)child>=(unsigned char *)row && (unsigned char *)child<(unsigned char *)row+0x3000);
  return (uint32_t)((unsigned char *)child-(unsigned char *)row); }
static void rr_base(void *row) { (void)row;rr_emit(rr_native->trace,RR_BASE,0,0,0,NULL,NULL); }
static void rr_ctor(void *buf,uint32_t str)
{ const char *s=(const char *)str;assert(strlen(s)<16);*(const char **)buf=s; }
static unsigned rr_dtor(void *buf) { (void)buf;return 0; }
static const char *rr_keys[6];
static void *rr_getrect(void *container,void *ustr)
{
    const char *key=*(const char *const *)ustr;unsigned i;
    (void)container;
    for(i=0;i<6;++i)if(!strcmp(key,rr_keys[i]))break;
    assert(i<6);
    rr_emit(rr_native->trace,RR_GET,(uint32_t)i,0,0,NULL,key);
    if(!rr_native->present[i])return NULL;
    return rr_native->row+0x400+i*32;
}
static unsigned rr_created(void *child)
{ rr_emit(rr_native->trace,RR_IS,rr_off(rr_row,child),0,0,NULL,NULL);return (unsigned)rr_native->created; }
static void rr_setname(void *child,uint32_t slot,uint32_t id)
{ rr_emit(rr_native->trace,RR_NAME,rr_off(rr_row,child),slot,id,NULL,NULL); }
static void rr_setfocus(void *child,uint32_t on)
{ rr_emit(rr_native->trace,RR_FOCUS,rr_off(rr_row,child),on,0,NULL,NULL); }
static void rr_setelipses(void *child,uint32_t on)
{ rr_emit(rr_native->trace,RR_ELIP,rr_off(rr_row,child),on,0,NULL,NULL); }
static void rr_setfont(void *child,uint32_t id)
{ rr_emit(rr_native->trace,RR_FONT,rr_off(rr_row,child),id,0,NULL,NULL); }
static void rr_point(void *buf,void *row)
{ (void)row;memcpy(buf,rr_native->pt,8); }
static void rr_setrect(void *child,void *rect)
{ uint32_t w[4];memcpy(w,rect,16);
  rr_emit(rr_native->trace,RR_RECT,rr_off(rr_row,child),0,0,w,NULL); }
static void rr_settext(void *child,uint32_t ustr)
{ char tmp[32];const char *s=*(const char *const *)(uintptr_t)ustr;size_t n=strlen(s);
  assert(n<sizeof tmp);memcpy(tmp,s,n+1);
  rr_emit(rr_native->trace,RR_TEXT,rr_off(rr_row,child),0,0,NULL,tmp); }
/* The EASIRect default ctor at 0x9a7f94 runs verbatim from the copied bytes
 * below; it zeroes a stack temp that the key4 copy immediately overwrites,
 * so it has no observable trace of its own. */
static void rr_ctext(void *child,uint32_t ignored,void *label,void *rect,uint32_t extra,void *parent)
{
    char text[32];size_t n;uint32_t w[4];
    (void)ignored;(void)parent;
    assert((unsigned char *)label==(unsigned char *)rr_row+0x2dd8);
    { const char *s=*(const char *const *)label;
      n=strlen(s);assert(n<sizeof text);memcpy(text,s,n+1); }
    memcpy(w,rect,16);
    rr_emit(rr_native->trace,RR_CTEXT,rr_off(rr_row,child),extra,ignored,w,text);
}
static void rr_align(void *child,uint32_t a)
{ rr_emit(rr_native->trace,RR_ALIGN,rr_off(rr_row,child),a,0,NULL,NULL); }
static void rr_checked(void *child,uint32_t f)
{ rr_emit(rr_native->trace,RR_CHECK,rr_off(rr_row,child),f,0,NULL,NULL); }
static void rr_ccb(void *child,uint32_t z,uint32_t a,uint32_t b,void *label,void *rect,void *parent)
{
    char text[32];size_t n;uint32_t w[4];const char *s;
    (void)parent;
    /* The create takes the UString object; our ctor stores the char* in the
     * first slot, so dereference to compare the string itself. */
    s=*(const char *const *)label;
    n=strlen(s);assert(n<sizeof text);memcpy(text,s,n+1);
    memcpy(w,rect,16);
    rr_emit(rr_native->trace,RR_CCB,rr_off(rr_row,child),z,(a<<16)|b,w,text);
    (void)rr_empty;
}
static void rr_selimg(void *row,uint32_t id)
{ rr_emit(rr_native->trace,RR_SELIMG,0,id,0,NULL,NULL);(void)row; }
static void rr_selid(void *child,uint32_t id)
{ rr_emit(rr_native->trace,RR_SELID,rr_off(rr_row,child),id,0,NULL,NULL); }
static void rr_csel(void *child,uint32_t z,uint32_t a,uint32_t b,void *label,void *rect,void *parent)
{
    char text[32];size_t n;uint32_t w[4];const char *s;
    (void)parent;(void)z;
    s=*(const char *const *)label;
    n=strlen(s);assert(n<sizeof text);memcpy(text,s,n+1);
    memcpy(w,rect,16);
    rr_emit(rr_native->trace,RR_CSEL,rr_off(rr_row,child),0,(a<<16)|b,w,text);
}
static void rr_cbtn(void *button,uint32_t ignored,void *label,void *rect,void *parent)
{
    char text[32];size_t n;uint32_t w[4];
    (void)ignored;(void)parent;
    assert((unsigned char *)label==(unsigned char *)rr_row+0x2de0);
    { const char *s=*(const char *const *)label;
      n=strlen(s);assert(n<sizeof text);memcpy(text,s,n+1); }
    memcpy(w,rect,16);
    rr_emit(rr_native->trace,RR_CBTN,rr_off(rr_row,button),0,0,w,text);
}
static struct rr_fixture *rr_ref;
static void rre_base(void *row) { (void)row;rr_emit(rr_ref->trace,RR_BASE,0,0,0,NULL,NULL); }
static void *rre_cont(void *row) { (void)row;return rr_ref->row+0xa8; }
static void *rre_get(void *container,const char *key)
{
    unsigned i;(void)container;
    for(i=0;i<6;++i)if(!strcmp(key,rr_keys[i]))break;
    assert(i<6);
    rr_emit(rr_ref->trace,RR_GET,i,0,0,NULL,key);
    if(!rr_ref->present[i])return NULL;
    return rr_ref->row+0x400+i*32;
}
static void rre_words(void *rect,uint32_t out[4]) { memcpy(out,(const unsigned char *)rect+8,16); }
static uint32_t rre_type(void *row) { (void)row;return rr_ref->type; }
static uint32_t rre_word(void *row,uint32_t off)
{ (void)row;
  if(off==0x2e54)return rr_ref->sel_img;
  if(off==0x2de8)return rr_ref->sel_id;
  assert(off>=0x2e8c && off<=0x2ed8 && !(off&3));return rr_ref->words[(off-0x2e8c)/4]; }
static int rre_flag(void *row,uint32_t off) { (void)row;assert(off==0x2e45||off==0x2e44);return off==0x2e44?rr_ref->checked:rr_ref->flag; }
static const char *rre_label(void *row) { (void)row;return *(const char *const *)(rr_ref->row+0x2dd8); }
static const char *rre_blabel(void *row) { (void)row;return *(const char *const *)(rr_ref->row+0x2de0); }
static void *rre_child(void *row,uint32_t off) { (void)row;return rr_ref->row+off; }
static int rre_created(void *child)
{ rr_emit(rr_ref->trace,RR_IS,rr_off(rr_ref->row,child),0,0,NULL,NULL);return rr_ref->created; }
static void rre_name(void *child,uint32_t slot,uint32_t id)
{ rr_emit(rr_ref->trace,RR_NAME,rr_off(rr_ref->row,child),slot,id,NULL,NULL); }
static void rre_focus(void *child,int on)
{ rr_emit(rr_ref->trace,RR_FOCUS,rr_off(rr_ref->row,child),(uint32_t)on,0,NULL,NULL); }
static void rre_elipses(void *child,int on)
{ rr_emit(rr_ref->trace,RR_ELIP,rr_off(rr_ref->row,child),(uint32_t)on,0,NULL,NULL); }
static void rre_font(void *child,uint32_t id)
{ rr_emit(rr_ref->trace,RR_FONT,rr_off(rr_ref->row,child),id,0,NULL,NULL); }
static void rre_point(void *row,int32_t pt[2]) { (void)row;pt[0]=rr_ref->pt[0];pt[1]=rr_ref->pt[1]; }
static void rre_rect(void *child,const uint32_t rect[4])
{ rr_emit(rr_ref->trace,RR_RECT,rr_off(rr_ref->row,child),0,0,rect,NULL); }
static void rre_text(void *child,const char *text)
{ rr_emit(rr_ref->trace,RR_TEXT,rr_off(rr_ref->row,child),0,0,NULL,text); }
static void rre_ctext(void *child,uint32_t zero,const char *label,const uint32_t rect[4],uint32_t extra,void *parent)
{
    (void)parent;
    rr_emit(rr_ref->trace,RR_CTEXT,rr_off(rr_ref->row,child),extra,zero,rect,label);
}
static void rre_cbtn(void *button,const char *label,const uint32_t rect[4],void *parent)
{ (void)parent;rr_emit(rr_ref->trace,RR_CBTN,rr_off(rr_ref->row,button),0,0,rect,label); }
static void rre_align(void *child,uint32_t a)
{ rr_emit(rr_ref->trace,RR_ALIGN,rr_off(rr_ref->row,child),a,0,NULL,NULL); }
static void rre_checked(void *child,uint32_t f)
{ rr_emit(rr_ref->trace,RR_CHECK,rr_off(rr_ref->row,child),f,0,NULL,NULL); }
static void rre_ccb(void *child,uint32_t z,uint32_t a,uint32_t b,const char *label,
    const uint32_t rect[4],void *parent)
{ (void)parent;rr_emit(rr_ref->trace,RR_CCB,rr_off(rr_ref->row,child),z,(a<<16)|b,rect,label); }
static void rre_selimg(void *row,uint32_t id)
{ (void)row;rr_emit(rr_ref->trace,RR_SELIMG,0,id,0,NULL,NULL); }
static void rre_selid(void *child,uint32_t id)
{ rr_emit(rr_ref->trace,RR_SELID,rr_off(rr_ref->row,child),id,0,NULL,NULL); }
static void rre_csel(void *child,uint32_t z,uint32_t a,uint32_t b,const char *label,
    const uint32_t rect[4],void *parent)
{ (void)parent;rr_emit(rr_ref->trace,RR_CSEL,rr_off(rr_ref->row,child),0,(a<<16)|b,rect,label);(void)z; }
static struct oa_setting_row_render_ops rr_ops(struct rr_fixture *f)
{
    return (struct oa_setting_row_render_ops){f,rre_base,rre_cont,rre_get,rre_words,rre_type,
        rre_word,rre_flag,rre_label,rre_blabel,rre_child,rre_created,rre_name,rre_focus,
        rre_elipses,rre_font,rre_point,rre_rect,rre_text,rre_ctext,rre_cbtn,
        rre_align,rre_checked,rre_ccb,rre_selid,rre_csel,rre_selimg};
}
typedef void (*rr_fn)(void *);
static void rr_compare(struct rr_result *vendor,struct rr_result *ref)
{
    if(vendor->count!=ref->count) {
        fprintf(stderr,"render count native=%u ours=%u\n",vendor->count,ref->count);
        for(unsigned i=0;i<vendor->count;++i)fprintf(stderr,"  native[%u]=%u/%x/%x/%x\n",i,
            vendor->events[i].kind,vendor->events[i].off,vendor->events[i].a,vendor->events[i].b);
        for(unsigned i=0;i<ref->count;++i)fprintf(stderr,"  ours[%u]=%u/%x/%x/%x\n",i,
            ref->events[i].kind,ref->events[i].off,ref->events[i].a,ref->events[i].b);
        exit(1);
    }
    for(unsigned i=0;i<ref->count;++i) {
        struct rr_event *v=&vendor->events[i],*o=&ref->events[i];
        if(v->kind!=o->kind || v->off!=o->off || v->a!=o->a || v->b!=o->b ||
            memcmp(v->w,o->w,16) || strcmp(v->text,o->text)) {
            fprintf(stderr,"render trace %u native=%u/%x/%x/%x/[%x %x %x %x]/%s ours=%u/%x/%x/%x/[%x %x %x %x]/%s\n",i,
                v->kind,v->off,v->a,v->b,v->w[0],v->w[1],v->w[2],v->w[3],v->text,
                o->kind,o->off,o->a,o->b,o->w[0],o->w[1],o->w[2],o->w[3],o->text);exit(1);
        }
    }
}
static void rr_setup_row(unsigned char *row,struct rr_fixture *f)
{
    uint32_t vt=(uint32_t)(uintptr_t)rr_vtable;
    memset(row,0xa5,sizeof rr_row);
    memcpy(row,&vt,4);
    memcpy(row+0x2dec,&f->type,4);
    /* Row label slots are UString objects: first word is the char*. */
    { uint32_t p=(uint32_t)(uintptr_t)f->l1;memcpy(row+0x2dd8,&p,4); }
    { uint32_t p=(uint32_t)(uintptr_t)f->l2;memcpy(row+0x2de0,&p,4); }
    for(unsigned i=0;i<4;++i)memcpy(row+0x400+i*32+8,f->rwords[i],16);
    for(unsigned k=0;k<20;++k)memcpy(row+0x2e8c+k*4,&f->words[k],4);
    memcpy(row+0x2e54,&f->sel_img,4);memcpy(row+0x2de8,&f->sel_id,4);
    row[0x2e45]=(unsigned char)f->flag;row[0x2e44]=(unsigned char)f->checked;
}
static const char rk0[]="alexa",rk1[]="text1",rk2[]="labelLong",rk3[]="button",rk4[]="checkbox",rk5[]="selectionbox";
static const char *rkcell[5];
static void check_settings_row_render(const unsigned char *elf,size_t bytes)
{
    unsigned char *row=page(0xd54000,12288),*ecopy=page(0xd51000,4096),*eoff=page(0xd2a000,4096);
    unsigned char *base=page(0x10bf000,4096),*ustr=page(0xc86000,4096),*getr=page(0xeed000,4096);
    unsigned char *win=page(0x1152000,4096),*name=page(0xeee000,4096),*box=page(0x10c8000,4096);
    unsigned char *textbox=page(0x10c7000,4096),*elip=page(0x10c6000,4096),*btn=page(0x10ad000,12288);
    unsigned char *zer=page(0x9a7000,4096),*btntext=page(0x10b0000,4096);
    unsigned char *cb=page(0x10b1000,8192),*sel=page(0x10b5000,4096),*sel2=page(0x10b4000,4096);
    rr_keys[0]=rk0;rr_keys[1]=rk1;rr_keys[2]=rk2;rr_keys[3]=rk3;rr_keys[4]=rk4;rr_keys[5]=rk5;
    rkcell[0]=rk1;rkcell[1]=rk2;rkcell[2]=rk3;rkcell[3]=rk4;rkcell[4]=rk5;
    {
        size_t off=file_offset(elf,bytes,0xd54400,8746);
        assert(crc32(elf+off,8746)==UINT32_C(0x5b7e119b));
        memcpy(stub_destination(row,0xd54000,0xd54400,8746),elf+off,8746);
    }
    {
        size_t off=file_offset(elf,bytes,0xd51d00,58);
        assert(crc32(elf+off,58)==UINT32_C(0x119bb116));
        memcpy(stub_destination(ecopy,0xd51000,0xd51d00,58),elf+off,58);
    }
    {
        size_t off=file_offset(elf,bytes,0xd2af88,74);
        assert(crc32(elf+off,74)==UINT32_C(0xbc62c27a));
        memcpy(stub_destination(eoff,0xd2a000,0xd2af88,74),elf+off,74);
    }
    {
        size_t off=file_offset(elf,bytes,0x9a7f94,52);
        assert(crc32(elf+off,52)==UINT32_C(0x2a2b3d80));
        memcpy(stub_destination(zer,0x9a7000,0x9a7f94,52),elf+off,52);
    }
    {
        uint32_t pointer=(uint32_t)(uintptr_t)&guard;
        memcpy(stub_destination(zer,0x9a7000,0x9a7fc8,4),&pointer,4);
        memcpy(stub_destination(row,0xd54000,0xd54744,4),&pointer,4);
        memcpy(stub_destination(ecopy,0xd51000,0xd51d3c,4),&pointer,4);
        memcpy(stub_destination(eoff,0xd2a000,0xd2afd4,4),&pointer,4);
        uint32_t k0=(uint32_t)(uintptr_t)rk0;memcpy(stub_destination(row,0xd54000,0xd54748,4),&k0,4);
        uint32_t k1=(uint32_t)(uintptr_t)&rkcell[0];memcpy(stub_destination(row,0xd54000,0xd5474c,4),&k1,4);
        uint32_t k2=(uint32_t)(uintptr_t)&rkcell[1];memcpy(stub_destination(row,0xd54000,0xd54750,4),&k2,4);
        uint32_t k3=(uint32_t)(uintptr_t)&rkcell[2];memcpy(stub_destination(row,0xd54000,0xd54754,4),&k3,4);
        /* Case 0x1a reads the same "button" key through its own literal
         * (0xd54e24 -> 0x178d5e8); the second-level slot needs the owned
         * string or the body dereferences vendor rodata. */
        memcpy(stub_destination(row,0xd54000,0xd54e24,4),&k3,4);
        /* Case 0x16 reads "checkbox" through 0xd54e28 and the empty label
         * directly at 0xd54e2c; both need owned storage. */
        { uint32_t k4=(uint32_t)(uintptr_t)&rkcell[3];
          memcpy(stub_destination(row,0xd54000,0xd54e28,4),&k4,4); }
        { uint32_t em=(uint32_t)(uintptr_t)rr_empty;
          memcpy(stub_destination(row,0xd54000,0xd54e2c,4),&em,4); }
        /* Cases 0x2/0x3 read "selectionbox" through 0xd54e30. */
        { uint32_t k5=(uint32_t)(uintptr_t)&rkcell[4];
          memcpy(stub_destination(row,0xd54000,0xd54e30,4),&k5,4); }
    }
    blank_stub(base,0x10bf000,0x10bf06c,rr_base);
    pair_stub(ustr,0xc86000,0xc86744,rr_ctor);
    thumb_stub(ustr,0xc86000,0xc86554,rr_dtor);
    rectget_stub(getr,0xeed000,0xeed2b8,rr_getrect);
    thumb_stub(win,0x1152000,0x1152a40,rr_created);
    tri_stub(name,0xeee000,0xeee5ac,rr_setname);
    pair_stub(box,0x10c8000,0x10c8a28,rr_setfocus);
    pair2_stub(box,0x10c8000,0x10c8ac4,rr_setrect);
    six_stub(box,0x10c8000,0x10c8888,rr_ctext);
    pair_stub(textbox,0x10c7000,0x10c7828,rr_settext);
    pair_stub(textbox,0x10c7000,0x10c70bc,rr_setfont);
    pair_stub(elip,0x10c6000,0x10c6f48,rr_setelipses);
    pair2_stub(btn,0x10ad000,0x10ade18,rr_setrect);
    five_stub(btn,0x10ad000,0x10ae9f0,rr_cbtn);
    pair_stub(btn,0x10ad000,0x10aefb4,rr_setfont);
    pair_stub(btntext,0x10b0000,0x10b0540,rr_settext);
    pair_stub(cb,0x10b1000,0x10b14b8,rr_align);
    pair_stub(cb,0x10b1000,0x10b1368,rr_checked);
    seven_stub(cb,0x10b1000,0x10b2500,rr_ccb);
    pair2_stub(cb,0x10b1000,0x10b2670,rr_setrect);
    pair_stub(cb,0x10b1000,0x10b2164,rr_settext);
    pair_stub(row,0xd54000,0xd54374,rr_selimg);
    pair_stub(sel,0x10b5000,0x10b571c,rr_selid);
    seven_stub(sel,0x10b5000,0x10b5cf0,rr_csel);
    pair2_stub(sel,0x10b5000,0x10b5e60,rr_setrect);
    pair_stub(sel2,0x10b4000,0x10b4588,rr_setfont);
    {
        uint32_t point=point_bits(rr_point);
        memcpy(rr_vtable+0x28/4,&point,4);
    }
    executable(row,12288);executable(ecopy,4096);executable(eoff,4096);executable(base,4096);
    executable(ustr,4096);executable(getr,4096);executable(win,4096);executable(name,4096);
    executable(box,4096);executable(textbox,4096);executable(elip,4096);executable(btn,12288);
    executable(zer,4096);executable(btntext,4096);executable(cb,8192);executable(sel,4096);
    executable(sel2,4096);
    {
        unsigned compared=0;rr_fn render=0;
        { uint32_t entry=0xd54401;memcpy(&render,&entry,4); }
        {
            const uint32_t types[]={0,1,4,0x1a,0x16,2,3};
            const char *l1s[]={"","Aa"},*l2s[]={"","Bb"};
            const int32_t pts[][2]={{0,0},{5,-3},{INT32_MAX,1}};
            for(unsigned a=0;a<7;++a)for(unsigned m=0;m<64;++m)for(unsigned c=0;c<2;++c)
            for(unsigned p=0;p<3;++p)for(unsigned w=0;w<2;++w)for(unsigned l=0;l<2;++l)
            for(unsigned g=0;g<2;++g) {
                /* Firmware crash input, verified by reading: type 4 with the
                 * rebuild flag set, null key3, live key4 and an uncreated
                 * button dereferences the null shared slot (ldr r4,[r0,#8]
                 * at 0xd54726 with r0==0). Never executed on either side. */
                if(types[a]==UINT32_C(4) && g && !((m>>2)&1) && ((m>>3)&1) && !c)continue;
                struct rr_result native={0},ref={0};
                struct rr_fixture n={.type=types[a],.created=c,.checked=0,
                    .sel_img=0x66,.sel_id=0x55,.flag=(int)g,.l1=l1s[l],.l2=l2s[l],.trace=&native,.row=rr_row};
                for(unsigned i=0;i<6;++i)n.present[i]=(m>>i)&1;
                struct rr_fixture r=n;
                n.pt[0]=pts[p][0];n.pt[1]=pts[p][1];
                for(unsigned i=0;i<6;++i)for(unsigned k=0;k<4;++k)n.rwords[i][k]=(uint32_t)(0x10*(i+1)+k+1);
                for(unsigned k=0;k<20;++k)n.words[k]=w?UINT32_C(0x11111111)*(k+1):0;
                r=n;r.trace=&ref;r.row=rr_refrow;
                struct oa_setting_row_render_ops ops=rr_ops(&r);
                rr_setup_row(rr_row,&n);rr_setup_row(rr_refrow,&r);
                rr_native=&n;{ rr_fn volatile call=render;call(rr_row); }
                rr_ref=&r;assert(!oa_setting_row_render(rr_refrow,&ops));
                rr_compare(&native,&ref);
                if(memcmp(rr_row,rr_refrow,sizeof rr_row)) {
                    fprintf(stderr,"render row mismatch type=%x m=%x\n",types[a],m);exit(1);
                }
                ++compared;
            }
        }
        {
            const uint32_t types[]={0x1b,0x1c,0x100,UINT32_MAX};
            for(unsigned a=0;a<4;++a)for(unsigned c=0;c<2;++c) {
                struct rr_result native={0},ref={0};
                struct rr_fixture n={.type=types[a],.created=(int)c,.checked=0,.flag=0,
                    .l1="Aa",.l2="Bb",.trace=&native,.row=rr_row},r=n;
                for(unsigned i=0;i<6;++i)n.present[i]=1;
                for(unsigned i=0;i<6;++i)for(unsigned k=0;k<4;++k)n.rwords[i][k]=(uint32_t)(0x10*(i+1)+k+1);
                r=n;r.trace=&ref;r.row=rr_refrow;
                struct oa_setting_row_render_ops ops=rr_ops(&r);
                rr_setup_row(rr_row,&n);rr_setup_row(rr_refrow,&r);
                rr_native=&n;{ rr_fn volatile call=render;call(rr_row); }
                rr_ref=&r;assert(!oa_setting_row_render(rr_refrow,&ops));
                rr_compare(&native,&ref);
                if(memcmp(rr_row,rr_refrow,sizeof rr_row)) {
                    fprintf(stderr,"render default row mismatch type=%x\n",types[a]);exit(1);
                }
                ++compared;
            }
        }
        rr_native=NULL;rr_ref=NULL;
        assert(!munmap(row,12288) && !munmap(ecopy,4096) && !munmap(eoff,4096) && !munmap(base,4096));
        assert(!munmap(ustr,4096) && !munmap(getr,4096) && !munmap(win,4096) && !munmap(name,4096));
        assert(!munmap(box,4096) && !munmap(textbox,4096) && !munmap(elip,4096) && !munmap(btn,12288));
        assert(!munmap(zer,4096) && !munmap(btntext,4096) && !munmap(cb,8192) && !munmap(sel,4096));
        assert(!munmap(sel2,4096));
        printf("ok offline shipped ARM settings row render: %u native/C comparisons, 0-4/0x1a/0x16 paths and defaults match\n",compared);
    }
}
