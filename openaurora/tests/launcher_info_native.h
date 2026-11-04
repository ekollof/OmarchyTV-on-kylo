#ifndef OA_TESTS_LAUNCHER_INFO_NATIVE_H
#define OA_TESTS_LAUNCHER_INFO_NATIVE_H

#include "openaurora/launcher_item.h"

/*
 * Differential for SqliteLauncherDatabaseAdapter::copyLauncherInfoToItem
 * (0xfac334, 0xf4 bytes, CRC 0xa089d47e). This is the adapter's row decode:
 * one NovoLauncherItemInfo fixed record to the NovoLauncherItem intermediate.
 * The body is branch-free; the only calls are ten
 * UString::operator=(char const*) at 0xc86a70. The body is copied verbatim and
 * that call is stubbed with a trace that records destination and source
 * offsets and tags the destination slot.
 */
enum {LI2_ASSIGN};
struct li2_ev { uint32_t kind,dst,src; };
static struct li2_ev li2_evn[16],li2_evc[16],li2_cmp[16];
static struct li2_ev *li2_tp;static unsigned li2_tn;
static const struct oa_launcher_info *li2_info;static unsigned char *li2_base;

static void li2_emit(uint32_t kind,uint32_t dst,uint32_t src)
{ assert(li2_tn<16);li2_tp[li2_tn].kind=kind;li2_tp[li2_tn].dst=dst;li2_tp[li2_tn].src=src;++li2_tn; }
static void li2_trace_assign(void *dst,const char *src)
{
    uint32_t d=(uint32_t)((unsigned char *)dst-li2_base);
    uint32_t s;
    if(li2_tp==li2_evc && (d==0x78 || d==0x80 || d==0x88)) {
        assert(src && !src[0]);s=UINT32_MAX;
    } else s=(uint32_t)((uintptr_t)src-(uintptr_t)li2_info);
    ((uint32_t *)dst)[0]=s;((uint32_t *)dst)[1]=UINT32_C(0xa5a5a5a5);
    li2_emit(LI2_ASSIGN,d,s);
}
static void li2n_assign(void *dst,uint32_t src)
{ li2_trace_assign(dst,(const char *)(uintptr_t)src); }
static void li2c_assign(void *dst,const char *src) { li2_trace_assign(dst,src); }
static void li2_unused_copy(void *d,const void *s) { (void)d;(void)s;assert(0); }
static void li2_unused_default(void *d) { (void)d;assert(0); }
static int li2_unused_empty(const void *s) { (void)s;assert(0);return 0; }
static void li2_unused_applit(void *d,const char *s) { (void)d;(void)s;assert(0); }
static void li2_unused_append(void *d,const void *s) { (void)d;(void)s;assert(0); }
static struct oa_launcher_item_ops li2_ops(void)
{ return (struct oa_launcher_item_ops){li2_unused_copy,li2_unused_default,
    li2_unused_empty,li2c_assign,li2_unused_applit,li2_unused_append}; }

static void check_launcher_info(const unsigned char *elf,size_t bytes)
{
    unsigned char *body=page(0xfac000,4096),*ustr=page(0xc86000,4096);
    static uint32_t guard=UINT32_C(0x51c3a7e9);
    uint32_t guardp=(uint32_t)(uintptr_t)&guard;
    size_t o=file_offset(elf,bytes,0xfac334,0xf4);
    assert(crc32(elf+o,0xf4)==UINT32_C(0xa089d47e));
    memcpy(stub_destination(body,0xfac000,0xfac334,0xf4),elf+o,0xf4);
    memcpy(stub_destination(body,0xfac000,0xfac428,4),&guardp,4);
    pair_stub(ustr,0xc86000,0xc86a70,li2n_assign);
    executable(body,4096);executable(ustr,4096);
    struct oa_launcher_info *info=calloc(1,sizeof *info);
    unsigned char *n_out=calloc(1,0xa0),*c_out=calloc(1,0xa0);
    assert(info && n_out && c_out);
    void (*decode)(void *,uint32_t,void *);
    { uint32_t entry=0xfac335;memcpy(&decode,&entry,4); }
    static const size_t text_off[]={
        0x038,0x164,0x290,0x3bc,0x3f8,0x524,0x650,0x77c,0x8a8,0x93e
    };
    unsigned compared=0;
    for(unsigned c=0;c<8;++c) {
        struct oa_launcher_item_ops ops=li2_ops();
        memset(info,0xa5,sizeof *info);
        for(unsigned i=0;i<sizeof text_off/sizeof text_off[0];++i) {
            unsigned char *t=(unsigned char *)info+text_off[i];
            for(unsigned k=0;k<32;++k)t[k]=(unsigned char)(c*31+i*7+k);
            t[31]=0;
        }
        info->image_crc=UINT32_C(0x10000000)+c;
        info->thumb_update_time=UINT32_C(0x20000000)+c;
        info->application_update_time=UINT32_C(0x30000000)+c;
        info->channel_handle=(int32_t)(UINT32_C(0x40000000)+c);
        info->id=(int32_t)(UINT32_C(0x50000000)+c);
        info->is_delete_disabled=(uint8_t)(c&1);
        info->is_deleted=(uint8_t)((c>>1)&1);
        info->is_featured=(uint8_t)((c>>2)&1);
        info->is_moved_by_user=(uint8_t)((c>>1)&1);
        info->is_out_of_sync=(uint8_t)(c&1);
        info->item_type=(int32_t)(UINT32_C(0x60000000)+c);
        info->menu_return_id=(int32_t)(UINT32_C(0x70000000)+c);
        info->order_index=(int32_t)(UINT32_C(0x80000000)+c);
        info->original_id=(int32_t)(UINT32_C(0x90000000)+c);
        info->parent_id=(int32_t)(UINT32_C(0xa0000000)+c);
        info->server_id=(int32_t)(UINT32_C(0xb0000000)+c);
        li2_info=info;
        memset(n_out,0x5a,0xa0);
        li2_base=n_out;li2_tp=li2_evn;li2_tn=0;
        decode((void *)(uintptr_t)0x5000,(uint32_t)(uintptr_t)info,n_out);
        memcpy(li2_cmp,li2_evn,li2_tn*sizeof li2_evn[0]);unsigned n=li2_tn;
        /* Owner-required advertising exclusion: retain native operand
         * evidence, but expect empty literal assignments in those3 slots.
         * Ordinary fields/ordering/bytes continue to compare unmodified. */
        static const uint32_t excluded_sources[]={0x290,0x38,0x164};
        for(unsigned i=7;i<10;++i) {
            assert(li2_cmp[i].dst==0x78+(i-7)*8 &&
                   li2_cmp[i].src==excluded_sources[i-7]);
            li2_cmp[i].src=UINT32_MAX;
            ((uint32_t *)(n_out+li2_cmp[i].dst))[0]=UINT32_MAX;
        }
        memset(c_out,0x5a,0xa0);
        li2_base=c_out;li2_tp=li2_evc;li2_tn=0;
        oa_launcher_info_to_item(c_out,info,&ops);
        if(n!=li2_tn || memcmp(li2_cmp,li2_evc,n*sizeof li2_evn[0])) {
            fprintf(stderr,"launcher info trace mismatch case=%u n=%u/%u\n",c,n,li2_tn);
            exit(1);
        }
        if(memcmp(n_out,c_out,0xa0)) {
            fprintf(stderr,"launcher info output mismatch case=%u\n",c);
            for(unsigned i=0;i<0xa0;++i)
                if(n_out[i]!=c_out[i])
                    fprintf(stderr,"  byte %x native=%02x ours=%02x\n",i,n_out[i],c_out[i]);
            exit(1);
        }
        ++compared;
    }
    free(info);free(n_out);free(c_out);
    assert(!munmap(body,4096) && !munmap(ustr,4096));
    printf("ok offline shipped ARM launcher info: %u native/C cases, ordinary row fields match; 3 advertising slots explicitly empty\n",compared);
}

#endif
