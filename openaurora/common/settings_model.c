#include "openaurora/settings_model.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct text { size_t length,capacity;char bytes[]; };
struct oa_settings_model {
    struct oa_settings_allocator allocator;
    struct oa_setting_item **items;size_t count,capacity;
    struct text *ok;
    int busy;
    void (*sink)(void *,void *,const struct oa_setting_draw_event *);void *sink_context;
};
struct snapshot { size_t count;struct oa_setting_value items[]; };
static void *default_alloc(void *p,size_t n) { (void)p;return malloc(n); }
static void default_free(void *p,void *v) { (void)p;free(v); }
static void *allocate(struct oa_settings_model *m,size_t n)
{ void *v=m->allocator.allocate(m->allocator.context,n);if(v)memset(v,0,n);return v; }
static void release(struct oa_settings_model *m,const void *p)
{ if(p)m->allocator.release(m->allocator.context,(void *)p); }
static struct text *text_from(struct oa_settings_model *m,const char *s,size_t capacity)
{
    size_t n=strlen(s);if(n>capacity || capacity>SIZE_MAX-sizeof(struct text)-1)return NULL;
    struct text *t=allocate(m,sizeof *t+capacity+1);if(!t)return NULL;
    t->length=n;t->capacity=capacity;memcpy(t->bytes,s,n+1);return t;
}
const char *oa_settings_text_data(const void *p) { return p?((const struct text *)p)->bytes:NULL; }
static void *text_new(void *p,const void *source)
{ struct oa_settings_model *m=p;return text_from(m,source?oa_settings_text_data(source):"",m->capacity); }
static void text_assign(void *p,void *target,const void *source)
{
    (void)p;struct text *t=target;const struct text *s=source;assert(s->length<=t->capacity);
    memmove(t->bytes,s->bytes,s->length+1);t->length=s->length;
}
static void text_append(void *p,void *target,uint32_t raw)
{
    (void)p;struct text *t=target;int32_t value;memcpy(&value,&raw,sizeof value);
    /* drawRow appends once to a freshly empty temporary. Reserve its largest
     * signed32 decimal representation before calling the infallible setter. */
    assert(t->capacity-t->length>=11);
    int n=snprintf(t->bytes+t->length,t->capacity-t->length+1,"%" PRId32,value);
    assert(n>=1 && n<=11);t->length+=(size_t)n;
}
static int text_empty(void *p,const void *text) { (void)p;return ((const struct text *)text)->length==0; }
static void text_free(void *p,void *text) { release(p,text); }
static void lookup(void *p,void *text) { struct oa_settings_model *m=p;text_assign(m,text,m->ok); }
static void store_type(void *p,struct oa_setting_item *item,uint32_t type) { (void)p;item->type=type; }
static void run(void *p,void *row,const struct oa_setting_draw_event *event)
{ struct oa_settings_model *m=p;m->sink(m->sink_context,row,event); }
static void snapshot_free(struct oa_settings_model *m,struct snapshot *s)
{ if(s) { for(size_t i=0;i<s->count;++i)release(m,s->items[i].text);release(m,s); } }
static int values_copy(void *p,const struct oa_setting_item *item,struct oa_setting_values *out)
{
    struct oa_settings_model *m=p;
    if(item->value_count>(SIZE_MAX-sizeof(struct snapshot))/sizeof(struct oa_setting_value))return EOVERFLOW;
    struct snapshot *s=allocate(m,sizeof *s+item->value_count*sizeof *s->items);if(!s)return ENOMEM;
    s->count=item->value_count;
    for(size_t i=0;i<s->count;++i) {
        const struct text *source=item->values[i].text;
        s->items[i].id=item->values[i].id;s->items[i].text=text_from(m,source->bytes,source->length);
        if(!s->items[i].text) { snapshot_free(m,s);return ENOMEM; }
    }
    *out=(struct oa_setting_values){s->items,s->count,s};return 0;
}
static void values_drop(void *p,struct oa_setting_values *v) { snapshot_free(p,v->owner); }
static void item_free(struct oa_settings_model *m,struct oa_setting_item *item)
{
    if(!item)return;
    release(m,item->label);release(m,item->glyph_path);release(m,item->image_url);
    for(size_t i=0;i<item->value_count;++i)release(m,item->values[i].text);
    release(m,item->values);release(m,item->image.handle);release(m,item->additional.handle);release(m,item);
}
int oa_settings_model_destroy(struct oa_settings_model *m)
{
    if(!m)return 0;
    if(m->busy)return EBUSY;
    for(size_t i=0;i<m->count;++i)item_free(m,m->items[i]);
    release(m,m->items);release(m,m->ok);struct oa_settings_allocator a=m->allocator;
    a.release(a.context,m);return 0;
}
static int measure(const char *s,size_t *maximum)
{ if(!s)return EINVAL;size_t n=strlen(s);if(n>*maximum)*maximum=n;return 0; }
static int image_valid(const struct oa_settings_image_input *image)
{ return (image->size && !image->data) || (image->id!=UINT32_MAX && !image->size)?EINVAL:0; }
static int image_copy(struct oa_settings_model *m,const struct oa_settings_image_input *source,struct oa_setting_image *out)
{
    out->id=source->id;
    if(source->size) {
        void *data=allocate(m,source->size);if(!data)return ENOMEM;
        memcpy(data,source->data,source->size);out->handle=data;
    }
    return 0;
}
int oa_settings_model_create(const struct oa_settings_item_input *source,size_t count,const char *ok,
                             const struct oa_settings_allocator *a,struct oa_settings_model **out)
{
    size_t maximum=0;int error;
    if(!out || (count && !source) || (a && (!a->allocate || !a->release)))return EINVAL;
    if(count>INT32_MAX || count>SIZE_MAX/sizeof(struct oa_setting_item *))return EOVERFLOW;
    if((error=measure(ok,&maximum)))return error;
    for(size_t i=0;i<count;++i) {
        const struct oa_settings_item_input *s=&source[i];
        if((error=measure(s->label,&maximum)) || (error=measure(s->glyph_path,&maximum)) ||
           (error=measure(s->image_url,&maximum)) || (error=image_valid(&s->image)) ||
           (error=image_valid(&s->additional)) || (s->value_count && !s->values))return error?error:EINVAL;
        if(s->value_count>(SIZE_MAX-sizeof(struct snapshot))/sizeof(struct oa_setting_value))return EOVERFLOW;
        for(size_t j=0;j<s->value_count;++j)if((error=measure(s->values[j].text,&maximum)))return error;
    }
    if(maximum>SIZE_MAX-sizeof(struct text)-12)return EOVERFLOW;
    struct oa_settings_allocator allocator=a?*a:(struct oa_settings_allocator){NULL,default_alloc,default_free};
    struct oa_settings_model *m=allocator.allocate(allocator.context,sizeof *m);if(!m)return ENOMEM;
    memset(m,0,sizeof *m);m->allocator=allocator;m->capacity=maximum+11;
    m->ok=text_from(m,ok,strlen(ok));if(!m->ok) { error=ENOMEM;goto fail; }
    if(count) { m->items=allocate(m,count*sizeof *m->items);if(!m->items) { error=ENOMEM;goto fail; } }
    for(size_t i=0;i<count;++i) {
        const struct oa_settings_item_input *s=&source[i];
        struct oa_setting_item *t=allocate(m,sizeof *t);if(!t) { error=ENOMEM;goto fail; }
        m->items[m->count++]=t;
        t->id=s->id;t->type=s->type;t->value=s->value;t->minimum=s->minimum;t->maximum=s->maximum;
        t->min_image=s->min_image;t->max_image=s->max_image;t->service_logo=s->service_logo;t->glyph=s->glyph;
        t->active=s->active;t->checked=s->checked;
        t->label=text_from(m,s->label,strlen(s->label));t->glyph_path=text_from(m,s->glyph_path,strlen(s->glyph_path));
        t->image_url=text_from(m,s->image_url,strlen(s->image_url));
        if(!t->label || !t->glyph_path || !t->image_url) { error=ENOMEM;goto fail; }
        if((error=image_copy(m,&s->image,&t->image)) || (error=image_copy(m,&s->additional,&t->additional)))goto fail;
        if(s->value_count) {
            struct oa_setting_value *values=allocate(m,s->value_count*sizeof *values);
            if(!values) { error=ENOMEM;goto fail; }t->values=values;t->value_count=s->value_count;
            for(size_t j=0;j<s->value_count;++j) {
                values[j].id=s->values[j].id;values[j].text=text_from(m,s->values[j].text,strlen(s->values[j].text));
                if(!values[j].text) { error=ENOMEM;goto fail; }
            }
        }
    }
    *out=m;return 0;
fail:
    (void)oa_settings_model_destroy(m);return error;
}
int oa_settings_model_draw(struct oa_settings_model *m,int32_t index,void *row,const struct oa_setting_draw_style *style,
                           void (*sink)(void *,void *,const struct oa_setting_draw_event *),void *context)
{
    if(!m || !sink || !row || !style)return EINVAL;
    if(m->busy)return EBUSY;
    m->busy=1;m->sink=sink;m->sink_context=context;
    struct oa_setting_draw_ops ops={m,run,store_type,values_copy,values_drop,text_new,text_assign,text_append,
                                   text_empty,text_empty,text_free,lookup};
    int error=oa_settings_draw_row(&ops,m->items,(int32_t)m->count,index,row,style);
    m->sink=NULL;m->sink_context=NULL;m->busy=0;return error;
}
size_t oa_settings_model_count(const struct oa_settings_model *m) { return m?m->count:0; }
const struct oa_setting_item *oa_settings_model_item(const struct oa_settings_model *m,size_t index)
{ return m && index<m->count?m->items[index]:NULL; }
