#include <errno.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include "openaurora/amp_i2c.h"
struct oa_amp_i2c { oa_amp_i2c_io io;pid_t pid;int fd,writable,error; };
#if defined(__arm__) && UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(struct i2c_msg)==12,"target i2c_msg ABI");
_Static_assert(sizeof(struct i2c_rdwr_ioctl_data)==8,"target I2C_RDWR ABI");
static int native_open(void *c,const char *path,int flags) { (void)c;return open(path,flags); }
static int native_stat(void *c,int fd,struct stat *st) { (void)c;return fstat(fd,st); }
static int native_transfer(void *c,int fd,struct i2c_rdwr_ioctl_data *r)
{ (void)c;return ioctl(fd,I2C_RDWR,r); }
static int native_close(void *c,int fd) { (void)c;return close(fd); }
static void native_delay(void *c,unsigned ms) { (void)c;(void)usleep(ms*1000); }
static const oa_amp_i2c_io native={NULL,native_open,native_stat,native_transfer,native_close,native_delay};
#endif
static int failure(void) { return -(errno?errno:EIO); }
static int valid(oa_amp_i2c *s) { return !s?-EINVAL:s->pid!=getpid()?-ECHILD:0; }
int oa_amp_i2c_open(oa_amp_i2c **out,int allow_write,const oa_amp_i2c_io *io)
{
    if(!out || (allow_write!=0 && allow_write!=1))return -EINVAL;
    if(!io) {
#if defined(__arm__) && UINTPTR_MAX == UINT32_MAX
        io=&native;
#else
        return -ENOTSUP;
#endif
    }
    if(!io->open || !io->stat || !io->transfer || !io->close || !io->delay_ms)return -EINVAL;
    oa_amp_i2c *s=calloc(1,sizeof *s);if(!s)return -ENOMEM;
    s->io=*io;s->pid=getpid();s->writable=allow_write;
    s->fd=io->open(io->context,"/dev/i2c-3",O_RDWR|O_CLOEXEC);
    if(s->fd<0){int rc=failure();free(s);return rc;}
    struct stat st;
    int rc=io->stat(io->context,s->fd,&st)<0?failure():0;
    if(!rc && (!S_ISCHR(st.st_mode) || major(st.st_rdev)!=89 || minor(st.st_rdev)!=3))rc=-ENODEV;
    if(rc){(void)io->close(io->context,s->fd);free(s);return rc;}
    *out=s;return 0;
}
static int transfer(oa_amp_i2c *s,uint8_t reg,uint8_t *bytes,unsigned count,int read)
{
    struct i2c_msg messages[2]={
        {.addr=0x34,.flags=read?I2C_M_RD:0,.len=1,.buf=&reg},
        {.addr=0x34,.flags=read?I2C_M_RD:0,.len=(uint16_t)count,.buf=bytes}
    };
    struct i2c_rdwr_ioctl_data request={.msgs=messages,.nmsgs=2};
    int done=s->io.transfer(s->io.context,s->fd,&request);
    s->error=done<0?failure():done==2?0:-EIO;
    return s->error;
}
int oa_amp_i2c_read_volume(oa_amp_i2c *s,uint8_t out[3])
{
    int rc=valid(s);if(rc)return rc;
    if(!out)return s->error=-EINVAL;
    uint8_t bytes[3]={0};rc=transfer(s,3,bytes,3,1);
    if(!rc)memcpy(out,bytes,sizeof bytes);
    return rc;
}
int oa_amp_i2c_write(oa_amp_i2c *s,unsigned reg,const uint8_t *value)
{
    int rc=valid(s);if(rc)return rc;
    if(!value || reg>=38)return s->error=-EINVAL;
    if(!s->writable)return s->error=-EACCES;
    uint8_t byte=*value;rc=transfer(s,(uint8_t)reg,&byte,1,0);
    if(!rc)s->io.delay_ms(s->io.context,10);
    return rc;
}
int oa_amp_i2c_write_cb(void *s,unsigned reg,const uint8_t *value)
{ return oa_amp_i2c_write(s,reg,value)==0; }
int oa_amp_i2c_error(oa_amp_i2c *s) { int rc=valid(s);return rc?rc:s->error; }
int oa_amp_i2c_close(oa_amp_i2c **session)
{
    if(!session)return -EINVAL;
    oa_amp_i2c *s=*session;int rc=valid(s);if(rc)return rc;
    rc=s->io.close(s->io.context,s->fd)<0?failure():0;
    *session=NULL;free(s);return rc;
}
