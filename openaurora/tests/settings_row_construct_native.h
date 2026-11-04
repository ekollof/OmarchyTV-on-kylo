#ifndef OA_TESTS_SETTINGS_ROW_CONSTRUCT_NATIVE_H
#define OA_TESTS_SETTINGS_ROW_CONSTRUCT_NATIVE_H

#include "openaurora/settings_row_construct.h"

/*
 * Differential for SettingItemsListBoxRow::SettingItemsListBoxRow
 * (0xd53d0c, Thumb). The shipped body is copied at its original address so
 * its bl targets resolve to stubs in place; the base and 17 child
 * constructors, the 10 UString constructors, the framework interface probe and
 * the OSD mirror getter are all stubbed. After the call the 47 default-field
 * offsets are compared byte-for-byte; row+0 (vtable) is excluded because it
 * holds a vendor address.
 */
enum {SC_BASE,SC_TEXT,SC_BUTTON,SC_SEL,SC_SLIDE,SC_PROG,SC_ENTRY,SC_CHECK,
      SC_IMG,SC_USTR,SC_USTRDEF,SC_IFACE,SC_MIRROR};

struct sc_ev { uint32_t kind,a,b; };
static struct sc_ev sc_evn[64],sc_evc[64],sc_cmp[64];
static struct sc_ev *sc_tp;
static unsigned sc_tn;
static unsigned char *sc_row_native,*sc_row_ref;
static int sc_mirror;

static void sc_emit(uint32_t kind,uint32_t a,uint32_t b)
{
    assert(sc_tn<64);sc_tp[sc_tn].kind=kind;sc_tp[sc_tn].a=a;sc_tp[sc_tn].b=b;++sc_tn;
}
static uint32_t sc_off(const void *p)
{ return (uint32_t)((uintptr_t)p-(uintptr_t)sc_row_native); }
static uint32_t sc_off_ref(const void *p)
{ return (uint32_t)((uintptr_t)p-(uintptr_t)sc_row_ref); }

static void scn_base(void *row,uint32_t fw,uint32_t ctx)
{ (void)fw;(void)ctx;sc_emit(SC_BASE,sc_off(row),0); }
#define SCN_CHILD(name,kind) static void name(void *c,uint32_t fw,uint32_t ctx) \
{ (void)fw;(void)ctx;sc_emit(kind,sc_off(c),0); }
SCN_CHILD(scn_text,SC_TEXT) SCN_CHILD(scn_button,SC_BUTTON)
SCN_CHILD(scn_sel,SC_SEL) SCN_CHILD(scn_slide,SC_SLIDE)
SCN_CHILD(scn_prog,SC_PROG) SCN_CHILD(scn_entry,SC_ENTRY)
SCN_CHILD(scn_check,SC_CHECK) SCN_CHILD(scn_img,SC_IMG)
static void scn_ustr(void *slot,uint32_t lit)
{ sc_emit(SC_USTR,sc_off(slot),lit?1:0); }
static void scn_ustrdef(void *slot) { sc_emit(SC_USTRDEF,sc_off(slot),0); }
static uint32_t scn_iface(void *fw) { (void)fw;sc_emit(SC_IFACE,0,0);return UINT32_C(0x5000); }
static uint32_t scn_mirror(void *control) { (void)control;sc_emit(SC_MIRROR,0,(uint32_t)sc_mirror);return (uint32_t)sc_mirror; }

static void scc_base(void *row,void *fw,void *ctx)
{ (void)fw;(void)ctx;sc_emit(SC_BASE,sc_off_ref(row),0); }
#define SCC_CHILD(name,kind) static void name(void *c,void *fw,void *ctx) \
{ (void)fw;(void)ctx;sc_emit(kind,sc_off_ref(c),0); }
SCC_CHILD(scc_text,SC_TEXT) SCC_CHILD(scc_button,SC_BUTTON)
SCC_CHILD(scc_sel,SC_SEL) SCC_CHILD(scc_slide,SC_SLIDE)
SCC_CHILD(scc_prog,SC_PROG) SCC_CHILD(scc_entry,SC_ENTRY)
SCC_CHILD(scc_check,SC_CHECK) SCC_CHILD(scc_img,SC_IMG)
static void scc_ustr(void *slot,const char *lit)
{ sc_emit(lit?SC_USTR:SC_USTRDEF,sc_off_ref(slot),lit?1:0); }
static void *scc_iface(void *fw) { (void)fw;sc_emit(SC_IFACE,0,0);return (void *)(uintptr_t)UINT32_C(0x5000); }
static int scc_mirror(void *control) { (void)control;sc_emit(SC_MIRROR,0,(uint32_t)sc_mirror);return sc_mirror; }
static struct oa_setting_row_construct_ops sc_ops(void)
{
    return (struct oa_setting_row_construct_ops){scc_base,scc_text,scc_button,
        scc_sel,scc_slide,scc_prog,scc_entry,scc_check,scc_img,scc_ustr,
        scc_iface,scc_mirror};
}

static void check_settings_row_construct(const unsigned char *elf,size_t bytes)
{
    unsigned char *body=page(0xd53000,8192);
    unsigned char *base=page(0x10bf000,4096),*text=page(0x10c5000,4096);
    unsigned char *button=page(0x10af000,4096),*sel=page(0x10b4000,4096);
    unsigned char *slide=page(0x10c4000,4096),*prog=page(0x10b2000,4096);
    unsigned char *entry=page(0x10b7000,4096),*check=page(0x10b1000,4096);
    unsigned char *img=page(0x10c8000,4096),*ustr=page(0xc86000,4096);
    unsigned char *iface=page(0x113b000,4096),*mirror=page(0x97b000,4096);
    uint32_t guard=UINT32_C(0x2f5ca1b7),pointer=(uint32_t)(uintptr_t)&guard;
    static const char empty[]="";
    size_t off=file_offset(elf,bytes,0xd53d0c,0x4cd);
    assert(crc32(elf+off,0x4cd)==UINT32_C(0xe68cf4e1));
    memcpy(stub_destination(body,0xd53000,0xd53d0c,0x4cd),elf+off,0x4cd);
    memcpy(stub_destination(body,0xd53000,0xd54098,4),&pointer,4);
    { uint32_t e=(uint32_t)(uintptr_t)empty;
      memcpy(stub_destination(body,0xd53000,0xd540a0,4),&e,4); }
    tri_stub(base,0x10bf000,0x10bf1ac,scn_base);
    tri_stub(text,0x10c5000,0x10c5ad8,scn_text);
    tri_stub(button,0x10af000,0x10af450,scn_button);
    tri_stub(sel,0x10b4000,0x10b4910,scn_sel);
    tri_stub(slide,0x10c4000,0x10c4530,scn_slide);
    tri_stub(prog,0x10b2000,0x10b2a80,scn_prog);
    tri_stub(entry,0x10b7000,0x10b766c,scn_entry);
    tri_stub(check,0x10b1000,0x10b1790,scn_check);
    tri_stub(img,0x10c8000,0x10c8fec,scn_img);
    pair_stub(ustr,0xc86000,0xc86744,scn_ustr);
    blank_stub(ustr,0xc86000,0xc8648c,scn_ustrdef);
    thumb_stub(iface,0x113b000,0x113b958,(getter)scn_iface);
    thumb_stub(mirror,0x97b000,0x97b0ec,(getter)scn_mirror);
    executable(body,8192);executable(base,4096);executable(text,4096);
    executable(button,4096);executable(sel,4096);executable(slide,4096);
    executable(prog,4096);executable(entry,4096);executable(check,4096);
    executable(img,4096);executable(ustr,4096);executable(iface,4096);
    executable(mirror,4096);
    sc_row_native=calloc(1,0x3000);sc_row_ref=calloc(1,0x3000);
    assert(sc_row_native && sc_row_ref);
    void (*ctor)(void *,uint32_t,uint32_t);
    { uint32_t entryp=0xd53d0d;memcpy(&ctor,&entryp,4); }
    unsigned compared=0;
    for(unsigned m=0;m<2;++m) {
        struct oa_setting_row_construct_ops ops=sc_ops();
        memset(sc_row_native,0xa5,0x3000);memset(sc_row_ref,0xa5,0x3000);
        memcpy(sc_row_native+4,&(uint32_t){UINT32_C(0x4000)},4);
        memcpy(sc_row_ref+4,&(uint32_t){UINT32_C(0x4000)},4);
        sc_mirror=(int)m;
        sc_tp=sc_evn;sc_tn=0;
        ctor(sc_row_native,UINT32_C(0x4000),UINT32_C(0x4004));
        memcpy(sc_cmp,sc_evn,sc_tn*sizeof sc_evn[0]);unsigned n=sc_tn;
        sc_tp=sc_evc;sc_tn=0;
        oa_setting_row_construct(sc_row_ref,(void *)(uintptr_t)UINT32_C(0x4000),
            (void *)(uintptr_t)UINT32_C(0x4004),&ops);
        if(n!=sc_tn || memcmp(sc_cmp,sc_evc,n*sizeof sc_evn[0])) {
            fprintf(stderr,"row construct call mismatch mirror=%u n=%u/%u\n",m,n,sc_tn);
            exit(1);
        }
        for(unsigned i=0;i<OA_ROW_FIELD_COUNT;++i) {
            uint32_t a,b,o=oa_setting_row_field_offsets[i];
            memcpy(&a,sc_row_native+o,4);memcpy(&b,sc_row_ref+o,4);
            if(a!=b) {
                fprintf(stderr,"row construct field mismatch mirror=%u off=%x native=%x ours=%x\n",
                    m,o,a,b);
                exit(1);
            }
            ++compared;
        }
        { uint32_t a=sc_row_native[0x2e49],b=sc_row_ref[0x2e49];
          if(a!=b) { fprintf(stderr,"row construct byte mismatch 2e49 %u/%u\n",a,b);exit(1);} }
        ++compared;
    }
    free(sc_row_native);free(sc_row_ref);
    assert(!munmap(body,8192) && !munmap(base,4096) && !munmap(text,4096));
    assert(!munmap(button,4096) && !munmap(sel,4096) && !munmap(slide,4096));
    assert(!munmap(prog,4096) && !munmap(entry,4096) && !munmap(check,4096));
    assert(!munmap(img,4096) && !munmap(ustr,4096) && !munmap(iface,4096) && !munmap(mirror,4096));
    printf("ok offline shipped ARM settings row construct: %u native/C cases, child/string/interface order and default fields match\n",compared);
}

#endif
