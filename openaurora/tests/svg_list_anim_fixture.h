#ifndef OA_ANIM_FIXTURE_H
#define OA_ANIM_FIXTURE_H
#include "openaurora/svg_list_anim.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
enum {A_EN,A_SEL,A_ROW,A_RECT,A_RCUR,A_WPREV,A_WCUR,A_STD,A_COUNT,A_AT,A_NEW,A_LEN,A_SUB,A_EQ,A_FREE,A_CTRL,A_START,A_PARSE};
struct anim_string {char text[128];unsigned live,role;};
struct anim_entry {const char *name,*text;};
struct anim_fixture {
    int enabled,has_row;uint32_t selected,owner,rect[4],cur[4],prev[4],nanims;
    const char *stdname,*names[4],*durs[4];int present[4];
    void *list,*manager,*row;
    struct anim_entry entries[4];struct anim_string strings[3];
    unsigned allocations,fail_on;int missing_controller;
    uint32_t started_bits;int started;
    unsigned count;struct {uint32_t id,a,b;} trace[96];
};
static inline void anim_emit(struct anim_fixture *f,uint32_t id,uint32_t a,uint32_t b)
{ assert(f->count<96);f->trace[f->count].id=id;f->trace[f->count].a=a;f->trace[f->count++].b=b; }
/* Test provider for valid UTF-8 only. Firmware length/walk count characters;
 * substring bounds use wrapping start+length, then clamp each walk to bytes.
 * This is not a production UString implementation or malformed-UTF8 oracle. */
static inline size_t af_walk(const char *s,size_t n,uint32_t chars)
{
    size_t p=0;while(p<n && chars--) {
        unsigned char c=(unsigned char)s[p];size_t step=c<0x80?1:c<0xe0?2:c<0xf0?3:4;
        assert(step<=n-p);p+=step;
    }return p;
}
static inline uint32_t af_length(const char *s)
{ size_t n=strlen(s),p=0;uint32_t count=0;while(p<n){size_t step=af_walk(s+p,n-p,1);p+=step;++count;}return count; }
static inline void *af_make(struct anim_fixture *f,unsigned role,const char *s,size_t n)
{
    assert(role<3 && !f->strings[role].live && n<sizeof f->strings[role].text);
    if(++f->allocations==f->fail_on)return NULL;
    struct anim_string *v=&f->strings[role];memcpy(v->text,s,n);v->text[n]=0;v->role=role;v->live=1;return v;
}
static inline const struct anim_string *af_string(const void *p)
{ const struct anim_string *s=p;assert(s && s->live);return s; }
static inline int af_enabled(void *p,void *l) { struct anim_fixture *f=p;assert(l==f->list);anim_emit(f,A_EN,0,0);return f->enabled; }
static inline uint32_t af_selected(void *p,void *m) { struct anim_fixture *f=p;assert(m==f->manager);anim_emit(f,A_SEL,0,0);return f->selected; }
static inline void *af_row(void *p,void *m,uint32_t i) { struct anim_fixture *f=p;assert(m==f->manager);anim_emit(f,A_ROW,i,0);return f->has_row?f->row:NULL; }
static inline void af_rect(void *p,void *r,uint32_t out[4]) { struct anim_fixture *f=p;assert(r==f->row);memcpy(out,f->rect,16);anim_emit(f,A_RECT,0,0); }
static inline void af_rcur(void *p,void *l,uint32_t out[4]) { struct anim_fixture *f=p;assert(l==f->list);memcpy(out,f->cur,16); }
static inline void af_wprev(void *p,void *l,const uint32_t in[4]) { struct anim_fixture *f=p;assert(l==f->list);memcpy(f->prev,in,16); }
static inline void af_wcur(void *p,void *l,const uint32_t in[4]) { struct anim_fixture *f=p;assert(l==f->list);memcpy(f->cur,in,16); }
static inline void *af_std(void *p,void *l,uint32_t id) { struct anim_fixture *f=p;assert(l==f->list && id==55);anim_emit(f,A_STD,id,0);return af_make(f,0,f->stdname,strlen(f->stdname)); }
static inline uint32_t af_count(void *p,void *c) { struct anim_fixture *f=p;assert(c==(char *)f->list+0x280);anim_emit(f,A_COUNT,0,0);return f->nanims; }
static inline void *af_at(void *p,void *c,uint32_t i) { struct anim_fixture *f=p;assert(c==(char *)f->list+0x280 && i<4);anim_emit(f,A_AT,i,0);f->entries[i]=(struct anim_entry){f->names[i],f->durs[i]};return f->present[i]?&f->entries[i]:NULL; }
static inline const char *af_name(void *p,void *a) { (void)p;return ((struct anim_entry *)a)->name; }
static inline const char *af_text(void *p,void *a) { (void)p;return ((struct anim_entry *)a)->text; }
static inline void *af_owner(void *p,void *l) { struct anim_fixture *f=p;assert(l==f->list);return (void *)(uintptr_t)f->owner; }
static inline void *af_container(void *p,void *l) { struct anim_fixture *f=p;assert(l==f->list);return (char *)l+0x280; }
static inline void *af_target(void *p,void *l) { struct anim_fixture *f=p;assert(l==f->list);return (char *)l+0x98; }
static inline void *af_ctrl(void *p,void *owner) { struct anim_fixture *f=p;assert((uintptr_t)owner==f->owner);anim_emit(f,A_CTRL,f->owner,0);return f->missing_controller?NULL:(void *)(uintptr_t)0x77aa; }
static inline void af_start(void *p,void *ctrl,void *target,float value) { struct anim_fixture *f=p;assert(ctrl==(void *)(uintptr_t)0x77aa && target==(char *)f->list+0x98 && f->strings[0].live && !f->strings[1].live && !f->strings[2].live);uint32_t bits;memcpy(&bits,&value,4);anim_emit(f,A_START,bits,0);f->started=1;f->started_bits=bits; }
static inline void *af_new(void *p,const char *s) { struct anim_fixture *f=p;if(!s)s="";anim_emit(f,A_NEW,(uint32_t)strlen(s),0);return af_make(f,1,s,strlen(s)); }
static inline uint32_t af_len(void *p,const void *s) { struct anim_fixture *f=p;uint32_t n=af_length(af_string(s)->text);anim_emit(f,A_LEN,n,0);return n; }
static inline void *af_sub(void *p,const void *s,uint32_t start,uint32_t length)
{
    struct anim_fixture *f=p;const char *text=af_string(s)->text;size_t n=strlen(text),a=0,b=0;
    anim_emit(f,A_SUB,start,length);
    if(start+length<=af_length(text)){a=af_walk(text,n,start);b=af_walk(text+a,n-a,length);}
    return af_make(f,2,text+a,b);
}
static inline int af_equal(void *p,const void *a,const void *b) { struct anim_fixture *f=p;const struct anim_string *l=af_string(a),*r=af_string(b);int match=!strcmp(l->text,r->text);anim_emit(f,A_EQ,(uint32_t)match,(l->role<<16)|r->role);return match; }
static inline void af_free(void *p,void *s) { struct anim_fixture *f=p;struct anim_string *v=s;assert(v && v->live);anim_emit(f,A_FREE,v->role,0);v->live=0;memset(v->text,0xdd,sizeof v->text); }
static inline double af_parse(void *p,const char *text) { struct anim_fixture *f=p;assert(f->strings[0].live && f->strings[1].live && f->strings[2].live);anim_emit(f,A_PARSE,(uint32_t)strlen(text),7);return atof(text); }
static inline struct oa_svg_list_anim_ops anim_ops(struct anim_fixture *f)
{ return (struct oa_svg_list_anim_ops){f,af_enabled,af_selected,af_row,af_rect,af_rcur,af_wprev,af_wcur,af_std,af_count,af_at,af_name,af_text,af_ctrl,af_owner,af_container,af_target,af_start,af_new,af_len,af_sub,af_equal,af_free,af_parse}; }
static inline void af_clean(const struct anim_fixture *f) { for(unsigned i=0;i<3;++i)assert(!f->strings[i].live); }
#endif
