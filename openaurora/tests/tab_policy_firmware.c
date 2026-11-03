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
    unsigned char *p=(unsigned char *)mapped+address-base;const uint16_t insn[]={0x4b00,0x4718};
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
int main(int argc,char **argv)
{
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
    check_settings_list_map(elf,bytes);check_settings_list_input(elf,bytes);free(elf);assert(!munmap(fail_page,4096));
    printf("ok offline shipped ARM availability: %" PRIu64 " comparisons, raw XOR bytes/default IDs, only stack-guard literal redirected\n",compared);
    return 0;
}
