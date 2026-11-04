#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/amp_gpio_query.h"
static unsigned calls,fail_at,bad_at,bad_kind,level,direction;
static int request(void *c,int fd,unsigned long command,void *payload)
{
    assert(c==&calls && fd==17);unsigned index=++calls;
    assert(index<=2 && command==(index==1?0x670d:0x670f));
    uint8_t *p=payload;uint32_t pin;memcpy(&pin,p,4);
    assert(pin==114 && p[4]==0xff && !p[5] && !p[6] && !p[7]);
    if(index==fail_at){errno=EINTR;return -1;}
    p[4]=(uint8_t)(index==1?level:direction);
    /* Kernel GET writes one byte; adjacent copied bytes must not participate. */
    p[5]=0xa5;p[6]=0x5a;p[7]=0xff;
    if(index==bad_at) {
        if(bad_kind==0)return 1;
        if(bad_kind==1)p[4]=2;
        if(bad_kind==2){pin=115;memcpy(p,&pin,4);}
    }
    return 0;
}
int main(void)
{
    oa_amp_reset_state out={0xa5,0x5a},saved=out;
    assert(oa_amp_reset_query(-1,&out,request,&calls)==-EINVAL && !calls);
    assert(oa_amp_reset_query(17,NULL,request,&calls)==-EINVAL && !calls);
    for(level=0;level<2;++level)for(direction=0;direction<2;++direction) {
        calls=0;assert(!oa_amp_reset_query(17,&out,request,&calls) && calls==2);
        assert(out.level==level && out.direction==direction);
    }
    puts("ok amp GPIO query: exact114/8-byte GET-only sequence, all levels/directions and byte-width replies");
    level=direction=0;
    for(fail_at=1;fail_at<=2;++fail_at) {
        calls=0;out=saved;
        assert(oa_amp_reset_query(17,&out,request,&calls)==-EINTR && calls==fail_at);
        assert(!memcmp(&out,&saved,sizeof out));
    }
    fail_at=0;level=direction=0;
    for(bad_at=1;bad_at<=2;++bad_at)for(bad_kind=0;bad_kind<3;++bad_kind) {
        calls=0;out=saved;
        assert(oa_amp_reset_query(17,&out,request,&calls)==-EPROTO && calls==bad_at);
        assert(!memcmp(&out,&saved,sizeof out));
    }
    puts("ok amp GPIO query: errors and malformed pin/boolean/status replies reject atomically without retry or setters");
    return 0;
}
