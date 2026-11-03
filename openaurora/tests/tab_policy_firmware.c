/* OFFLINE ARM/qemu differential only. Reads the local ELF, executes its
 * availability/group instructions in THIS process with synthetic getters. Never
 * attach to Aurora or run platform/HAL init. No vendor bytes are embedded. */
#if !defined(__arm__)
#error "Build this optional firmware differential for ARM and run under qemu-arm"
#endif
#include "openaurora/tab_policy.h"
#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
_Static_assert(sizeof(uintptr_t)==4,"ARM32 addresses required");
typedef unsigned (*getter)(void *);
typedef unsigned (*predicate)(void *,int32_t);
static struct oa_tab_context active;
static uint32_t guard=0x13572468;
static unsigned hotel(void *p) { (void)p;return active.hotel_active; }
static unsigned picture(void *p) { (void)p;return active.hotel_picture_flag; }
static unsigned mode(void *p) { (void)p;return active.hotel_osd_mode; }
static unsigned media(void *p) { (void)p;return active.in_media_browser; }
static unsigned app(void *p) { (void)p;return active.in_app; }
static unsigned failed(void *p) { (void)p;fputs("firmware stack guard failed\n",stderr);exit(1); }
static uint32_t bits(getter p)
{ uint32_t n;_Static_assert(sizeof p==sizeof n,"function pointer width");memcpy(&n,&p,sizeof n);return n; }
static uint32_t crc32(const unsigned char *p,size_t n)
{
    uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<n;++i) {
        crc^=p[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^((crc&1)?UINT32_C(0xedb88320):0);
    }
    return crc^UINT32_MAX;
}
static size_t file_offset(const unsigned char *b,size_t n,uint32_t address,size_t bytes)
{
    assert(n>=sizeof(Elf32_Ehdr));Elf32_Ehdr h;memcpy(&h,b,sizeof h);
    assert(!memcmp(h.e_ident,ELFMAG,SELFMAG) && h.e_ident[EI_CLASS]==ELFCLASS32 &&
           h.e_ident[EI_DATA]==ELFDATA2LSB && h.e_machine==EM_ARM && h.e_phentsize==sizeof(Elf32_Phdr));
    for(unsigned i=0;i<h.e_phnum;++i) {
        size_t off=(size_t)h.e_phoff+i*sizeof(Elf32_Phdr);assert(off<=n && sizeof(Elf32_Phdr)<=n-off);
        Elf32_Phdr p;memcpy(&p,b+off,sizeof p);
        if(p.p_type!=PT_LOAD || address<p.p_vaddr)continue;
        size_t delta=address-p.p_vaddr;
        if(delta>p.p_filesz || bytes>p.p_filesz-delta)continue;
        off=(size_t)p.p_offset+delta;assert(off<=n && bytes<=n-off);return off;
    }
    fputs("unmapped source address\n",stderr);exit(1);
}
static void *page(uint32_t address,size_t length)
{
    void *wanted=(void *)(uintptr_t)address;
    void *p=mmap(wanted,length,PROT_READ|PROT_WRITE,
                 MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    if(p==MAP_FAILED && errno==EINVAL)
        p=mmap(wanted,length,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(p==MAP_FAILED || p!=wanted) {
        if(p!=MAP_FAILED)assert(!munmap(p,length));
        fputs("offline reference address unavailable; refusing fixed overwrite\n",stderr);exit(77);
    }
    return p;
}
static void executable(void *p,size_t n)
{ __builtin___clear_cache(p,(char *)p+n);assert(!mprotect(p,n,PROT_READ|PROT_EXEC)); }
static void thumb_stub(void *mapped,uint32_t base,uint32_t address,getter function)
{
    unsigned char *p=(unsigned char *)mapped+address-base;
    const uint16_t insn[2]={0x4b00,0x4718}; /* ldr r3,[pc,#0]; bx r3 */
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target=bits(function);memcpy(p+4,&target,4);
}
static uint32_t children[5];
static unsigned child_count;
static unsigned begin_children(void *p) { (void)p;return (uint32_t)(uintptr_t)children; }
static unsigned end_children(void *p) { (void)p;return (uint32_t)(uintptr_t)(children+child_count); }
static unsigned num_children(void *p) { (void)p;return child_count; }
static unsigned child_focus(void *p,int32_t value)
{ ((unsigned char *)p)[0x20]=(unsigned char)value;return 0; }
static uint32_t predicate_bits(predicate p)
{ uint32_t value;_Static_assert(sizeof p==sizeof value,"function pointer width");memcpy(&value,&p,sizeof value);return value; }
static void check_selection(const unsigned char *elf,size_t bytes)
{
    const struct {uint32_t address,length,crc,guard_literal;} fragments[]={
        {0x1148424,112,UINT32_C(0xbf9398eb),0x1148490},
        {0x114854c,88,UINT32_C(0x8c289305),0x11485a0},
        {0x11485d4,84,UINT32_C(0x3418dcb3),0x1148624},
        {0x1149390,52,UINT32_C(0xd3c5e535),0x11493c0},
    };
    const uint32_t base=0x1148000;
    unsigned char *mapped=page(base,8192);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;
    for(size_t i=0;i<sizeof fragments/sizeof *fragments;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(mapped+fragments[i].address-base,elf+off,fragments[i].length);
        memcpy(mapped+fragments[i].guard_literal-base,&pointer,4);
    }
    thumb_stub(mapped,base,0x11492f8,begin_children);
    thumb_stub(mapped,base,0x1149328,end_children);
    thumb_stub(mapped,base,0x1149358,num_children);
    executable(mapped,8192);
    predicate select;pointer=0x11485d5;memcpy(&select,&pointer,sizeof select);
    typedef void (*focus_fn)(void *,unsigned);
    focus_fn focus;pointer=0x1148425;memcpy(&focus,&pointer,sizeof focus);
    getter focused;pointer=0x114854d;memcpy(&focused,&pointer,sizeof focused);
    uint32_t group_vtable[9]={0},child_vtable[8]={0};
    group_vtable[8]=0x1148425;child_vtable[7]=predicate_bits(child_focus);
    const int32_t requests[]={INT32_MIN,-1,0,1,2,3,4,5,INT32_MAX};
    unsigned compared=0;
    for(unsigned available=0;available<32;++available)
    for(unsigned described=0;described<32;++described)
    for(size_t r=0;r<sizeof requests/sizeof *requests;++r) {
        _Alignas(uint32_t) unsigned char group[0x30]={0},child[5][0x24]={{0}};
        pointer=(uint32_t)(uintptr_t)group_vtable;memcpy(group,&pointer,4);
        uint32_t ids[5]={0};child_count=0;
        for(unsigned tab=0;tab<5;++tab)if((available&described)&(1u<<tab)) {
            pointer=(uint32_t)(uintptr_t)child_vtable;memcpy(child[child_count],&pointer,4);
            ids[child_count]=tab;children[child_count]=(uint32_t)(uintptr_t)child[child_count];
            ++child_count;
            /* Attach's focus callback after append; no vector allocator needed. */
            focus(group,group[0x20]);
            if(requests[r]==(int32_t)tab)assert(select(group,(int32_t)child_count-1)==1);
        }
        /* init gives group focus, which focuses its selected child. */
        focus(group,1);group[0x20]=1;
        unsigned found=focused(group);int32_t focused_tab=-1;
        for(unsigned i=0;i<child_count;++i)if(found==children[i])focused_tab=(int32_t)ids[i];
        assert((found==0)==(child_count==0));
        uint32_t selected_index;memcpy(&selected_index,group+0x2c,4);
        struct oa_tab_selection ours;
        assert(!oa_tab_initial_selection((uint8_t)available,(uint8_t)described,requests[r],&ours));
        assert(ours.created_mask==(available&described) && ours.count==child_count &&
               ours.selected_index==selected_index && ours.focused_tab==focused_tab &&
               ours.content_tab==(focused_tab<0?0:focused_tab));
        /* Rejected selection leaves state and focus unchanged. */
        assert(select(group,-1)==0 && select(group,(int32_t)child_count)==0);
        uint32_t after;memcpy(&after,group+0x2c,4);assert(after==selected_index && focused(group)==found);
        ++compared;
    }
    assert(!munmap(mapped,8192));
    printf("ok offline shipped ARM group selection: %u availability/description/request cases, rejected indices and empty focus\n",compared);
}
int main(int argc,char **argv)
{
    assert(argc==2 && sysconf(_SC_PAGESIZE)==4096);
    FILE *f=fopen(argv[1],"rb");assert(f);struct stat st;
    assert(!fstat(fileno(f),&st) && S_ISREG(st.st_mode) && st.st_size>0 && st.st_size<128*1024*1024);
    size_t bytes=(size_t)st.st_size;unsigned char *elf=malloc(bytes);assert(elf);
    assert(fread(elf,1,bytes,f)==bytes && !fclose(f));
    size_t off=file_offset(elf,bytes,0xd6ef80,168);
    assert(crc32(elf+off,168)==UINT32_C(0x550b31ac));
    void *code=page(0xd6e000,8192),*media_page=page(0xccb000,4096);
    void *app_page=page(0xcd1000,4096),*fail_page=page(0x6af000,4096);
    memcpy((unsigned char *)code+0xf80,elf+off,168);
    uint32_t guard_address=(uint32_t)(uintptr_t)&guard;
    memcpy((unsigned char *)code+0x1024,&guard_address,4); /* redirect only guard literal */
    thumb_stub(media_page,0xccb000,0xccb754,media);
    thumb_stub(app_page,0xcd1000,0xcd1c98,app);
    /* __stack_chk_fail is reached through BLX/ARM PLT, not Thumb. */
    const uint32_t arm[3]={UINT32_C(0xe59fc000),UINT32_C(0xe12fff1c),bits(failed)};
    memcpy((unsigned char *)fail_page+0x740,arm,sizeof arm);
    executable(code,8192);executable(media_page,4096);executable(app_page,4096);executable(fail_page,4096);
    check_selection(elf,bytes);free(elf);
    _Alignas(uint32_t) unsigned char dialog[0x37a1]={0},demo[0x29c]={0};
    uint32_t vtable[21]={0};vtable[0x18/4]=bits(hotel);vtable[0x40/4]=bits(mode);vtable[0x50/4]=bits(picture);
    uint32_t hotel_object=(uint32_t)(uintptr_t)vtable;
    uint32_t pointer=(uint32_t)(uintptr_t)&hotel_object;memcpy(demo+0x298,&pointer,4);
    pointer=(uint32_t)(uintptr_t)demo;memcpy(dialog+0x484,&pointer,4);
    predicate reference;pointer=0xd6ef81;memcpy(&reference,&pointer,sizeof reference);
    const uint32_t modes[]={0,1,2,3,255,UINT32_MAX};
    const int32_t tabs[]={INT32_MIN,-1,0,1,2,3,4,5,INT32_MAX};
    uint64_t compared=0;
    for(unsigned mask=0;mask<64;++mask)for(size_t m=0;m<sizeof modes/sizeof *modes;++m)
    for(unsigned raw=0;raw<256;++raw) {
        uint8_t truth=(mask&2)?255:1; /* noncanonical true getter/field bytes */
        active=(struct oa_tab_context){.hotel_osd_mode=modes[m],.hotel_active=(mask&1)?truth:0,
            .hotel_picture_flag=(uint8_t)raw,.network_profile_flag=(mask&4)?truth:0,
            .from_launcher=(mask&8)?truth:0,.in_media_browser=(mask&16)?truth:0,
            .in_app=(mask&32)?(uint8_t)raw:0};
        dialog[0x37a0]=active.network_profile_flag;dialog[0x4b7]=active.from_launcher;
        for(size_t i=0;i<sizeof tabs/sizeof *tabs;++i) {
            uint8_t ours;assert(!oa_tab_available(&active,tabs[i],&ours));
            unsigned vendor=reference(dialog,tabs[i]);
            if(vendor!=ours) {
                fprintf(stderr,"difference tab=%" PRId32 " flags=%u mode=%" PRIu32 " raw=%u vendor=%u ours=%u\n",
                        tabs[i],mask,modes[m],raw,vendor,(unsigned)ours);return 1;
            }
            ++compared;
        }
    }
    assert(!munmap(code,8192) && !munmap(media_page,4096) && !munmap(app_page,4096) && !munmap(fail_page,4096));
    printf("ok offline shipped ARM availability: %" PRIu64 " comparisons, raw XOR bytes/default IDs, only stack-guard literal redirected\n",compared);
    return 0;
}
