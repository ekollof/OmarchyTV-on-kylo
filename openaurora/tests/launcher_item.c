#include "openaurora/launcher_item.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Side-table UString emulation: the module only moves opaque 8-byte slots, so
 * the test owns the string contents keyed by slot address. */

#define SLOT_MAX 24
struct slot { void *owner; char text[96]; };
static struct slot slots[SLOT_MAX];
static unsigned slot_count;

static struct slot *slot_of(const void *p,int create)
{
    for(unsigned i=0;i<slot_count;++i)
        if(slots[i].owner==p)return &slots[i];
    if(!create)return NULL;
    assert(slot_count<SLOT_MAX);
    slots[slot_count].owner=(void *)(uintptr_t)p;
    slots[slot_count].text[0]='\0';
    return &slots[slot_count++];
}
static void seed(void *p,const char *text)
{
    struct slot *s=slot_of(p,1);
    snprintf(s->text,sizeof s->text,"%s",text);
}
static const char *text_of(const void *p)
{
    struct slot *s=slot_of(p,0);
    return s?s->text:"<missing>";
}

enum {K_COPY,K_DEFAULT,K_EMPTY,K_ASSIGN,K_APPEND_LIT,K_APPEND};
static struct { uint32_t kind,dst,src; } trace[32];
static unsigned trace_count;
static void push(uint32_t kind,uint32_t dst,uint32_t src)
{
    assert(trace_count<32);
    trace[trace_count].kind=kind;trace[trace_count].dst=dst;
    trace[trace_count].src=src;trace_count++;
}

static void *g_item;
static uint32_t off_of(const void *p)
{ return (uint32_t)((uintptr_t)p-(uintptr_t)g_item); }

static void u_copy(void *dst,const void *src)
{
    struct slot *d=slot_of(dst,1);
    snprintf(d->text,sizeof d->text,"%s",text_of(src));
    push(K_COPY,off_of(dst),0);
}
static void u_default(void *dst)
{
    struct slot *d=slot_of(dst,1);
    d->text[0]='\0';
    push(K_DEFAULT,off_of(dst),0);
}
static int u_empty(const void *src)
{
    push(K_EMPTY,0,0);
    return text_of(src)[0]=='\0';
}
static void u_assign(void *dst,const char *literal)
{
    struct slot *d=slot_of(dst,1);
    snprintf(d->text,sizeof d->text,"%s",literal);
    push(K_ASSIGN,off_of(dst),(uint32_t)(literal==(const char *)OA_LAUNCHER_IMAGE_DIR));
}
static void u_append_lit(void *dst,const char *literal)
{
    struct slot *d=slot_of(dst,1);
    size_t n=strlen(d->text);
    snprintf(d->text+n,sizeof d->text-n,"%s",literal);
    push(K_APPEND_LIT,off_of(dst),(uint32_t)(literal==(const char *)OA_LAUNCHER_IMAGE_SEP));
}
static void u_append(void *dst,const void *src)
{
    struct slot *d=slot_of(dst,1);
    size_t n=strlen(d->text);
    snprintf(d->text+n,sizeof d->text-n,"%s",text_of(src));
    push(K_APPEND,off_of(dst),0);
}
static struct oa_launcher_item_ops ops(void)
{
    return (struct oa_launcher_item_ops){u_copy,u_default,u_empty,u_assign,
        u_append_lit,u_append};
}

static uint32_t rd32(const void *p,uint32_t off)
{ uint32_t v;memcpy(&v,(const unsigned char *)p+off,4);return v; }
static void wr32(void *p,uint32_t off,uint32_t v)
{ memcpy((unsigned char *)p+off,&v,4); }

int main(void)
{
    static unsigned char novo[OA_NOVO_LAUNCHER_ITEM_READ_BYTES];
    static unsigned char item[OA_LAUNCHER_ITEM_BYTES+8];
    struct oa_launcher_item_ops o=ops();
    /* The item stores the vendor's 32-bit shared_ptr control pointer. On a
     * 64-bit host the address cannot be represented, so the refcount branch is
     * covered only by the 32-bit ARM build (qemu/TV), not by the host unit. */
#define CONTROL_DISABLED (UINTPTR_MAX!=UINT32_MAX)
#if UINTPTR_MAX==UINT32_MAX
    static uint32_t refs[8];
    uint32_t control=(uint32_t)(uintptr_t)&refs[0];
#define REFS_RESET() (refs[1]=7)
#define REFS_VALUE() refs[1]
#else
    uint32_t control=0;
#define REFS_RESET() ((void)0)
#define REFS_VALUE() 0u
#endif
    static const char *const src_text[10]={
        "app-name","menu-string","recommendation","thumb-url","thumb-file",
        "app-url","ad-url","fvp-id","ad-image-url","ad-tracking-url"
    };
    static const uint32_t src_off[10]={0x1c,0x24,0x2c,0x34,0x3c,0x78,0x80,0x88,0x58,0x60};
    /* Destination slot order in the shipped body. */
    static const uint32_t dst_off[11]={0x10,0x1c,0x24,0x2c,0x34,0x3c,0x4c,0x54,0x5c,0x70,0x78};

    g_item=item;
    memset(novo,0xa5,sizeof novo);
    /* Distinct scalar fields at every read offset. */
    for(unsigned i=0;i<0x94;i+=4)wr32(novo,i,UINT32_C(0x10000000)+i);
    wr32(novo,0x70,control);          /* shared_ptr control block */
    wr32(novo,0x74,UINT32_C(0x6000)); /* shared_ptr raw pointer */
    novo[0x50]=0x11;novo[0x51]=0x22;novo[0x54]=0x33;
    for(unsigned i=0;i<10;++i)seed(novo+src_off[i],src_text[i]);
    REFS_RESET();

    memset(item,0xa5,sizeof item);
    trace_count=0;slot_count=0;
    /* re-seed after slot_count reset */
    for(unsigned i=0;i<10;++i)seed(novo+src_off[i],src_text[i]);

    oa_launcher_item_from_novo(item,novo,&o);

    /* 17 scalar copies. */
    assert(rd32(item,0x00)==rd32(novo,0x00));
    assert(rd32(item,0x04)==rd32(novo,0x18));
    assert(rd32(item,0x08)==rd32(novo,0x0c));
    assert(rd32(item,0x0c)==rd32(novo,0x10));
    assert(item[0x18]==1);
    assert(rd32(item,0x44)==rd32(novo,0x44));
    assert(rd32(item,0x48)==rd32(novo,0x48));
    assert(rd32(item,0x64)==rd32(novo,0x90));
    assert(rd32(item,0x68)==rd32(novo,0x08));
    assert(item[0x6c]==0x22 && item[0x6d]==0x11 && item[0x6e]==0x33);
    assert(rd32(item,0x80)==rd32(novo,0x14));
    assert(rd32(item,0x84)==rd32(novo,0x68));
    assert(rd32(item,0x88)==rd32(novo,0x6c));
    assert(rd32(item,0x8c)==control);
    assert(rd32(item,0x90)==UINT32_C(0x6000));
    assert(rd32(item,0x94)==UINT32_C(0xffffffff));
    assert(rd32(item,0x98)==0 && rd32(item,0x9c)==0 && rd32(item,0xa0)==0);
    assert(CONTROL_DISABLED || REFS_VALUE()==8);
    assert(item[OA_LAUNCHER_ITEM_BYTES]==0xa5); /* no write past +0xa3 */

    /* UString destinations: +0x10 <- src+0x1c, +0x1c <- src+0x24, +0x24 <-
     * src+0x2c, default at +0x2c, +0x34 <- src+0x34, +0x3c <- src+0x3c,
     * +0x4c <- src+0x78, +0x54 <- src+0x80, +0x5c <- src+0x88,
     * +0x70 <- src+0x58, +0x78 <- src+0x60. */
    static const int map[11]={0,1,2,-1,3,4,5,6,7,8,9};
    for(unsigned i=0;i<11;++i) {
        const char *got=text_of(item+dst_off[i]);
        if(map[i]<0)continue;   /* the built path is asserted separately */
        const char *want=i>=6 && i<=8?"":src_text[map[i]];
        if(strcmp(got,want)) {
            fprintf(stderr,"slot %x got %s want %s\n",dst_off[i],got,want);
            return 1;
        }
    }
    /* A non-empty thumbnail name builds directory + separator + name. */
    assert(!strcmp(text_of(item+0x2c),"/buffer/downloaded_images/thumb-file"));
    assert(trace_count==15);
    assert(trace[11].kind==K_EMPTY);
    assert(trace[12].kind==K_ASSIGN && trace[12].dst==0x2c);
    assert(trace[13].kind==K_APPEND_LIT && trace[13].dst==0x2c);
    assert(trace[14].kind==K_APPEND && trace[14].dst==0x2c);
    assert(trace[0].kind==K_COPY && trace[0].dst==0x10);
    assert(trace[1].kind==K_COPY && trace[1].dst==0x1c);
    assert(trace[2].kind==K_COPY && trace[2].dst==0x24);
    assert(trace[3].kind==K_DEFAULT && trace[3].dst==0x2c);
    assert(trace[4].kind==K_COPY && trace[4].dst==0x34);
    assert(trace[5].kind==K_COPY && trace[5].dst==0x3c);
    assert(trace[6].kind==K_DEFAULT && trace[6].dst==0x4c);
    assert(trace[7].kind==K_DEFAULT && trace[7].dst==0x54);
    assert(trace[8].kind==K_DEFAULT && trace[8].dst==0x5c);
    assert(trace[9].kind==K_COPY && trace[9].dst==0x70);
    assert(trace[10].kind==K_COPY && trace[10].dst==0x78);

    /* Empty thumbnail name: the path slot stays as the default-constructed
     * empty string and no assign/append calls happen. */
    slot_count=0;trace_count=0;
    for(unsigned i=0;i<10;++i)seed(novo+src_off[i],i==4?"":src_text[i]);
    memset(item,0xa5,sizeof item);
    REFS_RESET();
    oa_launcher_item_from_novo(item,novo,&o);
    assert(text_of(item+0x2c)[0]=='\0');
    assert(trace_count==12);
    assert(trace[11].kind==K_EMPTY);
    assert(CONTROL_DISABLED || REFS_VALUE()==8);

    /* The last word store is +0xa0..+0xa3; the next byte is untouched. */
    assert(item[0xa0]==0 && item[0xa1]==0 && item[0xa2]==0 && item[0xa3]==0);
    assert(item[OA_LAUNCHER_ITEM_BYTES]==0xa5);

    /* NovoLauncherItemInfo -> NovoLauncherItem (copyLauncherInfoToItem): the
     * fixed-record decode. Field and string assignments follow the body. */
    {
        static struct oa_launcher_info info;
        static const struct { const char *text; size_t off; } texts[]={
            {"menu-string-id",offsetof(struct oa_launcher_info,menu_string_id)},
            {"app-name",offsetof(struct oa_launcher_info,application_name)},
            {"thumb-url",offsetof(struct oa_launcher_info,thumb_url)},
            {"thumb-file",offsetof(struct oa_launcher_info,thumb_file_name)},
            {"app-url",offsetof(struct oa_launcher_info,application_url)},
            {"fvp-id",offsetof(struct oa_launcher_info,fvp_application_id)},
            {"recommendation",offsetof(struct oa_launcher_info,recommendation_url)},
        };
        memset(&info,0,sizeof info);
        memset(info.excluded_vendor_fields,0xa5,sizeof info.excluded_vendor_fields);
        for(unsigned i=0;i<sizeof texts/sizeof texts[0];++i)
            snprintf((char *)&info+texts[i].off,64,"%s",texts[i].text);
        info.id=0x101;info.parent_id=0x102;info.order_index=0x103;
        info.item_type=0x104;info.original_id=0x105;info.server_id=0x106;
        info.menu_return_id=0x107;info.application_update_time=0x108;
        info.thumb_update_time=0x109;info.image_crc=0x10a;
        info.is_featured=1;info.is_deleted=0;info.is_out_of_sync=1;
        info.is_moved_by_user=0;info.is_delete_disabled=1;info.channel_handle=0x10b;
        slot_count=0;trace_count=0;memset(item,0xa5,sizeof item);
        oa_launcher_info_to_item(item,&info,&o);
        assert(rd32(item,0x00)==0x101 && rd32(item,0x04)==0x102);
        assert(rd32(item,0x08)==0x103 && rd32(item,0x14)==0x104);
        assert(rd32(item,0x0c)==0x105 && rd32(item,0x10)==0x106);
        assert(rd32(item,0x18)==0x107);
        assert(rd32(item,0x44)==0x108 && rd32(item,0x48)==0x109 && rd32(item,0x4c)==0x10a);
        assert(item[0x51]==1 && item[0x50]==0 && item[0x52]==1 &&
               item[0x53]==0 && item[0x54]==1);
        assert(rd32(item,0x90)==0x10b);
        static const struct { uint32_t dst;size_t src; } map[]={
            {0x1c,offsetof(struct oa_launcher_info,menu_string_id)},
            {0x24,offsetof(struct oa_launcher_info,application_name)},
            {0x34,offsetof(struct oa_launcher_info,thumb_url)},
            {0x3c,offsetof(struct oa_launcher_info,thumb_file_name)},
            {0x2c,offsetof(struct oa_launcher_info,application_url)},
            {0x58,offsetof(struct oa_launcher_info,fvp_application_id)},
            {0x60,offsetof(struct oa_launcher_info,recommendation_url)},
        };
        for(unsigned i=0;i<sizeof map/sizeof map[0];++i) {
            const char *got=text_of(item+map[i].dst);
            const char *want=(const char *)&info+map[i].src;
            if(strcmp(got,want)) {
                fprintf(stderr,"info->item slot %x got %s want %s\n",
                    map[i].dst,got,want);
                return 1;
            }
            assert(trace[i].kind==K_ASSIGN && trace[i].dst==map[i].dst);
        }
        assert(!text_of(item+0x78)[0] && !text_of(item+0x80)[0] && !text_of(item+0x88)[0]);
        assert(trace_count==10);
    }

    /* Null control block: no refcount write. */
    wr32(novo,0x70,0);
    slot_count=0;trace_count=0;
    for(unsigned i=0;i<10;++i)seed(novo+src_off[i],src_text[i]);
    memset(item,0xa5,sizeof item);
    oa_launcher_item_from_novo(item,novo,&o);
    assert(rd32(item,0x8c)==0 && rd32(item,0x90)==UINT32_C(0x6000));

    puts("ok launcher item: novo-row conversion, 17 scalars, 11 UStrings and thumbnail path prefix");
    return 0;
}
