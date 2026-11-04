#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <unistd.h>
#include "openaurora/amp_i2c.h"
static struct {
    unsigned opens,stats,transfers,closes,delays,reg,byte;
    int open_error,stat_error,identity,done,io_error,close_error;
} hw={.identity=1,.done=2};
static int op_open(void *v,const char *path,int flags)
{
    assert(v==&hw && !strcmp(path,"/dev/i2c-3") && flags==(O_RDWR|O_CLOEXEC));++hw.opens;
    if(hw.open_error){errno=hw.open_error;return -1;}return 17;
}
static int op_stat(void *v,int fd,struct stat *out)
{
    assert(v==&hw && fd==17);++hw.stats;
    if(hw.stat_error){errno=hw.stat_error;return -1;}
    memset(out,0,sizeof *out);out->st_mode=hw.identity==2?S_IFREG:S_IFCHR;
    out->st_rdev=makedev(hw.identity==3?90:89,hw.identity==4?4:3);return 0;
}
static int op_transfer(void *v,int fd,struct i2c_rdwr_ioctl_data *r)
{
    assert(v==&hw && fd==17 && r && r->nmsgs==2);++hw.transfers;
    struct i2c_msg *m=r->msgs;assert(m && m[0].addr==0x34 && m[1].addr==0x34);
    assert(m[0].len==1 && m[0].buf && m[1].buf && m[0].flags==m[1].flags);
    if(m[1].flags==I2C_M_RD) {
        assert(*m[0].buf==3 && m[1].len==3);
        m[1].buf[0]=0x1b;m[1].buf[1]=5;m[1].buf[2]=6;
    } else {
        assert(!m[1].flags && m[1].len==1 && *m[0].buf==hw.reg && *m[1].buf==hw.byte);
    }
    errno=hw.io_error;return hw.done;
}
static int op_close(void *v,int fd)
{ assert(v==&hw && fd==17);++hw.closes;if(hw.close_error){errno=hw.close_error;return -1;}return 0; }
static void op_delay(void *v,unsigned ms) { assert(v==&hw && ms==10);++hw.delays; }
static oa_amp_i2c_io callbacks(void)
{ return (oa_amp_i2c_io){&hw,op_open,op_stat,op_transfer,op_close,op_delay}; }
int main(void)
{
    oa_amp_i2c *s=(void *)1;oa_amp_i2c_io io=callbacks();
    assert(oa_amp_i2c_open(&s,2,&io)==-EINVAL && s==(void *)1);
    oa_amp_i2c_io bad=io;bad.stat=NULL;
    assert(oa_amp_i2c_open(&s,0,&bad)==-EINVAL && !hw.opens);
    hw.open_error=ENOENT;assert(oa_amp_i2c_open(&s,0,&io)==-ENOENT && !hw.closes);hw.open_error=0;
    hw.stat_error=EIO;assert(oa_amp_i2c_open(&s,0,&io)==-EIO && hw.closes==1);hw.stat_error=0;
    for(hw.identity=2;hw.identity<=4;++hw.identity)
        assert(oa_amp_i2c_open(&s,0,&io)==-ENODEV && s==(void *)1);
    hw.identity=1;assert(!oa_amp_i2c_open(&s,0,&io));memset(&io,0,sizeof io);
    uint8_t out[3]={0},byte=77;unsigned count=hw.transfers;
    assert(oa_amp_i2c_write(s,0,&byte)==-EACCES && hw.transfers==count && !hw.delays);
    assert(!oa_amp_i2c_write_cb(s,0,&byte) && oa_amp_i2c_error(s)==-EACCES);
    assert(!oa_amp_i2c_read_volume(s,out) && out[0]==0x1b && out[1]==5 && out[2]==6);
    assert(!oa_amp_i2c_error(s) && !oa_amp_i2c_close(&s) && !s);
    puts("ok amp I2C: opened-device identity, copied callbacks, cleanup and explicit read-only permission");

    io=callbacks();assert(!oa_amp_i2c_open(&s,1,&io));
    for(hw.reg=0;hw.reg<38;++hw.reg) {
        hw.byte=(hw.reg*7)&255;byte=(uint8_t)hw.byte;unsigned delays=hw.delays;
        assert(oa_amp_i2c_write_cb(s,hw.reg,&byte)==1 && hw.delays==delays+1 && byte==hw.byte);
    }
    count=hw.transfers;
    assert(oa_amp_i2c_write(s,38,&byte)==-EINVAL && oa_amp_i2c_write(s,0,NULL)==-EINVAL);
    assert(oa_amp_i2c_read_volume(s,NULL)==-EINVAL && hw.transfers==count);
    hw.reg=3;hw.byte=byte;unsigned delays=hw.delays;
    const int results[]={0,1,3,-1,-1,-1};const int errors[]={0,0,0,EINTR,ENXIO,0};
    for(unsigned i=0;i<sizeof results/sizeof *results;++i) {
        hw.done=results[i];hw.io_error=errors[i];memset(out,0xa5,sizeof out);count=hw.transfers;
        int expected=results[i]<0 && errors[i]?-errors[i]:-EIO;
        assert(oa_amp_i2c_read_volume(s,out)==expected && hw.transfers==count+1);
        assert(out[0]==0xa5 && out[1]==0xa5 && out[2]==0xa5);
        assert(!oa_amp_i2c_write_cb(s,3,&byte) && oa_amp_i2c_error(s)==expected);
        assert(hw.transfers==count+2 && hw.delays==delays);
    }
    hw.done=2;hw.io_error=0;assert(!oa_amp_i2c_read_volume(s,out));
    puts("ok amp I2C: 38 single-byte writes and post-write delay; short/error/EINTR reject without replay or partial read publication");

    pid_t child=fork();assert(child>=0);
    if(!child) {
        int good=oa_amp_i2c_read_volume(s,out)==-ECHILD && oa_amp_i2c_write(s,0,&byte)==-ECHILD &&
            oa_amp_i2c_error(s)==-ECHILD && oa_amp_i2c_close(&s)==-ECHILD;
        _exit(good?0:1);
    }
    int status;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    unsigned closed=hw.closes;hw.close_error=EIO;
    assert(oa_amp_i2c_close(&s)==-EIO && !s && hw.closes==closed+1);
    assert(oa_amp_i2c_close(&s)==-EINVAL && hw.closes==closed+1);
    puts("ok amp I2C: inherited handles refuse I/O; final-close failure consumes handle and never replays fd");
    return 0;
}
