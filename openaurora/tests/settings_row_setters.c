#include "openaurora/settings_row_setters.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
struct trow {
    uint32_t type;int32_t lo,hi;
    int32_t stored_value,bounds_lo,bounds_hi;char label[32];
    uint32_t child_seen;int created_bar,created_box;
    int32_t percent;char text[16];int32_t range_lo,range_hi;
    unsigned calls;char order[16];
};
static struct trow *t;
static void emit(char c) { assert(t->calls<16);t->order[t->calls++]=c; }
static uint32_t q_type(void *r) { (void)r;return t->type; }
static int32_t q_min(void *r) { (void)r;return t->lo; }
static int32_t q_max(void *r) { (void)r;return t->hi; }
static void *q_child(void *r,uint32_t off) { (void)r;t->child_seen=off;return (void *)(uintptr_t)(0x1000+off); }
static int q_created(void *c)
{
    uint32_t off=(uint32_t)(uintptr_t)c-0x1000u;
    return off==0x24e8?t->created_box:t->created_bar;
}
static void q_text(void *c,const char *s) { (void)c;emit('T');snprintf(t->text,sizeof t->text,"%s",s); }
static void q_percent(void *b,int32_t p) { (void)b;emit('P');t->percent=p; }
static void q_range(void *b,int32_t lo,int32_t hi) { (void)b;emit('R');t->range_lo=lo;t->range_hi=hi; }
static void q_store_label(void *r,const char *s) { (void)r;emit('S');snprintf(t->label,sizeof t->label,"%s",s); }
static void q_store_value(void *r,int32_t v) { (void)r;emit('V');t->stored_value=v; }
static void q_store_bounds(void *r,int32_t lo,int32_t hi) { (void)r;emit('B');t->bounds_lo=lo;t->bounds_hi=hi; }
static struct oa_setting_row_setter_ops ops(void)
{
    return (struct oa_setting_row_setter_ops){t,q_type,q_min,q_max,q_child,q_created,q_text,
        q_percent,q_range,q_store_label,q_store_value,q_store_bounds};
}
int main(void)
{
    /* Shipped wrap/divide semantics, including the device edges. */
    assert(oa_setting_row_percent(50,0,100)==50);
    assert(oa_setting_row_percent(0,0,100)==0);
    assert(oa_setting_row_percent(-30,-100,100)==35);
    assert(oa_setting_row_percent(150,0,100)==150);
    assert(oa_setting_row_percent(7,0,0)==INT32_MAX);
    assert(oa_setting_row_percent(-7,0,0)==INT32_MIN);
    assert(oa_setting_row_percent(0x20000001,1,0)==INT32_MIN);
    assert(oa_setting_row_percent(INT32_MIN,0,1)==(int32_t)((uint32_t)INT32_MIN*100u));
    /* Label: type selects the child, text runs only when created, store always. */
    struct trow r;struct oa_setting_row_setter_ops o;
    memset(&r,0,sizeof r);t=&r;o=ops();r.type=1;r.created_bar=1;
    assert(!oa_setting_row_set_label(&r,&o,"Volume"));
    assert(r.child_seen==0x3c4 && !strcmp(r.text,"Volume") && !strcmp(r.label,"Volume"));
    assert(r.calls==2 && r.order[0]=='T' && r.order[1]=='S');
    memset(&r,0,sizeof r);t=&r;o=ops();r.type=9;r.created_bar=0;
    assert(!oa_setting_row_set_label(&r,&o,""));
    assert(r.child_seen==0x104 && !strcmp(r.label,"") && r.calls==1 && r.order[0]=='S');
    /* Progress: bar select, percent, store, then the +0x24e8 text. */
    memset(&r,0,sizeof r);t=&r;o=ops();r.type=0x17;r.lo=0;r.hi=100;r.created_bar=1;
    r.created_box=1;
    assert(!oa_setting_row_set_progress(&r,&o,50));
    assert(r.child_seen==0x24e8 && r.percent==50 && r.stored_value==50 && !strcmp(r.text,"50"));
    assert(r.calls==3 && r.order[0]=='P' && r.order[1]=='V' && r.order[2]=='T');
    /* Non-0x17 bar, missing text child: percent+store only. */
    memset(&r,0,sizeof r);t=&r;o=ops();r.type=9;r.lo=10;r.hi=20;r.created_bar=1;r.created_box=0;
    assert(!oa_setting_row_set_progress(&r,&o,15));
    assert(r.percent==50 && r.stored_value==15 && r.calls==2);
    /* Range: type 9 selects +0x13c0, others +0x1498; store always. */
    memset(&r,0,sizeof r);t=&r;o=ops();r.type=9;r.created_bar=1;
    assert(!oa_setting_row_set_bar_range(&r,&o,0,100));
    assert(r.child_seen==0x13c0 && r.range_lo==0 && r.range_hi==100);
    assert(r.bounds_lo==0 && r.bounds_hi==100 && r.calls==2 && r.order[0]=='R' && r.order[1]=='B');
    memset(&r,0,sizeof r);t=&r;o=ops();r.type=23;r.created_bar=0;
    assert(!oa_setting_row_set_bar_range(&r,&o,INT32_MIN,INT32_MAX));
    assert(r.child_seen==0x1498 && r.bounds_lo==INT32_MIN && r.bounds_hi==INT32_MAX && r.calls==1);
    /* Invalid ops/args change nothing. */
    o=ops();o.set_text=NULL;
    assert(oa_setting_row_set_label(&r,&o,"x")==EINVAL);
    assert(oa_setting_row_set_progress(&r,NULL,1)==EINVAL);
    assert(oa_setting_row_set_bar_range(NULL,&o,0,0)==EINVAL);
    assert(oa_setting_row_set_label(&r,&o,NULL)==EINVAL);
    puts("ok settings row setters: type-selected children, created gates, percent/divide edges and always-stores");
    return 0;
}
