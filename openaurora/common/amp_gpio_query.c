#include <errno.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "openaurora/amp_gpio_query.h"
struct pin_request { uint32_t pin;uint8_t value,padding[3]; };
_Static_assert(sizeof(struct pin_request)==8,"GPIO ioctl payload extent");
_Static_assert(offsetof(struct pin_request,value)==4,"GPIO ioctl byte value");
#if defined(__arm__) && UINTPTR_MAX == UINT32_MAX
static int native_request(void *c,int fd,unsigned long command,void *p)
{ (void)c;return ioctl(fd,command,p); }
#endif
int oa_amp_reset_query(int fd,oa_amp_reset_state *out,oa_amp_gpio_request request,void *context)
{
    if(fd<0 || !out)return -EINVAL;
    if(!request) {
#if defined(__arm__) && UINTPTR_MAX == UINT32_MAX
        struct stat st;
        if(fstat(fd,&st)<0)return -(errno?errno:EIO);
        if(!S_ISCHR(st.st_mode) || major(st.st_rdev)!=132 || minor(st.st_rdev)!=0)return -ENODEV;
        request=native_request;
#else
        return -ENOTSUP;
#endif
    }
    oa_amp_reset_state state;
    const unsigned long commands[]={0x670d,0x670f};
    for(unsigned i=0;i<2;++i) {
        struct pin_request p={.pin=114,.value=0xff};
        int rc=request(context,fd,commands[i],&p);
        if(rc<0)return -(errno?errno:EIO);
        if(rc || p.pin!=114 || p.value>1)return -EPROTO;
        if(i==0)state.level=p.value;else state.direction=p.value;
    }
    *out=state;return 0;
}
