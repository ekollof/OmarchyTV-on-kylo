/* OFFLINE ARM/qemu differential only. Reads the local ELF, executes its
 * tab/profile/controller instructions in THIS process with controlled callees. Never
 * attach to Aurora or run platform/HAL init. No vendor bytes are embedded. */
#if !defined(__arm__)
#error "Build this optional firmware differential for ARM and run under qemu-arm"
#endif
#include "openaurora/tab_policy.h"
#include "openaurora/profile.h"
#include "openaurora/tab_events.h"
#include "openaurora/tab_dispatch.h"
#include "tab_prehandle_fixture.h"
#include "tab_leaf_fixture.h"
#include "settings_input_fixture.h"
#include "settings_navigation_fixture.h"
#include "settings_list_fixture.h"
#include "openaurora/svg_list_input.h"
#include "openaurora/svg_list_bodies.h"
#include "openaurora/svg_list_move.h"
#include "openaurora/svg_list_rows.h"
#include "openaurora/svg_list_scroll.h"
#include "openaurora/svg_list_impl.h"
#include "openaurora/svg_list_scrollbar.h"
#include "openaurora/svg_list_update.h"
#include "openaurora/svg_list_widgets.h"
#include "openaurora/svg_list_anim.h"
#include "svg_list_anim_fixture.h"
#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>
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
static struct {uint32_t base;size_t size;} mapped_regions[128];
static int mapped_range(const void *mapped,uint32_t base,uint32_t address,size_t size)
{
    if((uintptr_t)mapped!=base || address<base)return 0;
    for(unsigned i=0;i<128;++i)if(mapped_regions[i].size && mapped_regions[i].base==base) {
        size_t delta=address-base;
        return delta<=mapped_regions[i].size && size<=mapped_regions[i].size-delta;
    }
    return 0;
}
static unsigned char *stub_destination(void *mapped,uint32_t base,uint32_t address,size_t size)
{
    if(!mapped_range(mapped,base,address,size)) {
        fprintf(stderr,"reference stub outside its mapping: base=%x address=%x bytes=%zu\n",base,address,size);exit(1);
    }
    return (unsigned char *)mapped+(address-base);
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
        fprintf(stderr,"offline reference address unavailable: 0x%x+0x%zx; refusing fixed overwrite\n",address,length);exit(77);
    }
    /* Calls unmap their regions directly. A successful non-replacing mmap
     * proves any overlapping old registry entry has ceased to be live. */
    unsigned slot=128;
    for(unsigned i=0;i<128;++i) {
        if(mapped_regions[i].size && (uint64_t)address+length>mapped_regions[i].base &&
           (uint64_t)mapped_regions[i].base+mapped_regions[i].size>address)mapped_regions[i].size=0;
        if(!mapped_regions[i].size)slot=i;
    }
    assert(slot<128);mapped_regions[slot].base=address;mapped_regions[slot].size=length;
    return p;
}
static void executable(void *p,size_t n)
{ __builtin___clear_cache(p,(char *)p+n);assert(!mprotect(p,n,PROT_READ|PROT_EXEC)); }
static void thumb_stub(void *mapped,uint32_t base,uint32_t address,getter function)
{
    unsigned char *p=stub_destination(mapped,base,address,8);
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
static unsigned reference_strlen(void *p) { return (unsigned)strlen(p); }
static unsigned char *read_regular(const char *path,size_t *size)
{
    FILE *f=fopen(path,"rb");assert(f);struct stat st;
    assert(!fstat(fileno(f),&st) && S_ISREG(st.st_mode) && st.st_size>0 && st.st_size<128*1024*1024);
    *size=(size_t)st.st_size;unsigned char *data=malloc(*size);assert(data);
    assert(fread(data,1,*size,f)==*size && !fclose(f));return data;
}
static void check_retained_setters(const unsigned char *elf,size_t bytes)
{
    const struct {uint32_t address,length,crc,literal;} fragments[]={
        {0xccca0c,56,UINT32_C(0x6fea0252),0xccca40},
        {0xccca44,52,UINT32_C(0x5e86d1a0),0xccca74},
        {0xcccaac,52,UINT32_C(0x6b0e9511),0xcccadc},
    };
    unsigned char *mapped=page(0xccc000,4096);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;
    for(size_t i=0;i<sizeof fragments/sizeof *fragments;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(mapped+fragments[i].address-0xccc000,elf+off,fragments[i].length);
        memcpy(mapped+fragments[i].literal-0xccc000,&pointer,4);
    }
    executable(mapped,4096);
    typedef void (*setter)(void *,uint32_t);
    setter open,item,tab;
    pointer=0xccca0d;memcpy(&open,&pointer,sizeof open);
    pointer=0xccca45;memcpy(&item,&pointer,sizeof item);
    pointer=0xcccaad;memcpy(&tab,&pointer,sizeof tab);
    const uint32_t values[]={0,1,4,5,UINT32_C(0x80000000),INT32_MAX,UINT32_MAX};
    _Alignas(uint32_t) unsigned char demo[0x125c],expected[0x125c];unsigned compared=0;
    for(size_t i=0;i<sizeof values/sizeof *values;++i)for(unsigned raw=0;raw<256;++raw) {
        memset(demo,0xa5,sizeof demo);memset(expected,0xa5,sizeof expected);
        for(unsigned j=0;j<4;++j)expected[0xfd8+j]=(unsigned char)(values[i]>>(8*j));
        tab(demo,values[i]);assert(!memcmp(demo,expected,sizeof demo));
        for(unsigned j=0;j<4;++j)expected[0xfd0+j]=(unsigned char)(~values[i]>>(8*j));
        item(demo,~values[i]);assert(!memcmp(demo,expected,sizeof demo));
        expected[0x1259]=(unsigned char)raw;
        open(demo,raw);assert(!memcmp(demo,expected,sizeof demo));++compared;
    }
    assert(!munmap(mapped,4096));
    printf("ok offline shipped ARM retained setters: %u signed32/raw byte cases, exact fields and surrounding bytes preserved\n",compared);
}
static unsigned hotel_step_result,hotel_columns[3];
static unsigned hotel_step(void *p) { (void)p;return hotel_step_result; }
static unsigned hotel_column(void *p,int32_t index)
{ (void)p;assert(index>=0 && index<3);return hotel_columns[index]; }
static void check_hotel_rows(const unsigned char *elf,size_t bytes)
{
    unsigned char *mapped=page(0xbd2000,8192),*step_page=page(0x6b2000,4096),*column_page=page(0x6b4000,4096);
    size_t off=file_offset(elf,bytes,0xbd2f2c,272);
    assert(crc32(elf+off,272)==UINT32_C(0x90099888));memcpy(mapped+0xf2c,elf+off,272);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;memcpy(mapped+0x1038,&pointer,4);
    const uint32_t step_stub[]={UINT32_C(0xe59fc000),UINT32_C(0xe12fff1c),bits(hotel_step)};
    const uint32_t column_stub[]={UINT32_C(0xe59fc000),UINT32_C(0xe12fff1c),predicate_bits(hotel_column)};
    memcpy(step_page+0xb94,step_stub,sizeof step_stub);memcpy(column_page+0x2bc,column_stub,sizeof column_stub);
    executable(mapped,8192);executable(step_page,4096);executable(column_page,4096);
    typedef unsigned (*row_fn)(void *,void *);
    row_fn row;pointer=0xbd2f2d;memcpy(&row,&pointer,sizeof row);
    uint32_t types[]={1,4,1},targets[3],access[9]={3,0,0,0,0,0,0,0,0},query=0x1234;
    access[2]=(uint32_t)(uintptr_t)types;access[3]=(uint32_t)(uintptr_t)targets;
    _Alignas(uint32_t) unsigned char hotel[0x40],expected[0x40];
    targets[0]=(uint32_t)(uintptr_t)(hotel+0x1d);targets[1]=(uint32_t)(uintptr_t)(hotel+0x28);
    targets[2]=(uint32_t)(uintptr_t)(hotel+0x2d);
    const uint32_t values[]={0,1,2,255,UINT32_C(0x80000000),INT32_MAX,UINT32_MAX};unsigned compared=0;
    hotel_step_result=100; /* SQLITE_ROW, with integer conversion stubbed */
    for(size_t a=0;a<7;++a)for(size_t b=0;b<7;++b)for(size_t c=0;c<7;++c) {
        hotel_columns[0]=values[a];hotel_columns[1]=values[b];hotel_columns[2]=values[c];
        memset(hotel,0xa5,sizeof hotel);memset(expected,0xa5,sizeof expected);
        expected[0x1d]=(unsigned char)values[a];expected[0x2d]=(unsigned char)values[c];
        for(unsigned j=0;j<4;++j)expected[0x28+j]=(unsigned char)(values[b]>>(8*j));
        assert(row(&query,access)==1 && !memcmp(hotel,expected,sizeof hotel));++compared;
    }
    const unsigned no_row[]={101,5}; /* DONE/BUSY both leave member fields alone */
    for(size_t i=0;i<2;++i) {
        hotel_step_result=no_row[i];memcpy(expected,hotel,sizeof hotel);
        assert(!row(&query,access) && !memcmp(hotel,expected,sizeof hotel));++compared;
    }
    assert(!munmap(mapped,8192) && !munmap(step_page,4096) && !munmap(column_page,4096));
    printf("ok offline shipped ARM hotel row: %u integer store/no-row cases, byte/int32 widths and surrounding fields preserved\n",compared);
}
static void check_profile(const unsigned char *elf,size_t bytes,const char *path)
{
    const struct {uint32_t address,length,crc,guard_literal;} fragments[]={
        {0xc5e100,144,UINT32_C(0x39caf1e3),0xc5e188},
        {0xc5e190,108,UINT32_C(0x330d0938),0xc5e1f8},
        {0xc5e298,108,UINT32_C(0x56b95047),0xc5e300},
        {0xc5e304,76,UINT32_C(0x4c3a84f3),0xc5e34c},
    };
    unsigned char *mapped=page(0xc5e000,4096),*divide=page(0x1285000,4096);
    unsigned char *libc_page=page(0x6b0000,4096);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;
    for(size_t i=0;i<sizeof fragments/sizeof *fragments;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(mapped+fragments[i].address-0xc5e000,elf+off,fragments[i].length);
        memcpy(mapped+fragments[i].guard_literal-0xc5e000,&pointer,4);
    }
    size_t off=file_offset(elf,bytes,0x128547e,682);
    assert(crc32(elf+off,682)==UINT32_C(0x4c973710));memcpy(divide+0x47e,elf+off,682);
    const uint32_t strlen_stub[3]={UINT32_C(0xe59fc000),UINT32_C(0xe12fff1c),bits(reference_strlen)};
    memcpy(libc_page+0xa9c,strlen_stub,sizeof strlen_stub);
    executable(mapped,4096);executable(divide,4096);executable(libc_page,4096);
    typedef unsigned (*hash_fn)(void *,const char *);
    typedef unsigned (*section_fn)(void *,void *,const char *);
    typedef unsigned (*read_fn)(void *,void *,const char *,uint32_t *,uint32_t);
    hash_fn hash;section_fn section;read_fn read;
    pointer=0xc5e101;memcpy(&hash,&pointer,sizeof hash);
    pointer=0xc5e191;memcpy(&section,&pointer,sizeof section);
    pointer=0xc5e305;memcpy(&read,&pointer,sizeof read);
    unsigned comparisons=0;char key[3]={0};
    assert(hash(NULL,key)==oa_profile_hash(key));++comparisons;
    for(unsigned first=1;first<256;++first) {
        key[0]=(char)first;key[1]=0;
        assert(hash(NULL,key)==oa_profile_hash(key));++comparisons;
        for(unsigned second=1;second<256;++second) {
            key[1]=(char)second;assert(hash(NULL,key)==oa_profile_hash(key));++comparisons;
        }
    }
    size_t n;unsigned char *profile=read_regular(path,&n);assert(n>=0x44);
    unsigned config=section(NULL,profile+0x30,"Config");assert(config);
    const char *sections[]={"Connectivity","HotelTv","Missing"};
    const char *keys[]={"Connectivity","HotelMode","Missing"};
    const uint32_t defaults[]={0,1,UINT32_MAX};unsigned lookups=0;
    for(size_t s=0;s<3;++s)for(size_t k=0;k<3;++k)for(size_t d=0;d<3;++d) {
        unsigned found=section(NULL,(void *)(uintptr_t)config,sections[s]);
        uint32_t vendor=42,ours=43;unsigned success=read(NULL,(void *)(uintptr_t)found,keys[k],&vendor,defaults[d]);
        char full_path[80];assert(snprintf(full_path,sizeof full_path,"Config/%s",sections[s])>0);
        int present;assert(!oa_profile_u32(profile,n,full_path,keys[k],defaults[d],&ours,&present));
        assert(vendor==ours && success==(unsigned)present);++lookups;
    }
    struct oa_tab_profile snapshot;assert(!oa_tab_profile_from_sw(profile,n,&snapshot));
    printf("ok offline shipped ARM profile: %u signed-byte hashes, %u saved-profile lookups/defaults; connectivity=%u hotel=%u\n",
           comparisons,lookups,snapshot.connectivity_raw,snapshot.hotel_raw);
    free(profile);assert(!munmap(mapped,4096) && !munmap(divide,4096) && !munmap(libc_page,4096));
}
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
struct event_fixture {
    uint32_t open,buttons,present,item,update,key;
    unsigned count;
    struct {enum oa_tab_action action;uint32_t value;const void *message;} trace[16];
};
static struct event_fixture *event_fixture;
static _Alignas(uint32_t) unsigned char event_dialog[0x4c4];
static uint32_t event_dummy;
static void sync_event_dialog(void)
{
    event_dialog[0x4b6]=(unsigned char)event_fixture->buttons;
    uint32_t pointer=event_fixture->present?(uint32_t)(uintptr_t)&event_dummy:0;
    memcpy(event_dialog+0x4c0,&pointer,4);
    pointer=(uint32_t)(uintptr_t)&event_dummy;memcpy(event_dialog+0x484,&pointer,4);
}
static uint32_t event_query(void *p,enum oa_tab_query q)
{
    struct event_fixture *f=p;
    switch(q){case OA_TAB_OPEN_SUBMENU:return f->open;case OA_TAB_BUTTONS_ACTIVE:return f->buttons;case OA_TAB_CONTENT_PRESENT:return f->present;}
    assert(0);return 0;
}
static void event_action(void *p,enum oa_tab_action a,uint32_t argument,const void *message)
{
    struct event_fixture *f=p;assert(f->count<16);
    f->trace[f->count].action=a;f->trace[f->count].message=message;f->trace[f->count++].value=argument;
    if(a==OA_TAB_SET_ITEM)f->item=argument;
    if(a==OA_TAB_SET_OPEN_SUBMENU)f->open=argument;
    if(a==OA_TAB_UPDATE_ACTIVE_LIST && f->update) {
        f->buttons=f->update==1?0:255;f->present=f->update==1?1:0;
    }
    if(a==OA_TAB_HIDE_CONTENT){f->buttons=0;f->present=0;}
    if(a==OA_TAB_SHOW_CONTENT)f->present=1;
    sync_event_dialog();
}
static unsigned event_open(void *p) { (void)p;return event_fixture->open; }
static unsigned event_key(void *p) { (void)p;return event_fixture->key; }
static unsigned event_hide(void *p) { (void)p;event_action(event_fixture,OA_TAB_HIDE_CONTENT,0,NULL);return 0; }
static unsigned event_show(void *p) { (void)p;event_action(event_fixture,OA_TAB_SHOW_CONTENT,0,NULL);return 0; }
static unsigned event_focus(void *p) { (void)p;event_action(event_fixture,OA_TAB_FOCUS_CONTENT,0,NULL);return 0; }
static unsigned event_title(void *p) { (void)p;event_action(event_fixture,OA_TAB_UPDATE_TITLE,0,NULL);return 0; }
static unsigned event_update(void *p) { (void)p;event_action(event_fixture,OA_TAB_UPDATE_ACTIVE_LIST,0,NULL);return 0; }
static unsigned event_set_item(void *p,int32_t value) { (void)p;event_action(event_fixture,OA_TAB_SET_ITEM,(uint32_t)value,NULL);return 0; }
static unsigned event_set_open(void *p,int32_t value) { (void)p;event_action(event_fixture,OA_TAB_SET_OPEN_SUBMENU,(uint8_t)value,NULL);return 0; }
static unsigned event_group(void *p,int32_t message) { (void)p;event_action(event_fixture,OA_TAB_GROUP_KEY,event_fixture->key,(const void *)(uintptr_t)(uint32_t)message);return 0; }
static void predicate_stub(void *mapped,uint32_t base,uint32_t address,predicate function)
{
    unsigned char *p=stub_destination(mapped,base,address,8);const uint16_t insn[]={0x4b00,0x4718};
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target=predicate_bits(function);memcpy(p+4,&target,4);
}
static void check_tab_events(const unsigned char *elf,size_t bytes)
{
    unsigned char *tab=page(0xd6f000,4096),*demo=page(0xcd1000,4096);
    unsigned char *setters=page(0xccc000,4096),*group=page(0x1148000,8192),*settings=page(0xd56000,4096);
    const struct {uint32_t address,length,crc,literal;} fragments[]={
        {0xd6f874,128,UINT32_C(0x5d5b95db),0xd6f8f0},
        {0xd6fdc0,84,UINT32_C(0x1e26b4d6),0xd6fe10},
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;
    for(size_t i=0;i<2;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(tab+fragments[i].address-0xd6f000,elf+off,fragments[i].length);
        memcpy(tab+fragments[i].literal-0xd6f000,&pointer,4);
    }
    thumb_stub(tab,0xd6f000,0xd6f09c,event_hide);thumb_stub(tab,0xd6f000,0xd6f140,event_show);
    thumb_stub(tab,0xd6f000,0xd6f61c,event_focus);thumb_stub(tab,0xd6f000,0xd6faec,event_title);
    thumb_stub(demo,0xcd1000,0xcd1924,event_open);thumb_stub(group,0x1148000,0x11489e0,event_key);
    predicate_stub(group,0x1148000,0x114925c,event_group);
    predicate_stub(setters,0xccc000,0xccca44,event_set_item);predicate_stub(setters,0xccc000,0xccca0c,event_set_open);
    thumb_stub(settings,0xd56000,0xd56fe8,event_update);
    executable(tab,4096);executable(demo,4096);executable(setters,4096);executable(group,8192);executable(settings,4096);
    typedef unsigned (*handler)(void *,void *);handler native[2];
    pointer=0xd6f875;memcpy(&native[0],&pointer,sizeof pointer);pointer=0xd6fdc1;memcpy(&native[1],&pointer,sizeof pointer);
    const uint32_t opens[]={0,1,2,255},buttons[]={0,1,255},keys[]={0,0x20a,0x20b,0x20c,0x20d,0x251,UINT32_MAX};
    unsigned compared=0;
    for(size_t o=0;o<4;++o)for(size_t b=0;b<3;++b)for(unsigned present=0;present<2;++present)
    for(unsigned update=0;update<3;++update)for(size_t k=0;k<7;++k)for(unsigned method=0;method<2;++method) {
        struct event_fixture original={.open=opens[o],.buttons=buttons[b],.present=present,.item=109,.update=update,.key=keys[k]};
        struct event_fixture vendor=original,ours=original;
        event_fixture=&vendor;sync_event_dialog();assert(native[method](event_dialog,&event_dummy)==1);
        event_fixture=&ours;sync_event_dialog();struct oa_tab_event_ops ops={&ours,event_query,event_action,&event_dummy};
        assert(!(method?oa_tab_event_right_left(&ops,keys[k]):oa_tab_event_up_down(&ops,keys[k])));
        assert(vendor.open==ours.open && vendor.buttons==ours.buttons && vendor.present==ours.present && vendor.item==ours.item && vendor.count==ours.count);
        for(unsigned i=0;i<vendor.count;++i)assert(vendor.trace[i].action==ours.trace[i].action && vendor.trace[i].value==ours.trace[i].value && vendor.trace[i].message==ours.trace[i].message);
        ++compared;
    }
    event_fixture=NULL;
    assert(!munmap(tab,4096) && !munmap(demo,4096) && !munmap(setters,4096) && !munmap(group,8192) && !munmap(settings,4096));
    printf("ok offline shipped ARM tab events: %u handler cases, ordered actions and dynamic callback state match\n",compared);
}
static struct pre_fixture *pre_fixture;
static _Alignas(uint32_t) unsigned char pre_dialog[0x500];
static uint32_t pre_child,pre_image,pre_window,pre_dummy,pre_clone[4],pre_original;
static void pre_sync(void)
{
    pre_dialog[0x4b6]=(uint8_t)pre_fixture->q[OA_TP_BUTTONS_ACTIVE];
    uint32_t pointer=pre_fixture->q[OA_TP_PRESENT]?(uint32_t)(uintptr_t)&pre_child:0;
    memcpy(pre_dialog+0x4c0,&pointer,4);
    pointer=pre_fixture->q[OA_TP_CONTENT_TAB];memcpy(pre_dialog+0x4ac,&pointer,4);
}
static uint32_t pre_invoke(enum oa_tab_pre_action action,uint32_t arg,const void *message)
{ uint32_t r=pre_run(pre_fixture,action,arg,message);pre_sync();return r; }
#define PRE_GET(name,field) static unsigned name(void *p) { (void)p;return pre_fixture->q[field]; }
#define PRE_ACT(name,action) static unsigned name(void *p) { (void)p;return pre_invoke(action,0,NULL); }
#define PRE_SET(name,action) static unsigned name(void *p,int32_t arg) { (void)p;return pre_invoke(action,(uint32_t)arg,NULL); }
PRE_GET(pre_media,OA_TP_IN_MEDIA)
PRE_GET(pre_focused,OA_TP_FOCUSED)
PRE_GET(pre_circular,OA_TP_CIRCULAR)
PRE_GET(pre_jump,OA_TP_TAB_JUMP)
PRE_GET(pre_back,OA_TP_BACK_HANDLED)
PRE_GET(pre_index,OA_TP_INDEX)
PRE_GET(pre_small,OA_TP_SMALLEST)
PRE_GET(pre_big,OA_TP_BIGGEST)
PRE_ACT(pre_unfocus,OA_TP_UNFOCUS)
PRE_ACT(pre_hide,OA_TP_HIDE)
PRE_ACT(pre_show,OA_TP_SHOW)
PRE_ACT(pre_focus,OA_TP_FOCUS)
PRE_ACT(pre_title,OA_TP_UPDATE_TITLE)
PRE_ACT(pre_close,OA_TP_CLOSE_MODALS)
PRE_ACT(pre_release,OA_TP_RELEASE_IMAGE)
PRE_SET(pre_end,OA_TP_END_DIALOG)
PRE_SET(pre_buttons,OA_TP_BUTTONS)
PRE_SET(pre_select,OA_TP_SELECT_CHILD)
PRE_SET(pre_content_focus,OA_TP_CONTENT_FOCUS)
PRE_SET(pre_dialog_focus,OA_TP_DIALOG_FOCUS)
PRE_SET(pre_set_item,OA_TP_SET_ITEM)
#undef PRE_GET
#undef PRE_ACT
#undef PRE_SET
static unsigned pre_nop(void *p) { (void)p;return 0; }
static unsigned pre_allocate(void *p) { (void)p;return (uint32_t)(uintptr_t)pre_clone; }
static unsigned pre_ctor(void *p,int32_t key) { *(uint32_t *)p=(uint32_t)key;return (uint32_t)(uintptr_t)p; }
static unsigned pre_key(void *p) { return *(uint32_t *)p; }
static unsigned pre_description(void *p) { (void)p;return pre_fixture->q[OA_TP_VERTICAL_TABS]; }
static unsigned pre_group_focus(void *p) { (void)p;return pre_fixture->q[OA_TP_GROUP_HAS_FOCUS]?(uint32_t)(uintptr_t)&pre_window:0; }
static unsigned pre_window_id(void *p) { (void)p;uint32_t id=pre_fixture->q[OA_TP_GROUP_ID];pre_invoke(OA_TP_SET_CONTENT_TAB,id,NULL);return id; }
static unsigned pre_content_key(void *p,int32_t message) { (void)p;return pre_invoke(OA_TP_CONTENT_KEY,*(uint32_t *)(uintptr_t)(uint32_t)message,(const void *)(uintptr_t)(uint32_t)message); }
static unsigned pre_base(void *p,int32_t message) { (void)p;return pre_invoke(OA_TP_BASE_PREHANDLE,*(uint32_t *)(uintptr_t)(uint32_t)message,(const void *)(uintptr_t)(uint32_t)message); }
static unsigned pre_group(void *p,int32_t message) { (void)p;return pre_invoke(OA_TP_GROUP_KEY,*(uint32_t *)(uintptr_t)(uint32_t)message,NULL); }
static unsigned pre_right_left(void *p,int32_t message) { (void)p;return pre_invoke(OA_TP_RIGHT_LEFT,*(uint32_t *)(uintptr_t)(uint32_t)message,NULL); }
static unsigned pre_post(void *p,int32_t message) { (void)p;return pre_invoke(OA_TP_POST_KEY,*(uint32_t *)(uintptr_t)(uint32_t)message,NULL); }
static unsigned pre_push(void *p,int32_t message) { (void)p;return pre_invoke(OA_TP_PUSH_FRONT_KEY,*(uint32_t *)(uintptr_t)(uint32_t)message,NULL); }
static unsigned pre_interface(void *p) { (void)p;return (uint32_t)(uintptr_t)&pre_dummy; }
static unsigned pre_request(void *p) { (void)p;pre_invoke(OA_TP_REQUEST_IMAGE,0,NULL);return pre_fixture->image?(uint32_t)(uintptr_t)&pre_image:0; }
static void check_prehandle(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0xd70000,0xd6f000,0x1148000,0xc86000,0x10c5000,0xccb000,0xcd1000,0x113a000,0x6b0000,0xcdc000,0x979000,0x1152000};
    unsigned char *pages[12];size_t lengths[12];
    for(unsigned i=0;i<12;++i){lengths[i]=(i==2 || i==7)?8192:4096;pages[i]=page(bases[i],lengths[i]);}
    size_t off=file_offset(elf,bytes,0xd700a0,1200);assert(crc32(elf+off,1200)==UINT32_C(0x0bec24b5));
    memcpy(pages[0]+0xa0,elf+off,1200);uint32_t pointer=(uint32_t)(uintptr_t)&guard;memcpy(pages[0]+0x3c4,&pointer,4);
#define PG(index,address,fn) thumb_stub(pages[index],bases[index],address,fn)
#define PS(index,address,fn) predicate_stub(pages[index],bases[index],address,fn)
    PG(1,0xd6f304,pre_unfocus);PG(1,0xd6f09c,pre_hide);PG(1,0xd6f140,pre_show);PG(1,0xd6f61c,pre_focus);
    PG(1,0xd6faec,pre_title);PS(1,0xd6f028,pre_buttons);PS(1,0xd6fdc0,pre_right_left);
    PG(2,0x11489e0,pre_key);PS(2,0x114899c,pre_ctor);PG(2,0x114854c,pre_group_focus);
    PS(2,0x11485d4,pre_select);PS(2,0x114925c,pre_group);PS(2,0x11492a0,pre_base);
    PG(3,0xc86744,pre_nop);PG(3,0xc867f4,pre_nop);PG(3,0xc86554,pre_nop);
    PG(4,0x10c5504,pre_description);PS(4,0x10c5478,pre_end);PG(5,0xccb754,pre_media);
    PG(6,0xcd1a5c,pre_circular);PG(6,0xcd1a90,pre_jump);
    PG(7,0x113a3d0,pre_close);PS(7,0x113b748,pre_post);PS(7,0x113b7b8,pre_push);PG(7,0x113b958,pre_interface);
    /* operator new PLT is ARM; its callee only returns an owned test message. */
    const uint32_t alloc[]={UINT32_C(0xe59fc000),UINT32_C(0xe12fff1c),bits(pre_allocate)};
    memcpy(pages[8]+0x13c,alloc,sizeof alloc);PG(9,0xcdcf8c,pre_nop);
    PG(10,0x9791e0,pre_request);PG(11,0x11529e0,pre_window_id);
#undef PG
#undef PS
    for(unsigned i=0;i<12;++i)executable(pages[i],lengths[i]);
    uint32_t child_vtable[15]={0},dialog_vtable[8]={0},image_vtable[7]={0};
    child_vtable[4]=predicate_bits(pre_content_focus);child_vtable[7]=bits(pre_focused);
    child_vtable[8]=predicate_bits(pre_content_key);child_vtable[10]=bits(pre_index);
    child_vtable[11]=bits(pre_big);child_vtable[12]=bits(pre_small);child_vtable[13]=bits(pre_back);child_vtable[14]=predicate_bits(pre_set_item);
    dialog_vtable[7]=predicate_bits(pre_dialog_focus);image_vtable[6]=bits(pre_release);
    pre_child=(uint32_t)(uintptr_t)child_vtable;pre_image=(uint32_t)(uintptr_t)image_vtable;
    pointer=(uint32_t)(uintptr_t)dialog_vtable;memcpy(pre_dialog,&pointer,4);
    pointer=(uint32_t)(uintptr_t)&pre_dummy;memcpy(pre_dialog+4,&pointer,4);memcpy(pre_dialog+0x484,&pointer,4);
    typedef unsigned (*handler)(void *,void *);handler native;pointer=0xd700a1;memcpy(&native,&pointer,sizeof native);
    const uint32_t keys[]={0x20c,0x20d,0x22d,0x251,0x210,0x21f,0x28b,0x20a,0x20b,0x25b,0x2a7,0x20e,0x230,0x24c,0x25d,0x274,0,UINT32_MAX};
    unsigned compared=0;
    for(unsigned mask=0;mask<1024;++mask)for(unsigned result=0;result<2;++result)
    for(unsigned activation=0;activation<3;++activation)for(size_t k=0;k<18;++k) {
        struct pre_fixture initial={.child_result=result?9:0,.base_result=result?7:0,.group_result=result?11:0,
            .image=result,.jump_present=(mask>>9)&1,.show_success=1,.allow_focus=result};
        initial.q[OA_TP_VERTICAL_TABS]=mask&1;initial.q[OA_TP_PRESENT]=(mask>>1)&1;
        initial.q[OA_TP_FOCUSED]=(mask&4)?(result?255:1):0;initial.q[OA_TP_IN_MEDIA]=(mask>>3)&1;
        initial.q[OA_TP_GROUP_HAS_FOCUS]=(mask>>4)&1;initial.q[OA_TP_CIRCULAR]=(mask>>5)&1;
        initial.q[OA_TP_TAB_JUMP]=(mask>>6)&1;initial.q[OA_TP_BACK_HANDLED]=(mask>>7)&1;
        initial.q[OA_TP_INDEX]=(mask&256)?(keys[k]==0x20b?5:0):2;
        initial.q[OA_TP_SMALLEST]=0;initial.q[OA_TP_BIGGEST]=5;initial.q[OA_TP_BUTTONS_ACTIVE]=activation==2?255:activation;
        initial.q[OA_TP_GROUP_ID]=result?UINT32_MAX:4;initial.q[OA_TP_CONTENT_TAB]=1;
        int early=keys[k]==0x25d || keys[k]==0x274 || ((keys[k]==0x230 || keys[k]==0x24c) && initial.q[OA_TP_IN_MEDIA]);
        if(!initial.q[OA_TP_PRESENT] && (initial.q[OA_TP_FOCUSED] || (!early && (initial.q[OA_TP_VERTICAL_TABS] || initial.q[OA_TP_CIRCULAR]))))continue;
        struct pre_fixture vendor=initial,ours=initial;pre_original=keys[k];
        pre_fixture=&vendor;pre_sync();unsigned native_handled=native(pre_dialog,&pre_original);
        pre_fixture=&ours;pre_sync();struct oa_tab_pre_ops ops={&ours,pre_query,pre_run,&pre_original};int handled=-1;
        int error=oa_tab_prehandle(&ops,keys[k],&handled);
        if(error || handled!=(native_handled!=0) || vendor.count!=ours.count) {
            fprintf(stderr,"pre mismatch mask=%u result=%u key=%x error=%d handled=%d native=%x counts=%u/%u\n",mask,result,keys[k],error,handled,native_handled,vendor.count,ours.count);exit(1);
        }
        for(unsigned i=0;i<vendor.count;++i)if(vendor.trace[i].action!=ours.trace[i].action || vendor.trace[i].argument!=ours.trace[i].argument || vendor.trace[i].message!=ours.trace[i].message) {
            fprintf(stderr,"pre action mismatch mask=%u result=%u key=%x at=%u action=%u/%u arg=%u/%u\n",mask,result,keys[k],i,(unsigned)vendor.trace[i].action,(unsigned)ours.trace[i].action,vendor.trace[i].argument,ours.trace[i].argument);exit(1);
        }
        assert(!memcmp(vendor.q,ours.q,sizeof vendor.q));++compared;
    }
    pre_fixture=NULL;for(unsigned i=0;i<12;++i)assert(!munmap(pages[i],lengths[i]));
    printf("ok offline shipped ARM pre-handler: %u valid-tree cases, action/message/state and handled truth match\n",compared);
}
struct dispatch_fixture {
    uint32_t result;
    enum oa_tab_dispatch_target route;
    unsigned count,pre_calls;
    const void *message;
    struct pre_fixture pre;
};
static struct dispatch_fixture *dispatch_fixture;
struct dispatch_message { uint32_t vtable,type,id; };
static uint32_t dispatch_invoke(void *context,enum oa_tab_dispatch_target route,const void *message)
{
    struct dispatch_fixture *f=context;
    f->route=route;++f->count;f->message=message;return f->result;
}
#define DISPATCH_STUB(name,route) static unsigned name(void *p,int32_t message) \
{ (void)p;return dispatch_invoke(dispatch_fixture,route,(const void *)(uintptr_t)(uint32_t)message); }
DISPATCH_STUB(dispatch_menu,OA_TD_BASE_MENU)
DISPATCH_STUB(dispatch_up_down,OA_TD_UP_DOWN)
DISPATCH_STUB(dispatch_right_left,OA_TD_RIGHT_LEFT)
DISPATCH_STUB(dispatch_volume,OA_TD_VOLUME)
DISPATCH_STUB(dispatch_quit,OA_TD_QUIT_IGNORE)
DISPATCH_STUB(dispatch_guide,OA_TD_GUIDE)
DISPATCH_STUB(dispatch_digit,OA_TD_DIGIT)
DISPATCH_STUB(dispatch_removed,OA_TD_REMOVED)
DISPATCH_STUB(dispatch_base,OA_TD_BASE)
#undef DISPATCH_STUB
static unsigned dispatch_type(void *p) { return ((struct dispatch_message *)p)->type; }
static unsigned dispatch_id(void *p) { return ((struct dispatch_message *)p)->id; }
static unsigned dispatch_pre(void *p,int32_t message)
{
    (void)p;++dispatch_fixture->pre_calls;
    const struct dispatch_message *event=(const void *)(uintptr_t)(uint32_t)message;
    struct oa_tab_pre_ops ops={&dispatch_fixture->pre,pre_query,pre_run,event};int handled=-1;
    assert(!oa_tab_prehandle(&ops,event->id,&handled));return (unsigned)handled;
}
static void check_dispatch(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0xd6f000,0xcc4000,0xd56000,0xd6e000,0x1149000};
    const size_t lengths[]={8192,4096,8192,4096,4096};unsigned char *pages[5];
    for(unsigned i=0;i<5;++i)pages[i]=page(bases[i],lengths[i]);
    const struct {unsigned page;uint32_t address,length,crc,literal;} fragments[]={
        {0,0xd6fe14,652,UINT32_C(0x050d1bd2),0xd7009c},
        {1,0xcc4f38,96,UINT32_C(0x90529f1b),0xcc4f94},
        {4,0x114925c,68,UINT32_C(0xeb6eb524),0x114929c}
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;
    for(unsigned i=0;i<3;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(pages[fragments[i].page]+fragments[i].address-bases[fragments[i].page],elf+off,fragments[i].length);
        memcpy(pages[fragments[i].page]+fragments[i].literal-bases[fragments[i].page],&pointer,4);
    }
    predicate_stub(pages[0],bases[0],0xd6f874,dispatch_up_down);
    predicate_stub(pages[0],bases[0],0xd6fdc0,dispatch_right_left);
    predicate_stub(pages[2],bases[2],0xd56d38,dispatch_menu);
    predicate_stub(pages[2],bases[2],0xd56c94,dispatch_quit);
    predicate_stub(pages[2],bases[2],0xd56e5c,dispatch_guide);
    predicate_stub(pages[2],bases[2],0xd57238,dispatch_base);
    predicate_stub(pages[3],bases[3],0xd6edb4,dispatch_volume);
    predicate_stub(pages[3],bases[3],0xd6ee68,dispatch_digit);
    predicate_stub(pages[3],bases[3],0xd6ef0c,dispatch_removed);
    for(unsigned i=0;i<5;++i)executable(pages[i],lengths[i]);
    uint32_t message_vtable[4]={0,0,bits(dispatch_type),bits(dispatch_id)};
    struct dispatch_message message={(uint32_t)(uintptr_t)message_vtable,0,0};
    typedef unsigned (*handler)(void *,void *);handler native,bridge;
    pointer=0xd6fe15;memcpy(&native,&pointer,sizeof native);
    pointer=0x114925d;memcpy(&bridge,&pointer,sizeof bridge);
    uint32_t dialog_vtable[6]={0,0,0,0,0xd6fe15,predicate_bits(dispatch_pre)};
    uint32_t dialog=(uint32_t)(uintptr_t)dialog_vtable;
    const uint32_t types[]={0,0x1004,0x11004,UINT32_MAX},results[]={0,1,255,UINT32_MAX};
    const uint32_t extra[]={0x10000209,0x10000,INT32_MAX,UINT32_MAX};unsigned compared=0,bridges=0;
    for(unsigned t=0;t<4;++t)for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4100;++k) {
        message.type=types[t];message.id=k<4096?k:extra[k-4096];
        struct dispatch_fixture vendor={.result=results[r]},ours=vendor;
        dispatch_fixture=&vendor;unsigned expected=native(&dialog,&message);
        struct oa_tab_dispatch_ops ops={&ours,dispatch_invoke,&message};int handled=-1;
        assert(!oa_tab_dispatch(&ops,message.type,message.id,&handled));
        if(handled!=(expected!=0) || vendor.count!=1 || ours.count!=1 || vendor.route!=ours.route ||
           vendor.message!=ours.message || ours.message!=&message) {
            fprintf(stderr,"dispatch mismatch type=%x id=%x result=%x native=%x handled=%d route=%u/%u\n",
                    message.type,message.id,results[r],expected,handled,(unsigned)vendor.route,(unsigned)ours.route);exit(1);
        }
        ++compared;
    }
    /* Join verification executes the shipped caller/map/matcher with the
     * separately differential-tested C pre-handler as its virtual method.
     * This tests ordering/identity, not new independent pre-handler evidence. */
    message.type=0x1004;
    for(unsigned p=0;p<4;++p)for(unsigned r=0;r<4;++r)for(unsigned k=0;k<1024;++k) {
        message.id=k;struct dispatch_fixture vendor={.result=results[r]},ours;
        vendor.pre.base_result=results[p];ours=vendor;
        dispatch_fixture=&vendor;unsigned expected=bridge(&dialog,&message);
        struct oa_tab_dispatch_ops ops={&ours,dispatch_invoke,&message};
        struct oa_tab_pre_ops pre={&ours.pre,pre_query,pre_run,&message};int handled=-1;
        assert(!oa_tab_handle_key(&pre,&ops,k,&handled));
        assert(handled==(expected!=0) && vendor.pre_calls==1 && vendor.count==ours.count);
        if(ours.count)assert(vendor.route==ours.route && vendor.message==ours.message && ours.message==&message);
        assert(!memcmp(&vendor.pre,&ours.pre,sizeof ours.pre));++bridges;
    }
    dispatch_fixture=NULL;for(unsigned i=0;i<5;++i)assert(!munmap(pages[i],lengths[i]));
    printf("ok offline shipped ARM tab dispatch: %u type/id/result routes, %u pre/fallback joins; original identity and handled truth match\n",compared,bridges);
}
static struct leaf_fixture *native_leaf_fixture;
static enum oa_tab_leaf_kind native_leaf_kind;
static _Alignas(uint32_t) unsigned char native_leaf_dialog[0x500];
static uint32_t native_leaf_child,native_leaf_dummy,native_leaf_event,native_leaf_clone[23],native_leaf_nav;
static uint32_t native_leaf_key_result;
static unsigned native_leaf_showing(void *p) { (void)p;return native_leaf_fixture->q[OA_TL_CONTENT_SHOWING]; }
static unsigned native_leaf_media(void *p) { (void)p;return native_leaf_fixture->q[OA_TL_IN_MEDIA]; }
static void native_leaf_observe_digits(void)
{
    uint32_t digits;memcpy(&digits,native_leaf_dialog+0x4b0,4);
    leaf_run(native_leaf_fixture,OA_TL_SET_DIGITS,digits,NULL);
}
static unsigned native_leaf_end(void *p,int32_t value)
{
    assert(p==native_leaf_dialog);
    assert(native_leaf_kind!=OA_TL_DIGIT);
    leaf_run(native_leaf_fixture,OA_TL_END_DIALOG,(uint32_t)value,NULL);return UINT32_MAX;
}
static unsigned native_leaf_svg_end(void *p,int32_t value)
{
    assert(p==native_leaf_dialog && native_leaf_kind==OA_TL_DIGIT);
    native_leaf_observe_digits();
    leaf_run(native_leaf_fixture,OA_TL_SVG_END_DIALOG,(uint32_t)value,NULL);return UINT32_MAX;
}
static unsigned native_leaf_reset(void *p)
{ (void)p;leaf_run(native_leaf_fixture,OA_TL_RESET_PICTURE_INDEX,0,NULL);return UINT32_MAX; }
static unsigned native_leaf_key(void *p) { return *(uint32_t *)p; }
static unsigned native_leaf_content(void *p,int32_t event)
{
    (void)p;assert((uintptr_t)(uint32_t)event==(uintptr_t)&native_leaf_event);
    leaf_run(native_leaf_fixture,OA_TL_CONTENT_KEY,native_leaf_event,&native_leaf_event);return native_leaf_key_result;
}
static unsigned native_leaf_allocate(void *size)
{
    assert((uintptr_t)size==(native_leaf_kind==OA_TL_GUIDE?0x5c:0x10));
    return (uint32_t)(uintptr_t)native_leaf_clone;
}
static unsigned native_leaf_key_ctor(void *p,int32_t key)
{ assert(p==native_leaf_clone);*(uint32_t *)p=(uint32_t)key;native_leaf_nav=0;return (uint32_t)(uintptr_t)p; }
static unsigned native_leaf_nav_ctor(void *p,int32_t code)
{ assert(p==native_leaf_clone);*(uint32_t *)p=(uint32_t)code;native_leaf_nav=1;return (uint32_t)(uintptr_t)p; }
static unsigned native_leaf_push(void *p,int32_t event)
{
    (void)p;assert((uintptr_t)(uint32_t)event==(uintptr_t)native_leaf_clone);
    leaf_run(native_leaf_fixture,native_leaf_nav?OA_TL_PUSH_NAVIGATION:OA_TL_PUSH_KEY,native_leaf_clone[0],NULL);return UINT32_MAX;
}
typedef unsigned (*leaf_handler)(void *,void *);
static void compare_leaf(leaf_handler native,enum oa_tab_leaf_kind kind,uint32_t key,struct leaf_fixture initial)
{
    struct leaf_fixture vendor=initial,ours=initial;native_leaf_fixture=&vendor;native_leaf_kind=kind;native_leaf_event=key;
    memcpy(native_leaf_dialog+0x4b0,&initial.q[OA_TL_DIGITS],4);
    unsigned result=native(native_leaf_dialog,&native_leaf_event);
    if(kind==OA_TL_DIGIT && !vendor.count)native_leaf_observe_digits();
    struct oa_tab_leaf_ops ops={&ours,leaf_query,leaf_run,&native_leaf_event};int handled=-1;
    assert(!oa_tab_leaf(&ops,kind,key,&handled));
    if(handled!=(result!=0) || vendor.count!=ours.count || memcmp(vendor.q,ours.q,sizeof ours.q)) {
        fprintf(stderr,"leaf mismatch kind=%u key=%x digits=%x result=%x handled=%d counts=%u/%u\n",
                (unsigned)kind,key,initial.q[OA_TL_DIGITS],result,handled,vendor.count,ours.count);exit(1);
    }
    for(unsigned i=0;i<ours.count;++i)assert(vendor.trace[i].action==ours.trace[i].action && vendor.trace[i].argument==ours.trace[i].argument && vendor.trace[i].message==ours.trace[i].message);
}
static void check_leaf(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0xd6e000,0xd56000,0x1285000,0x1148000,0x113b000,0xccc000,0xccb000,0x6b0000,0xd0e000,0x10c5000};
    unsigned char *pages[10];for(unsigned i=0;i<10;++i)pages[i]=page(bases[i],4096);
    const struct {unsigned page;uint32_t address,length,crc,literal;} fragments[]={
        {0,0xd6edb4,76,UINT32_C(0x79a58c0c),0xd6edfc},
        {0,0xd6ee68,108,UINT32_C(0x4a7036bc),0xd6eed0},
        {0,0xd6ef0c,116,UINT32_C(0x8db6171a),0xd6ef7c},
        {1,0xd56c94,108,UINT32_C(0x98417176),0xd56cfc},
        {1,0xd56d38,132,UINT32_C(0x42e4691a),0xd56db8},
        {1,0xd56e5c,96,UINT32_C(0x8d84d9bd),0xd56eb8},
        {2,0x128547e,682,UINT32_C(0x4c973710),0}
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;
    for(unsigned i=0;i<7;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(pages[fragments[i].page]+fragments[i].address-bases[fragments[i].page],elf+off,fragments[i].length);
        if(fragments[i].literal)memcpy(pages[fragments[i].page]+fragments[i].literal-bases[fragments[i].page],&pointer,4);
    }
    thumb_stub(pages[3],bases[3],0x11489e0,native_leaf_key);
    predicate_stub(pages[3],bases[3],0x114899c,native_leaf_key_ctor);
    predicate_stub(pages[4],bases[4],0x113b7b8,native_leaf_push);
    thumb_stub(pages[5],bases[5],0xccccb4,native_leaf_reset);
    thumb_stub(pages[6],bases[6],0xccb754,native_leaf_media);
    predicate_stub(pages[8],bases[8],0xd0e9b8,native_leaf_nav_ctor);
    predicate_stub(pages[9],bases[9],0x10c5478,native_leaf_svg_end);
    const uint32_t alloc[]={UINT32_C(0xe59fc000),UINT32_C(0xe12fff1c),bits(native_leaf_allocate)};
    memcpy(pages[7]+0x13c,alloc,sizeof alloc);
    for(unsigned i=0;i<10;++i)executable(pages[i],4096);
    uint32_t child_vtable[9]={0},dialog_vtable[31]={0};
    child_vtable[6]=bits(native_leaf_showing);child_vtable[8]=predicate_bits(native_leaf_content);
    native_leaf_child=(uint32_t)(uintptr_t)child_vtable;
    dialog_vtable[30]=predicate_bits(native_leaf_end);
    pointer=(uint32_t)(uintptr_t)dialog_vtable;memcpy(native_leaf_dialog,&pointer,4);
    pointer=(uint32_t)(uintptr_t)&native_leaf_child;memcpy(native_leaf_dialog+0x4c0,&pointer,4);
    pointer=(uint32_t)(uintptr_t)&native_leaf_dummy;memcpy(native_leaf_dialog+4,&pointer,4);memcpy(native_leaf_dialog+0x484,&pointer,4);
    const uint32_t entries[]={0xd6edb5,0xd6ee69,0xd56d39,0xd56c95,0xd56e5d,0xd6ef0d};leaf_handler native[6];
    for(unsigned i=0;i<6;++i)memcpy(&native[i],&entries[i],4);
    const uint32_t truth[]={0,1,255},results[]={0,1,UINT32_MAX},extra[]={0x10210,0x80000000,INT32_MAX,UINT32_MAX};
    unsigned compared=0;
    for(unsigned kind=0;kind<6;++kind)if(kind!=OA_TL_DIGIT)
    for(unsigned s=0;s<3;++s)for(unsigned m=0;m<3;++m)for(unsigned r=0;r<3;++r)for(unsigned k=0;k<1028;++k) {
        struct leaf_fixture initial={.q={1,truth[s],truth[m],0}};native_leaf_key_result=results[r];
        compare_leaf(native[kind],(enum oa_tab_leaf_kind)kind,k<1024?k:extra[k-1024],initial);++compared;
    }
    for(unsigned previous=0;previous<10000;++previous)for(unsigned digit=0;digit<10;++digit) {
        struct leaf_fixture initial={.q={1,0,0,previous}};
        compare_leaf(native[OA_TL_DIGIT],OA_TL_DIGIT,0x200+digit,initial);++compared;
    }
    const uint32_t raw[]={0,UINT32_MAX,INT32_MAX,0x80000000,429496729,429496730,214748364,214748365,0x450,0x1275};
    const uint32_t keys[]={0,0x1ff,0x200,0x209,0x20a,INT32_MAX,0x80000000,UINT32_MAX};
    for(unsigned r=0;r<10;++r)for(unsigned k=0;k<8;++k) {
        struct leaf_fixture initial={.q={1,0,0,raw[r]}};
        compare_leaf(native[OA_TL_DIGIT],OA_TL_DIGIT,keys[k],initial);++compared;
    }
    native_leaf_fixture=NULL;for(unsigned i=0;i<10;++i)assert(!munmap(pages[i],4096));
    printf("ok offline shipped ARM tab leaves: %u cases, six bodies and real signed division; logical writes/actions, message identity and handled truth match\n",compared);
}
static struct {enum oa_settings_route route;unsigned count;const void *message;uint32_t result;} settings_map;
static unsigned settings_map_call(enum oa_settings_route route,const void *message)
{ settings_map.route=route;++settings_map.count;settings_map.message=message;return settings_map.result; }
#define SETTINGS_MAP(name,target) static unsigned name(void *p,int32_t message) \
{ (void)p;return settings_map_call(target,(const void *)(uintptr_t)(uint32_t)message); }
SETTINGS_MAP(settings_map_menu,OA_SD_MENU)
SETTINGS_MAP(settings_map_rl,OA_SD_RIGHT_LEFT)
SETTINGS_MAP(settings_map_ud,OA_SD_UP_DOWN)
SETTINGS_MAP(settings_map_select,OA_SD_SELECT)
SETTINGS_MAP(settings_map_quit,OA_SD_QUIT)
SETTINGS_MAP(settings_map_digit,OA_SD_DIGIT)
SETTINGS_MAP(settings_map_guide,OA_SD_GUIDE)
SETTINGS_MAP(settings_map_base,OA_SD_BASE)
#undef SETTINGS_MAP
static unsigned settings_map_mute(void *p) { (void)p;return settings_map_call(OA_SD_MUTE,NULL); }
static uint32_t settings_map_run(void *p,enum oa_settings_route target,const void *message)
{ (void)p;return settings_map_call(target,message); }
static void check_settings_map(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0xd56000,0xcc4000,0xcd8000,0xe86000};
    const size_t lengths[]={8192,4096,4096,4096};unsigned char *pages[4];
    for(unsigned i=0;i<4;++i)pages[i]=page(bases[i],lengths[i]);
    size_t off=file_offset(elf,bytes,0xd57238,644);assert(crc32(elf+off,644)==UINT32_C(0x8c5f1f20));
    memcpy(pages[0]+0x1238,elf+off,644);
    off=file_offset(elf,bytes,0xcc4f38,96);assert(crc32(elf+off,96)==UINT32_C(0x90529f1b));memcpy(pages[1]+0xf38,elf+off,96);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;memcpy(pages[0]+0x14b8,&pointer,4);memcpy(pages[1]+0xf94,&pointer,4);
    predicate_stub(pages[0],bases[0],0xd56d38,settings_map_menu);predicate_stub(pages[0],bases[0],0xd570bc,settings_map_rl);
    predicate_stub(pages[0],bases[0],0xd56b5c,settings_map_ud);predicate_stub(pages[0],bases[0],0xd56bb0,settings_map_select);
    predicate_stub(pages[0],bases[0],0xd56c94,settings_map_quit);predicate_stub(pages[0],bases[0],0xd56bf4,settings_map_digit);
    predicate_stub(pages[0],bases[0],0xd56e5c,settings_map_guide);predicate_stub(pages[3],bases[3],0xe86d38,settings_map_base);
    thumb_stub(pages[2],bases[2],0xcd82a8,settings_map_mute);
    for(unsigned i=0;i<4;++i)executable(pages[i],lengths[i]);
    uint32_t vtable[4]={0,0,bits(dispatch_type),bits(dispatch_id)};
    struct dispatch_message message={(uint32_t)(uintptr_t)vtable,0,0};
    _Alignas(uint32_t) unsigned char dialog[0x488]={0};uint32_t dummy=0;
    pointer=(uint32_t)(uintptr_t)&dummy;memcpy(dialog+0x484,&pointer,4);
    leaf_handler native;pointer=0xd57239;memcpy(&native,&pointer,4);
    const uint32_t types[]={0,0x1004,0x11004,UINT32_MAX},results[]={0,1,255,UINT32_MAX};
    const uint32_t extra[]={0x10222,0x10000,INT32_MAX,UINT32_MAX};unsigned compared=0;
    for(unsigned t=0;t<4;++t)for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4100;++k) {
        message.type=types[t];message.id=k<4096?k:extra[k-4096];
        settings_map.count=0;settings_map.result=results[r];unsigned expected=native(dialog,&message);
        assert(settings_map.count==1);enum oa_settings_route target=settings_map.route;const void *original=settings_map.message;
        settings_map.count=0;struct oa_settings_dispatch_ops ops={NULL,settings_map_run,&message};int handled=-1;
        assert(!oa_settings_dispatch(&ops,message.type,message.id,&handled));
        assert(settings_map.count==1 && settings_map.route==target && settings_map.message==original && handled==(expected!=0));++compared;
    }
    for(unsigned i=0;i<4;++i)assert(!munmap(pages[i],lengths[i]));
    printf("ok offline shipped ARM settings map: %u type/id/result cases, qualified routes, mute truth and original identity match\n",compared);
}
static struct settings_fixture *native_settings;
static _Alignas(uint32_t) unsigned char settings_dialog[0x500],settings_item[16];
static uint32_t settings_event,settings_items[3],settings_dummy,settings_sentinel;
static const char settings_other_description[]="BLANK"; /* same text, distinct source-token identity */
static unsigned settings_nop(void *p) { (void)p;return UINT32_MAX; }
static unsigned settings_key(void *p) { return *(uint32_t *)p; }
static unsigned settings_ctor(void *p,int32_t key) { *(uint32_t *)p=(uint32_t)key;return (uint32_t)(uintptr_t)p; }
static unsigned settings_interface(void *p) { (void)p;return (uint32_t)(uintptr_t)&settings_dummy; }
static unsigned settings_mirror(void *p) { (void)p;return native_settings->q[OA_SI_MIRROR]; }
static unsigned settings_selected(void *p)
{ assert(p==settings_dialog+0x17c);return native_settings->q[OA_SI_SELECTED_ID]; }
static unsigned settings_index(void *p,int32_t id)
{ assert(p==settings_dialog+0x17c);return settings_query(native_settings,OA_SI_ITEM_INDEX,(uint32_t)id); }
static unsigned settings_empty(void *p)
{ assert(p==settings_dialog+0x120);return native_settings->q[OA_SI_TEXT_EMPTY]; }
static unsigned settings_record(enum oa_settings_action action,uint32_t arg,const void *message)
{ settings_run(native_settings,action,arg,message);return UINT32_MAX; }
static unsigned settings_list_key(void *p,int32_t message)
{
    assert(p==settings_dialog+0x17c && (uintptr_t)(uint32_t)message==(uintptr_t)&settings_event);
    return settings_record(OA_SI_LIST_KEY,settings_event,&settings_event);
}
static unsigned settings_navigate(void *p,int32_t message)
{
    assert(p==settings_dialog);const void *original=(const void *)(uintptr_t)(uint32_t)message;
    return settings_record(OA_SI_NAVIGATE,*(const uint32_t *)original,original==&settings_event?original:NULL);
}
static unsigned settings_update(void *p) { assert(p==settings_dialog);return settings_record(OA_SI_UPDATE_SETTINGS,0,NULL); }
static unsigned settings_display(void *p) { (void)p;return settings_record(OA_SI_UPDATE_DISPLAY,0,NULL); }
static unsigned settings_info(void *p) { assert(p==settings_dialog);return settings_record(OA_SI_ADD_MENU_INFO,0,NULL); }
static unsigned settings_mute(void *p) { (void)p;return settings_record(OA_SI_TOGGLE_MUTE,0,NULL); }
static unsigned settings_description(void *p,int32_t text)
{
    assert(p==settings_dialog+0x120 && (uint32_t)text!=settings_sentinel);
    return settings_record(OA_SI_ASSIGN_DESCRIPTION,native_settings->q[OA_SI_ITEM_INDEX],NULL);
}
static unsigned settings_label(void *p,int32_t text)
{
    assert(p==settings_dialog+0x120 && (uintptr_t)(uint32_t)text==(uintptr_t)(settings_item+4));
    return settings_record(OA_SI_ASSIGN_LABEL,native_settings->q[OA_SI_ITEM_INDEX],NULL);
}
static void check_settings_input(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0xd56000,0x1148000,0xd50000,0xd21000,0xc86000,0x113b000,0x97b000,0xcdc000,0xcd8000};
    const size_t lengths[]={8192,8192,4096,4096,4096,4096,4096,4096,4096};unsigned char *pages[9];
    for(unsigned i=0;i<9;++i)pages[i]=page(bases[i],lengths[i]);
    const struct {uint32_t address,length,crc,literal;} fragments[]={
        {0xd56b5c,84,UINT32_C(0xa0f13041),0xd56bac},
        {0xd56bb0,68,UINT32_C(0x586acb19),0xd56bf0},
        {0xd56bf4,160,UINT32_C(0xf8fa89c9),0xd56c8c},
        {0xd570bc,380,UINT32_C(0x244af783),0xd57230},
        {0xd56d00,56,UINT32_C(0x6b63d6ad),0xd56d34}
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;leaf_handler native[5];
    for(unsigned i=0;i<5;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(pages[0]+fragments[i].address-bases[0],elf+off,fragments[i].length);
        memcpy(pages[0]+fragments[i].literal-bases[0],&pointer,4);
        uint32_t entry=fragments[i].address|1;memcpy(&native[i],&entry,4);
    }
    memcpy(&settings_sentinel,pages[0]+0xc90,4);uint32_t other_sentinel;memcpy(&other_sentinel,pages[0]+0x1234,4);assert(settings_sentinel==other_sentinel);
    predicate_stub(pages[0],bases[0],0xd56a24,settings_navigate);
    predicate_stub(pages[1],bases[1],0x114925c,settings_list_key);predicate_stub(pages[1],bases[1],0x114899c,settings_ctor);
    thumb_stub(pages[1],bases[1],0x11489e0,settings_key);
    thumb_stub(pages[2],bases[2],0xd50ce0,settings_selected);predicate_stub(pages[2],bases[2],0xd50be4,settings_index);
    thumb_stub(pages[3],bases[3],0xd21488,settings_info);thumb_stub(pages[4],bases[4],0xc86c60,settings_empty);
    predicate_stub(pages[4],bases[4],0xc86a70,settings_description);predicate_stub(pages[4],bases[4],0xc86b00,settings_label);
    thumb_stub(pages[5],bases[5],0x113baf8,settings_display);thumb_stub(pages[5],bases[5],0x113b958,settings_interface);
    thumb_stub(pages[6],bases[6],0x97b0ec,settings_mirror);thumb_stub(pages[7],bases[7],0xcdcf8c,settings_nop);
    thumb_stub(pages[8],bases[8],0xcd82a8,settings_mute);
    for(unsigned i=0;i<9;++i)executable(pages[i],lengths[i]);
    uint32_t vtable[34]={0};vtable[33]=bits(settings_update);
    pointer=(uint32_t)(uintptr_t)vtable;memcpy(settings_dialog,&pointer,4);
    pointer=(uint32_t)(uintptr_t)&settings_dummy;memcpy(settings_dialog+4,&pointer,4);memcpy(settings_dialog+0x484,&pointer,4);
    pointer=(uint32_t)(uintptr_t)settings_items;memcpy(settings_dialog+0x474,&pointer,4);
    const uint32_t values[]={0,1,255},indices[]={UINT32_MAX,0x80000000,0,2};
    const uint32_t keys[]={0,0x20a,0x20b,0x20c,0x20d,0x20e,0x2db,0x2dc,0x2da,UINT32_MAX};unsigned compared=0;
    for(unsigned kind=0;kind<5;++kind)for(unsigned c=0;c<3;++c)for(unsigned h=0;h<3;++h)for(unsigned m=0;m<3;++m)
    for(unsigned ix=0;ix<4;++ix)for(unsigned flags=0;flags<16;++flags)for(unsigned k=0;k<10;++k) {
        struct settings_fixture initial={.selected_after=33,.empty_after=(flags&4)?0:1,.mutate=(flags>>3)&1};
        initial.q[OA_SI_ACTIVE_COUNT]=values[c];initial.q[OA_SI_HORIZONTAL]=values[h];initial.q[OA_SI_MIRROR]=values[m];
        initial.q[OA_SI_SELECTED_ID]=17;initial.q[OA_SI_ITEM_INDEX]=indices[ix];initial.q[OA_SI_ITEM_PRESENT]=flags&1;
        initial.q[OA_SI_DESCRIPTION_SENTINEL]=(flags>>1)&1;initial.q[OA_SI_TEXT_EMPTY]=(flags>>2)&1;
        struct settings_fixture vendor=initial,ours=initial;native_settings=&vendor;settings_event=keys[k];
        pointer=0x1000;memcpy(settings_dialog+0x48c,&pointer,4);pointer+=values[c]*4;memcpy(settings_dialog+0x490,&pointer,4);
        settings_dialog[0x49c]=(uint8_t)values[h];
        pointer=initial.q[OA_SI_ITEM_PRESENT]?(uint32_t)(uintptr_t)settings_item:0;for(unsigned i=0;i<3;++i)settings_items[i]=pointer;
        pointer=initial.q[OA_SI_DESCRIPTION_SENTINEL]?settings_sentinel:(uint32_t)(uintptr_t)settings_other_description;memcpy(settings_item+12,&pointer,4);
        unsigned result=native[kind](settings_dialog,&settings_event);
        struct oa_settings_input_ops ops={&ours,settings_query,settings_run,&settings_event};int handled=-1;
        assert(!oa_settings_input(&ops,(enum oa_settings_input_kind)kind,keys[k],&handled));
        if(handled!=(result!=0) || vendor.count!=ours.count || memcmp(vendor.q,ours.q,sizeof ours.q)) {
            fprintf(stderr,"settings mismatch kind=%u c=%u h=%u m=%u index=%x flags=%u key=%x counts=%u/%u\n",kind,c,h,m,indices[ix],flags,keys[k],vendor.count,ours.count);exit(1);
        }
        for(unsigned i=0;i<ours.count;++i)assert(vendor.trace[i].action==ours.trace[i].action && vendor.trace[i].argument==ours.trace[i].argument && vendor.trace[i].message==ours.trace[i].message);
        ++compared;
    }
    native_settings=NULL;for(unsigned i=0;i<9;++i)assert(!munmap(pages[i],lengths[i]));
    printf("ok offline shipped ARM settings input: %u cases, five bodies, dynamic selection/text and original/synthetic action order match\n",compared);
}
static struct nav_fixture *native_nav;
static _Alignas(uint32_t) unsigned char nav_dialog[0x500];
static uint32_t nav_ids[8],nav_event;
static unsigned nav_native_index(void *p,int32_t id)
{
    assert(p==nav_dialog+0x17c);
    /* The direct cursor write precedes this source call. Represent it as a
     * logical action, without patching/intercepting the native store. */
    uint32_t cursor;memcpy(&cursor,nav_dialog+0x498,4);
    if(cursor!=native_nav->cursor)nav_run(native_nav,OA_SN_SET_CURSOR,cursor);
    return nav_query(native_nav,OA_SN_ITEM_INDEX,(uint32_t)id);
}
static unsigned nav_native_biggest(void *p)
{ assert(p==nav_dialog+0x17c);return native_nav->biggest; }
static unsigned nav_native_rows(void *p)
{ assert(p==nav_dialog+0x17c);return native_nav->rows; }
static unsigned nav_native_item(void *p,int32_t item)
{ assert(p==nav_dialog+0x17c);nav_run(native_nav,OA_SN_SET_ITEM,(uint32_t)item);return UINT32_MAX; }
static unsigned nav_native_row(void *p,int32_t row)
{ assert(p==nav_dialog+0x17c);nav_run(native_nav,OA_SN_SET_ROW,(uint32_t)row);return UINT32_MAX; }
typedef void (*nav_handler)(void *,void *);
static void compare_navigation(nav_handler native,struct nav_fixture initial,uint32_t key)
{
    struct nav_fixture vendor=initial,ours=initial;native_nav=&vendor;nav_event=key;
    memcpy(nav_ids,initial.ids,sizeof nav_ids);uint32_t pointer=(uint32_t)(uintptr_t)nav_ids;
    memcpy(nav_dialog+0x48c,&pointer,4);pointer+=initial.count*4;memcpy(nav_dialog+0x490,&pointer,4);
    memcpy(nav_dialog+0x498,&initial.cursor,4);native(nav_dialog,&nav_event);
    struct oa_settings_nav_ops ops={&ours,nav_query,nav_run};assert(!oa_settings_navigate(&ops,key));
    if(vendor.cursor!=ours.cursor || vendor.item!=ours.item || vendor.row!=ours.row || vendor.rows!=ours.rows ||
       vendor.biggest!=ours.biggest || vendor.actions!=ours.actions) {
        fprintf(stderr,"navigation mismatch count=%u cursor=%x key=%x index=%x biggest=%x rows=%x actions=%u/%u item=%x/%x row=%x/%x\n",
                initial.count,initial.cursor,key,initial.indices[0],initial.biggest,initial.rows,vendor.actions,ours.actions,vendor.item,ours.item,vendor.row,ours.row);exit(1);
    }
    for(unsigned i=0;i<ours.actions;++i)assert(vendor.trace[i].action==ours.trace[i].action && vendor.trace[i].argument==ours.trace[i].argument);
}
static void check_navigation(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0xd56000,0xd50000,0x10bc000,0x1148000};unsigned char *pages[4];
    for(unsigned i=0;i<4;++i)pages[i]=page(bases[i],4096);
    size_t off=file_offset(elf,bytes,0xd56a24,312);assert(crc32(elf+off,312)==UINT32_C(0x59f359fd));
    memcpy(pages[0]+0xa24,elf+off,312);uint32_t pointer=(uint32_t)(uintptr_t)&guard;memcpy(pages[0]+0xb58,&pointer,4);
    predicate_stub(pages[1],bases[1],0xd50be4,nav_native_index);
    thumb_stub(pages[2],bases[2],0x10bcd20,nav_native_biggest);thumb_stub(pages[2],bases[2],0x10bc8fc,nav_native_rows);
    predicate_stub(pages[2],bases[2],0x10bc640,nav_native_item);predicate_stub(pages[2],bases[2],0x10bc878,nav_native_row);
    thumb_stub(pages[3],bases[3],0x11489e0,settings_key);
    for(unsigned i=0;i<4;++i)executable(pages[i],4096);
    nav_handler native;pointer=0xd56a25;memcpy(&native,&pointer,4);
    const uint32_t keys[]={0x20a,0x20b,0x2db,0x2dc,0,0x20c,0x20e,UINT32_MAX};
    const uint32_t indices[]={UINT32_MAX,0,1,2,5,9,INT32_MAX,0x80000000};
    const uint32_t biggest[]={UINT32_MAX,0,2,5,INT32_MAX,0x80000000},rows[]={0,1,2,4,INT32_MAX,0x80000000};unsigned compared=0;
    for(unsigned count=0;count<7;++count)for(unsigned cursor=0;cursor<(count?count:1);++cursor)
    for(unsigned k=0;k<8;++k)for(unsigned ix=0;ix<8;++ix)for(unsigned b=0;b<6;++b)for(unsigned r=0;r<6;++r)for(unsigned m=0;m<2;++m) {
        struct nav_fixture initial=nav_initial();initial.count=count;initial.cursor=count?cursor:UINT32_MAX;
        initial.biggest=biggest[b];initial.rows=rows[r];initial.mutate=m;
        for(unsigned i=0;i<8;++i)initial.indices[i]=indices[ix];
        compare_navigation(native,initial,keys[k]);++compared;
    }
    const struct {uint32_t cursor,key;} repair[]={
        {UINT32_MAX,0x20b},{UINT32_MAX,0x2db},{UINT32_MAX,0x2dc},
        {0x80000000,0x2db},{0x80000000,0x2dc},{3,0x20a},{3,0x2db},{3,0x2dc}
    };
    for(unsigned i=0;i<8;++i) {
        struct nav_fixture initial=nav_initial();initial.count=3;initial.cursor=repair[i].cursor;initial.biggest=2;initial.rows=3;
        compare_navigation(native,initial,repair[i].key);++compared;
    }
    native_nav=NULL;for(unsigned i=0;i<4;++i)assert(!munmap(pages[i],4096));
    printf("ok offline shipped ARM settings navigation: %u cases, active-ID wrap/jumps, signed viewport arithmetic and captured geometry match\n",compared);
}
static struct {enum oa_settings_list_route route;unsigned count;const void *message;uint32_t result;} sl_map;
static uint32_t sl_map_call(void *p,enum oa_settings_list_route route,const void *message)
{ (void)p;sl_map.route=route;++sl_map.count;sl_map.message=message;return sl_map.result; }
static unsigned sl_map_rl(void *p,int32_t message)
{ return sl_map_call(p,OA_SL_ROUTE_RIGHT_LEFT,(const void *)(uintptr_t)(uint32_t)message); }
static unsigned sl_map_digit(void *p,int32_t message)
{ return sl_map_call(p,OA_SL_ROUTE_DIGIT,(const void *)(uintptr_t)(uint32_t)message); }
static unsigned sl_map_base(void *p,int32_t message)
{ return sl_map_call(p,OA_SL_ROUTE_BASE,(const void *)(uintptr_t)(uint32_t)message); }
static void check_settings_list_map(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0xd50000,0xcc4000,0x10bd000};unsigned char *pages[3];
    for(unsigned i=0;i<3;++i)pages[i]=page(bases[i],4096);
    size_t off=file_offset(elf,bytes,0xd509f0,328);assert(crc32(elf+off,328)==UINT32_C(0x472aef94));memcpy(pages[0]+0x9f0,elf+off,328);
    off=file_offset(elf,bytes,0xcc4f38,96);assert(crc32(elf+off,96)==UINT32_C(0x90529f1b));memcpy(pages[1]+0xf38,elf+off,96);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;memcpy(pages[0]+0xb34,&pointer,4);memcpy(pages[1]+0xf94,&pointer,4);
    predicate_stub(pages[0],bases[0],0xd508b8,sl_map_rl);predicate_stub(pages[0],bases[0],0xd509a0,sl_map_digit);
    predicate_stub(pages[2],bases[2],0x10bd060,sl_map_base);
    for(unsigned i=0;i<3;++i)executable(pages[i],4096);
    uint32_t vtable[4]={0,0,bits(dispatch_type),bits(dispatch_id)},dummy=0;
    struct dispatch_message message={(uint32_t)(uintptr_t)vtable,0,0};leaf_handler native;pointer=0xd509f1;memcpy(&native,&pointer,4);
    const uint32_t types[]={0,0x1004,0x11004,UINT32_MAX},results[]={0,1,255,UINT32_MAX};
    const uint32_t extra[]={0x1020c,0x10000,INT32_MAX,UINT32_MAX};unsigned compared=0;
    for(unsigned t=0;t<4;++t)for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4100;++k) {
        message.type=types[t];message.id=k<4096?k:extra[k-4096];sl_map.count=0;sl_map.result=results[r];
        unsigned expected=native(&dummy,&message);assert(sl_map.count==1);
        enum oa_settings_list_route route=sl_map.route;const void *original=sl_map.message;
        sl_map.count=0;struct oa_settings_list_dispatch_ops ops={NULL,sl_map_call,&message};int handled=-1;
        assert(!oa_settings_list_dispatch(&ops,message.type,message.id,&handled));
        assert(sl_map.count==1 && sl_map.route==route && sl_map.message==original && original==&message && handled==(expected!=0));++compared;
    }
    for(unsigned i=0;i<3;++i)assert(!munmap(pages[i],4096));
    printf("ok offline shipped ARM settings list map: %u type/id/result cases, original identity and handled truth match\n",compared);
}
static struct sl_fixture *native_sl;
static _Alignas(uint32_t) unsigned char sl_object[0x290],sl_records[3][0x24];
static uint32_t sl_rows[3],sl_record_pointers[3],sl_vector,sl_event,sl_dummy;
static void sl_sync_records(void)
{
    for(unsigned i=0;i<3;++i) {
        uint32_t record=native_sl->records[i];assert(record && record<=3);
        sl_record_pointers[i]=(uint32_t)(uintptr_t)sl_records[record-1];
    }
}
static unsigned sl_native_selected(void *p) { assert(p==sl_object);return native_sl->selected; }
static unsigned sl_native_item(void *p,int32_t selected)
{
    assert(p==sl_object);uint32_t row=sl_query(native_sl,OA_SL_ROW,(uint32_t)selected);
    return row?(uint32_t)(uintptr_t)&sl_rows[row-1]:0;
}
static uint32_t sl_row_handle(const void *p)
{ for(unsigned i=0;i<3;++i)if(p==&sl_rows[i])return i+1;assert(0);return 0; }
static unsigned sl_native_row_key(void *p,int32_t event)
{
    assert((uintptr_t)(uint32_t)event==(uintptr_t)&sl_event);
    uint32_t result=sl_run(native_sl,OA_SL_ROW_KEY,sl_row_handle(p),sl_event,&sl_event);sl_sync_records();return result;
}
static unsigned sl_native_value(void *p) { return sl_query(native_sl,OA_SL_ROW_VALUE,sl_row_handle(p)); }
static unsigned sl_native_mirror(void *p) { (void)p;return native_sl->mirror; }
static unsigned sl_native_base(void *p,int32_t event)
{
    assert(p==sl_object);const void *message=(const void *)(uintptr_t)(uint32_t)event;
    return sl_run(native_sl,OA_SL_BASE_KEY,0,*(const uint32_t *)message,message==&sl_event?message:NULL);
}
static void check_settings_list_input(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0xd50000,0x10bc000,0x1148000,0x1139000,0x97b000,0x113b000,0xd53000};
    const size_t lengths[]={4096,4096,8192,4096,4096,4096,4096};unsigned char *pages[7];
    for(unsigned i=0;i<7;++i)pages[i]=page(bases[i],lengths[i]);
    const struct {uint32_t address,length,crc,literal;} fragments[]={
        {0xd508b8,232,UINT32_C(0xde92e067),0xd50998},
        {0xd509a0,80,UINT32_C(0x03344216),0xd509ec}
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;leaf_handler native[2];
    for(unsigned i=0;i<2;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(pages[0]+fragments[i].address-bases[0],elf+off,fragments[i].length);
        memcpy(pages[0]+fragments[i].literal-bases[0],&pointer,4);uint32_t entry=fragments[i].address|1;memcpy(&native[i],&entry,4);
    }
    thumb_stub(pages[1],bases[1],0x10bc600,sl_native_selected);predicate_stub(pages[1],bases[1],0x10bc714,sl_native_item);
    predicate_stub(pages[1],bases[1],0x10bcf4c,sl_native_base);predicate_stub(pages[2],bases[2],0x114925c,sl_native_row_key);
    thumb_stub(pages[2],bases[2],0x11489e0,settings_key);predicate_stub(pages[2],bases[2],0x114899c,settings_ctor);
    thumb_stub(pages[3],bases[3],0x1139e5c,settings_nop);thumb_stub(pages[4],bases[4],0x97b0ec,sl_native_mirror);
    thumb_stub(pages[5],bases[5],0x113b958,settings_interface);thumb_stub(pages[6],bases[6],0xd53494,sl_native_value);
    for(unsigned i=0;i<7;++i)executable(pages[i],lengths[i]);
    pointer=(uint32_t)(uintptr_t)&sl_dummy;memcpy(sl_object+4,&pointer,4);
    sl_vector=(uint32_t)(uintptr_t)sl_record_pointers;pointer=(uint32_t)(uintptr_t)&sl_vector;memcpy(sl_object+0x28c,&pointer,4);
    const uint32_t truth[]={0,1,255},selected[]={0,1,2,UINT32_MAX},results[]={0,1,255,UINT32_MAX},values[]={0,1,0x80000000,UINT32_MAX};
    const uint32_t keys[]={0,0x200,0x209,0x20c,0x20d,0x20e,0x2da,0x2db,0x1020c,UINT32_MAX};unsigned compared=0;
    for(unsigned kind=0;kind<2;++kind)for(unsigned h=0;h<3;++h)for(unsigned m=0;m<3;++m)for(unsigned s=0;s<4;++s)
    for(unsigned present=0;present<2;++present)for(unsigned mutate=0;mutate<2;++mutate)for(unsigned after=0;after<3;++after)
    for(unsigned r=0;r<4;++r)for(unsigned v=0;v<4;++v)for(unsigned k=0;k<10;++k) {
        struct sl_fixture initial=sl_initial();initial.horizontal=truth[h];initial.mirror=truth[m];initial.selected=selected[s];
        initial.present=present;initial.mutate=mutate;initial.post_selected=after;initial.result=results[r];initial.value_after=values[v]^UINT32_C(0xa5a5a5a5);
        for(unsigned i=0;i<3;++i)initial.values[i]=values[v]+i*UINT32_C(7);
        struct sl_fixture vendor=initial,ours=initial;native_sl=&vendor;sl_event=keys[k];sl_object[0xe8]=(uint8_t)truth[h];sl_sync_records();
        for(unsigned i=0;i<3;++i)memcpy(sl_records[i]+0x20,&initial.record_values[i],4);
        unsigned expected=native[kind](sl_object,&sl_event);
        /* Observe the native direct backing-record store after return. Corpus
         * values deliberately differ from every initial record sentinel. */
        for(unsigned i=0;i<3;++i) {
            uint32_t value;memcpy(&value,sl_records[i]+0x20,4);
            if(value!=vendor.record_values[i])sl_run(&vendor,OA_SL_STORE_VALUE,i+1,value,NULL);
        }
        struct oa_settings_list_ops ops={&ours,sl_query,sl_run,&sl_event};int handled=-1;
        assert(!oa_settings_list_input(&ops,(enum oa_settings_list_kind)kind,keys[k],&handled));
        if(handled!=(expected!=0) || vendor.count!=ours.count || vendor.selected!=ours.selected ||
           memcmp(vendor.values,ours.values,sizeof ours.values) || memcmp(vendor.records,ours.records,sizeof ours.records) ||
           memcmp(vendor.record_values,ours.record_values,sizeof ours.record_values)) {
            fprintf(stderr,"list mismatch kind=%u h=%u m=%u selected=%x present=%u mutate=%u after=%u result=%x value=%x key=%x native=%x handled=%d count=%u/%u\n",
                    kind,h,m,selected[s],present,mutate,after,results[r],values[v],keys[k],expected,handled,vendor.count,ours.count);exit(1);
        }
        for(unsigned i=0;i<ours.count;++i)assert(vendor.trace[i].action==ours.trace[i].action && vendor.trace[i].subject==ours.trace[i].subject && vendor.trace[i].value==ours.trace[i].value && vendor.trace[i].message==ours.trace[i].message);
        ++compared;
    }
    native_sl=NULL;for(unsigned i=0;i<7;++i)assert(!munmap(pages[i],lengths[i]));
    printf("ok offline shipped ARM settings list input: %u cases, row-presence truth, original-row/new-record stores and mirrored identity match\n",compared);
}
struct svg_fixture {uint32_t fields[3],result;unsigned count;enum oa_svg_list_action action;const void *message;};
static struct svg_fixture *native_svg;
static uint32_t svg_query(void *p,enum oa_svg_list_query query) { return ((struct svg_fixture *)p)->fields[query]; }
static uint32_t svg_run(void *p,enum oa_svg_list_action action,const void *message)
{ struct svg_fixture *f=p;f->action=action;f->message=message;++f->count;return f->result; }
#define SVG_ACT(name,action) static unsigned name(void *p) { (void)p;return svg_run(native_svg,action,NULL); }
#define SVG_MSG(name,action) static unsigned name(void *p,int32_t message) \
{ (void)p;return svg_run(native_svg,action,(const void *)(uintptr_t)(uint32_t)message); }
SVG_ACT(svg_select,OA_SVG_LIST_SELECT)
SVG_ACT(svg_down,OA_SVG_LIST_DOWN)
SVG_ACT(svg_up,OA_SVG_LIST_UP)
SVG_ACT(svg_page_down,OA_SVG_LIST_PAGE_DOWN)
SVG_ACT(svg_page_up,OA_SVG_LIST_PAGE_UP)
SVG_MSG(svg_first,OA_SVG_LIST_FIRST)
SVG_MSG(svg_last,OA_SVG_LIST_LAST)
SVG_MSG(svg_window,OA_SVG_LIST_WINDOW)
#undef SVG_ACT
#undef SVG_MSG
static unsigned svg_key(void *p) { return ((struct dispatch_message *)p)->id; }
static void compare_svg(leaf_handler native,struct svg_fixture initial,struct dispatch_message *message,void *object,int outer)
{
    struct svg_fixture vendor=initial,ours=initial;native_svg=&vendor;
    unsigned expected=native(object,message);
    struct oa_svg_list_ops ops={&ours,svg_query,svg_run,message};int handled=-1;
    assert(!(outer?oa_svg_list_handle(&ops,message->type,message->id,&handled):oa_svg_list_key(&ops,message->id,&handled)));
    if(handled!=(expected!=0) || vendor.count!=ours.count || (ours.count && (vendor.action!=ours.action || vendor.message!=ours.message))) {
        fprintf(stderr,"SVG mismatch outer=%d type=%x key=%x horizontal=%u down=%x up=%x result=%x native=%x handled=%d count=%u/%u action=%u/%u\n",
                outer,message->type,message->id,initial.fields[0],initial.fields[1],initial.fields[2],initial.result,expected,handled,vendor.count,ours.count,(unsigned)vendor.action,(unsigned)ours.action);exit(1);
    }
}
static void check_svg_input(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0x10bc000,0x1148000,0x1152000};const size_t lengths[]={8192,4096,4096};unsigned char *pages[3];
    for(unsigned i=0;i<3;++i)pages[i]=page(bases[i],lengths[i]);
    const struct {uint32_t address,length,crc,literal;} fragments[]={
        {0x10bcf4c,276,UINT32_C(0x5d13e755),0x10bd05c},
        {0x10bd060,84,UINT32_C(0xb2f5e4c8),0x10bd0b0}
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;leaf_handler native[2];
    for(unsigned i=0;i<2;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(pages[0]+fragments[i].address-bases[0],elf+off,fragments[i].length);
        memcpy(pages[0]+fragments[i].literal-bases[0],&pointer,4);uint32_t entry=fragments[i].address|1;memcpy(&native[i],&entry,4);
    }
    thumb_stub(pages[0],bases[0],0x10bcbe4,svg_select);thumb_stub(pages[0],bases[0],0x10bc9f8,svg_down);
    thumb_stub(pages[0],bases[0],0x10bc964,svg_up);thumb_stub(pages[0],bases[0],0x10bcb40,svg_page_down);
    thumb_stub(pages[0],bases[0],0x10bcaa8,svg_page_up);predicate_stub(pages[0],bases[0],0x10bce98,svg_first);
    predicate_stub(pages[0],bases[0],0x10bcef4,svg_last);thumb_stub(pages[1],bases[1],0x11489e0,svg_key);
    predicate_stub(pages[2],bases[2],0x1152f34,svg_window);
    for(unsigned i=0;i<3;++i)executable(pages[i],lengths[i]);
    uint32_t vtable[4]={0,0,bits(dispatch_type),bits(dispatch_id)};
    struct dispatch_message message={(uint32_t)(uintptr_t)vtable,0x1004,0};
    _Alignas(uint32_t) unsigned char object[0x22c]={0};
    const uint32_t horizontal[]={0,1,255},aliases[]={0,0x20e,0x20a,0x20b,0x20c,0x20d,0x2db,0x2dc,UINT32_MAX};
    const uint32_t results[]={0,1,255,UINT32_MAX},extra[]={0x1020e,INT32_MAX,0x80000000,UINT32_MAX};unsigned keys=0,messages=0;
    for(unsigned h=0;h<3;++h)for(unsigned d=0;d<9;++d)for(unsigned u=0;u<9;++u)for(unsigned r=0;r<4;++r)for(unsigned k=0;k<1028;++k) {
        struct svg_fixture initial={.fields={horizontal[h],aliases[d],aliases[u]},.result=results[r]};
        object[0xe8]=(uint8_t)horizontal[h];memcpy(object+0x228,&aliases[d],4);memcpy(object+0x224,&aliases[u],4);
        message.id=k<1024?k:extra[k-1024];compare_svg(native[0],initial,&message,object,0);++keys;
    }
    const uint32_t types[]={0,0x1004,0x11004,UINT32_MAX};
    uint32_t down=0x2dc,up=0x2db;memcpy(object+0x228,&down,4);memcpy(object+0x224,&up,4);
    for(unsigned h=0;h<3;++h)for(unsigned t=0;t<4;++t)for(unsigned r=0;r<4;++r)for(unsigned k=0;k<1028;++k) {
        struct svg_fixture initial={.fields={horizontal[h],down,up},.result=results[r]};
        object[0xe8]=(uint8_t)horizontal[h];message.type=types[t];message.id=k<1024?k:extra[k-1024];
        compare_svg(native[1],initial,&message,object,1);++messages;
    }
    native_svg=NULL;for(unsigned i=0;i<3;++i)assert(!munmap(pages[i],lengths[i]));
    printf("ok offline shipped ARM SVG list input: %u key cases, %u message joins, alias precedence/operands and handled truth match\n",keys,messages);
}
struct row_value_fixture {uint32_t values[5];unsigned calls;enum oa_settings_row_query query;};
static struct row_value_fixture *native_row_value;
static _Alignas(uint32_t) unsigned char value_row[0x2e00];
static uint32_t row_value_query(void *p,enum oa_settings_row_query query)
{ struct row_value_fixture *f=p;f->query=query;++f->calls;return f->values[query]; }
static unsigned row_selection(void *p)
{ assert(p==value_row+0x1014);return row_value_query(native_row_value,OA_ROW_SELECTION_ID); }
static unsigned row_numeric(void *p)
{ assert(p==value_row+0x18d8);return row_value_query(native_row_value,OA_ROW_NUMERIC); }
static unsigned row_checked(void *p)
{ assert(p==value_row+0x27a8);return row_value_query(native_row_value,OA_ROW_CHECKED); }
static void check_row_values(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0xd53000,0x10b4000,0x10b7000,0x10b1000};unsigned char *pages[4];
    for(unsigned i=0;i<4;++i)pages[i]=page(bases[i],4096);
    size_t off=file_offset(elf,bytes,0xd53494,144);assert(crc32(elf+off,144)==UINT32_C(0xe82eb084));memcpy(pages[0]+0x494,elf+off,144);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;memcpy(pages[0]+0x520,&pointer,4);
    thumb_stub(pages[1],bases[1],0x10b41b0,row_selection);thumb_stub(pages[2],bases[2],0x10b701c,row_numeric);thumb_stub(pages[3],bases[3],0x10b1298,row_checked);
    for(unsigned i=0;i<4;++i)executable(pages[i],4096);
    getter native;pointer=0xd53495;memcpy(&native,&pointer,4);
    const uint32_t types[]={0x10002,INT32_MAX,0x80000000,UINT32_MAX};
    const uint32_t edge[]={0,1,255,0x100,INT32_MAX,0x80000000,0xffffff00,UINT32_MAX};unsigned compared=0;
    for(unsigned t=0;t<260;++t)for(unsigned raw=0;raw<264;++raw) {
        uint32_t type=t<256?t:types[t-256],seed=raw<256?raw:edge[raw-256];struct row_value_fixture vendor={0},ours;
        for(unsigned i=0;i<5;++i)vendor.values[i]=seed^((i+UINT32_C(1))*UINT32_C(0x11111111));
        ours=vendor;
        memcpy(value_row+0x2dec,&type,4);memcpy(value_row+0x2df8,&vendor.values[OA_ROW_CACHE_PRIMARY],4);memcpy(value_row+0x2dfc,&vendor.values[OA_ROW_CACHE_SECONDARY],4);
        native_row_value=&vendor;uint32_t expected=native(value_row),value=42;
        struct oa_settings_row_ops ops={&ours,row_value_query};assert(!oa_settings_row_value(&ops,type,&value));
        assert(value==expected && vendor.calls<=1 && ours.calls<=1);
        if(vendor.calls)assert(ours.calls==1 && vendor.query==ours.query);
        else if(ours.calls)assert(ours.query==OA_ROW_CACHE_PRIMARY || ours.query==OA_ROW_CACHE_SECONDARY);
        ++compared;
    }
    native_row_value=NULL;for(unsigned i=0;i<4;++i)assert(!munmap(pages[i],4096));
    printf("ok offline shipped ARM row values: %u type/raw-value cases, widget getter selection, cache offsets and raw32 bits match\n",compared);
}
enum body_trace_id {B_SIZE,B_ROW,B_ITEM,B_FIRST,B_FOCUS,B_SEL_UP,B_SEL_DOWN,B_WIN_UP,B_WIN_DOWN,
    B_WIN_TOP,B_WIN_BOTTOM,B_TO_TOP,B_TO_BOTTOM,B_ANIM,B_PARENT,B_ITEM_ID,B_WID,B_CTOR,B_POST,B_DTOR,B_GROUP,
    B_PARTIAL,B_SCROLLBAR};
struct body_fixture {
    uint32_t aux,size,row,item,first,item_id,wid;void *parent;uint32_t post_parent,ctor_item,ctor_wid,grouped;
    unsigned count;struct {uint32_t id,a,b;} trace[16];
};
static struct body_fixture *native_body;
static _Alignas(uint32_t) unsigned char body_object[0x200],body_parent[16],body_list_vtable[19*4],body_parent_vtable[8*4];
static void body_emit(struct body_fixture *f,uint32_t id,uint32_t a,uint32_t b)
{ assert(f->count<16);f->trace[f->count].id=id;f->trace[f->count].a=a;f->trace[f->count].b=b;++f->count; }
static unsigned native_b_size(void *p) { assert(p==body_object+0xf8);body_emit(native_body,B_SIZE,0,0);return native_body->size; }
static unsigned native_b_row(void *p) { assert(p==body_object+0xf8);body_emit(native_body,B_ROW,0,0);return native_body->row; }
static unsigned native_b_item(void *p) { assert(p==body_object+0xf8);body_emit(native_body,B_ITEM,0,0);return native_body->item; }
static unsigned native_b_first(void *p) { assert(p==body_object+0xf8);body_emit(native_body,B_FIRST,0,0);return native_body->first; }
static unsigned native_b_focus(void *p,int32_t i) { assert(p==body_object+0xf8);body_emit(native_body,B_FOCUS,(uint32_t)i,0);return 0; }
static unsigned native_b_sel_up(void *p,int32_t n) { assert(p==body_object);body_emit(native_body,B_SEL_UP,(uint32_t)n,0);return 0; }
static unsigned native_b_sel_down(void *p,int32_t n) { assert(p==body_object);body_emit(native_body,B_SEL_DOWN,(uint32_t)n,0);return 0; }
static unsigned native_b_win_up(void *p,int32_t n) { assert(p==body_object);body_emit(native_body,B_WIN_UP,(uint32_t)n,0);return 0; }
static unsigned native_b_win_down(void *p,int32_t n) { assert(p==body_object);body_emit(native_body,B_WIN_DOWN,(uint32_t)n,0);return 0; }
static unsigned native_b_win_top(void *p) { assert(p==body_object);body_emit(native_body,B_WIN_TOP,0,0);return 0; }
static unsigned native_b_win_bottom(void *p) { assert(p==body_object);body_emit(native_body,B_WIN_BOTTOM,0,0);return 0; }
static unsigned native_b_to_top(void *p) { assert(p==body_object);body_emit(native_body,B_TO_TOP,0,0);return 0; }
static unsigned native_b_to_bottom(void *p) { assert(p==body_object);body_emit(native_body,B_TO_BOTTOM,0,0);return 0; }
static unsigned native_b_anim(void *p) { assert(p==body_object);body_emit(native_body,B_ANIM,0,0);return 0; }
static unsigned native_b_parent(void *p) { assert(p==body_object);body_emit(native_body,B_PARENT,0,0);return (uint32_t)(uintptr_t)native_body->parent; }
static unsigned native_b_item_id(void *p) { assert(p==body_object);body_emit(native_body,B_ITEM_ID,0,0);return native_body->item_id; }
static unsigned native_b_wid(void *p) { assert(p==body_object);body_emit(native_body,B_WID,0,0);return native_body->wid; }
typedef unsigned (*ctor_fn)(void *,int32_t,uint32_t);
static unsigned native_b_ctor(void *buf,int32_t item,uint32_t wid)
{ (void)buf;body_emit(native_body,B_CTOR,(uint32_t)item,wid);native_body->ctor_item=(uint32_t)item;native_body->ctor_wid=wid;return 0; }
static unsigned native_b_post(void *parent,int32_t message)
{ (void)message;assert(parent==native_body->parent);body_emit(native_body,B_POST,(uint32_t)(uintptr_t)parent,native_body->ctor_item);
    native_body->post_parent=(uint32_t)(uintptr_t)parent;return 0; }
static unsigned native_b_dtor(void *p) { (void)p;body_emit(native_body,B_DTOR,native_body->post_parent,native_body->ctor_item);return 0; }
static unsigned native_b_group(void *self,int32_t index)
{ assert(self==body_object);body_emit(native_body,B_GROUP,(uint32_t)index,0);native_body->grouped=(uint32_t)index;return 0; }
static void ctor_stub(void *mapped,uint32_t base,uint32_t address,ctor_fn function)
{
    unsigned char *p=stub_destination(mapped,base,address,8);const uint16_t insn[]={0x4b00,0x4718};
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target;memcpy(&target,&function,4);memcpy(p+4,&target,4);
}
static uint32_t body_q_aux(void *p) { return ((struct body_fixture *)p)->aux; }
static uint32_t body_q_size(void *p) { struct body_fixture *f=p;body_emit(f,B_SIZE,0,0);return f->size; }
static uint32_t body_q_row(void *p) { struct body_fixture *f=p;body_emit(f,B_ROW,0,0);return f->row; }
static uint32_t body_q_item(void *p) { struct body_fixture *f=p;body_emit(f,B_ITEM,0,0);return f->item; }
static uint32_t body_q_first(void *p) { struct body_fixture *f=p;body_emit(f,B_FIRST,0,0);return f->first; }
static void body_e_focus(void *p,uint32_t i) { body_emit(p,B_FOCUS,i,0); }
static void body_e_sel_up(void *p,uint32_t n) { body_emit(p,B_SEL_UP,n,0); }
static void body_e_sel_down(void *p,uint32_t n) { body_emit(p,B_SEL_DOWN,n,0); }
static void body_e_win_up(void *p,uint32_t n) { body_emit(p,B_WIN_UP,n,0); }
static void body_e_win_down(void *p,uint32_t n) { body_emit(p,B_WIN_DOWN,n,0); }
static void body_e_win_top(void *p) { body_emit(p,B_WIN_TOP,0,0); }
static void body_e_win_bottom(void *p) { body_emit(p,B_WIN_BOTTOM,0,0); }
static void body_e_to_top(void *p) { body_emit(p,B_TO_TOP,0,0); }
static void body_e_to_bottom(void *p) { body_emit(p,B_TO_BOTTOM,0,0); }
static void body_e_anim(void *p) { body_emit(p,B_ANIM,0,0); }
static void *body_e_parent(void *p) { struct body_fixture *f=p;body_emit(f,B_PARENT,0,0);return f->parent; }
static uint32_t body_e_item_id(void *p) { struct body_fixture *f=p;body_emit(f,B_ITEM_ID,0,0);return f->item_id; }
static uint32_t body_e_wid(void *p) { struct body_fixture *f=p;body_emit(f,B_WID,0,0);return f->wid; }
static uint32_t body_e_notify(void *p,void *parent,uint32_t item,uint32_t wid)
{ struct body_fixture *f=p;body_emit(f,B_CTOR,item,wid);body_emit(f,B_POST,(uint32_t)(uintptr_t)parent,item);body_emit(f,B_DTOR,(uint32_t)(uintptr_t)parent,item);return 0; }
static void body_e_group(void *p,uint32_t index) { struct body_fixture *f=p;body_emit(f,B_GROUP,index,0);f->grouped=index; }
static unsigned native_b_partial(void *p,int32_t index)
{ assert(p==body_object+0xf8);body_emit(native_body,B_PARTIAL,(uint32_t)index,0);return 0; }
static unsigned native_b_scrollbar(void *p)
{ assert(p==body_object);body_emit(native_body,B_SCROLLBAR,0,0);return 0; }
static uint32_t body_m_aux(void *p) { return ((struct body_fixture *)p)->aux; }
static uint32_t body_m_size(void *p) { struct body_fixture *f=p;body_emit(f,B_SIZE,0,0);return f->size; }
static uint32_t body_m_item(void *p) { struct body_fixture *f=p;body_emit(f,B_ITEM,0,0);return f->item; }
static uint32_t body_m_first(void *p) { struct body_fixture *f=p;body_emit(f,B_FIRST,0,0);return f->first; }
static void body_m_focus(void *p,uint32_t i) { body_emit(p,B_FOCUS,i,0); }
static void body_m_partial(void *p,uint32_t i) { body_emit(p,B_PARTIAL,i,0); }
static void body_m_scrollbar(void *p) { body_emit(p,B_SCROLLBAR,0,0); }
static void body_m_anim(void *p) { body_emit(p,B_ANIM,0,0); }
typedef void (*move0_fn)(void *);
typedef void (*move1_fn)(void *,uint32_t);
struct rows_fixture { uint32_t first,size,selected;unsigned count;struct {uint32_t id,a;int b;} trace[8]; };
static struct rows_fixture *native_rows;
static _Alignas(uint32_t) unsigned char mgr_obj[0x40],mgr_ptrs[16*4],mgr_rowobjs[16][8];
static uint32_t row_vtable[8];
static void rows_emit(struct rows_fixture *f,uint32_t id,uint32_t a,int b)
{ assert(f->count<8);f->trace[f->count].id=id;f->trace[f->count].a=a;f->trace[f->count].b=b;++f->count; }
static unsigned native_r_focus(void *row,int32_t on)
{ rows_emit(native_rows,1,*(uint32_t *)((unsigned char *)row+4),on!=0);return 0; }
typedef void (*impl_fn)(void *,uint32_t,int);
typedef void (*rotate_fn)(void *,int32_t);
static void native_r_impl(void *mgr,uint32_t index,int full)
{ assert(mgr==mgr_obj);rows_emit(native_rows,2,index,full); }
static void impl_stub(void *mapped,uint32_t base,uint32_t address,impl_fn function)
{
    /* Preserving indirect branch, matching the other multi-arg installers. */
    unsigned char *p=stub_destination(mapped,base,address,16);
    const uint16_t insn[]={0xb40f,0x4b02,0x469c,0xbc0f,0x4760,0x46c0};
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target;memcpy(&target,&function,4);memcpy(p+12,&target,4);
}
enum {R_FOCUS=1,R_IMPL=2};
static uint32_t rows_q_first(void *p) { return ((struct rows_fixture *)p)->first; }
static uint32_t rows_q_size(void *p) { return ((struct rows_fixture *)p)->size; }
static uint32_t rows_q_sel(void *p) { return ((struct rows_fixture *)p)->selected; }
static uint32_t rows_q_slots(void *p) { (void)p;return 16; }
static void *rows_q_row(void *p,uint32_t rel) { (void)p;return (void *)(uintptr_t)rel; }
static struct rows_fixture *ref_rows;
static void rows_e_focus(void *row,int focused) { rows_emit(ref_rows,R_FOCUS,(uint32_t)(uintptr_t)row,focused); }
static void rows_e_store(void *p,uint32_t rel) { ((struct rows_fixture *)p)->selected=rel; }
static void rows_e_impl(void *p,uint32_t index,int full) { rows_emit((struct rows_fixture *)p,R_IMPL,index,full); }
static void compare_move_traces(struct body_fixture *vendor,struct body_fixture *ref,unsigned body,
    uint32_t n,struct body_fixture *initial)
{
    if(vendor->count!=ref->count) {
        fprintf(stderr,"move %u count native=%u ours=%u n=%x aux=%x size=%x item=%x first=%x\n",
            body,vendor->count,ref->count,n,initial->aux,initial->size,initial->item,initial->first);
        for(unsigned i=0;i<vendor->count;++i)fprintf(stderr,"  native[%u]=%u/%x\n",i,vendor->trace[i].id,vendor->trace[i].a);
        for(unsigned i=0;i<ref->count;++i)fprintf(stderr,"  ours[%u]=%u/%x\n",i,ref->trace[i].id,ref->trace[i].a);
        exit(1);
    }
    for(unsigned i=0;i<vendor->count;++i)if(vendor->trace[i].id!=ref->trace[i].id || vendor->trace[i].a!=ref->trace[i].a) {
        fprintf(stderr,"move %u trace %u native=%u/%x ours=%u/%x\n",body,i,vendor->trace[i].id,
            vendor->trace[i].a,ref->trace[i].id,ref->trace[i].a);exit(1);
    }
}
static void compare_move0(move0_fn native,int (*ours)(const struct oa_svg_list_move_ops *),
    struct body_fixture initial,unsigned body)
{
    struct body_fixture vendor=initial,ref=initial;struct oa_svg_list_move_ops ops={&ref,body_m_aux,body_m_size,
        body_m_item,body_m_first,body_m_focus,body_m_partial,body_m_scrollbar,body_m_anim};
    memcpy(body_object+0x1ec,&vendor.aux,4);
    native_body=&vendor;native(body_object);
    assert(!ours(&ops));compare_move_traces(&vendor,&ref,body,0,&initial);
}
static void compare_move1(move1_fn native,int (*ours)(const struct oa_svg_list_move_ops *,uint32_t),
    struct body_fixture initial,unsigned body,uint32_t n)
{
    struct body_fixture vendor=initial,ref=initial;struct oa_svg_list_move_ops ops={&ref,body_m_aux,body_m_size,
        body_m_item,body_m_first,body_m_focus,body_m_partial,body_m_scrollbar,body_m_anim};
    memcpy(body_object+0x1ec,&vendor.aux,4);
    native_body=&vendor;native(body_object,n);
    assert(!ours(&ops,n));compare_move_traces(&vendor,&ref,body,n,&initial);
}
typedef unsigned (*body_fn)(void *);
typedef unsigned (*body_msg_fn)(void *,void *);
static void compare_body(body_fn native,int (*ours)(const struct oa_svg_list_box_ops *,int *),
    struct body_fixture initial,unsigned body,int use_msg)
{
    struct body_fixture vendor=initial,ref=initial;struct oa_svg_list_box_ops ops={&ref,body_q_aux,body_q_size,
        body_q_row,body_q_item,body_q_first,body_e_focus,body_e_sel_up,body_e_sel_down,body_e_win_up,body_e_win_down,
        body_e_win_top,body_e_win_bottom,body_e_to_top,body_e_to_bottom,body_e_anim,body_e_parent,body_e_item_id,
        body_e_wid,body_e_notify,body_e_group};
    memcpy(body_object+0x1ec,&vendor.aux,4);
    native_body=&vendor;
    unsigned expected;
    if(use_msg) { body_msg_fn two;memcpy(&two,&native,sizeof two);expected=two(body_object,NULL); }
    else expected=native(body_object);
    int handled=-1;assert(!ours(&ops,&handled));
    if(handled!=(expected!=0)) {
        fprintf(stderr,"body %u handled native=%x ours=%d aux=%x size=%x row=%x item=%x first=%x\n",
            body,expected,handled,initial.aux,initial.size,initial.row,initial.item,initial.first);exit(1);
    }
    for(unsigned i=0;i<vendor.count;++i)if(vendor.trace[i].id!=ref.trace[i].id ||
        vendor.trace[i].a!=ref.trace[i].a || vendor.trace[i].b!=ref.trace[i].b) {
        fprintf(stderr,"body %u trace %u native=%u/%x/%x ours=%u/%x/%x\n",body,i,vendor.trace[i].id,
            vendor.trace[i].a,vendor.trace[i].b,ref.trace[i].id,ref.trace[i].a,ref.trace[i].b);exit(1);
    }
    if(vendor.count!=ref.count) {
        fprintf(stderr,"body %u count native=%u ours=%u\n",body,vendor.count,ref.count);
        for(unsigned i=0;i<vendor.count;++i)fprintf(stderr,"  native[%u]=%u/%x/%x\n",i,vendor.trace[i].id,vendor.trace[i].a,vendor.trace[i].b);
        for(unsigned i=0;i<ref.count;++i)fprintf(stderr,"  ours[%u]=%u/%x/%x\n",i,ref.trace[i].id,ref.trace[i].a,ref.trace[i].b);
        exit(1);
    }
}
static void check_svg_bodies(const unsigned char *elf,size_t bytes)
{
    unsigned char *list=page(0x10bc000,8192),*rows=page(0x10be000,4096),*anim=page(0x10ba000,8192);
    unsigned char *ctor=page(0x1148000,4096),*parent=page(0x1152000,4096),*dtor=page(0x1138000,8192);
    const struct {uint32_t address,length,crc,literal;} fragments[]={
        {0x10bc964,148,UINT32_C(0x6d2f8674),0x10bc9f4},
        {0x10bc9f8,176,UINT32_C(0xe84f6632),0x10bcaa4},
        {0x10bcaa8,152,UINT32_C(0x66ad2f6d),0x10bcb3c},
        {0x10bcb40,164,UINT32_C(0x69caf4f4),0x10bcbe0},
        {0x10bcbe4,404,UINT32_C(0xbc4274b5),0x10bcc70},
        {0x10bce98,92,UINT32_C(0x7deea255),0x10bcef0},
        {0x10bcef4,88,UINT32_C(0x272417eb),0x10bcf48}
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;body_fn native[7];
    for(unsigned i=0;i<7;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(list+fragments[i].address-0x10bc000,elf+off,fragments[i].length);
        memcpy(list+fragments[i].literal-0x10bc000,&pointer,4);
        uint32_t entry=fragments[i].address|1;memcpy(&native[i],&entry,4);
    }
    thumb_stub(list,0x10bc000,0x10bc2e0,native_b_win_top);
    thumb_stub(list,0x10bc000,0x10bc324,native_b_win_bottom);
    thumb_stub(list,0x10bc000,0x10bc37c,native_b_to_top);
    thumb_stub(list,0x10bc000,0x10bc3bc,native_b_to_bottom);
    predicate_stub(list,0x10bc000,0x10bc408,native_b_win_up);
    predicate_stub(list,0x10bc000,0x10bc458,native_b_win_down);
    predicate_stub(list,0x10bc000,0x10bc4a8,native_b_sel_up);
    predicate_stub(list,0x10bc000,0x10bc5bc,native_b_sel_down);
    thumb_stub(list,0x10bc000,0x10bc600,native_b_item_id);
    predicate_stub(rows,0x10be000,0x10be3cc,native_b_focus);
    thumb_stub(rows,0x10be000,0x10be6e4,native_b_size);
    thumb_stub(rows,0x10be000,0x10be74c,native_b_row);
    thumb_stub(rows,0x10be000,0x10be77c,native_b_first);
    thumb_stub(rows,0x10be000,0x10be814,native_b_item);
    thumb_stub(anim,0x10ba000,0x10bbedc,native_b_anim);
    ctor_stub(ctor,0x1148000,0x1148b88,native_b_ctor);
    thumb_stub(parent,0x1152000,0x11528a0,(getter)native_b_parent);
    thumb_stub(parent,0x1152000,0x11529e0,native_b_wid);
    thumb_stub(dtor,0x1138000,0x1139e5c,native_b_dtor);
    executable(list,8192);executable(rows,4096);executable(anim,8192);
    executable(ctor,4096);executable(parent,4096);executable(dtor,8192);
    uint32_t list_vtable=(uint32_t)(uintptr_t)body_list_vtable,parent_vtable=(uint32_t)(uintptr_t)body_parent_vtable;
    uint32_t group_bits=predicate_bits(native_b_group),post_bits=predicate_bits(native_b_post);
    memcpy(body_list_vtable+0x48,&group_bits,4);
    memcpy(body_object,&list_vtable,4);memcpy(body_parent_vtable+0x10,&post_bits,4);memcpy(body_parent,&parent_vtable,4);
    static int (*const bodies[])(const struct oa_svg_list_box_ops *,int *)={oa_svg_list_up_body,
        oa_svg_list_down_body,oa_svg_list_page_up_body,oa_svg_list_page_down_body,oa_svg_list_select_body,
        oa_svg_list_first_body,oa_svg_list_last_body};
    const uint32_t auxes[]={0,1,2,7,100,UINT32_C(0x80000000),UINT32_MAX};
    const uint32_t sizes[]={0,1,2,5,UINT32_C(0x80000000)};
    const uint32_t rows4[]={0,1,4,UINT32_MAX},items[]={0,1,6,UINT32_C(0xfffffffe)},firsts[]={0,1,3,UINT32_MAX};
    const uint32_t ids[]={0,0x1234,UINT32_MAX};unsigned compared=0;
    for(unsigned a=0;a<7;++a)for(unsigned s=0;s<5;++s)for(unsigned r=0;r<4;++r)for(unsigned m=0;m<4;++m)
    for(unsigned q=0;q<4;++q) {
        struct body_fixture initial={.aux=auxes[a],.size=sizes[s],.row=rows4[r],.item=items[m],.first=firsts[q]};
        for(unsigned b=0;b<7;++b) { compare_body(native[b],bodies[b],initial,b,b>=5);++compared; }
        if(a<3 && s<3)for(unsigned p=0;p<2;++p)for(unsigned u=0;u<3;++u)for(unsigned w=0;w<3;++w) {
            struct body_fixture select=initial;select.parent=p?body_parent:NULL;select.item_id=ids[u];select.wid=ids[w];
            compare_body(native[4],bodies[4],select,4,0);++compared;
        }
    }
    native_body=NULL;
    assert(!munmap(list,8192) && !munmap(rows,4096) && !munmap(anim,8192));
    assert(!munmap(ctor,4096) && !munmap(parent,4096) && !munmap(dtor,8192));
    printf("ok offline shipped ARM SVG list bodies: %u native/C comparisons, exact call order/args and handled truth match\n",compared);
}
static void check_svg_moves(const unsigned char *elf,size_t bytes)
{
    unsigned char *list=page(0x10bc000,8192),*rows=page(0x10be000,4096),*anim=page(0x10ba000,8192);
    const struct {uint32_t address,length,crc,literal;} fragments[]={
        {0x10bc2e0,68,UINT32_C(0x188ffe10),0x10bc320},
        {0x10bc324,88,UINT32_C(0x7e9785c6),0x10bc378},
        {0x10bc37c,64,UINT32_C(0x818828e6),0x10bc3b8},
        {0x10bc3bc,76,UINT32_C(0x2c731f65),0x10bc404},
        {0x10bc408,80,UINT32_C(0xbae06b9a),0x10bc454},
        {0x10bc458,80,UINT32_C(0x915e414d),0x10bc4a4},
        {0x10bc4a8,68,UINT32_C(0xd2fd2b9d),0x10bc4e8},
        {0x10bc5bc,68,UINT32_C(0x2e6b9b7f),0x10bc5fc}
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;move0_fn plain[4];move1_fn step[4];
    for(unsigned i=0;i<8;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(list+fragments[i].address-0x10bc000,elf+off,fragments[i].length);
        memcpy(list+fragments[i].literal-0x10bc000,&pointer,4);
        uint32_t entry=fragments[i].address|1;
        if(i<4)memcpy(&plain[i],&entry,4);else memcpy(&step[i-4],&entry,4);
    }
    thumb_stub(rows,0x10be000,0x10be6e4,native_b_size);
    thumb_stub(rows,0x10be000,0x10be814,native_b_item);
    thumb_stub(rows,0x10be000,0x10be77c,native_b_first);
    predicate_stub(rows,0x10be000,0x10be3cc,native_b_focus);
    predicate_stub(rows,0x10be000,0x10be6b0,native_b_partial);
    thumb_stub(anim,0x10ba000,0x10bbe04,native_b_scrollbar);
    thumb_stub(anim,0x10ba000,0x10bbedc,native_b_anim);
    executable(list,8192);executable(rows,4096);executable(anim,8192);
    memset(body_object,0,sizeof body_object);
    static int (*const zeroes[])(const struct oa_svg_list_move_ops *)={oa_svg_list_move_window_top,
        oa_svg_list_move_window_bottom,oa_svg_list_move_selection_to_top,oa_svg_list_move_selection_to_bottom};
    static int (*const steps[])(const struct oa_svg_list_move_ops *,uint32_t)={oa_svg_list_move_window_up,
        oa_svg_list_move_window_down,oa_svg_list_move_selection_up,oa_svg_list_move_selection_down};
    const uint32_t auxes[]={0,1,2,9,UINT32_MAX};
    const uint32_t sizes[]={0,1,5},sels[]={0,1,7,UINT32_MAX},firsts[]={0,1,3};
    const uint32_t ns[]={0,1,2,5,UINT32_C(0x80000000),UINT32_MAX};unsigned compared=0;
    for(unsigned a=0;a<5;++a)for(unsigned s=0;s<3;++s)for(unsigned m=0;m<4;++m)for(unsigned q=0;q<3;++q) {
        struct body_fixture initial={.aux=auxes[a],.size=sizes[s],.item=sels[m],.first=firsts[q]};
        for(unsigned b=0;b<4;++b) { compare_move0(plain[b],zeroes[b],initial,b);++compared; }
        for(unsigned n=0;n<6;++n)for(unsigned b=0;b<4;++b)
            { compare_move1(step[b],steps[b],initial,b+4,ns[n]);++compared; }
    }
    native_body=NULL;
    assert(!munmap(list,8192) && !munmap(rows,4096) && !munmap(anim,8192));
    printf("ok offline shipped ARM SVG list moves: %u native/C comparisons, exact call order/args match\n",compared);
}
typedef unsigned (*rows_fn)(void *);
typedef void (*focus_fn)(void *,uint32_t);
static void compare_rows_focus(focus_fn native,int (*ours)(const struct oa_svg_list_manager_ops *,uint32_t),
    struct rows_fixture initial,uint32_t index)
{
    struct rows_fixture vendor=initial,ref=initial;struct oa_svg_list_manager_ops ops={&ref,rows_q_first,rows_q_size,
        rows_q_sel,rows_q_row,rows_e_focus,rows_e_store,rows_e_impl,rows_q_slots};
    memcpy(mgr_obj+12,&vendor.first,4);memcpy(mgr_obj+16,&vendor.size,4);memcpy(mgr_obj+20,&vendor.selected,4);
    native_rows=&vendor;ref_rows=&ref;native(mgr_obj,index);
    assert(!ours(&ops,index));
    if(vendor.count!=ref.count) {
        fprintf(stderr,"rows focus count native=%u ours=%u index=%x\n",vendor.count,ref.count,index);
        for(unsigned i=0;i<vendor.count;++i)fprintf(stderr,"  native[%u]=%u/%x/%d\n",i,vendor.trace[i].id,vendor.trace[i].a,vendor.trace[i].b);
        for(unsigned i=0;i<ref.count;++i)fprintf(stderr,"  ours[%u]=%u/%x/%d\n",i,ref.trace[i].id,ref.trace[i].a,ref.trace[i].b);
        exit(1);
    }
    for(unsigned i=0;i<vendor.count;++i)if(vendor.trace[i].id!=ref.trace[i].id || vendor.trace[i].a!=ref.trace[i].a ||
        vendor.trace[i].b!=ref.trace[i].b) {
        fprintf(stderr,"rows focus trace %u native=%u/%x/%d ours=%u/%x/%d\n",i,vendor.trace[i].id,
            vendor.trace[i].a,vendor.trace[i].b,ref.trace[i].id,ref.trace[i].a,ref.trace[i].b);exit(1);
    }
    if(*(uint32_t *)(mgr_obj+20)!=ref.selected) {
        fprintf(stderr,"rows focus store native=%x ours=%x index=%x\n",*(uint32_t *)(mgr_obj+20),ref.selected,index);exit(1);
    }
}
static void check_svg_rows(const unsigned char *elf,size_t bytes)
{
    unsigned char *rows=page(0x10be000,4096);
    const struct {uint32_t address,length,crc,literal;} fragments[]={
        {0x10be6b0,52,UINT32_C(0x35e7a0e3),0x10be6e0},
        {0x10be6e4,48,UINT32_C(0x50e85ab7),0x10be710},
        {0x10be74c,48,UINT32_C(0xce283c69),0x10be778},
        {0x10be814,52,UINT32_C(0x9f7c88dd),0x10be844},
        {0x10be77c,48,UINT32_C(0x687fe952),0x10be7a8},
        {0x10be3cc,96,UINT32_C(0xb224a508),0x10be428}
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;rows_fn native[6];focus_fn focus=0;move1_fn partial=0;
    for(unsigned i=0;i<6;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(rows+fragments[i].address-0x10be000,elf+off,fragments[i].length);
        memcpy(rows+fragments[i].literal-0x10be000,&pointer,4);
        uint32_t entry=fragments[i].address|1;
        if(i==0)memcpy(&partial,&entry,4);
        else if(i<5)memcpy(&native[i],&entry,4);else memcpy(&focus,&entry,4);
    }
    impl_stub(rows,0x10be000,0x10be504,native_r_impl);
    executable(rows,4096);
    uint32_t focus_bits=predicate_bits(native_r_focus);
    memcpy(row_vtable+7,&focus_bits,4);
    for(unsigned r=0;r<16;++r) {
        uint32_t vtable=(uint32_t)(uintptr_t)row_vtable,index=r;
        memcpy(mgr_rowobjs[r],&vtable,4);memcpy(mgr_rowobjs[r]+4,&index,4);
        uint32_t slot=(uint32_t)(uintptr_t)mgr_rowobjs[r];memcpy(mgr_ptrs+r*4,&slot,4);
    }
    uint32_t array=(uint32_t)(uintptr_t)mgr_ptrs;memcpy(mgr_obj+28,&array,4);
    const uint32_t firsts[]={0,1,5},sizes[]={0,1,4,9},sels[]={0,1,7};
    const uint32_t indices[]={0,1,3,4,8,UINT32_C(0xffffffff),UINT32_C(0x80000000)};unsigned compared=0;
    for(unsigned a=0;a<3;++a)for(unsigned s=0;s<4;++s)for(unsigned m=0;m<3;++m) {
        struct rows_fixture initial={.first=firsts[a],.size=sizes[s],.selected=sels[m]};
        struct rows_fixture vendor=initial,ref=initial;struct oa_svg_list_manager_ops ops={&ref,rows_q_first,
            rows_q_size,rows_q_sel,rows_q_row,rows_e_focus,rows_e_store,rows_e_impl,rows_q_slots};
        memcpy(mgr_obj+12,&vendor.first,4);memcpy(mgr_obj+16,&vendor.size,4);memcpy(mgr_obj+20,&vendor.selected,4);
        int e1=0,e2=0,e3=0,e4=0;
        unsigned vsize=native[1](mgr_obj),vrow=native[2](mgr_obj);
        unsigned vitem=native[3](mgr_obj),vfirst=native[4](mgr_obj);
        unsigned osize=oa_svg_list_manager_size(&ops,&e1);
        unsigned orow=oa_svg_list_manager_selected_row(&ops,&e2);
        unsigned oitem=oa_svg_list_manager_selected_item(&ops,&e3);
        unsigned ofirst=oa_svg_list_manager_first_visible(&ops,&e4);
        if(osize!=vsize || orow!=vrow || oitem!=vitem || ofirst!=vfirst || e1 || e2 || e3 || e4) {
            fprintf(stderr,"rows getter mismatch first=%x size=%x sel=%x native=%x/%x/%x/%x ours=%x/%x/%x/%x err=%d/%d/%d/%d\n",
                initial.first,initial.size,initial.selected,vsize,vrow,vitem,vfirst,osize,orow,oitem,ofirst,e1,e2,e3,e4);
            exit(1);
        }
        ++compared;
        native_rows=&vendor;ref_rows=&ref;
        partial(mgr_obj,7);
        assert(!oa_svg_list_manager_update_partial(&ops,7));
        if(vendor.count!=ref.count || vendor.trace[0].id!=R_IMPL || ref.trace[0].id!=R_IMPL ||
            vendor.trace[0].a!=7 || ref.trace[0].a!=7 || vendor.trace[0].b || ref.trace[0].b) {
            fprintf(stderr,"rows partial mismatch counts %u/%u\n",vendor.count,ref.count);
            for(unsigned i=0;i<vendor.count;++i)fprintf(stderr,"  native[%u]=%u/%x/%d\n",i,vendor.trace[i].id,vendor.trace[i].a,vendor.trace[i].b);
            for(unsigned i=0;i<ref.count;++i)fprintf(stderr,"  ours[%u]=%u/%x/%d\n",i,ref.trace[i].id,ref.trace[i].a,ref.trace[i].b);
            exit(1);
        }
        ++compared;
        for(unsigned k=0;k<7;++k) { compare_rows_focus(focus,oa_svg_list_manager_focus_item,initial,indices[k]);++compared; }
    }
    native_rows=NULL;ref_rows=NULL;
    assert(!munmap(rows,4096));
    printf("ok offline shipped ARM SVG list rows: %u native/C comparisons, field layout/focus-store/partial match\n",compared);
}
enum {SB_SIZE,SB_TOTAL,SB_CREATED,SB_CREATE,SB_VISIBLE,SB_SETVIS,SB_POS,SB_RECT};
struct scroll_fixture {
    uint32_t size,total,flag,fields[6],visible,first;int created;
    unsigned count;struct {uint32_t id,a,b,c,d;} trace[16];
};
static struct scroll_fixture *native_scroll;
static _Alignas(uint32_t) unsigned char scroll_list[0x300];
static uint32_t scroll_vtable[32];
static void scroll_emit(struct scroll_fixture *f,uint32_t id,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{ assert(f->count<16);f->trace[f->count].id=id;f->trace[f->count].a=a;f->trace[f->count].b=b;f->trace[f->count].c=c;f->trace[f->count].d=d;++f->count; }
static unsigned native_sb_total(void *p,int32_t ignored)
{ (void)ignored;assert(p==scroll_list);scroll_emit(native_scroll,SB_TOTAL,0,0,0,0);return native_scroll->total; }
static unsigned native_sb_created(void *p)
{ assert(p==scroll_list+0x120);scroll_emit(native_scroll,SB_CREATED,0,0,0,0);return (unsigned)native_scroll->created; }
static unsigned native_sb_isvisible(void *p)
{ assert(p==scroll_list);return native_scroll->visible; }
static unsigned native_sb_setvisible(void *p,int32_t v)
{ assert(p==scroll_list+0x120);scroll_emit(native_scroll,SB_SETVIS,(uint32_t)v,0,0,0);return 0; }
typedef void (*quad_fn)(void *,uint32_t,uint32_t,uint32_t);
static void quad_stub(void *mapped,uint32_t base,uint32_t address,quad_fn function)
{
    /* Preserving indirect branch: r3 carries the fourth argument. */
    unsigned char *p=stub_destination(mapped,base,address,16);
    const uint16_t insn[]={0xb40f,0x4b02,0x469c,0xbc0f,0x4760,0x46c0};
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target;memcpy(&target,&function,4);memcpy(p+12,&target,4);
}
typedef void (*rect_fn)(void *,uint32_t,uint32_t,uint32_t,uint32_t);
static void native_sb_create(void *sb,uint32_t created,uint32_t rect,uint32_t parent)
{
    assert(sb==scroll_list+0x120 && parent==(uint32_t)(uintptr_t)scroll_list);
    (void)rect;
    scroll_emit(native_scroll,SB_CREATE,created,parent==(uint32_t)(uintptr_t)scroll_list,0,0);
}
static void native_sb_setpos(void *sb,uint32_t vis,uint32_t size,uint32_t first)
{ assert(sb==scroll_list+0x120);scroll_emit(native_scroll,SB_POS,vis,size,first,0); }
static void native_sb_rect(void *r,uint32_t x,uint32_t y,uint32_t w,uint32_t h)
{ scroll_emit(native_scroll,SB_RECT,x,y,w,h);(void)r; }
static void rect_stub(void *mapped,uint32_t base,uint32_t address,rect_fn function)
{
    /* Preserving indirect branch: r3 carries the fourth argument. */
    unsigned char *p=stub_destination(mapped,base,address,16);
    const uint16_t insn[]={0xb40f,0x4b02,0x469c,0xbc0f,0x4760,0x46c0};
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target;memcpy(&target,&function,4);memcpy(p+12,&target,4);
}
static uint32_t scroll_q_size(void *p) { return ((struct scroll_fixture *)p)->size; }
static uint32_t scroll_q_total(void *p) { struct scroll_fixture *f=p;scroll_emit(f,SB_TOTAL,0,0,0,0);return f->total; }
static uint32_t scroll_q_flag(void *p) { return ((struct scroll_fixture *)p)->flag; }
static uint32_t scroll_q_field(void *p,uint32_t off) {
    struct scroll_fixture *f=p;
    switch(off) { case 0x1f4:return f->fields[0];case 0x1fc:return f->fields[1];case 0x1f0:return f->fields[2];
    case 0x1f8:return f->fields[3];case 0x21c:return f->fields[4];case 0x214:return f->fields[5];default:assert(0);return 0; }
}
static void *scroll_q_sb(void *p) { (void)p;return scroll_list+0x120; }
static void *scroll_q_list(void *p) { (void)p;return scroll_list; }
static int scroll_q_visible(void *p) { return (int)((struct scroll_fixture *)p)->visible; }
static uint32_t scroll_q_first(void *p) { return ((struct scroll_fixture *)p)->first; }
static int scroll_q_created(void *p,void *sb) { struct scroll_fixture *f=p;scroll_emit(f,SB_CREATED,0,0,0,0);(void)sb;return f->created; }
static void scroll_e_create(void *p,void *sb,int created,uint32_t x,uint32_t y,uint32_t w,uint32_t h,void *parent)
{ struct scroll_fixture *f=p;scroll_emit(f,SB_RECT,x,y,w,h);scroll_emit(f,SB_CREATE,(uint32_t)created,parent==scroll_list,0,0);(void)sb; }
static void scroll_e_setvis(void *p,void *sb,int v) { struct scroll_fixture *f=p;scroll_emit(f,SB_SETVIS,(uint32_t)v,0,0,0);(void)sb; }
static void scroll_e_pos(void *p,void *sb,uint32_t vis,uint32_t size,uint32_t first)
{ struct scroll_fixture *f=p;scroll_emit(f,SB_POS,vis,size,first,0);(void)sb; }
typedef void (*pair_fn)(void *,uint32_t);
static uint32_t pair_bits(pair_fn f) { uint32_t v;_Static_assert(sizeof f==sizeof v,"fn width");memcpy(&v,&f,sizeof v);return v; }
static void pair_stub(void *mapped,uint32_t base,uint32_t address,pair_fn function)
{
    unsigned char *p=stub_destination(mapped,base,address,8);const uint16_t insn[]={0x4b00,0x4718};
    assert(!(address&3));memcpy(p,insn,sizeof insn);uint32_t target=pair_bits(function);memcpy(p+4,&target,4);
}
static void raw_stub(void *mapped,uint32_t base,uint32_t address,uint32_t target)
{
    /* Untyped installer for callees no typed installer covers (float register
     * arguments, raw pointer pairs). Caller and stub honor the AAPCS; only
     * the entry address is installed here. */
    unsigned char *p=stub_destination(mapped,base,address,8);const uint16_t insn[]={0x4b00,0x4718};
    assert(!(address&3));memcpy(p,insn,sizeof insn);memcpy(p+4,&target,4);
}
#include "settings_draw_native.h"
#include "settings_row_type_native.h"
#include "settings_row_setters_native.h"
#include "settings_row_render_native.h"
#include "launcher_image_id_native.h"
#include "svg_image_native.h"
#include "settings_row_construct_native.h"
#include "svg_widget_state_native.h"
#include "launcher_item_native.h"
#include "launcher_info_native.h"
#include "svg_scrollbar_native.h"
#include "svg_scrollbar_skin_native.h"
#include "svg_scrollbar_render_native.h"
#include "audio_amp_native.h"
#include "profile_data_native.h"
#include "amp_init_native.h"
typedef void (*scroll_fn)(void *);
static scroll_fn native_sb_update;
enum {R_SET_FOCUS,R_SET_VISIBLE,R_GET_RECT,R_SET_POS,R_IS_VISIBLE,R_DRAW,R_DRAW_EMPTY,R_EMPTY_MSG,R_GOT_FOCUS};
struct impl_fixture {
    uint32_t cap,first,size,selected,slots;
    uint32_t rows[16];unsigned char rowvis[16];uint32_t rowrect[16][4];
    int empty_message,got_focus;unsigned count;struct {uint32_t id,a,b,c;} trace[256];
};
static struct impl_fixture *native_impl;
static _Alignas(uint32_t) unsigned char impl_mgr[0x40],impl_rows[16*8],impl_owner[16],impl_ptrarray[16*4];
static uint32_t impl_row_vtable[16],impl_owner_vtable[0x70/4];
static void impl_emit(struct impl_fixture *f,uint32_t id,uint32_t a,uint32_t b,uint32_t c)
{ assert(f->count<256);f->trace[f->count].id=id;f->trace[f->count].a=a;f->trace[f->count].b=b;f->trace[f->count].c=c;++f->count; }
static unsigned impl_row_index(void *row) { return *(uint32_t *)((unsigned char *)row+4); }
static unsigned native_i_setfocus(void *row,int32_t v)
{ impl_emit(native_impl,R_SET_FOCUS,impl_row_index(row),(uint32_t)(v!=0),0);return 0; }
static unsigned native_i_setvisible(void *row,int32_t v)
{ impl_emit(native_impl,R_SET_VISIBLE,impl_row_index(row),(uint32_t)(v!=0),0);return 0; }
typedef unsigned (*rect2_fn)(void *,void *);
typedef unsigned (*draw3_fn)(void *,uint32_t,void *);
static unsigned native_i_getrect(void *buf,void *row)
{ uint32_t i=impl_row_index(row);uint32_t *r=(uint32_t *)buf;
    r[0]=native_impl->rowrect[i][0];r[1]=native_impl->rowrect[i][1];r[2]=native_impl->rowrect[i][2];r[3]=native_impl->rowrect[i][3];
    impl_emit(native_impl,R_GET_RECT,i,0,0);return 0; }
static unsigned native_i_setpos(void *row,void *point)
{ uint32_t *p=(uint32_t *)point;impl_emit(native_impl,R_SET_POS,impl_row_index(row),p[0],p[1]);return 0; }
static unsigned native_i_isvisible(void *row)
{ impl_emit(native_impl,R_IS_VISIBLE,impl_row_index(row),0,0);return native_impl->rowvis[impl_row_index(row)]; }
static unsigned native_i_drawrow(void *owner,uint32_t index,void *row)
{ (void)owner;impl_emit(native_impl,R_DRAW,index,impl_row_index(row),0);return 0; }
static unsigned native_i_drawempty(void *owner,void *row)
{ (void)owner;impl_emit(native_impl,R_DRAW_EMPTY,impl_row_index(row),0,0);return 0; }
static unsigned native_i_empty(void *owner)
{ (void)owner;impl_emit(native_impl,R_EMPTY_MSG,0,0,0);return (unsigned)native_impl->empty_message; }
static unsigned native_i_gotfocus(void *owner)
{ (void)owner;impl_emit(native_impl,R_GOT_FOCUS,0,0,0);return (unsigned)native_impl->got_focus; }
static uint32_t impl_q_cap(void *p) { return ((struct impl_fixture *)p)->cap; }
static uint32_t impl_q_first(void *p) { return ((struct impl_fixture *)p)->first; }
static uint32_t impl_q_size(void *p) { return ((struct impl_fixture *)p)->size; }
static uint32_t impl_q_sel(void *p) { return ((struct impl_fixture *)p)->selected; }
static uint32_t impl_q_slots(void *p) { (void)p;return 16; }
static void *impl_q_row(void *p,uint32_t rel) { return (void *)(uintptr_t)((struct impl_fixture *)p)->rows[rel]; }
static void impl_s_row(void *p,uint32_t rel,void *row) { ((struct impl_fixture *)p)->rows[rel]=(uint32_t)(uintptr_t)row; }
static void *impl_q_owner(void *p) { (void)p;return (void *)(uintptr_t)0x99aa; }
static void impl_s_first(void *p,uint32_t v) { ((struct impl_fixture *)p)->first=v; }
static void impl_s_size(void *p,uint32_t v) { ((struct impl_fixture *)p)->size=v; }
static void impl_s_sel(void *p,uint32_t v) { ((struct impl_fixture *)p)->selected=v; }
static struct impl_fixture *ref_impl;
static int impl_q_vis(void *row) { impl_emit(ref_impl,R_IS_VISIBLE,(uint32_t)(uintptr_t)row,0,0);return ref_impl->rowvis[(uintptr_t)row]; }
static void impl_e_focus(void *row,int v) { impl_emit(ref_impl,R_SET_FOCUS,(uint32_t)(uintptr_t)row,(uint32_t)v,0); }
static void impl_e_show(void *row,int v) { impl_emit(ref_impl,R_SET_VISIBLE,(uint32_t)(uintptr_t)row,(uint32_t)v,0); }
static void impl_e_rect(void *row,uint32_t r[4]) { uint32_t i=(uint32_t)(uintptr_t)row;
    r[0]=ref_impl->rowrect[i][0];r[1]=ref_impl->rowrect[i][1];r[2]=ref_impl->rowrect[i][2];r[3]=ref_impl->rowrect[i][3];impl_emit(ref_impl,R_GET_RECT,i,0,0); }
static void impl_e_pos(void *row,uint32_t x,uint32_t y) { impl_emit(ref_impl,R_SET_POS,(uint32_t)(uintptr_t)row,x,y); }
static int impl_q_empty(void *p,void *o) { struct impl_fixture *f=p;(void)o;impl_emit(f,R_EMPTY_MSG,0,0,0);return f->empty_message; }
static void impl_e_draw(void *p,void *o,uint32_t i,void *row) { struct impl_fixture *f=p;(void)o;impl_emit(f,R_DRAW,i,(uint32_t)(uintptr_t)row,0); }
static void impl_e_drow(void *p,void *o,void *row) { struct impl_fixture *f=p;(void)o;impl_emit(f,R_DRAW_EMPTY,(uint32_t)(uintptr_t)row,0,0); }
static int impl_q_got(void *p,void *o) { struct impl_fixture *f=p;(void)o;impl_emit(f,R_GOT_FOCUS,0,0,0);return f->got_focus; }
static struct oa_svg_list_impl_ops impl_ops(struct impl_fixture *f)
{ return (struct oa_svg_list_impl_ops){f,impl_q_cap,impl_q_first,impl_q_size,impl_q_sel,impl_q_slots,impl_q_row,
    impl_s_row,impl_q_owner,impl_s_first,impl_s_size,impl_s_sel,impl_q_vis,impl_e_focus,impl_e_show,impl_e_rect,
    impl_e_pos,impl_q_empty,impl_e_draw,impl_e_drow,impl_q_got}; }
static void compare_impl_traces(struct impl_fixture *vendor,struct impl_fixture *ref,unsigned body,int full)
{
    if(vendor->count!=ref->count) {
        fprintf(stderr,"impl %u full=%d count native=%u ours=%u\n",body,full,vendor->count,ref->count);
        for(unsigned i=0;i<vendor->count;++i)fprintf(stderr,"  native[%u]=%u/%x/%x/%x\n",i,vendor->trace[i].id,
            vendor->trace[i].a,vendor->trace[i].b,vendor->trace[i].c);
        for(unsigned i=0;i<ref->count;++i)fprintf(stderr,"  ours[%u]=%u/%x/%x/%x\n",i,ref->trace[i].id,
            ref->trace[i].a,ref->trace[i].b,ref->trace[i].c);
        exit(1);
    }
    for(unsigned i=0;i<vendor->count;++i)if(vendor->trace[i].id!=ref->trace[i].id || vendor->trace[i].a!=ref->trace[i].a ||
        vendor->trace[i].b!=ref->trace[i].b || vendor->trace[i].c!=ref->trace[i].c) {
        fprintf(stderr,"impl %u trace %u native=%u/%x/%x/%x ours=%u/%x/%x/%x\n",body,i,vendor->trace[i].id,
            vendor->trace[i].a,vendor->trace[i].b,vendor->trace[i].c,ref->trace[i].id,ref->trace[i].a,ref->trace[i].b,ref->trace[i].c);exit(1);
    }
}
static void compare_rotate(rotate_fn native,int (*ours)(const struct oa_svg_list_impl_ops *,int32_t),
    struct impl_fixture initial,int32_t delta)
{
    struct impl_fixture vendor=initial,ref=initial;struct oa_svg_list_impl_ops ops=impl_ops(&ref);
    memcpy(impl_mgr+8,&vendor.cap,4);memcpy(impl_mgr+12,&vendor.first,4);memcpy(impl_mgr+16,&vendor.size,4);
    memcpy(impl_mgr+20,&vendor.selected,4);
    for(unsigned i=0;i<16;++i) { uint32_t slot=(uint32_t)(uintptr_t)&impl_rows[i*8];memcpy(impl_ptrarray+i*4,&slot,4); }
    uint32_t array=(uint32_t)(uintptr_t)impl_ptrarray;memcpy(impl_mgr+28,&array,4);
    uint32_t end=array+16u*4u;memcpy(impl_mgr+32,&end,4);
    uint32_t owner=(uint32_t)(uintptr_t)impl_owner;memcpy(impl_mgr+4,&owner,4);
    native_impl=&vendor;ref_impl=&ref;
    native(impl_mgr,delta);
    assert(!ours(&ops,delta));
    compare_impl_traces(&vendor,&ref,1,0);
    for(unsigned i=0;i<16;++i) {
        uint32_t slot;memcpy(&slot,impl_ptrarray+i*4,4);
        uint32_t got=*(uint32_t *)((unsigned char *)(uintptr_t)slot+4);
        if(got!=ref.rows[i]) { fprintf(stderr,"impl rotate row %u native=%u ours=%u delta=%d\n",i,got,ref.rows[i],delta);exit(1); }
    }
}
static void compare_impl(impl_fn native,int (*ours)(const struct oa_svg_list_impl_ops *,uint32_t,int),
    struct impl_fixture initial,uint32_t index,int full)
{
    struct impl_fixture vendor=initial,ref=initial;struct oa_svg_list_impl_ops ops=impl_ops(&ref);
    memcpy(impl_mgr+8,&vendor.cap,4);memcpy(impl_mgr+12,&vendor.first,4);memcpy(impl_mgr+16,&vendor.size,4);
    memcpy(impl_mgr+20,&vendor.selected,4);
    for(unsigned i=0;i<16;++i) { uint32_t slot=(uint32_t)(uintptr_t)&impl_rows[i*8];memcpy(impl_ptrarray+i*4,&slot,4); }
    uint32_t array=(uint32_t)(uintptr_t)impl_ptrarray;memcpy(impl_mgr+28,&array,4);
    uint32_t end=array+16u*4u;memcpy(impl_mgr+32,&end,4);
    uint32_t owner=(uint32_t)(uintptr_t)impl_owner;memcpy(impl_mgr+4,&owner,4);
    native_impl=&vendor;ref_impl=&ref;
    native(impl_mgr,index,full);
    assert(!ours(&ops,index,full));
    if(vendor.count!=ref.count) {
        fprintf(stderr,"impl update mismatch cap=%x first=%x size=%x sel=%x index=%x full=%d\n",
            initial.cap,initial.first,initial.size,initial.selected,index,full);
    }
    compare_impl_traces(&vendor,&ref,0,full);
    if(*(uint32_t *)(impl_mgr+12)!=ref.first || *(uint32_t *)(impl_mgr+16)!=ref.size || *(uint32_t *)(impl_mgr+20)!=ref.selected) {
        fprintf(stderr,"impl fields native=%x/%x/%x ours=%x/%x/%x\n",*(uint32_t *)(impl_mgr+12),*(uint32_t *)(impl_mgr+16),
            *(uint32_t *)(impl_mgr+20),ref.first,ref.size,ref.selected);exit(1);
    }
    for(unsigned i=0;i<16;++i) {
        uint32_t slot;memcpy(&slot,impl_ptrarray+i*4,4);
        uint32_t got=*(uint32_t *)((unsigned char *)(uintptr_t)slot+4);
        if(got!=ref.rows[i]) { fprintf(stderr,"impl row %u native=%u ours=%u\n",i,got,ref.rows[i]);exit(1); }
    }
}
static void compare_scroll(struct scroll_fixture initial)
{
    struct scroll_fixture vendor=initial,ref=initial;struct oa_svg_list_scroll_ops ops={&ref,scroll_q_size,
        scroll_q_total,scroll_q_flag,scroll_q_field,scroll_q_sb,scroll_q_list,scroll_q_visible,scroll_q_first,
        scroll_q_created,scroll_e_create,scroll_e_setvis,scroll_e_pos};
    memcpy(scroll_list+0x1f0,&vendor.fields[2],4);memcpy(scroll_list+0x1f4,&vendor.fields[0],4);
    memcpy(scroll_list+0x1f8,&vendor.fields[3],4);memcpy(scroll_list+0x1fc,&vendor.fields[1],4);
    memcpy(scroll_list+0x214,&vendor.fields[5],4);memcpy(scroll_list+0x21c,&vendor.fields[4],4);
    memcpy(scroll_list+0x22c,&vendor.flag,1);
    memcpy(scroll_list+0xf8+16,&vendor.size,4);memcpy(scroll_list+0xf8+12,&vendor.first,4);
    uint32_t vtable=(uint32_t)(uintptr_t)scroll_vtable;memcpy(scroll_list,&vtable,4);
    native_scroll=&vendor;
    native_sb_update(scroll_list);
    assert(!oa_svg_list_update_scrollbar(&ops));
    if(vendor.count!=ref.count) {
        fprintf(stderr,"scroll count native=%u ours=%u size=%x total=%x flag=%x vis=%x first=%x created=%d\n",
            vendor.count,ref.count,initial.size,initial.total,initial.flag,initial.visible,initial.first,initial.created);
        for(unsigned i=0;i<vendor.count;++i)fprintf(stderr,"  native[%u]=%u/%x/%x/%x/%x\n",i,vendor.trace[i].id,
            vendor.trace[i].a,vendor.trace[i].b,vendor.trace[i].c,vendor.trace[i].d);
        for(unsigned i=0;i<ref.count;++i)fprintf(stderr,"  ours[%u]=%u/%x/%x/%x/%x\n",i,ref.trace[i].id,
            ref.trace[i].a,ref.trace[i].b,ref.trace[i].c,ref.trace[i].d);
        exit(1);
    }
    for(unsigned i=0;i<vendor.count;++i)if(vendor.trace[i].id!=ref.trace[i].id || vendor.trace[i].a!=ref.trace[i].a ||
        vendor.trace[i].b!=ref.trace[i].b || vendor.trace[i].c!=ref.trace[i].c || vendor.trace[i].d!=ref.trace[i].d) {
        fprintf(stderr,"scroll trace %u native=%u/%x/%x/%x/%x ours=%u/%x/%x/%x/%x\n",i,vendor.trace[i].id,
            vendor.trace[i].a,vendor.trace[i].b,vendor.trace[i].c,vendor.trace[i].d,ref.trace[i].id,
            ref.trace[i].a,ref.trace[i].b,ref.trace[i].c,ref.trace[i].d);exit(1);
    }
}
static uint32_t rect_bits(rect2_fn f) { uint32_t v;_Static_assert(sizeof f==sizeof v,"fn width");memcpy(&v,&f,sizeof v);return v; }
static uint32_t draw3_bits(draw3_fn f) { uint32_t v;_Static_assert(sizeof f==sizeof v,"fn width");memcpy(&v,&f,sizeof v);return v; }
enum {MK_CREATED=40,MK_BG=41,MK_BAR=42,MK_POS=43,MK_CREATE=44};
struct scroll_bar_fixture {
    int created;uint32_t c8;uint32_t vis,size,first;
    unsigned count;struct {uint32_t id,a,b,c,d;} trace[24];
    uint32_t cx,cy,cw,ch;void *cparent;int ccreated;
    uint32_t pvis,psize,pfirst;int pset;
};
static struct scroll_bar_fixture *native_sbar;
static _Alignas(uint32_t) unsigned char sbar_obj[0x100];
static void sbar_emit(struct scroll_bar_fixture *f,uint32_t id,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{ assert(f->count<24);f->trace[f->count].id=id;f->trace[f->count].a=a;f->trace[f->count].b=b;f->trace[f->count].c=c;f->trace[f->count].d=d;++f->count; }
static unsigned native_sb_created2(void *sb)
{ assert(sb==sbar_obj);sbar_emit(native_sbar,MK_CREATED,0,0,0,0);return (unsigned)native_sbar->created; }
static unsigned native_sb_renderbg2(void *sb)
{ assert(sb==sbar_obj);sbar_emit(native_sbar,MK_BG,0,0,0,0);return 0; }
static unsigned native_sb_renderbar2(void *sb)
{ assert(sb==sbar_obj);sbar_emit(native_sbar,MK_BAR,0,0,0,0);return 0; }
static void native_sb_mkcreate(void *sb,uint32_t created,uint32_t rect,uint32_t parent)
{ assert(sb==sbar_obj);uint32_t *r=(uint32_t *)rect;
    sbar_emit(native_sbar,MK_CREATE,r[0],r[1],r[2],r[3]);(void)created;(void)parent; }
static void *scroll_q_sb2(void *p) { (void)p;return sbar_obj; }
static int scroll_q_created2(void *p,void *sb) { struct scroll_bar_fixture *f=p;sbar_emit(f,MK_CREATED,0,0,0,0);(void)sb;return f->created; }
static uint32_t scroll_q_flag2(void *p,void *sb) { (void)sb;return ((struct scroll_bar_fixture *)p)->c8; }
static void scroll_e_bg2(void *p,void *sb) { struct scroll_bar_fixture *f=p;sbar_emit(f,MK_BG,0,0,0,0);(void)sb; }
static void scroll_e_bar2(void *p,void *sb) { struct scroll_bar_fixture *f=p;sbar_emit(f,MK_BAR,0,0,0,0);(void)sb; }
static void scroll_s_pos2(void *p,void *sb,uint32_t vis,uint32_t size,uint32_t first)
{ struct scroll_bar_fixture *f=p;f->pvis=vis;f->psize=size;f->pfirst=first;f->pset=1;(void)sb; }
static void scroll_e_mk2(void *p,void *sb,int created,uint32_t x,uint32_t y,uint32_t w,uint32_t h,void *parent)
{ struct scroll_bar_fixture *f=p;sbar_emit(f,MK_CREATE,x,y,w,h);f->cx=x;f->cy=y;f->cw=w;f->ch=h;f->cparent=parent;f->ccreated=created;(void)sb; }
static struct oa_svg_list_scrollbar_ops sbar_ops(struct scroll_bar_fixture *f)
{ return (struct oa_svg_list_scrollbar_ops){f,scroll_q_sb2,scroll_q_created2,scroll_q_flag2,scroll_e_bg2,scroll_e_bar2,scroll_s_pos2,scroll_e_mk2}; }
typedef void (*create_fn)(void *,uint32_t,void *,void *);
static void compare_sbar_create(create_fn native,int (*ours)(const struct oa_svg_list_scrollbar_ops *,int,uint32_t,uint32_t,uint32_t,uint32_t,void *),
    struct scroll_bar_fixture initial,uint32_t x,uint32_t y,uint32_t w,uint32_t h)
{
    struct scroll_bar_fixture vendor=initial,ref=initial;struct oa_svg_list_scrollbar_ops ops=sbar_ops(&ref);
    static uint32_t rectbuf[4];
    rectbuf[0]=x;rectbuf[1]=y;rectbuf[2]=w;rectbuf[3]=h;
    memset(sbar_obj,0,0x100);
    native_sbar=&vendor;native(sbar_obj,initial.created,rectbuf,(void *)0x99aa);
    assert(!ours(&ops,initial.created,x,y,w,h,(void *)0x99aa));
    if(vendor.count!=ref.count) {
        fprintf(stderr,"scrollbar create count native=%u ours=%u\n",vendor.count,ref.count);exit(1);
    }
    for(unsigned i=0;i<vendor.count;++i)if(vendor.trace[i].id!=ref.trace[i].id || vendor.trace[i].a!=ref.trace[i].a ||
        vendor.trace[i].b!=ref.trace[i].b || vendor.trace[i].c!=ref.trace[i].c || vendor.trace[i].d!=ref.trace[i].d) {
        fprintf(stderr,"scrollbar create trace %u native=%u/%x/%x/%x/%x ours=%u/%x/%x/%x/%x\n",i,vendor.trace[i].id,
            vendor.trace[i].a,vendor.trace[i].b,vendor.trace[i].c,vendor.trace[i].d,ref.trace[i].id,
            ref.trace[i].a,ref.trace[i].b,ref.trace[i].c,ref.trace[i].d);exit(1);
    }
    if(ref.cx!=x || ref.cy!=y || ref.cw!=w || ref.ch!=h || ref.cparent!=(void *)0x99aa || ref.ccreated!=initial.created) {
        fprintf(stderr,"scrollbar create args mismatch\n");exit(1);
    }
    (void)initial;
}
static void compare_sbar_pos(quad_fn native,int (*ours)(const struct oa_svg_list_scrollbar_ops *,uint32_t,uint32_t,uint32_t),
    struct scroll_bar_fixture initial)
{
    struct scroll_bar_fixture vendor=initial,ref=initial;struct oa_svg_list_scrollbar_ops ops=sbar_ops(&ref);
    memset(sbar_obj,0,0x100);memcpy(sbar_obj+0xc8,&vendor.c8,1);
    native_sbar=&vendor;
    native(sbar_obj,initial.vis,initial.size,initial.first);
    assert(!ours(&ops,initial.vis,initial.size,initial.first));
    if(vendor.count!=ref.count) {
        fprintf(stderr,"scrollbar pos count native=%u ours=%u\n",vendor.count,ref.count);
        for(unsigned i=0;i<vendor.count;++i)fprintf(stderr,"  native[%u]=%u/%x/%x/%x/%x\n",i,vendor.trace[i].id,vendor.trace[i].a,vendor.trace[i].b,vendor.trace[i].c,vendor.trace[i].d);
        for(unsigned i=0;i<ref.count;++i)fprintf(stderr,"  ours[%u]=%u/%x/%x/%x/%x\n",i,ref.trace[i].id,ref.trace[i].a,ref.trace[i].b,ref.trace[i].c,ref.trace[i].d);
        exit(1);
    }
    for(unsigned i=0;i<vendor.count;++i)if(vendor.trace[i].id!=ref.trace[i].id || vendor.trace[i].a!=ref.trace[i].a ||
        vendor.trace[i].b!=ref.trace[i].b || vendor.trace[i].c!=ref.trace[i].c || vendor.trace[i].d!=ref.trace[i].d) {
        fprintf(stderr,"scrollbar pos trace %u native=%u/%x/%x/%x/%x ours=%u/%x/%x/%x/%x\n",i,vendor.trace[i].id,
            vendor.trace[i].a,vendor.trace[i].b,vendor.trace[i].c,vendor.trace[i].d,ref.trace[i].id,ref.trace[i].a,ref.trace[i].b,ref.trace[i].c,ref.trace[i].d);exit(1);
    }
    if(ref.pset && (*(uint32_t *)(sbar_obj+0xbc)!=ref.pvis || *(uint32_t *)(sbar_obj+0xc0)!=ref.psize || *(uint32_t *)(sbar_obj+0xc4)!=ref.pfirst)) {
        fprintf(stderr,"scrollbar pos store native=%x/%x/%x ours=%x/%x/%x\n",*(uint32_t *)(sbar_obj+0xbc),*(uint32_t *)(sbar_obj+0xc0),
            *(uint32_t *)(sbar_obj+0xc4),ref.pvis,ref.psize,ref.pfirst);exit(1);
    }
    if(!ref.pset && (*(uint32_t *)(sbar_obj+0xbc) || *(uint32_t *)(sbar_obj+0xc0) || *(uint32_t *)(sbar_obj+0xc4))) {
        fprintf(stderr,"scrollbar pos unexpected store\n");exit(1);
    }
}
static void check_svg_scrollbar(const unsigned char *elf,size_t bytes)
{
    unsigned char *bar=page(0x10c4000,4096),*win=page(0x1152000,4096),*rows2=page(0x10c3000,4096);
    const struct {uint32_t address,length,crc,literal;} fragments[]={
        {0x10c401c,52,UINT32_C(0x52a07cab),0x10c404c},
        {0x10c4138,112,UINT32_C(0x71cda0c0),0x10c41a4}
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;create_fn create=0;quad_fn setpos=0;
    for(unsigned i=0;i<2;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(bar+fragments[i].address-0x10c4000,elf+off,fragments[i].length);
        memcpy(bar+fragments[i].literal-0x10c4000,&pointer,4);
        uint32_t entry=fragments[i].address|1;
        if(!i)memcpy(&create,&entry,4);else memcpy(&setpos,&entry,4);
    }
    thumb_stub(win,0x1152000,0x1152a40,native_sb_created2);
    assert(!mapped_range(bar,0x10c4000,0x10c3f50,16));
    assert(!mapped_range(bar,0x10c4000,0x10c4ffc,8));
    quad_stub(rows2,0x10c3000,0x10c3f50,native_sb_mkcreate);
    thumb_stub(rows2,0x10c3000,0x10c3658,native_sb_renderbg2);
    thumb_stub(rows2,0x10c3000,0x10c3bac,native_sb_renderbar2);
    executable(bar,4096);executable(win,4096);executable(rows2,4096);
    {
        const uint32_t viss[]={0,1,5,UINT32_C(0x80000000)},sizes[]={0,1,5},firsts[]={0,1,5,UINT32_C(0x80000000)};
        const int creates[]={0,1},c8s[]={0,1,7};unsigned compared=0;
        for(unsigned v=0;v<4;++v)for(unsigned s=0;s<3;++s)for(unsigned f=0;f<4;++f)
        for(unsigned c=0;c<2;++c)for(unsigned g=0;g<3;++g) {
            struct scroll_bar_fixture initial={.created=creates[c],.c8=(uint32_t)c8s[g]};
            initial.vis=viss[v];initial.size=sizes[s];initial.first=firsts[f];
            compare_sbar_pos(setpos,oa_svg_list_scrollbar_set_position,initial);++compared;
        }
        const uint32_t rects[][4]={{0,0,0,0},{3,7,11,13},{1,2,3,4}};
        for(unsigned c=0;c<2;++c)for(unsigned r=0;r<3;++r) {
            struct scroll_bar_fixture initial={.created=c};
            compare_sbar_create(create,oa_svg_list_scrollbar_create,initial,
                rects[r][0],rects[r][1],rects[r][2],rects[r][3]);++compared;
        }
        native_sbar=NULL;
        assert(!munmap(bar,4096) && !munmap(win,4096) && !munmap(rows2,4096));
        printf("ok offline shipped ARM SVG list scrollbar bodies: %u native/C comparisons, create forward and setpos gating/store/show match\n",compared);
    }
}
enum {U_TOTAL,U_STORE,U_FIRST,U_FULL,U_SCROLL,U_ANIM};
struct update_fixture {
    uint32_t total,aux,first,nrows;void *slots[16];
    unsigned count;struct {uint32_t id,a,b;} trace[24];
};
static struct update_fixture *native_up;
static _Alignas(uint32_t) unsigned char upd_list[0x300],upd_rows[16*8],upd_ptrs[16*4];
static _Alignas(uint32_t) uint32_t upd_vtable[0x70/4];
static void up_emit(struct update_fixture *f,uint32_t id,uint32_t a,uint32_t b)
{ assert(f->count<24);f->trace[f->count].id=id;f->trace[f->count].a=a;f->trace[f->count].b=b;++f->count; }
static unsigned native_up_total(void *p) { assert(p==upd_list);up_emit(native_up,U_TOTAL,0,0);return native_up->total; }
static void native_up_full(void *p,uint32_t i,uint32_t t,uint32_t ignored)
{ (void)ignored;assert(p==upd_list+0xf8);up_emit(native_up,U_FULL,i,t); }
static unsigned native_up_scroll(void *p) { assert(p==upd_list);up_emit(native_up,U_SCROLL,0,0);return 0; }
static unsigned native_up_anim(void *p) { assert(p==upd_list);up_emit(native_up,U_ANIM,0,0);return 0; }
static uint32_t up_q_total(void *p) { struct update_fixture *f=p;up_emit(f,U_TOTAL,0,0);return f->total; }
static void up_s_store(void *p,uint32_t v) { struct update_fixture *f=p;up_emit(f,U_STORE,v,0);f->aux=v; }
static uint32_t up_q_first(void *p) { struct update_fixture *f=p;up_emit(f,U_FIRST,0,0);return f->first; }
static void up_e_full(void *p,uint32_t i,uint32_t t) { struct update_fixture *f=p;up_emit(f,U_FULL,i,t); }
static void up_e_scroll(void *p) { struct update_fixture *f=p;up_emit(f,U_SCROLL,0,0); }
static void up_e_anim(void *p) { struct update_fixture *f=p;up_emit(f,U_ANIM,0,0); }
static struct oa_svg_list_update_ops up_ops(struct update_fixture *f)
{ return (struct oa_svg_list_update_ops){f,up_q_total,up_s_store,up_q_first,up_e_full,up_e_scroll,up_e_anim}; }
typedef void (*update_fn)(void *);
static void compare_update(update_fn native,int (*ours)(const struct oa_svg_list_update_ops *),
    struct update_fixture initial)
{
    struct update_fixture vendor=initial,ref=initial;struct oa_svg_list_update_ops ops=up_ops(&ref);
    uint32_t vtable=(uint32_t)(uintptr_t)upd_vtable;memcpy(upd_list,&vtable,4);
    for(unsigned i=0;i<16;++i) { uint32_t slot=(uint32_t)(uintptr_t)&upd_rows[i*8];memcpy(upd_ptrs+i*4,&slot,4); }
    uint32_t array=(uint32_t)(uintptr_t)upd_ptrs;memcpy(upd_list+0xf8+28,&array,4);
    uint32_t end=array+(uint32_t)initial.nrows*4u;memcpy(upd_list+0xf8+32,&end,4);
    memcpy(upd_list+0xf8+12,&vendor.first,4);
    uint32_t size=7;memcpy(upd_list+0xf8+16,&size,4);
    native_up=&vendor;
    { void (*volatile ncall)(void *)=native;ncall(upd_list); }
    assert(!ours(&ops));
    {
        unsigned rn=0;struct {uint32_t id,a,b;} filt[24];
        for(unsigned i=0;i<ref.count;++i) {
            if(ref.trace[i].id==U_STORE || ref.trace[i].id==U_FIRST)continue;
            assert(rn<24);filt[rn].id=ref.trace[i].id;filt[rn].a=ref.trace[i].a;filt[rn].b=ref.trace[i].b;++rn;
        }
        if(vendor.count!=rn) {
            fprintf(stderr,"update count native=%u ours=%u total=%x first=%x\n",vendor.count,rn,initial.total,initial.first);
            for(unsigned i=0;i<vendor.count;++i)fprintf(stderr,"  native[%u]=%u/%x/%x\n",i,vendor.trace[i].id,vendor.trace[i].a,vendor.trace[i].b);
            for(unsigned i=0;i<rn;++i)fprintf(stderr,"  ours[%u]=%u/%x/%x\n",i,filt[i].id,filt[i].a,filt[i].b);
            exit(1);
        }
        for(unsigned i=0;i<vendor.count;++i)if(vendor.trace[i].id!=filt[i].id || vendor.trace[i].a!=filt[i].a ||
            vendor.trace[i].b!=filt[i].b) {
            fprintf(stderr,"update trace %u native=%u/%x/%x ours=%u/%x/%x\n",i,vendor.trace[i].id,
                vendor.trace[i].a,vendor.trace[i].b,filt[i].id,filt[i].a,filt[i].b);exit(1);
        }
    }
    if(*(uint32_t *)(upd_list+0x1ec)!=ref.aux) {
        fprintf(stderr,"update count word native=%x ours=%x\n",*(uint32_t *)(upd_list+0x1ec),ref.aux);exit(1);
    }
    (void)native_up;
}
struct rowview_fixture { uint32_t first,size,n;uint32_t addrs[16]; };
static _Alignas(uint32_t) unsigned char rv_mgr[0x40],rv_ptrs[16*4],rv_slots[16][8];
static uint32_t rv_q_first(void *p) { return ((struct rowview_fixture *)p)->first; }
static uint32_t rv_q_size(void *p) { return ((struct rowview_fixture *)p)->size; }
static uint32_t rv_q_sel(void *p) { (void)p;return 0; }
static uint32_t rv_q_slots(void *p) { return ((struct rowview_fixture *)p)->n; }
static void *rv_q_row(void *p,uint32_t rel)
{ struct rowview_fixture *f=p;assert(rel<16);return (void *)(uintptr_t)f->addrs[rel]; }
static void rv_e_focus(void *row,int v) { (void)row;(void)v; }
static void rv_e_store(void *p,uint32_t v) { (void)p;(void)v; }
static void rv_e_impl(void *p,uint32_t i,int fl) { (void)p;(void)i;(void)fl; }
static struct oa_svg_list_manager_ops rv_ops(struct rowview_fixture *f)
{ return (struct oa_svg_list_manager_ops){f,rv_q_first,rv_q_size,rv_q_sel,rv_q_row,rv_e_focus,rv_e_store,rv_e_impl,rv_q_slots}; }
typedef uint32_t (*rows0_fn)(void *);
typedef void *(*rowidx_fn)(void *,uint32_t);
static void check_svg_rowview(const unsigned char *elf,size_t bytes)
{
    unsigned char *rows=page(0x10be000,4096);
    const struct {uint32_t address,length,crc,literal;} fragments[]={
        {0x10be714,56,UINT32_C(0x80e1c9cf),0x10be748},
        {0x10be7ac,56,UINT32_C(0xec0fa064),0x10be7e0},
        {0x10be848,72,UINT32_C(0xcc441597),0x10be88c}
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;rows0_fn total=0,last=0;rowidx_fn item=0;
    for(unsigned i=0;i<3;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(rows+fragments[i].address-0x10be000,elf+off,fragments[i].length);
        memcpy(rows+fragments[i].literal-0x10be000,&pointer,4);
        uint32_t entry=fragments[i].address|1;
        if(!i)memcpy(&total,&entry,4);else if(i==1)memcpy(&last,&entry,4);else memcpy(&item,&entry,4);
    }
    executable(rows,4096);
    {
        unsigned compared=0;
        for(unsigned n=0;n<4;++n) {
            uint32_t ns[]={0,1,4,16};
            struct rowview_fixture initial={.n=ns[n]};
            struct rowview_fixture ref=initial;struct oa_svg_list_manager_ops ops=rv_ops(&ref);
            for(unsigned i=0;i<16;++i) { uint32_t slot=(uint32_t)(uintptr_t)&rv_slots[i];memcpy(rv_ptrs+i*4,&slot,4); }
            for(unsigned i=0;i<16;++i)initial.addrs[i]=ref.addrs[i]=0x5000u+i;
            uint32_t array=(uint32_t)(uintptr_t)rv_ptrs;memcpy(rv_mgr+28,&array,4);
            uint32_t end=array+ns[n]*4u;memcpy(rv_mgr+32,&end,4);
            int error=-1;uint32_t v;{ rows0_fn volatile tcall=total;v=tcall(rv_mgr); }
            uint32_t got=oa_svg_list_manager_total_rows(&ops,&error);
            if(error || v!=got) {
                fprintf(stderr,"rowview total mismatch n=%x native=%x ours=%x err=%d\n",ns[n],v,got,error);exit(1);
            }
            ++compared;
        }
        const uint32_t firsts[]={0,1,5,UINT32_C(0xfffffffe)},sizes[]={0,1,4};
        for(unsigned a=0;a<4;++a)for(unsigned s=0;s<3;++s) {
            struct rowview_fixture initial={.first=firsts[a],.size=sizes[s],.n=16};
            struct rowview_fixture ref=initial;struct oa_svg_list_manager_ops ops=rv_ops(&ref);
            for(unsigned i=0;i<16;++i) { uint32_t slot=(uint32_t)(uintptr_t)&rv_slots[i];memcpy(rv_ptrs+i*4,&slot,4); }
            for(unsigned i=0;i<16;++i)initial.addrs[i]=ref.addrs[i]=0x5000u+i;
            uint32_t array=(uint32_t)(uintptr_t)rv_ptrs;memcpy(rv_mgr+28,&array,4);
            uint32_t end=array+16u*4u;memcpy(rv_mgr+32,&end,4);
            memcpy(rv_mgr+12,&initial.first,4);memcpy(rv_mgr+16,&initial.size,4);
            int error=-1;uint32_t v;{ rows0_fn volatile lcall=last;v=lcall(rv_mgr); }
            uint32_t e=oa_svg_list_manager_last_visible(&ops,&error);
            if(v!=e || error) { fprintf(stderr,"rowview last mismatch first=%x size=%x native=%x ours=%x\n",initial.first,initial.size,v,e);exit(1); }
            ++compared;
        }
        const uint32_t indices[]={0,1,3,4,5,UINT32_C(0xffffffff),UINT32_C(0x80000000)};
        for(unsigned a=0;a<4;++a)for(unsigned s=0;s<3;++s)for(unsigned k=0;k<7;++k) {
            struct rowview_fixture initial={.first=firsts[a],.size=sizes[s],.n=16};
            struct rowview_fixture ref=initial;struct oa_svg_list_manager_ops ops=rv_ops(&ref);
            for(unsigned i=0;i<16;++i) { uint32_t slot=(uint32_t)(uintptr_t)&rv_slots[i];memcpy(rv_ptrs+i*4,&slot,4); }
            for(unsigned i=0;i<16;++i) { uint32_t slot;memcpy(&slot,rv_ptrs+i*4,4);initial.addrs[i]=ref.addrs[i]=slot; }
            uint32_t array=(uint32_t)(uintptr_t)rv_ptrs;memcpy(rv_mgr+28,&array,4);
            uint32_t end=array+16u*4u;memcpy(rv_mgr+32,&end,4);
            memcpy(rv_mgr+12,&initial.first,4);memcpy(rv_mgr+16,&initial.size,4);
            void *v;{ rowidx_fn volatile icall=item;v=icall(rv_mgr,indices[k]); }
            int error2=-1;void *e=oa_svg_list_manager_item_row(&ops,indices[k],&error2);
            if(v!=e || error2) {
                fprintf(stderr,"rowview item mismatch first=%x size=%x index=%x native=%p ours=%p\n",
                    initial.first,initial.size,indices[k],v,e);exit(1);
            }
            ++compared;
        }
        native_up=NULL;
        assert(!munmap(rows,4096));
        printf("ok offline shipped ARM SVG list rowview: %u native/C comparisons, total/last-visible/item-row match\n",compared);
    }
}
enum {W_GETNUM=60,W_CREATED,W_SELITEM,W_IDTABLE};
struct widget_fixture {
    int created;uint32_t selected,numeric;
    unsigned count;struct {uint32_t id,a;} trace[8];
};
static struct widget_fixture *native_widget;
static _Alignas(uint32_t) unsigned char widget_row[0x2000],widget_list[0x400];
static _Alignas(uint32_t) unsigned char widget_entries[4][8],widget_array[4*4];
static _Alignas(uint32_t) uint32_t widget_cell;
static void widget_emit(struct widget_fixture *f,uint32_t id,uint32_t a)
{ assert(f->count<8);f->trace[f->count].id=id;f->trace[f->count].a=a;++f->count; }
static unsigned native_w_getnum(void *entry)
{ assert(entry==widget_row+0x18d8);widget_emit(native_widget,W_GETNUM,(uint32_t)(uintptr_t)entry);return native_widget->numeric; }
static unsigned native_w_created(void *list)
{ assert(list==widget_list);widget_emit(native_widget,W_CREATED,0);return (unsigned)native_widget->created; }
static unsigned native_w_selitem(void *list)
{ assert(list==widget_list);widget_emit(native_widget,W_SELITEM,0);return native_widget->selected; }
static struct widget_fixture *ref_widget;
static void *widget_e_entry(void *row) { (void)row;return widget_row+0x18d8; }
static uint32_t widget_e_num(void *entry)
{ assert(entry==widget_row+0x18d8);widget_emit(ref_widget,W_GETNUM,(uint32_t)(uintptr_t)entry);return ref_widget->numeric; }
static int widget_e_created(void *list) { (void)list;widget_emit(ref_widget,W_CREATED,0);return ref_widget->created; }
static uint32_t widget_e_sel(void *list) { (void)list;widget_emit(ref_widget,W_SELITEM,0);return ref_widget->selected; }
static void *widget_e_table(void *list) { (void)list;return widget_array; }
static void *widget_e_ident(void *table,uint32_t index) { (void)table;return widget_entries[index&3]; }
static uint32_t widget_e_id(void *entry) { return *(uint32_t *)entry; }
typedef uint32_t (*widget0_fn)(void *);
static void compare_widget_traces(struct widget_fixture *vendor,struct widget_fixture *ref)
{
    unsigned rn=0;uint32_t ids[8],args[8];
    for(unsigned i=0;i<ref->count;++i) {
        if(ref->trace[i].id==W_GETNUM || ref->trace[i].id==W_CREATED ||
           ref->trace[i].id==W_SELITEM || ref->trace[i].id==W_IDTABLE) {
            assert(rn<8);ids[rn]=ref->trace[i].id;args[rn]=ref->trace[i].a;++rn;
        }
    }
    if(vendor->count!=rn) {
        fprintf(stderr,"widget count native=%u ours=%u\n",vendor->count,rn);exit(1);
    }
    for(unsigned i=0;i<vendor->count;++i)if(vendor->trace[i].id!=ids[i] || vendor->trace[i].a!=args[i]) {
        fprintf(stderr,"widget trace %u native=%u/%x ours=%u/%x\n",i,vendor->trace[i].id,
            vendor->trace[i].a,ids[i],args[i]);exit(1);
    }
}
enum {SL_PROGRESS,SL_CREATED,SL_SLIDER,SL_STORE,SL_BAR,SL_BOX,SL_FORMAT,SL_NEW,SL_APPEND,SL_SET,SL_FREE};
struct slide_fixture {
    uint32_t fa,fb,type;int bar_on,box_on;int32_t stored;
    unsigned count;struct {uint32_t id,a,b;} trace[24];
};
static struct slide_fixture *native_slide;
static _Alignas(uint32_t) unsigned char slide_row[0x3000];
static void slide_emit(struct slide_fixture *f,uint32_t id,uint32_t a,uint32_t b)
{ assert(f->count<24);f->trace[f->count].id=id;f->trace[f->count].a=a;f->trace[f->count].b=b;++f->count; }
static void native_sl_progress(void *row,uint32_t value)
{ assert(row==slide_row);slide_emit(native_slide,SL_PROGRESS,(uint32_t)(uintptr_t)row,value); }
static unsigned native_sl_created(void *h)
{ assert(h==slide_row+0x1498 || h==slide_row+0x13c0 || h==slide_row+0x1ca8);
  slide_emit(native_slide,SL_CREATED,(uint32_t)(uintptr_t)h,0);
  if(h==slide_row+0x1ca8) { return (unsigned)native_slide->box_on; }
  return (unsigned)native_slide->bar_on; }
static void native_sl_slider(void *bar,uint32_t value)
{ slide_emit(native_slide,SL_SLIDER,(uint32_t)(uintptr_t)bar,value); }
static void native_sl_new(void *buf,uint32_t fmt)
{ (void)buf;slide_emit(native_slide,SL_NEW,fmt,0); }
static void native_sl_append(void *buf,uint32_t value)
{ (void)buf;slide_emit(native_slide,SL_APPEND,value,0); }
static void native_sl_set(void *box,uint32_t text)
{ (void)text;slide_emit(native_slide,SL_SET,(uint32_t)(uintptr_t)box,0); }
static void native_sl_free(void *buf,uint32_t ignored) { (void)buf;(void)ignored;slide_emit(native_slide,SL_FREE,0,0); }
static const char slide_format_empty[]="";
static struct slide_fixture *ref_slide;
static uint32_t slide_e_fa(void *row) { (void)row;return ref_slide->fa; }
static uint32_t slide_e_fb(void *row) { (void)row;return ref_slide->fb; }
static uint32_t slide_e_type(void *row) { (void)row;return ref_slide->type; }
static void slide_e_progress(void *row,int value)
{ slide_emit(ref_slide,SL_PROGRESS,(uint32_t)(uintptr_t)row,(uint32_t)value); }
static void *slide_e_bar(void *row,uint32_t offset)
{ slide_emit(ref_slide,SL_BAR,(uint32_t)(uintptr_t)row,offset);return (unsigned char *)row+offset; }
static int slide_e_created(void *h)
{ slide_emit(ref_slide,SL_CREATED,(uint32_t)(uintptr_t)h,0);
  if(h==slide_row+0x1ca8) { return ref_slide->box_on; }
  return ref_slide->bar_on; }
static void slide_e_slider(void *bar,int value)
{ slide_emit(ref_slide,SL_SLIDER,(uint32_t)(uintptr_t)bar,(uint32_t)value); }
static void slide_e_store(void *row,int value)
{ slide_emit(ref_slide,SL_STORE,(uint32_t)(uintptr_t)row,(uint32_t)value);ref_slide->stored=value; }
static void *slide_e_box(void *row)
{ slide_emit(ref_slide,SL_BOX,(uint32_t)(uintptr_t)row,0);return (unsigned char *)row+0x1ca8; }
static const char *slide_e_format(void *row) { (void)row;slide_emit(ref_slide,SL_FORMAT,0,0);return slide_format_empty; }
static void slide_e_new(void *buf,const char *fmt)
{ (void)buf;slide_emit(ref_slide,SL_NEW,(uint32_t)(uintptr_t)fmt,0); }
static void slide_e_append(void *buf,int value) { (void)buf;slide_emit(ref_slide,SL_APPEND,(uint32_t)value,0); }
static void slide_e_set(void *box,const void *text)
{ (void)text;slide_emit(ref_slide,SL_SET,(uint32_t)(uintptr_t)box,0); }
static void slide_e_free(void *buf) { (void)buf;slide_emit(ref_slide,SL_FREE,0,0); }
static struct oa_svg_row_slide_ops slide_ops(struct slide_fixture *f)
{ return (struct oa_svg_row_slide_ops){f,slide_e_fa,slide_e_fb,slide_e_type,slide_e_progress,slide_e_bar,
    slide_e_created,slide_e_slider,slide_e_store,slide_e_box,slide_e_format,slide_e_new,slide_e_append,slide_e_set,slide_e_free}; }
typedef void (*slide_fn)(void *,int32_t);
static void compare_slide_traces(struct slide_fixture *vendor,struct slide_fixture *ref)
{
    unsigned rn=0;uint32_t ids[24],args[24],args2[24];
    for(unsigned i=0;i<ref->count;++i) {
        if(ref->trace[i].id==SL_BAR || ref->trace[i].id==SL_BOX ||
           ref->trace[i].id==SL_STORE || ref->trace[i].id==SL_FORMAT)continue;
        assert(rn<24);ids[rn]=ref->trace[i].id;args[rn]=ref->trace[i].a;args2[rn]=ref->trace[i].b;++rn;
    }
    if(vendor->count!=rn) {
        fprintf(stderr,"slide count native=%u ours=%u\n",vendor->count,rn);
        for(unsigned i=0;i<vendor->count;++i)fprintf(stderr,"  native[%u]=%u/%x/%x\n",i,vendor->trace[i].id,vendor->trace[i].a,vendor->trace[i].b);
        for(unsigned i=0;i<rn;++i)fprintf(stderr,"  ours[%u]=%u/%x/%x\n",i,ids[i],args[i],args2[i]);
        exit(1);
    }
    for(unsigned i=0;i<vendor->count;++i)if(vendor->trace[i].id!=ids[i] || vendor->trace[i].a!=args[i] ||
        vendor->trace[i].b!=args2[i]) {
        fprintf(stderr,"slide trace %u native=%u/%x/%x ours=%u/%x/%x\n",i,vendor->trace[i].id,
            vendor->trace[i].a,vendor->trace[i].b,ids[i],args[i],args2[i]);exit(1);
    }
}
static void check_svg_slide(const unsigned char *elf,size_t bytes)
{
    unsigned char *rowpage=page(0xd52000,8192),*winpage=page(0x1152000,4096);
    unsigned char *barpage=page(0x10c4000,4096),*textpage=page(0x10c7000,4096);
    unsigned char *ustrpage=page(0xc86000,4096),*usoppage=page(0xc88000,4096);
    size_t off=file_offset(elf,bytes,0xd52efc,272);
    assert(crc32(elf+off,272)==UINT32_C(0x621d5980));
    memcpy(rowpage+0xefc,elf+off,272);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;memcpy(rowpage+0x1004,&pointer,4);
    uint32_t fmt=(uint32_t)(uintptr_t)slide_format_empty;memcpy(rowpage+0x1008,&fmt,4);
    pair_stub(rowpage,0xd52000,0xd52e40,native_sl_progress);
    thumb_stub(winpage,0x1152000,0x1152a40,native_sl_created);
    pair_stub(barpage,0x10c4000,0x10c493c,native_sl_slider);
    pair_stub(ustrpage,0xc86000,0xc86744,native_sl_new);
    pair_stub(ustrpage,0xc86000,0xc86554,native_sl_free);
    pair_stub(usoppage,0xc88000,0xc8865c,native_sl_append);
    pair_stub(textpage,0x10c7000,0x10c7828,native_sl_set);
    executable(rowpage,8192);executable(winpage,4096);executable(barpage,4096);
    executable(ustrpage,4096);executable(usoppage,4096);executable(textpage,4096);
    {
        unsigned compared=0;slide_fn slide=0;
        { uint32_t entry=0xd52efd;memcpy(&slide,&entry,4); }
        const uint32_t fafb[]={0,1},types[]={9,23,7},values[]={0,5,UINT32_C(0xfffffffd)};
        const int creates[]={0,1};
        for(unsigned a=0;a<2;++a)for(unsigned b=0;b<2;++b)for(unsigned t=0;t<3;++t)
        for(unsigned v=0;v<3;++v)for(unsigned c=0;c<2;++c) {
            struct slide_fixture initial={.fa=fafb[a],.fb=fafb[b],.type=types[t],
                .bar_on=creates[c],.box_on=creates[(c+t)&1]};
            struct slide_fixture vendor=initial,ref=initial;struct oa_svg_row_slide_ops ops=slide_ops(&ref);
            native_slide=&vendor;ref_slide=&ref;
            slide_row[0x2e47]=(unsigned char)initial.fa;slide_row[0x2e48]=(unsigned char)initial.fb;
            { uint32_t t=initial.type;memcpy(slide_row+0x2dec,&t,4); }
            { volatile slide_fn call=slide;call(slide_row,(int)values[v]); }
            assert(!oa_setting_row_set_slide(&ops,slide_row,(int)values[v]));
            compare_slide_traces(&vendor,&ref);
            {
                int stored=0;
                for(unsigned i=0;i<ref.count;++i)if(ref.trace[i].id==SL_STORE)stored=1;
                if(stored && *(uint32_t *)(slide_row+0x2df8)!=(uint32_t)ref.stored) {
                    fprintf(stderr,"slide store native=%x ours=%x fa=%u fb=%u type=%u\n",
                        *(uint32_t *)(slide_row+0x2df8),ref.stored,initial.fa,initial.fb,initial.type);exit(1);
                }
            }
            ++compared;
        }
        native_slide=NULL;ref_slide=NULL;
        assert(!munmap(rowpage,8192) && !munmap(winpage,4096) && !munmap(barpage,4096));
        assert(!munmap(ustrpage,4096) && !munmap(usoppage,4096) && !munmap(textpage,4096));
        printf("ok offline shipped ARM SVG list slide: %u native/C comparisons, progress/slider/text paths match\n",compared);
    }
}
typedef unsigned (*eq_fn)(const void *,const void *);
static uint32_t eq_bits(eq_fn f) { uint32_t v;memcpy(&v,&f,4);return v; }
typedef void (*start_fn)(void *,void *,float);
static uint32_t start_bits(start_fn f) { uint32_t v;memcpy(&v,&f,4);return v; }
typedef double (*dtoa_fn)(const char *);
static uint32_t dtoa_bits(dtoa_fn f) { uint32_t v;memcpy(&v,&f,4);return v; }
static struct anim_fixture *native_anim;
static _Alignas(uint32_t) unsigned char anim_list[0x400],anim_row[16],anim_row_vtable[64],anim_handles[4][32];
static unsigned native_a_enabled(void *l) { return (unsigned)af_enabled(native_anim,l); }
static unsigned native_a_sel(void *m) { return af_selected(native_anim,m); }
static unsigned native_a_itemrow(void *m,int32_t i) { return (uint32_t)(uintptr_t)af_row(native_anim,m,(uint32_t)i); }
static unsigned native_a_rect(void *out,void *row) { af_rect(native_anim,row,out);return 0; }
static void native_a_stdname(void *out,uint32_t list,uint32_t id,uint32_t ignored)
{ (void)ignored;*(void **)out=af_std(native_anim,(void *)(uintptr_t)list,id);assert(*(void **)out); }
static unsigned native_a_count(void *c) { return af_count(native_anim,c); }
static unsigned native_a_at(void *c,int32_t i)
{ return af_at(native_anim,c,(uint32_t)i)?(uint32_t)(uintptr_t)anim_handles[i]:0; }
static void native_a_new(void *out,uint32_t str)
{ *(void **)out=af_new(native_anim,(const char *)(uintptr_t)str);assert(*(void **)out); }
static unsigned native_a_len(void *s) { return af_len(native_anim,*(void **)s); }
static void native_a_sub(void *out,uint32_t src,uint32_t start,uint32_t length)
{ *(void **)out=af_sub(native_anim,*(void **)(uintptr_t)src,start,length);assert(*(void **)out); }
static unsigned native_a_eq(const void *a,const void *b)
{ return (unsigned)af_equal(native_anim,*(void *const *)a,*(void *const *)b); }
static unsigned native_a_free(void *s) { af_free(native_anim,*(void **)s);return 0; }
static unsigned native_a_ctrl(void *owner) { return (uint32_t)(uintptr_t)af_ctrl(native_anim,owner); }
static void native_a_start(void *ctrl,void *target,float value) { af_start(native_anim,ctrl,target,value); }
static double native_a_parse(const char *text) { return af_parse(native_anim,text); }
typedef void (*anim_fn)(void *);
static void compare_anim(anim_fn native,struct anim_fixture initial)
{
    struct anim_fixture vendor=initial,ref=initial;
    vendor.list=ref.list=anim_list;vendor.manager=ref.manager=anim_list+0xf8;vendor.row=ref.row=anim_row;
    memcpy(anim_list+0x24c,vendor.prev,16);memcpy(anim_list+0x25c,vendor.cur,16);memcpy(anim_list+4,&vendor.owner,4);
    for(unsigned i=0;i<4;++i) { memcpy(anim_handles[i]+12,&vendor.names[i],4);memcpy(anim_handles[i]+28,&vendor.durs[i],4); }
    native_anim=&vendor;native(anim_list);
    struct oa_svg_list_anim_ops ops=anim_ops(&ref);float output=-1.0f;
    assert(!oa_svg_list_start_animation(&ops,anim_list,anim_list+0xf8,&output));
    assert(vendor.started==ref.started && (!vendor.started || vendor.started_bits==ref.started_bits));
    assert(!memcmp(anim_list+0x24c,ref.prev,16) && !memcmp(anim_list+0x25c,ref.cur,16));
    if(vendor.started) { uint32_t bits;memcpy(&bits,&output,4);assert(bits==vendor.started_bits); }
    else assert(output==-1.0f);
    if(vendor.count!=ref.count) { fprintf(stderr,"animation trace count %u/%u\n",vendor.count,ref.count);exit(1); }
    for(unsigned i=0;i<ref.count;++i)if(vendor.trace[i].id!=ref.trace[i].id || vendor.trace[i].a!=ref.trace[i].a || vendor.trace[i].b!=ref.trace[i].b) {
        fprintf(stderr,"animation trace %u native=%u/%x/%x ours=%u/%x/%x\n",i,vendor.trace[i].id,vendor.trace[i].a,vendor.trace[i].b,ref.trace[i].id,ref.trace[i].a,ref.trace[i].b);exit(1);
    }
    af_clean(&vendor);af_clean(&ref);
}
static struct anim_fixture anim_initial(void)
{
    struct anim_fixture f={.enabled=1,.has_row=1,.owner=0x99aa,.stdname="M",.nanims=1,
        .present={1},.names={"xM"},.durs={"2.5"}};
    f.rect[3]=1;return f;
}
static void check_svg_anim(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0x10bb000,0x10be000,0xeee000,0xeef000,0x113b000,0x1139000,0x6b1000,0xc86000};
    const size_t lengths[]={8192,4096,4096,4096,4096,4096,4096,8192};unsigned char *pages[8];
    for(unsigned i=0;i<8;++i)pages[i]=page(bases[i],lengths[i]);
    size_t off=file_offset(elf,bytes,0x10bbedc,372);assert(crc32(elf+off,372)==UINT32_C(0x10f1780d));
    memcpy(pages[0]+0xedc,elf+off,372);uint32_t pointer=(uint32_t)(uintptr_t)&guard;memcpy(pages[0]+0x1048,&pointer,4);
    thumb_stub(pages[2],bases[2],0xeeeb04,native_a_enabled);quad_stub(pages[2],bases[2],0xeee798,native_a_stdname);
    thumb_stub(pages[3],bases[3],0xeefa70,native_a_count);predicate_stub(pages[3],bases[3],0xeefaac,native_a_at);
    thumb_stub(pages[1],bases[1],0x10be814,native_a_sel);predicate_stub(pages[1],bases[1],0x10be848,native_a_itemrow);
    thumb_stub(pages[4],bases[4],0x113b9f0,native_a_ctrl);raw_stub(pages[5],bases[5],0x1139898,start_bits(native_a_start));
    pair_stub(pages[7],bases[7],0xc86744,native_a_new);thumb_stub(pages[7],bases[7],0xc86b4c,native_a_len);
    quad_stub(pages[7],bases[7],0xc8724c,native_a_sub);raw_stub(pages[7],bases[7],0xc87e88,eq_bits(native_a_eq));
    thumb_stub(pages[7],bases[7],0xc86554,native_a_free);
    /* BLX at10bbfd4 enters ARM state; double returns in d0, start float in s0.
     * Redirect with ARM instructions, not Thumb, preserving native PCS. */
    const uint32_t arm[]={UINT32_C(0xe59fc000),UINT32_C(0xe12fff1c),dtoa_bits(native_a_parse)};
    memcpy(pages[6]+0x274,arm,sizeof arm);
    uint32_t rb=rect_bits(native_a_rect);memcpy(anim_row_vtable+0x28,&rb,4);
    uint32_t vt=(uint32_t)(uintptr_t)anim_row_vtable;memcpy(anim_row,&vt,4);
    for(unsigned i=0;i<8;++i)executable(pages[i],lengths[i]);
    anim_fn native;pointer=0x10bbedd;memcpy(&native,&pointer,4);unsigned compared=0;
    for(unsigned gate=0;gate<6;++gate) {
        struct anim_fixture f=anim_initial();
        if(gate==0)f.enabled=0;
        else if(gate==1)f.has_row=0;
        else if(gate==2)memcpy(f.cur,f.rect,16);
        else if(gate==3)f.nanims=0;
        else if(gate==4)f.nanims=UINT32_MAX;
        else f.nanims=0x80000000;
        compare_anim(native,f);++compared;
    }
    const struct {const char *standard,*name;} names[]={
        {"M","xM"},{"M","yNo"},{"M","M"},{"M",""},{"",""},{"M","éM"},
        {"界","x界"},{"界","🚀界"},{"M","🚀M"},{"","x"},{"Q","xQ"},{"M",NULL},{"",NULL}
    };
    const char *durations[]={"0","-0","2.5","0.5","1.234567890123","1e-50","1e50","inf","nan","  -3.5junk","0x1.8p2"};
    for(unsigned i=0;i<13;++i)for(unsigned d=0;d<11;++d) {
        struct anim_fixture f=anim_initial();f.stdname=names[i].standard;f.names[0]=names[i].name;f.durs[0]=durations[d];
        compare_anim(native,f);++compared;
    }
    for(unsigned variant=0;variant<4;++variant) {
        struct anim_fixture f=anim_initial();f.nanims=3;f.present[1]=f.present[2]=1;
        f.names[0]="zNo";f.names[1]="xM";f.names[2]="xM";f.durs[1]="0.5";f.durs[2]="7";
        if(variant==0)f.present[0]=0;
        if(variant==1)f.names[1]=f.names[2]="zNo";
        if(variant==2)f.present[1]=0;
        if(variant==3)f.names[0]="xM";
        compare_anim(native,f);++compared;
    }
    native_anim=NULL;for(unsigned i=0;i<8;++i)assert(!munmap(pages[i],lengths[i]));
    printf("ok offline shipped ARM SVG list anim: %u native/C comparisons, owned strings/parse-free order/signed count/start bits match\n",compared);
}
static void check_svg_widgets(const unsigned char *elf,size_t bytes)
{
    unsigned char *textpage=page(0xd53000,4096),*listpage=page(0xd50000,4096);
    unsigned char *entrypage=page(0x10b7000,4096),*winpage=page(0x1152000,4096);
    unsigned char *listbase=page(0x10bc000,4096);
    size_t off=file_offset(elf,bytes,0xd531f4,56);
    assert(crc32(elf+off,56)==UINT32_C(0x9b91e0ae));
    memcpy(textpage+0x1f4,elf+off,56);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;memcpy(textpage+0x228,&pointer,4);
    size_t lo=file_offset(elf,bytes,0xd50ce0,84);
    assert(crc32(elf+lo,84)==UINT32_C(0xe58623fa));
    memcpy(listpage+0xce0,elf+lo,84);memcpy(listpage+0xd30,&pointer,4);
    thumb_stub(entrypage,0x10b7000,0x10b701c,native_w_getnum);
    thumb_stub(winpage,0x1152000,0x1152a40,native_w_created);
    thumb_stub(listbase,0x10bc000,0x10bc600,native_w_selitem);
    executable(textpage,4096);executable(listpage,4096);executable(entrypage,4096);
    executable(winpage,4096);executable(listbase,4096);
    {
        unsigned compared=0;widget0_fn numeric=0;widget0_fn selected=0;
        { uint32_t entry=0xd531f5;memcpy(&numeric,&entry,4); }
        { uint32_t entry=0xd50ce1;memcpy(&selected,&entry,4); }
        const uint32_t nums[]={0,1,UINT32_C(0x7fffffff),UINT32_C(0x80000000),UINT32_MAX};
        for(unsigned n=0;n<5;++n) {
            struct widget_fixture vendor={.numeric=nums[n]},ref={.numeric=nums[n]};
            struct oa_svg_row_text_ops ops={&ref,widget_e_entry,widget_e_num};
            native_widget=&vendor;ref_widget=&ref;int error=-1;
            uint32_t v;{ widget0_fn volatile call=numeric;v=call(widget_row); }
            uint32_t e=oa_setting_row_numeric(&ops,&error);
            if(v!=e || error) { fprintf(stderr,"widget numeric mismatch native=%x ours=%x\n",v,e);exit(1); }
            compare_widget_traces(&vendor,&ref);++compared;
        }
        const int32_t sels[]={-1,0,1,3};
        const uint32_t ids[2][4]={{0x10,0x20,0x30,0x40},{0xffffffff,0,1,0xfffffffe}};
        for(unsigned c=0;c<2;++c)for(unsigned s=0;s<4;++s)for(unsigned k=0;k<2;++k) {
            struct widget_fixture vendor={.created=c,.selected=(uint32_t)sels[s]};
            struct widget_fixture ref={.created=c,.selected=(uint32_t)sels[s]};
            struct oa_svg_list_select_ops ops={&ref,widget_e_created,widget_e_sel,widget_e_table,widget_e_ident,widget_e_id};
            for(unsigned i=0;i<4;++i) { memcpy(widget_entries[i],&ids[k][i],4); }
            for(unsigned i=0;i<4;++i) { uint32_t slot=(uint32_t)(uintptr_t)widget_entries[i];memcpy(widget_array+i*4,&slot,4); }
            widget_cell=(uint32_t)(uintptr_t)widget_array;
            { uint32_t cell=(uint32_t)(uintptr_t)&widget_cell;memcpy(widget_list+652,&cell,4); }
            native_widget=&vendor;ref_widget=&ref;int error=-1;
            uint32_t v;{ widget0_fn volatile call=selected;v=call(widget_list); }
            uint32_t e=oa_setting_list_selected_id(&ops,&error);
            if(v!=e || error) {
                fprintf(stderr,"widget selected mismatch created=%d sel=%d native=%x ours=%x\n",c,sels[s],v,e);exit(1);
            }
            compare_widget_traces(&vendor,&ref);++compared;
        }
        native_widget=NULL;ref_widget=NULL;
        assert(!munmap(textpage,4096) && !munmap(listpage,4096) && !munmap(entrypage,4096));
        assert(!munmap(winpage,4096) && !munmap(listbase,4096));
        printf("ok offline shipped ARM SVG list widgets: %u native/C comparisons, numeric forward and selected-id gates/walk match\n",compared);
    }
}
struct refresh_result { unsigned calls;uint32_t index;void *row; };
struct refresh_case { uint32_t first,total,slots;struct refresh_result result; };
static _Alignas(uint32_t) unsigned char refresh_list[0x180];
static uint32_t refresh_array[16],refresh_vtable[25];
static struct refresh_result *native_refresh_result;
static void native_refresh(void *parent,uint32_t index,void *row)
{ assert(parent==refresh_list);++native_refresh_result->calls;native_refresh_result->index=index;native_refresh_result->row=row; }
static uint32_t refresh_first(void *p) { return ((struct refresh_case *)p)->first; }
static uint32_t refresh_total(void *p) { return ((struct refresh_case *)p)->total; }
static uint32_t refresh_slots(void *p) { return ((struct refresh_case *)p)->slots; }
static void *refresh_row(void *p,uint32_t rel)
{ struct refresh_case *f=p;assert(rel<f->slots && rel<16);return (void *)(uintptr_t)refresh_array[rel]; }
static void refresh_parent(void *p,uint32_t index,void *row)
{ struct refresh_case *f=p;++f->result.calls;f->result.index=index;f->result.row=row; }
static void check_svg_refresh(const unsigned char *elf,size_t bytes)
{
    unsigned char *list=page(0x10bd000,4096),*rows=page(0x10be000,4096);
    const struct {uint32_t address,length,crc,literal;} fragments[]={
        {0x10bd0b4,52,UINT32_C(0xd995d3f6),0x10bd0e4},
        {0x10be368,100,UINT32_C(0x4bbe4149),0x10be3c8}
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;
    for(unsigned i=0;i<2;++i) {
        unsigned char *mapping=i?rows:list;uint32_t base=i?0x10be000:0x10bd000;
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(stub_destination(mapping,base,fragments[i].address,fragments[i].length),elf+off,fragments[i].length);
        memcpy(stub_destination(mapping,base,fragments[i].literal,4),&pointer,4);
    }
    void (*callback)(void *,uint32_t,void *)=native_refresh;
    memcpy(refresh_vtable+0x60/4,&callback,4);
    pointer=(uint32_t)(uintptr_t)refresh_vtable;memcpy(refresh_list,&pointer,4);
    pointer=(uint32_t)(uintptr_t)refresh_list;memcpy(refresh_list+0xf8+4,&pointer,4);
    pointer=(uint32_t)(uintptr_t)refresh_array;memcpy(refresh_list+0xf8+28,&pointer,4);
    for(unsigned i=0;i<16;++i)refresh_array[i]=(uint32_t)(uintptr_t)&rv_slots[i];
    executable(list,4096);executable(rows,4096);
    const uint32_t firsts[]={0,1,3,UINT32_MAX-1,UINT32_MAX};
    const uint32_t totals[]={0,1,3,9,UINT32_C(0x80000000),UINT32_MAX},slots[]={0,1,4,16};
    const uint32_t indices[]={0,1,2,3,4,5,16,INT32_MAX,UINT32_C(0x80000000),UINT32_MAX-2,UINT32_MAX-1,UINT32_MAX};
    unsigned compared=0;
    for(unsigned entry=0;entry<2;++entry)for(unsigned a=0;a<5;++a)for(unsigned t=0;t<6;++t)
    for(unsigned n=0;n<4;++n)for(unsigned k=0;k<12;++k) {
        struct refresh_case ref={.first=firsts[a],.total=totals[t],.slots=slots[n]};
        struct refresh_result vendor={0};native_refresh_result=&vendor;
        struct oa_svg_list_refresh_ops ops={&ref,refresh_first,refresh_total,refresh_slots,refresh_row,refresh_parent};
        memcpy(refresh_list+0xf8+12,&ref.first,4);memcpy(refresh_list+0xf8+8,&ref.total,4);
        uint32_t end=(uint32_t)(uintptr_t)refresh_array+slots[n]*4;
        memcpy(refresh_list+0xf8+32,&end,4);
        /* Deliberately unrelated: refresh must not read visible size+0x10. */
        memset(refresh_list+0xf8+16,0xa5,4);
        void (*fn)(void *,uint32_t)=NULL;
        uint32_t address=entry?0x10bd0b5:0x10be369;memcpy(&fn,&address,4);
        { void (*volatile call)(void *,uint32_t)=fn;call(entry?refresh_list:refresh_list+0xf8,indices[k]); }
        assert(!oa_svg_list_refresh_row(&ops,indices[k]));
        assert(vendor.calls==ref.result.calls && vendor.index==ref.result.index && vendor.row==ref.result.row);
        ++compared;
    }
    native_refresh_result=NULL;
    assert(!munmap(list,4096) && !munmap(rows,4096));
    printf("ok offline shipped ARM SVG list refresh: %u native/C comparisons, manager/base join and parent operands match\n",compared);
}
struct checked_result { uint32_t created,value;unsigned calls;unsigned char old;void *box; };
static struct checked_result *native_checked;
static _Alignas(uint32_t) unsigned char checked_row[0x3000];
static unsigned native_checked_created(void *box)
{ assert(box==checked_row+0x27a8);++native_checked->calls;return native_checked->created; }
static void native_checked_set(void *box,uint32_t value)
{ assert(native_checked->calls++==1);native_checked->box=box;native_checked->value=value;native_checked->old=checked_row[0x2e44]; }
static void *checked_box(void *p,void *row) { (void)p;assert(row==checked_row);return checked_row+0x27a8; }
static int checked_created(void *p,void *box)
{ struct checked_result *f=p;assert(box==checked_row+0x27a8);++f->calls;return f->created!=0; }
static void checked_set(void *p,void *box,bool value)
{ struct checked_result *f=p;assert(f->calls++==1);f->box=box;f->value=value;f->old=checked_row[0x2e44]; }
static void checked_store(void *p,void *row,bool value)
{ (void)p;assert(row==checked_row);checked_row[0x2e44]=(unsigned char)value; }
static void check_svg_checked(const unsigned char *elf,size_t bytes)
{
    unsigned char *row=page(0xd51000,4096),*win=page(0x1152000,4096),*check=page(0x10b1000,4096);
    size_t off=file_offset(elf,bytes,0xd51cac,84);
    assert(crc32(elf+off,84)==UINT32_C(0xd5ec3f94));
    memcpy(stub_destination(row,0xd51000,0xd51cac,84),elf+off,84);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;memcpy(row+0xcfc,&pointer,4);
    thumb_stub(win,0x1152000,0x1152a40,native_checked_created);
    pair_stub(check,0x10b1000,0x10b1368,native_checked_set);
    executable(row,4096);executable(win,4096);executable(check,4096);
    const uint32_t created[]={0,1,255,UINT32_MAX};const unsigned char old[]={0,1,0xa5};
    unsigned compared=0;void (*fn)(void *,uint32_t)=NULL;
    pointer=0xd51cad;memcpy(&fn,&pointer,4);
    for(unsigned c=0;c<4;++c)for(unsigned v=0;v<2;++v)for(unsigned k=0;k<3;++k) {
        struct checked_result vendor={.created=created[c]},ref=vendor;
        unsigned char saved[sizeof checked_row];
        memset(checked_row,0x6d,sizeof checked_row);checked_row[0x2e44]=old[k];
        native_checked=&vendor;
        { void (*volatile call)(void *,uint32_t)=fn;call(checked_row,v); }
        memcpy(saved,checked_row,sizeof saved);
        memset(checked_row,0x6d,sizeof checked_row);checked_row[0x2e44]=old[k];
        struct oa_svg_row_check_ops ops={&ref,checked_box,checked_created,checked_set,checked_store};
        assert(!oa_setting_row_set_checked(&ops,checked_row,v!=0));
        assert(vendor.calls==ref.calls && vendor.box==ref.box && vendor.value==ref.value && vendor.old==ref.old);
        assert(!memcmp(saved,checked_row,sizeof saved));++compared;
    }
    native_checked=NULL;
    assert(!munmap(row,4096) && !munmap(win,4096) && !munmap(check,4096));
    printf("ok offline shipped ARM SVG list checked: %u native/C comparisons, created gate and set-before-cache match\n",compared);
}
static void check_svg_update(const unsigned char *elf,size_t bytes)
{
    unsigned char *list=page(0x10bc000,4096),*rows=page(0x10be000,4096),*anim=page(0x10ba000,8192);
    size_t off=file_offset(elf,bytes,0x10bc050,92);
    assert(crc32(elf+off,92)==UINT32_C(0x22020b30));
    memcpy(list+0x50,elf+off,92);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;memcpy(list+0xa8,&pointer,4);
    size_t fo=file_offset(elf,bytes,0x10be77c,48);
    assert(crc32(elf+fo,48)==UINT32_C(0x687fe952));
    memcpy(rows+0x77c,elf+fo,48);memcpy(rows+0x7a8,&pointer,4);
    uint32_t total_bits=bits(native_up_total);memcpy(upd_vtable+0x6c/4,&total_bits,4);
    quad_stub(rows,0x10be000,0x10be678,native_up_full);
    thumb_stub(anim,0x10ba000,0x10bbe04,native_up_scroll);
    thumb_stub(anim,0x10ba000,0x10bbedc,native_up_anim);
    executable(list,4096);executable(rows,4096);executable(anim,8192);
    {
        const uint32_t totals[]={0,1,5,9,UINT32_C(0x80000000)},firsts[]={0,1,5,UINT32_C(0xfffffffe)};
        unsigned compared=0;update_fn update=0;
        { uint32_t entry=0x10bc051;memcpy(&update,&entry,4); }
        for(unsigned t=0;t<5;++t)for(unsigned f=0;f<4;++f) {
            struct update_fixture initial={.total=totals[t],.first=firsts[f],.nrows=16};
            compare_update(update,oa_svg_list_update,initial);++compared;
        }
        native_up=NULL;
        assert(!munmap(list,4096) && !munmap(rows,4096) && !munmap(anim,8192));
        printf("ok offline shipped ARM SVG list update: %u native/C comparisons, total/store/refresh order match\n",compared);
    }
}
static void check_svg_impl(const unsigned char *elf,size_t bytes)
{
    unsigned char *rows=page(0x10be000,4096),*win=page(0x1152000,4096),*app=page(0x1148000,8192);
    const struct {uint32_t address,length,crc,literal;} fragments[]={
        {0x10be504,372,UINT32_C(0x6ec6aabd),0x10be674},
        {0x10be42c,216,UINT32_C(0x5808927c),0x10be500}
    };
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;impl_fn update=0;rotate_fn rotate=0;
    for(unsigned i=0;i<2;++i) {
        size_t off=file_offset(elf,bytes,fragments[i].address,fragments[i].length);
        assert(crc32(elf+off,fragments[i].length)==fragments[i].crc);
        memcpy(rows+fragments[i].address-0x10be000,elf+off,fragments[i].length);
        memcpy(rows+fragments[i].literal-0x10be000,&pointer,4);
        uint32_t entry=fragments[i].address|1;
        if(!i)memcpy(&update,&entry,4);else memcpy(&rotate,&entry,4);
    }
    thumb_stub(win,0x1152000,0x1152a74,native_i_isvisible);
    thumb_stub(app,0x1148000,0x1149390,native_i_gotfocus);
    {
        uint32_t setfocus=predicate_bits(native_i_setfocus),getrect=rect_bits(native_i_getrect);
        uint32_t setpos=rect_bits(native_i_setpos),setvis=predicate_bits(native_i_setvisible);
        uint32_t empty=bits(native_i_empty),drawempty=rect_bits(native_i_drawempty);
        uint32_t drawrow=draw3_bits(native_i_drawrow);
        memcpy(impl_row_vtable+0x1c/4,&setfocus,4);memcpy(impl_row_vtable+0x28/4,&getrect,4);
        memcpy(impl_row_vtable+0x2c/4,&setpos,4);memcpy(impl_row_vtable+0x34/4,&setvis,4);
        memcpy(impl_owner_vtable+0x58/4,&empty,4);memcpy(impl_owner_vtable+0x5c/4,&drawempty,4);
        memcpy(impl_owner_vtable+0x60/4,&drawrow,4);
    }
    {
        uint32_t vtable=(uint32_t)(uintptr_t)impl_row_vtable;
        for(unsigned i=0;i<16;++i) { memcpy(impl_rows+i*8,&vtable,4);memcpy(impl_rows+i*8+4,&i,4); }
        uint32_t ownervtable=(uint32_t)(uintptr_t)impl_owner_vtable;memcpy(impl_owner,&ownervtable,4);
    }
    executable(rows,4096);executable(win,4096);executable(app,8192);
    {
        const uint32_t caps[]={0,3,8,20},indices[]={0,2,7,UINT32_C(0xfffffffe)};
        const uint32_t firsts[]={0,3},sels[]={0,7,20},counts[]={0,4,9},vis[]={0,1,2};
        const int fulls[]={0,1},gots[]={0,1},empties[]={0,1};unsigned compared=0;
        for(unsigned a=0;a<4;++a)for(unsigned b=0;b<4;++b)for(unsigned c=0;c<2;++c)for(unsigned d=0;d<2;++d)
        for(unsigned e=0;e<3;++e)for(unsigned f=0;f<3;++f)for(unsigned g=0;g<3;++g)
        for(unsigned h=0;h<2;++h)for(unsigned k=0;k<2;++k) {
            struct impl_fixture initial={.cap=caps[a],.first=firsts[d],.size=counts[f],.selected=sels[e]};
            for(unsigned m=0;m<16;++m) {
                initial.rows[m]=m;initial.rowvis[m]=(unsigned char)(vis[g]==1 || (vis[g]==2 && !(m&1)));
                initial.rowrect[m][0]=m;initial.rowrect[m][1]=10*m;initial.rowrect[m][2]=100;initial.rowrect[m][3]=70;
            }
            initial.empty_message=empties[k];initial.got_focus=gots[h];
            compare_impl(update,oa_svg_list_manager_update_impl,initial,indices[b],fulls[c]);++compared;
        }
        const int32_t deltas[]={0,1,2,5,-1,-2,-3,INT32_C(0x7fffffff),INT32_C(0x80000000),7};
        const uint32_t ns[]={0,1,3,8};
        for(unsigned n=0;n<4;++n)for(unsigned q=0;q<10;++q) {
            struct impl_fixture initial={.size=ns[n]};
            for(unsigned m=0;m<16;++m) {
                initial.rows[m]=m;initial.rowvis[m]=1;
                initial.rowrect[m][0]=m;initial.rowrect[m][1]=10*m;initial.rowrect[m][2]=100;initial.rowrect[m][3]=70;
            }
            compare_rotate(rotate,oa_svg_list_manager_rotate_rows,initial,deltas[q]);++compared;
        }
        native_impl=NULL;
        assert(!munmap(rows,4096) && !munmap(win,4096) && !munmap(app,4096));
        printf("ok offline shipped ARM SVG list impl: %u native/C comparisons, update/rotate order/args/state match\n",compared);
    }
}
static void check_svg_scroll(const unsigned char *elf,size_t bytes)
{
    unsigned char *list=page(0x10ba000,16384),*rows=page(0x10be000,4096),*win=page(0x1152000,4096);
    unsigned char *bar=page(0x10c4000,4096),*geom=page(0x9a7000,8192);
    size_t off=file_offset(elf,bytes,0x10bbe04,216);
    assert(crc32(elf+off,216)==UINT32_C(0xfbc862b8));
    memcpy(list+0x1e04,elf+off,216);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;memcpy(list+0x1ed8,&pointer,4);
    size_t so=file_offset(elf,bytes,0x10be6e4,48);
    assert(crc32(elf+so,48)==UINT32_C(0x50e85ab7));
    memcpy(rows+0x6e4,elf+so,48);memcpy(rows+0x710,&pointer,4);
    size_t fo=file_offset(elf,bytes,0x10be77c,48);
    assert(crc32(elf+fo,48)==UINT32_C(0x687fe952));
    memcpy(rows+0x77c,elf+fo,48);memcpy(rows+0x7a8,&pointer,4);
    size_t go=file_offset(elf,bytes,0x9a7fcc,56);
    assert(crc32(elf+go,56)==UINT32_C(0xe9107f7d));
    memcpy(geom+0xfcc,elf+go,56);memcpy(geom+0x1000,&pointer,4);
    rect_stub(geom,0x9a7000,0x9a7fcc,native_sb_rect);
    thumb_stub(win,0x1152000,0x1152a40,native_sb_created);
    thumb_stub(win,0x1152000,0x1152a74,native_sb_isvisible);
    predicate_stub(win,0x1152000,0x11525b8,native_sb_setvisible);
    quad_stub(bar,0x10c4000,0x10c401c,native_sb_create);
    quad_stub(bar,0x10c4000,0x10c4138,native_sb_setpos);
    executable(list,16384);executable(rows,4096);executable(win,4096);executable(bar,4096);executable(geom,8192);
    uint32_t entry=0x10bbe05;memcpy(&native_sb_update,&entry,4);
    uint32_t total_bits=predicate_bits(native_sb_total);memcpy(scroll_vtable+0x6c/4,&total_bits,4);
    const uint32_t sizes[]={0,1,5,9},totals[]={0,1,5,9,UINT32_C(0x80000000)},flags[]={0,1,7};
    const uint32_t visibles[]={0,1,7},firsts[]={0,1,5};const int creates[]={0,1};unsigned compared=0;
    const uint32_t corners[4][6]={{0,0,0,0,0,0},{3,7,11,13,17,19},{1,2,3,4,5,6},
        {UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX}};
    for(unsigned s=0;s<4;++s)for(unsigned t=0;t<5;++t)for(unsigned g=0;g<3;++g)for(unsigned c=0;c<4;++c)
    for(unsigned v=0;v<3;++v)for(unsigned q=0;q<3;++q)for(unsigned k=0;k<2;++k) {
        struct scroll_fixture initial={.size=sizes[s],.total=totals[t],.flag=flags[g],.visible=visibles[v],
            .first=firsts[q],.created=creates[k]};
        for(unsigned m=0;m<6;++m)initial.fields[m]=corners[c][m];
        compare_scroll(initial);++compared;
    }
    native_scroll=NULL;
    assert(!munmap(list,16384) && !munmap(rows,4096) && !munmap(win,4096));
    assert(!munmap(bar,4096) && !munmap(geom,8192));
    printf("ok offline shipped ARM SVG list scrollbar: %u native/C comparisons, gating/rect/show/position match\n",compared);
}
int main(int argc,char **argv)
{
    /* Preserve the last completed group even if a later guest call faults. */
    assert(!setvbuf(stdout,NULL,_IOLBF,0));
    assert(argc==3 && sysconf(_SC_PAGESIZE)==4096);
    size_t bytes;unsigned char *elf=read_regular(argv[1],&bytes);
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
    check_selection(elf,bytes);check_profile(elf,bytes,argv[2]);check_retained_setters(elf,bytes);check_hotel_rows(elf,bytes);
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
    assert(!munmap(code,8192) && !munmap(media_page,4096) && !munmap(app_page,4096));
    check_tab_events(elf,bytes);check_prehandle(elf,bytes);check_dispatch(elf,bytes);check_leaf(elf,bytes);
    check_settings_map(elf,bytes);check_settings_input(elf,bytes);check_navigation(elf,bytes);
    check_settings_list_map(elf,bytes);check_settings_list_input(elf,bytes);check_svg_input(elf,bytes);check_svg_bodies(elf,bytes);check_svg_moves(elf,bytes);check_svg_rows(elf,bytes);check_svg_scroll(elf,bytes);check_svg_impl(elf,bytes);check_svg_scrollbar(elf,bytes);check_svg_rowview(elf,bytes);check_svg_widgets(elf,bytes);check_svg_slide(elf,bytes);check_svg_anim(elf,bytes);check_svg_update(elf,bytes);check_svg_refresh(elf,bytes);check_svg_checked(elf,bytes);check_row_values(elf,bytes);
    check_settings_draw(elf,bytes);
    check_settings_row_type(elf,bytes);
    check_settings_row_setters(elf,bytes);
    check_settings_row_render(elf,bytes);
    check_launcher_image_id(elf,bytes);
    check_svg_image(elf,bytes);
    check_settings_row_construct(elf,bytes);
    check_svg_widget_state(elf,bytes);
    check_launcher_item(elf,bytes);
    check_launcher_info(elf,bytes);
    check_svg_scrollbar_geom(elf,bytes);
    check_svg_scrollbar_skin(elf,bytes);
    check_svg_scrollbar_render(elf,bytes);
    check_audio_amp(elf,bytes);
    check_profile_data(elf,bytes);
    check_amp_init(elf,bytes);
    free(elf);assert(!munmap(fail_page,4096));
    printf("ok offline shipped ARM availability: %" PRIu64 " comparisons, raw XOR bytes/default IDs, only stack-guard literal redirected\n",compared);
    return 0;
}
