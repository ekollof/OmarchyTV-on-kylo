#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/amp_stbc.h"
static unsigned calls,setting,level,reply16,reply8;
static int error;
static int request(void *c,int fd,unsigned long command,struct v4l2_ext_controls *list)
{
    assert(c==&calls && fd==17 && command==UINT32_C(0xc0185648));++calls;
    assert(list->ctrl_class==0x0f170000 && list->count==(setting?3u:4u));
    assert(!list->error_idx && !list->request_fd && !list->reserved[0]);
    uint8_t *bytes=list->controls[0].ptr;
    for(unsigned i=0;i<list->count;++i) {
        struct v4l2_ext_control *p=list->controls+i;
        assert(p->id==(setting?0x0f170f0fu:0x0f170f10u) && p->size==1 && !p->reserved2[0]);
        assert(p->ptr==bytes+i);
    }
    assert(!bytes[0] && bytes[1]==8);
    if(setting)assert(bytes[2]==(level?2:3));
    else { assert(!bytes[2] && !bytes[3]);bytes[2]=(uint8_t)reply16;bytes[3]=(uint8_t)reply8; }
    if(error==1){errno=EINTR;return -1;}
    bytes[0]=error==2?0:error==3?2:1;
    if(error==4)bytes[1]=9;
    if(error==5 && setting)bytes[2]=4;
    return 1; /* positive ioctl result is distinct from device-success byte */
}
int main(void)
{
    oa_amp_stbc_state out={0xa5,0x5a},saved=out;
    assert(oa_amp_stbc_query(-1,&out,request,&calls)==-EINVAL && !calls);
    assert(oa_amp_stbc_query(17,NULL,request,&calls)==-EINVAL && !calls);
    assert(oa_amp_stbc_query(17,&out,NULL,NULL)==-ENOTSUP && !memcmp(&out,&saved,sizeof out));
    for(reply16=0;reply16<256;++reply16) {
        reply8=255-reply16;assert(!oa_amp_stbc_query(17,&out,request,&calls));
        assert(out.byte16==reply16 && out.byte8==reply8);
    }
    for(error=1;error<=4;++error) {
        unsigned before=calls;out=saved;
        int expected=error==1?-EINTR:error==2?-EIO:-EPROTO;
        assert(oa_amp_stbc_query(17,&out,request,&calls)==expected && calls==before+1);
        assert(!memcmp(&out,&saved,sizeof out));
    }
    puts("ok amp STBC: exact class/ID/control/payload layout, 256 raw query replies, separate device status and atomic/no-replay failure");
    error=0;setting=1;
    for(level=0;level<2;++level)assert(!oa_amp_stbc_set_level(17,level,request,&calls));
    unsigned before=calls;
    assert(oa_amp_stbc_set_level(17,2,request,&calls)==-EINVAL && calls==before);
    assert(oa_amp_stbc_set_level(17,0,NULL,NULL)==-ENOTSUP && calls==before);
    level=0;
    for(error=1;error<=5;++error) {
        int expected=error==1?-EINTR:error==2?-EIO:-EPROTO;before=calls;
        assert(oa_amp_stbc_set_level(17,level,request,&calls)==expected && calls==before+1);
    }
    puts("ok amp STBC: fixed MCU8 level0/1 -> command3/2, explicit injection only, failure/error/shape checks and no replay");
    return 0;
}
