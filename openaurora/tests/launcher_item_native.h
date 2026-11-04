#ifndef OA_TESTS_LAUNCHER_ITEM_NATIVE_H
#define OA_TESTS_LAUNCHER_ITEM_NATIVE_H

#include "openaurora/launcher_item.h"

/*
 * Differential for LauncherItem::LauncherItem(NovoLauncherItem const&)
 * (0xe4794c, Thumb, 500 bytes, CRC 0xd1bcb8c5). The body is copied verbatim at
 * its original address. Its literal pool sits just past the copied range, so
 * the canary, the gthread probe and the two path strings are written into the
 * mapped page: a non-zero probe keeps the shipped atomic refcount branch.
 *
 * UString constructors/operations and the shared_count destructor are stubs.
 * Both the native trampolines and the owned ops call the same trace helpers,
 * so the ordered calls, the 164-byte output and the control-block refcount are
 * compared directly.
 */
enum {LI_COPY,LI_DEFAULT,LI_EMPTY,LI_ASSIGN,LI_APPLIT,LI_APPEND};
struct li_ev { uint32_t kind,dst,src; };
static struct li_ev li_evn[32],li_evc[32],li_cmp[32];
static struct li_ev *li_tp;static unsigned li_tn;
static const unsigned char *li_src;static unsigned char *li_base;
static int li_empty_flag;
static const char li_dir[]=OA_LAUNCHER_IMAGE_DIR;
static const char li_sep[]=OA_LAUNCHER_IMAGE_SEP;

static void li_emit(uint32_t kind,uint32_t dst,uint32_t src)
{ assert(li_tn<32);li_tp[li_tn].kind=kind;li_tp[li_tn].dst=dst;li_tp[li_tn].src=src;++li_tn; }
static uint32_t li_doff(const void *p)
{ return (uint32_t)((uintptr_t)p-(uintptr_t)li_base); }
static uint32_t li_soff(const void *p)
{ return (uint32_t)((uintptr_t)p-(uintptr_t)li_src); }

static void li_trace_copy(void *dst,const void *src)
{
    memcpy(dst,src,8);
    li_emit(LI_COPY,li_doff(dst),li_soff(src));
}
static void li_trace_default(void *dst)
{
    memset(dst,0,8);
    li_emit(LI_DEFAULT,li_doff(dst),0);
}
static int li_trace_empty(const void *src)
{
    li_emit(LI_EMPTY,0,li_soff(src));
    return li_empty_flag;
}
static void li_trace_assign(void *dst,const char *literal)
{
    assert(!strcmp(literal,li_dir));
    memset(dst,0,8);
    ((uint32_t *)dst)[0]=UINT32_C(0xab000000);
    li_emit(LI_ASSIGN,li_doff(dst),0);
}
static void li_trace_append_lit(void *dst,const char *literal)
{
    assert(!strcmp(literal,li_sep));
    ((uint32_t *)dst)[1]=UINT32_C(0xac000001);
    li_emit(LI_APPLIT,li_doff(dst),1);
}
static void li_trace_append(void *dst,const void *src)
{
    ((uint32_t *)dst)[1]=((const uint32_t *)src)[0];
    li_emit(LI_APPEND,li_doff(dst),li_soff(src));
}

/* Native trampolines: uint32_t arguments, Thumb stubs installed in place. */
static void lin_copy(void *dst,uint32_t src) { li_trace_copy(dst,(const void *)(uintptr_t)src); }
static void lin_default(void *dst) { li_trace_default(dst); }
static unsigned lin_empty(void *src) { return (unsigned)li_trace_empty((const void *)(uintptr_t)src); }
static void lin_assign(void *dst,uint32_t lit) { li_trace_assign(dst,(const char *)(uintptr_t)lit); }
static void lin_applit(void *dst,uint32_t lit) { li_trace_append_lit(dst,(const char *)(uintptr_t)lit); }
static void lin_append(void *dst,uint32_t src) { li_trace_append(dst,(const void *)(uintptr_t)src); }
static void lin_release(void *p) { (void)p; }

/* Owned-side ops. */
static void lic_copy(void *dst,const void *src) { li_trace_copy(dst,src); }
static void lic_default(void *dst) { li_trace_default(dst); }
static int lic_empty(const void *src) { return li_trace_empty(src); }
static void lic_assign(void *dst,const char *lit) { li_trace_assign(dst,lit); }
static void lic_applit(void *dst,const char *lit) { li_trace_append_lit(dst,lit); }
static void lic_append(void *dst,const void *src) { li_trace_append(dst,src); }
static struct oa_launcher_item_ops li_ops(void)
{ return (struct oa_launcher_item_ops){lic_copy,lic_default,lic_empty,
    lic_assign,lic_applit,lic_append}; }

static void check_launcher_item(const unsigned char *elf,size_t bytes)
{
    unsigned char *body=page(0xe47000,4096),*ustr=page(0xc86000,4096);
    unsigned char *dtor=page(0x95a000,4096),*ctrl=page(0x6a0000,4096);
    static uint32_t guard=UINT32_C(0x2f5ca1b7);
    uint32_t guardp=(uint32_t)(uintptr_t)&guard;
    uint32_t dirp=(uint32_t)(uintptr_t)li_dir,sepp=(uint32_t)(uintptr_t)li_sep;
    size_t o=file_offset(elf,bytes,0xe4794c,0x1f4);
    assert(crc32(elf+o,0x1f4)==UINT32_C(0xd1bcb8c5));
    memcpy(stub_destination(body,0xe47000,0xe4794c,0x1f4),elf+o,0x1f4);
    memcpy(stub_destination(body,0xe47000,0xe47b40,4),&guardp,4);
    { uint32_t probe=1;memcpy(stub_destination(body,0xe47000,0xe47b44,4),&probe,4); }
    memcpy(stub_destination(body,0xe47000,0xe47b48,4),&dirp,4);
    memcpy(stub_destination(body,0xe47000,0xe47b4c,4),&sepp,4);
    pair_stub(ustr,0xc86000,0xc867f4,lin_copy);
    blank_stub(ustr,0xc86000,0xc8648c,lin_default);
    thumb_stub(ustr,0xc86000,0xc86c28,lin_empty);
    pair_stub(ustr,0xc86000,0xc86a70,lin_assign);
    pair_stub(ustr,0xc86000,0xc86700,lin_applit);
    pair_stub(ustr,0xc86000,0xc86880,lin_append);
    blank_stub(dtor,0x95a000,0x95a990,lin_release);
    executable(body,4096);executable(ustr,4096);executable(dtor,4096);
    unsigned char *novo=calloc(1,OA_NOVO_LAUNCHER_ITEM_READ_BYTES);
    unsigned char *n_out=calloc(1,256),*c_out=calloc(1,256);
    assert(novo && n_out && c_out);
    uint32_t *refs=(uint32_t *)(ctrl+0x100);
    uint32_t control=(uint32_t)(uintptr_t)refs;
    void (*lictor)(void *,uint32_t);
    { uint32_t entry=0xe4794d;memcpy(&lictor,&entry,4); }
    unsigned compared=0;
    for(unsigned pat=0;pat<2;++pat)for(unsigned empty=0;empty<2;++empty)
    for(unsigned has_control=0;has_control<2;++has_control) {
        struct oa_launcher_item_ops ops=li_ops();
        li_src=novo;
        for(unsigned i=0;i<OA_NOVO_LAUNCHER_ITEM_READ_BYTES;i+=4) {
            uint32_t v=pat?UINT32_C(0xa1b2c3d4)^i:UINT32_C(0x01020304)+i;
            memcpy(novo+i,&v,4);
        }
        { uint32_t zero=0,raw=UINT32_C(0x6000);
          memcpy(novo+0x70,has_control?&control:&zero,4);
          memcpy(novo+0x74,&raw,4); }
        novo[0x50]=0x11;novo[0x51]=0x22;novo[0x54]=0x33;
        li_empty_flag=(int)empty;
        refs[1]=7;
        memset(n_out,0xa5,256);
        li_base=n_out;li_tp=li_evn;li_tn=0;
        lictor(n_out,(uint32_t)(uintptr_t)novo);
        memcpy(li_cmp,li_evn,li_tn*sizeof li_evn[0]);unsigned n=li_tn;
        /* Explicit privacy deviation, not native parity: the vendor copies
         * three advertising slots; ours constructs them empty. Check the
         * actual vendor operands before normalizing only these destinations
         * in the expected trace/output. All ordinary bytes remain compared. */
        for(unsigned i=6;i<=8;++i) {
            assert(li_cmp[i].kind==LI_COPY && li_cmp[i].dst==0x4c+(i-6)*8 &&
                   li_cmp[i].src==0x78+(i-6)*8);
            memset(n_out+li_cmp[i].dst,0,8);
            li_cmp[i].kind=LI_DEFAULT;li_cmp[i].src=0;
        }
        uint32_t native_refs=has_control?refs[1]:0;
        refs[1]=7;
        memset(c_out,0xa5,256);
        li_base=c_out;li_tp=li_evc;li_tn=0;
        oa_launcher_item_from_novo(c_out,novo,&ops);
        if(n!=li_tn || memcmp(li_cmp,li_evc,n*sizeof li_evn[0])) {
            fprintf(stderr,"launcher item trace mismatch pat=%u empty=%u ctrl=%u n=%u/%u\n",
                pat,empty,has_control,n,li_tn);
            exit(1);
        }
        if(memcmp(n_out,c_out,256)) {
            fprintf(stderr,"launcher item output mismatch pat=%u empty=%u ctrl=%u\n",
                pat,empty,has_control);
            for(unsigned i=0;i<256;++i)
                if(n_out[i]!=c_out[i])
                    fprintf(stderr,"  byte %x native=%02x ours=%02x\n",
                        i,n_out[i],c_out[i]);
            exit(1);
        }
        if(has_control && refs[1]!=native_refs) {
            fprintf(stderr,"launcher item refcount mismatch %u/%u\n",
                (unsigned)native_refs,(unsigned)refs[1]);
            exit(1);
        }
        if(!has_control && refs[1]!=7) {
            fprintf(stderr,"launcher item refcount written for null control\n");
            exit(1);
        }
        ++compared;
    }
    free(novo);free(n_out);free(c_out);
    assert(!munmap(body,4096) && !munmap(ustr,4096));
    assert(!munmap(dtor,4096) && !munmap(ctrl,4096));
    printf("ok offline shipped ARM launcher item: %u native/C cases, ordinary fields/path/refcount match; 3 advertising slots explicitly empty\n",compared);
}

#endif
