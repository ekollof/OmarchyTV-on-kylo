#include <errno.h>
#include <stddef.h>
#include "openaurora/amp_stbc.h"
#if defined(__arm__) && UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(struct v4l2_ext_control)==20,"ARM private control extent");
_Static_assert(offsetof(struct v4l2_ext_control,ptr)==12,"ARM control pointer");
_Static_assert(sizeof(struct v4l2_ext_controls)==24,"ARM private list extent");
_Static_assert(offsetof(struct v4l2_ext_controls,controls)==20,"ARM list pointer");
#endif
static int exchange(int fd,uint32_t id,uint8_t *bytes,unsigned count,oa_amp_stbc_request request,void *context)
{
    if(!request)return -ENOTSUP;
    struct v4l2_ext_control controls[4]={0};
    for(unsigned i=0;i<count;++i) {
        controls[i].id=id;controls[i].size=1;controls[i].ptr=bytes+i;
    }
    struct v4l2_ext_controls list={.ctrl_class=0x0f170000,.count=count,.controls=controls};
    int rc=request(context,fd,UINT32_C(0xc0185648),&list);
    if(rc<0)return -(errno?errno:EIO);
    /* Kernel stores DRV_STBC_*'s boolean in control0. The NDAL wrapper's
     * transport success ignores this separate result; do not publish fake
     * zero-valued state or report a completed MCU operation on that basis. */
    if(bytes[0]!=1)return bytes[0]==0?-EIO:-EPROTO;
    return bytes[1]==8?0:-EPROTO;
}
int oa_amp_stbc_query(int fd,oa_amp_stbc_state *out,oa_amp_stbc_request request,void *context)
{
    if(fd<0 || !out)return -EINVAL;
    uint8_t bytes[4]={0,8,0,0};
    int rc=exchange(fd,UINT32_C(0x0f170f10),bytes,4,request,context);
    if(!rc)*out=(oa_amp_stbc_state){bytes[2],bytes[3]};
    return rc;
}
int oa_amp_stbc_set_level(int fd,unsigned level,oa_amp_stbc_request request,void *context)
{
    if(fd<0 || level>1)return -EINVAL;
    uint8_t command=level?2:3,bytes[3]={0,8,command};
    int rc=exchange(fd,UINT32_C(0x0f170f0f),bytes,3,request,context);
    if(!rc && bytes[2]!=command)return -EPROTO;
    return rc;
}
