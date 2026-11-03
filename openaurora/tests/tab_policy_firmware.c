/* OFFLINE ARM/qemu differential only. Reads the local ELF, executes its
 * availability/group instructions in THIS process with synthetic getters. Never
 * attach to Aurora or run platform/HAL init. No vendor bytes are embedded. */
#if !defined(__arm__)
#error "Build this optional firmware differential for ARM and run under qemu-arm"
#endif
#include "openaurora/tab_policy.h"
#include "openaurora/profile.h"
#include "openaurora/tab_events.h"
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
    check_tab_events(elf,bytes);free(elf);assert(!munmap(fail_page,4096));
    printf("ok offline shipped ARM availability: %" PRIu64 " comparisons, raw XOR bytes/default IDs, only stack-guard literal redirected\n",compared);
    return 0;
}
