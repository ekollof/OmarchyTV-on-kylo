#include "openaurora/settings_row_construct.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned calls;
static struct { uint32_t kind,off,arg; } ev[64];
static int mirror;
static void push(uint32_t kind,uint32_t off,uint32_t arg)
{ assert(calls<64);ev[calls].kind=kind;ev[calls].off=off;ev[calls].arg=arg;calls++; }
static uint32_t rel(const void *p,const void *base)
{ return (uint32_t)((uintptr_t)p-(uintptr_t)base); }

static void *row;
static void c_base(void *row,void *fw,void *ctx) { (void)fw;(void)ctx;push(1,rel(row,row),0); }
#define CHILD(name,kind) static void name(void *c,void *fw,void *ctx) \
{ (void)fw;(void)ctx;push(kind,rel(c,row),0); }
CHILD(c_text,2) CHILD(c_button,3) CHILD(c_sel,4) CHILD(c_slide,5)
CHILD(c_prog,6) CHILD(c_entry,7) CHILD(c_check,8) CHILD(c_img,9)
static void c_ustr(void *slot,const char *literal)
{ push(literal?10:11,rel(slot,row),0); }
static void *c_iface(void *fw) { (void)fw;push(12,0,0);return (void *)0x5000; }
static int c_mirror(void *control) { (void)control;push(13,0,(uint32_t)mirror);return mirror; }
static struct oa_setting_row_construct_ops ops(void)
{ return (struct oa_setting_row_construct_ops){c_base,c_text,c_button,c_sel,c_slide,
    c_prog,c_entry,c_check,c_img,c_ustr,c_iface,c_mirror}; }

int main(void)
{
    static unsigned char buf[0x3000];
    struct oa_setting_row_construct_ops o=ops();
    row=buf;

    memset(buf,0xa5,sizeof buf);
    calls=0;mirror=0;
    oa_setting_row_construct(buf,(void *)0x4000,(void *)0x4004,&o);

    /* base + 17 children + 10 strings + interface + mirror probe */
    assert(calls==30);
    assert(ev[0].kind==1);
    static const struct { uint32_t off,kind; } kids[]={
        {0x104,2},{0x3c4,2},{0x684,3},{0x1014,4},{0x13c0,5},{0x1498,5},
        {0x1570,6},{0x1724,6},{0x18d8,7},{0x1ca8,2},{0x1f68,2},{0x2228,2},
        {0x24e8,2},{0x27a8,8},{0x2b28,9},{0x2c0c,9},{0x2cf0,9}
    };
    for(unsigned i=0;i<17;++i) {
        assert(ev[1+i].kind==kids[i].kind && ev[1+i].off==kids[i].off);
    }
    static const struct { uint32_t off;int def; } strs[]={
        {0x2dd8,0},{0x2de0,0},{0x2e04,0},{0x2e0c,0},{0x2e14,0},
        {0x2e1c,0},{0x2e24,1},{0x2e2c,0},{0x2e34,0},{0x2e3c,0}
    };
    for(unsigned i=0;i<10;++i) {
        assert(ev[18+i].kind==(strs[i].def?11u:10u) && ev[18+i].off==strs[i].off);
    }
    assert(ev[28].kind==12 && ev[29].kind==13);

    /* Default fields, including the mirror-off zeros. */
    static const struct { uint32_t off,value; } words[]={
        {0x2dd4,0},{0x2de8,0},{0x2dec,27},{0x2df0,0xffffffff},{0x2df4,1},
        {0x2df8,0},{0x2dfc,0},{0x2e4c,0},{0x2e50,0},{0x2e54,0},
        {0x2e58,0xffffffff},{0x2e5c,0},{0x2e60,0},{0x2e68,0x234},
        {0x2e6c,0x1f1},{0x2e70,0x1f0},{0x2e74,0x1f3},{0x2e78,0x1f2},
        {0x2e7c,0x1f1},{0x2e80,0x1f0},{0x2e84,0x1f3},{0x2e88,0x1f2},
        {0x2e8c,0x1e9},{0x2e90,0x1e8},{0x2e94,0x1eb},{0x2e98,0x1ea},
        {0x2e9c,0x1fa},{0x2ea0,0x1f8},{0x2ea4,0x1fb},{0x2ea8,0x1f9},
        {0x2eac,0x1f1},{0x2eb0,0x1f0},{0x2eb4,0x1f3},{0x2eb8,0x1f2},
        {0x2ec0,0x1fc},{0x2ebc,0x1fe},{0x2ec8,0x1fd},{0x2ec4,0x1ff},
        {0x2ecc,0x1f5},{0x2ed0,0x1f4},{0x2ed4,0x1f7},{0x2ed8,0x1f6}
    };
    for(unsigned i=0;i<sizeof words/sizeof words[0];++i) {
        uint32_t v;memcpy(&v,buf+words[i].off,4);assert(v==words[i].value);
    }
    static const struct { uint32_t off,value; } bytes[]={
        {0x2e45,0},{0x2e46,0},{0x2e47,0},{0x2e48,0},{0x2e49,1}
    };
    for(unsigned i=0;i<sizeof bytes/sizeof bytes[0];++i) assert(buf[bytes[i].off]==bytes[i].value);

    /* Mirror on rewrites 0x2e4c/0x2e50 to 120/119. */
    memset(buf,0xa5,sizeof buf);
    calls=0;mirror=1;
    oa_setting_row_construct(buf,(void *)0x4000,(void *)0x4004,&o);
    { uint32_t a,b;memcpy(&a,buf+0x2e4c,4);memcpy(&b,buf+0x2e50,4);
      assert(a==120 && b==119); }

    puts("ok settings row construct: base/17 children/10 strings/mirror and defaults");
    return 0;
}
