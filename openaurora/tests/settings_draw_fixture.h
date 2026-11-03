#ifndef SETTINGS_DRAW_FIXTURE_H
#define SETTINGS_DRAW_FIXTURE_H
#include "openaurora/settings_draw.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
enum { SD_NEW=100,SD_ASSIGN,SD_APPEND,SD_EMPTY,SD_EQUAL,SD_FREE,SD_LOOKUP,SD_COPY,SD_DROP };
struct sd_text { uint32_t token,id; };
struct sd_trace { uint32_t id,a,b,c,d,image; };
struct sd_fixture {
    struct { struct sd_text text;void *owner;int live; } pool[16];
    unsigned allocated,fail_at,count;int copy_error,vector_live;void *row;
    struct sd_trace trace[128];
    struct oa_setting_value copied[3];struct sd_text copied_text[3];
};
static inline void sd_record(struct sd_fixture *f,uint32_t id,uint32_t a,uint32_t b,uint32_t c,uint32_t d,const void *image)
{
    assert(f->count<128);struct sd_trace *t=&f->trace[f->count++];memset(t,0,sizeof *t);
    t->id=id;t->a=a;t->b=b;t->c=c;t->d=d;t->image=(uint32_t)(uintptr_t)image;
}
static inline const struct sd_text *sd_read(struct sd_fixture *f,const void *p)
{
    const struct sd_text *t=p;assert(t);
    if(t->id) { assert(t->id<=f->allocated && t->id<16);assert(f->pool[t->id].live && f->pool[t->id].owner==p); }
    return t;
}
static inline void *sd_construct(struct sd_fixture *f,void *target,const void *source)
{
    uint32_t token=source?sd_read(f,source)->token:0,source_id=source?sd_read(f,source)->id:0;
    unsigned id=++f->allocated;assert(id<16);
    if(f->fail_at==id)return NULL;
    if(!target)target=&f->pool[id].text;
    struct sd_text t={token,id};memcpy(target,&t,sizeof t);
    f->pool[id].owner=target;f->pool[id].live=1;
    sd_record(f,SD_NEW,id,token,source_id,0,NULL);return target;
}
static inline void *sd_new(void *p,const void *source) { return sd_construct(p,NULL,source); }
static inline void sd_assign(void *p,void *target,const void *source)
{
    struct sd_fixture *f=p;const struct sd_text *s=sd_read(f,source);struct sd_text *t=(struct sd_text *)sd_read(f,target);
    sd_record(f,SD_ASSIGN,t->id,s->token,s->id,0,NULL);t->token=s->token;
}
static inline void sd_append(void *p,void *target,uint32_t value)
{
    struct sd_fixture *f=p;struct sd_text *t=(struct sd_text *)sd_read(f,target);
    sd_record(f,SD_APPEND,t->id,value,0,0,NULL);t->token=value^UINT32_C(0x12345678);
}
static inline int sd_empty(void *p,const void *text)
{
    struct sd_fixture *f=p;const struct sd_text *t=sd_read(f,text);
    sd_record(f,SD_EMPTY,t->token,t->id,0,0,NULL);return t->token==0;
}
static inline int sd_equal(void *p,const void *text)
{
    struct sd_fixture *f=p;const struct sd_text *t=sd_read(f,text);
    sd_record(f,SD_EQUAL,t->token,t->id,0,0,NULL);return t->token==0;
}
static inline void sd_free(void *p,void *text)
{
    struct sd_fixture *f=p;const struct sd_text *t=sd_read(f,text);assert(t->id);
    sd_record(f,SD_FREE,t->id,t->token,0,0,NULL);f->pool[t->id].live=0;
    memset(text,0xdd,sizeof *t);
}
static inline void sd_lookup(void *p,void *text)
{
    struct sd_fixture *f=p;struct sd_text *t=(struct sd_text *)sd_read(f,text);
    sd_record(f,SD_LOOKUP,t->id,0,0,0,NULL);t->token=99;
}
static inline void sd_run(void *p,void *row,const struct oa_setting_draw_event *e)
{
    struct sd_fixture *f=p;assert(row==f->row);
    sd_record(f,e->action,e->a,e->b,e->text?sd_read(f,e->text)->token:0,
              e->text2?sd_read(f,e->text2)->token:0,e->image);
}
static inline void sd_store_type(void *p,struct oa_setting_item *item,uint32_t type)
{ (void)p;item->type=type; }
static inline void sd_no_leak(struct sd_fixture *f)
{ for(unsigned i=1;i<16;++i)assert(!f->pool[i].live);assert(!f->vector_live); }
static inline int sd_copy(void *p,const struct oa_setting_item *item,struct oa_setting_values *v)
{
    struct sd_fixture *f=p;assert(!f->vector_live && item->value_count<=3);
    if(f->copy_error)return f->copy_error;
    f->vector_live=1;sd_record(f,SD_COPY,(uint32_t)item->value_count,0,0,0,NULL);
    for(size_t i=0;i<item->value_count;++i) {
        f->copied_text[i]=(struct sd_text){sd_read(f,item->values[i].text)->token,0};
        f->copied[i]=(struct oa_setting_value){item->values[i].id,&f->copied_text[i]};
    }
    *v=(struct oa_setting_values){f->copied,item->value_count,f};return 0;
}
static inline void sd_drop(void *p,struct oa_setting_values *v)
{
    struct sd_fixture *f=p;assert(f->vector_live && v->owner==f);
    for(unsigned i=1;i<16;++i)assert(!f->pool[i].live);
    sd_record(f,SD_DROP,(uint32_t)v->count,0,0,0,NULL);f->vector_live=0;
    memset(f->copied_text,0xdd,sizeof f->copied_text);
}
static inline struct oa_setting_draw_ops sd_ops(struct sd_fixture *f)
{ return (struct oa_setting_draw_ops){f,sd_run,sd_store_type,sd_copy,sd_drop,sd_new,sd_assign,sd_append,sd_empty,sd_equal,sd_free,sd_lookup}; }
#endif
