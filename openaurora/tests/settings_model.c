#include "openaurora/settings_model.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct allocations { size_t calls,fail,live; };
union allocation_header { max_align_t alignment;size_t size; };
static void *test_allocate(void *p,size_t n)
{
    struct allocations *a=p;++a->calls;if(a->calls==a->fail)return NULL;
    assert(n<=SIZE_MAX-sizeof(union allocation_header));
    union allocation_header *h=malloc(sizeof *h+n);assert(h);h->size=n;++a->live;
    memset(h+1,0xa5,n);return h+1;
}
static void test_release(void *p,void *v)
{
    struct allocations *a=p;union allocation_header *h=(union allocation_header *)v-1;
    assert(a->live);--a->live;memset(v,0xdd,h->size);free(h);
}
struct receipt { enum oa_setting_draw_action action;uint32_t a,b;char text[256],text2[256];unsigned char image[16]; };
struct sink {
    struct oa_settings_model *model;void *row;unsigned count;
    struct receipt events[32];
};
static void copy_text(char *target,size_t capacity,const void *handle)
{ const char *s=handle?oa_settings_text_data(handle):"";assert(strlen(s)<capacity);memcpy(target,s,strlen(s)+1); }
static void receive(void *p,void *row,const struct oa_setting_draw_event *e)
{
    struct sink *s=p;assert(row==s->row && s->count<32);
    struct receipt *r=&s->events[s->count++];memset(r,0,sizeof *r);
    r->action=e->action;r->a=e->a;r->b=e->b;copy_text(r->text,sizeof r->text,e->text);copy_text(r->text2,sizeof r->text2,e->text2);
    if(e->image)memcpy(r->image,e->image,sizeof r->image);
    struct oa_setting_draw_style style={0};
    assert(oa_settings_model_destroy(s->model)==EBUSY);
    assert(oa_settings_model_draw(s->model,0,row,&style,receive,s)==EBUSY);
}
static const struct receipt *find(struct sink *s,enum oa_setting_draw_action action)
{ for(unsigned i=0;i<s->count;++i)if(s->events[i].action==action)return &s->events[i];return NULL; }
static struct oa_settings_item_input input(struct oa_settings_value_input *v,size_t n)
{
    return (struct oa_settings_item_input){.id=42,.type=10,.value=17,.minimum=UINT32_C(0x80000000),.maximum=INT32_MAX,
        .label="Instellingen – 音量",.glyph_path="",.image_url="",.service_logo=UINT32_MAX,.glyph=12,.active=255,.checked=1,
        .image={UINT32_MAX,NULL,0},.additional={UINT32_MAX,NULL,0},.values=v,.value_count=n};
}
int main(void)
{
    struct allocations a={0};struct oa_settings_allocator allocator={&a,test_allocate,test_release};
    struct oa_settings_value_input v[]={{99,"minimum"},{7,"maximum"},{UINT32_MAX,"waarde"}};
    struct oa_settings_item_input in=input(v,3);unsigned tested=0;int row;
    for(unsigned type=0;type<32;++type)for(unsigned flags=0;flags<16;++flags)for(unsigned n=0;n<4;++n) {
        in.type=type;in.value_count=n;struct oa_settings_model *m=NULL;
        assert(!oa_settings_model_create(&in,1,"Bevestigen",&allocator,&m));
        struct sink sink={.model=m,.row=&row};
        struct oa_setting_draw_style style={flags&1,(flags>>1)&1,(flags>>2)&1,(flags>>3)&1};
        size_t live=a.live;
        assert(!oa_settings_model_draw(m,0,&row,&style,receive,&sink) && a.live==live);
        const struct receipt *r=find(&sink,OA_SD_MINMAX_TEXT);
        if(r)assert(!strcmp(r->text,n>=2?"minimum":"-2147483648") && !strcmp(r->text2,n>=2?"maximum":"2147483647"));
        r=find(&sink,OA_SD_BUTTON_TEXT);
        if(r && (type==4 || type==8 || type==26))assert(!strcmp(r->text,n?"minimum":style.no_default?"":"Bevestigen"));
        r=find(&sink,OA_SD_APPEND_ITEM);if(r)assert(r->a==99 && !strcmp(r->text,"minimum"));
        assert(!strcmp(oa_settings_text_data(oa_settings_model_item(m,0)->label),in.label));
        assert(!oa_settings_model_destroy(m) && !a.live);++tested;
    }
    /* Source buffers, vectors, image bytes and default text can disappear after
     * construction. Events are copied while the dispatcher owns temporaries. */
    char label[]="café 音量",text[]="Keuze",ok[]="Bevestigen";unsigned char image[16];memset(image,0x37,sizeof image);
    struct oa_settings_value_input vv={3,text};in=input(&vv,1);in.type=5;in.label=label;
    in.image=(struct oa_settings_image_input){3,image,sizeof image};
    in.additional=(struct oa_settings_image_input){7,image,sizeof image};
    struct oa_settings_model *m=NULL;assert(!oa_settings_model_create(&in,1,ok,&allocator,&m));
    memset(label,'x',sizeof label-1);memset(text,'x',sizeof text-1);memset(ok,'x',sizeof ok-1);memset(image,0xee,sizeof image);
    struct sink sink={.model=m,.row=&row};struct oa_setting_draw_style style={0};
    assert(!oa_settings_model_draw(m,0,&row,&style,receive,&sink));
    const struct receipt *button=find(&sink,OA_SD_BUTTON_TEXT),*additional=find(&sink,OA_SD_ADDITIONAL),*detail=find(&sink,OA_SD_IMAGE_DETAIL);
    assert(button && additional && detail);
    assert(!strcmp(button->text,"café 音量"));
    assert(additional->image[0]==0x37 && detail->image[15]==0x37);
    assert(oa_settings_model_item(m,0)->values[0].id==3 && !strcmp(oa_settings_text_data(oa_settings_model_item(m,0)->values[0].text),"Keuze"));
    assert(!oa_settings_model_destroy(m) && !a.live);
    /* Exhaust every allocation in construction, including partial text/value/
     * image copies, with poisoned memory and an unchanged unpublished out. */
    a.calls=0;m=NULL;assert(!oa_settings_model_create(&in,1,"OK",&allocator,&m));
    size_t construction=a.calls;assert(!oa_settings_model_destroy(m));
    for(size_t fail=1;fail<=construction;++fail) {
        a.calls=0;a.fail=fail;m=NULL;
        assert(oa_settings_model_create(&in,1,"OK",&allocator,&m)==ENOMEM && !m && !a.live);
    }
    a.fail=0;
    /* Every drawing allocation can fail without leaking or invalidating the
     * backing model. Type writeback is retained before failed construction. */
    for(unsigned n=0;n<4;++n) {
        in=input(v,n);in.type=10;style=(struct oa_setting_draw_style){.progress=1};
        assert(!oa_settings_model_create(&in,1,"OK",&allocator,&m));
        sink=(struct sink){.model=m,.row=&row};a.calls=0;
        assert(!oa_settings_model_draw(m,0,&row,&style,receive,&sink));size_t draws=a.calls;
        assert(oa_settings_model_item(m,0)->type==24 && !oa_settings_model_destroy(m));
        for(size_t fail=1;fail<=draws;++fail) {
            a.calls=0;a.fail=0;assert(!oa_settings_model_create(&in,1,"OK",&allocator,&m));size_t live=a.live;
            sink=(struct sink){.model=m,.row=&row};a.calls=0;a.fail=fail;
            assert(oa_settings_model_draw(m,0,&row,&style,receive,&sink)==ENOMEM && a.live==live);
            assert(oa_settings_model_item(m,0)->type==24);
            a.fail=0;sink.count=0;assert(!oa_settings_model_draw(m,0,&row,&style,receive,&sink));
            assert(!oa_settings_model_destroy(m) && !a.live);
        }
    }
    m=NULL;assert(!oa_settings_model_create(NULL,0,"OK",&allocator,&m));
    sink=(struct sink){.model=m,.row=&row};
    assert(!oa_settings_model_count(m) && !oa_settings_model_item(m,0));
    assert(!oa_settings_model_draw(m,0,&row,&style,receive,&sink) && !sink.count);
    assert(!oa_settings_model_destroy(m) && !a.live && !oa_settings_model_destroy(NULL));
    in=input(v,3);in.label=NULL;m=NULL;
    assert(oa_settings_model_create(&in,1,"OK",&allocator,&m)==EINVAL && !m && !a.live);
    in=input(v,3);in.image.id=3;
    assert(oa_settings_model_create(&in,1,"OK",&allocator,&m)==EINVAL && !m && !a.live);
    assert(oa_settings_model_create(&in,(size_t)INT32_MAX+1,"OK",&allocator,&m)==EOVERFLOW && !m && !a.live);
    printf("ok settings model: %u real-provider draws, deep ownership, decimal limits, reentry and allocation unwind\n",tested);
    return 0;
}
