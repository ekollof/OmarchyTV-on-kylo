/* Offline ARM fragment: constructors/renderItems/visibility implementations
 * are controlled. Included after the checked ARM harness stub installers. */
#include "openaurora/settings_row_type.h"
struct srt_entry { uint32_t kind,offset,value,type,changed; };
struct srt_result {
    struct srt_entry entries[24];unsigned count;uint32_t visible;int force_visible;
    struct oa_setting_row_type_state *state;
};
static struct srt_result *srt_native;
static _Alignas(uint32_t) unsigned char srt_row[0x3000];
static uint32_t srt_vtable[24];
static void srt_record(uint32_t kind,uint32_t offset,uint32_t value)
{
    struct srt_result *r=srt_native;assert(r->count<24);
    struct srt_entry *e=&r->entries[r->count++];memset(e,0,sizeof *e);
    e->kind=kind;e->offset=offset;e->value=value;
    if(r->state) { e->type=r->state->type;e->changed=r->state->rebuild; }
    else { memcpy(&e->type,srt_row+0x2dec,4);e->changed=srt_row[0x2e49]; }
}
static unsigned srt_render(void *row)
{ assert(row==srt_row);srt_record(4,0,0);if(srt_native->force_visible>=0)srt_native->visible=(uint32_t)srt_native->force_visible;return 0; }
static unsigned srt_visible(void *row) { assert(row==srt_row);srt_record(5,0,0);return srt_native->visible; }
static void srt_window(void *child,uint32_t visible) { srt_record(0,(uint32_t)((unsigned char *)child-srt_row),visible); }
static void srt_selection(void *child,uint32_t visible) { srt_record(1,(uint32_t)((unsigned char *)child-srt_row),visible); }
static void srt_text(void *child,uint32_t visible) { srt_record(2,(uint32_t)((unsigned char *)child-srt_row),visible); }
static void srt_check(void *child,uint32_t visible) { srt_record(3,(uint32_t)((unsigned char *)child-srt_row),visible); }
static void srte_render(void *p)
{ assert(p==srt_native);srt_record(4,0,0);if(srt_native->force_visible>=0)srt_native->visible=(uint32_t)srt_native->force_visible; }
static int srte_visible(void *p) { assert(p==srt_native);srt_record(5,0,0);return srt_native->visible!=0; }
static void srte_child(void *p,enum oa_setting_child_kind kind,uint32_t offset,int visible)
{ assert(p==srt_native);srt_record((uint32_t)kind,offset,(uint32_t)visible); }
static void check_settings_row_type(const unsigned char *elf,size_t bytes)
{
    unsigned char *row=page(0xd51000,8192),*win=page(0x1152000,4096),*sel=page(0x10b4000,4096),
                  *text=page(0x10b7000,4096),*check=page(0x10b1000,4096);
    size_t off=file_offset(elf,bytes,0xd51e00,1580);
    /* CRC is checked below before executing any source instruction. */
    assert(crc32(elf+off,1580)==UINT32_C(0x370237d3));
    memcpy(stub_destination(row,0xd51000,0xd51e00,1580),elf+off,1580);
    uint32_t pointer=(uint32_t)(uintptr_t)&guard;memcpy(row+0x1158,&pointer,4);
    srt_vtable[0x5c/4]=bits(srt_render);pointer=(uint32_t)(uintptr_t)srt_vtable;memcpy(srt_row,&pointer,4);
    thumb_stub(win,0x1152000,0x1152a74,srt_visible);pair_stub(win,0x1152000,0x11525b8,srt_window);
    pair_stub(sel,0x10b4000,0x10b40f8,srt_selection);pair_stub(text,0x10b7000,0x10b7158,srt_text);
    pair_stub(check,0x10b1000,0x10b1478,srt_check);
    executable(row,8192);executable(win,4096);executable(sel,4096);executable(text,4096);executable(check,4096);
    void (*fn)(void *,uint32_t)=NULL;pointer=0xd51e01;memcpy(&fn,&pointer,4);
    if(getenv("OA_DUMP_ROW_VISIBILITY"))for(uint32_t type=0;type<28;++type) {
        struct srt_result native={.visible=1,.force_visible=-1};srt_native=&native;
        uint32_t old=UINT32_MAX;memcpy(srt_row+0x2dec,&old,4);
        { void (*volatile call)(void *,uint32_t)=fn;call(srt_row,type); }
        printf("row-type %u:",type);
        for(unsigned i=0;i<native.count;++i)if(native.entries[i].kind<4)
            printf(" %u/%x/%u",native.entries[i].kind,native.entries[i].offset,native.entries[i].value);
        putchar('\n');
    }
    unsigned compared=0;
    const uint32_t visible[]={0,1,255,UINT32_MAX},outliers[]={UINT32_MAX,UINT32_C(0x80000000),UINT32_C(0x10000009),UINT32_C(0xffffff09)};
    for(unsigned t=0;t<260;++t)for(unsigned oldcase=0;oldcase<4;++oldcase)
    for(unsigned v=0;v<4;++v)for(int force=-1;force<=1;++force) {
        uint32_t type=t<256?t:outliers[t-256],old=oldcase==0?0:oldcase==1?9:oldcase==2?UINT32_MAX:type;
        struct oa_setting_row_type_state state={old,255};
        struct srt_result native={.visible=visible[v],.force_visible=force},
                          ref={.visible=visible[v],.force_visible=force,.state=&state};
        struct oa_setting_row_type_ops ops={&ref,srte_render,srte_visible,srte_child};
        memcpy(srt_row+0x2dec,&old,4);srt_row[0x2e49]=255;
        unsigned char expected[sizeof srt_row];memcpy(expected,srt_row,sizeof expected);
        srt_native=&native;{ void (*volatile call)(void *,uint32_t)=fn;call(srt_row,type); }
        srt_native=&ref;assert(!oa_setting_row_set_type(&state,&ops,type));
        memcpy(expected+0x2dec,&state.type,4);expected[0x2e49]=state.rebuild;
        if(native.count!=ref.count || native.visible!=ref.visible || memcmp(native.entries,ref.entries,ref.count*sizeof *ref.entries) || memcmp(srt_row,expected,sizeof expected)) {
            fprintf(stderr,"row type mismatch new=%x old=%x visible=%x force=%d counts=%u/%u\n",type,old,visible[v],force,native.count,ref.count);exit(1);
        }
        ++compared;
    }
    srt_native=NULL;
    assert(!munmap(row,8192) && !munmap(win,4096) && !munmap(sel,4096) && !munmap(text,4096) && !munmap(check,4096));
    printf("ok offline shipped ARM settings row type: %u native/C comparisons, state-before-render and ordered qualified child visibility match\n",compared);
}
