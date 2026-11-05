/* Actual host + dlopen display module; every device call resolves to this
 * executable's exported fixture functions. No real display is opened. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "host.h"
#include "openaurora/fb_snapshot.h"

static struct fb_var_screeninfo var;
static uint32_t region[8], mau;
static unsigned char pixels[128];
static unsigned opens, closes, loads, unloads;
static int kernel_suspended, block_worker, entered, released;
static pthread_t main_thread;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

void display_fixture_event(int loaded)
{ assert(pthread_equal(main_thread, pthread_self()));if(loaded)++loads;else ++unloads; }
int __wrap_open(const char *path, int flags, ...)
{ assert(!strcmp(path,"/dev/oa-fixture") && flags==O_RDWR);++opens;return 123; }
int __wrap_close(int fd) { assert(fd==123);++closes;return 0; }
int __wrap_system(const char *s) { (void)s;assert(!"unexpected system call");return -1; }
void *__wrap_mmap(void *p,size_t n,int prot,int flags,int fd,off_t offset)
{
    assert(!p && n==sizeof pixels && flags==MAP_SHARED && fd==123 && !offset);
    assert(prot==PROT_READ || prot==(PROT_READ|PROT_WRITE));return pixels;
}
int __wrap_munmap(void *p,size_t n) { assert(p==pixels && n==sizeof pixels);return 0; }
int __wrap_msync(void *p,size_t n,int flags)
{
    const unsigned char *a=p;
    assert(flags==MS_SYNC && n>0);
    assert(a>=pixels && a+n<=pixels+sizeof pixels);
    return 0;
}
int __wrap_ioctl(int fd,unsigned long request,...)
{
    assert(fd==123);
    if(block_worker && !pthread_equal(main_thread,pthread_self())) {
        pthread_mutex_lock(&mu);entered=1;pthread_cond_broadcast(&cv);
        while(!released)pthread_cond_wait(&cv,&mu);
        pthread_mutex_unlock(&mu);
    }
    va_list ap;va_start(ap,request);void *p=va_arg(ap,void *);va_end(ap);
    switch(request) {
    case FBIOGET_VSCREENINFO:memcpy(p,&var,sizeof var);return 0;
    case FBIOGET_FSCREENINFO: {
        struct fb_fix_screeninfo f={.smem_start=0x10000000,.smem_len=sizeof pixels,.line_length=32};
        memcpy(p,&f,sizeof f);return 0;
    }
    case OA_FB_REGION_RD:memcpy(p,region,sizeof region);return 0;
    case OA_FB_REGION_WR:memcpy(region,p,sizeof region);return 0;
    case OA_FB_MAU_RD:*(uint32_t *)p=mau;return 0;
    case FBIOPAN_DISPLAY:
        if(kernel_suspended){errno=EFAULT;return -1;}
        var=*(struct fb_var_screeninfo *)p;return 0;
    default:assert(!"unexpected ioctl");return -1;
    }
}
static const char *config(void *v,const char *key,const char *def)
{
    (void)v;
    if(!strcmp(key,"display.fb"))return "/dev/oa-fixture";
    if(!strcmp(key,"display.restore_snapshot"))return "1";
    if(!strcmp(key,"display.mirror"))return "0";
    return def;
}
static int command(oa_host *h,const char *s)
{
    char line[128],reply[512];snprintf(line,sizeof line,"%s",s);
    return oa_control_command(h,line,reply,sizeof reply);
}
static void retry(oa_host *h,const char *s)
{
    for(unsigned i=0;command(h,s);++i) {
        assert(i<10000);const struct timespec t={.tv_nsec=1000000};nanosleep(&t,NULL);
    }
}
static const oa_display_iface *display(oa_host *h)
{
    const void *s=h->api.lookup(h,OA_SVC_DISPLAY,2);assert(s);return s;
}
static void wait_worker(void)
{
    struct timespec until;assert(!clock_gettime(CLOCK_REALTIME,&until));until.tv_sec+=5;
    pthread_mutex_lock(&mu);
    while(!entered)assert(!pthread_cond_timedwait(&cv,&mu,&until));
    pthread_mutex_unlock(&mu);
}
static void release_worker(void)
{ pthread_mutex_lock(&mu);released=1;pthread_cond_broadcast(&cv);pthread_mutex_unlock(&mu); }
static void block_next(void) { entered=released=0;block_worker=1; }
static oa_mod *start(oa_host *h)
{
    oa_mod *m=oa_module_load(h,"display_fb");assert(m && !oa_module_start(h,m));return m;
}
static unsigned beats;
static uint64_t timer;
static void heartbeat(void *v)
{
    oa_host *h=v;++beats;
    pthread_mutex_lock(&mu);int ready=entered;pthread_mutex_unlock(&mu);
    if(!ready)return;
    oa_mod *m=oa_host_find_module(h,"display_fb");assert(m && m->cleanup_pending && h->shutting_down);
    release_worker();h->api.timer_del(h,timer);
}
int main(int argc,char **argv)
{
    assert(argc==3);assert(!setvbuf(stdout,NULL,_IONBF,0));alarm(30);main_thread=pthread_self();
    var=(struct fb_var_screeninfo){.xres=8,.xres_virtual=8,.yres=2,.yres_virtual=4,
        .bits_per_pixel=32,.yoffset=2};
    memset(pixels,0x67,sizeof pixels);for(unsigned i=0;i<8;++i)region[i]=100+i;
    oa_host *h=calloc(1,sizeof *h);assert(h && !oa_host_init(h,argv[1],NULL));
    h->log_level=-1;h->api.config_get=config;
    oa_mod *m=start(h);uint32_t id=m->id;
    const oa_display_iface *s=display(h);assert(!s->clear(10,20,30) && !s->present());
    oa_mod *peer=oa_module_load(h,argv[2]);assert(peer);
    h->cur_module=peer->id;assert(h->api.lookup(h,OA_SVC_DISPLAY,2)==s);h->cur_module=0;
    assert(command(h,"reload display_fb")==-1 && !m->cleanup_pending);
    assert(!s->clear(10,20,30)); /* ordinary hold rejection never starts cleanup */
    h->cur_module=peer->id;h->api.release(h,OA_SVC_DISPLAY);h->cur_module=0;
    assert(!oa_module_unload(h,peer->name,0));
    puts("ok display DSO: actual registry hold refuses reload before cleanup and keeps rendering usable");

    block_next();unsigned closed=closes, gone=unloads;
    assert(command(h,"reload display_fb")==-1);wait_worker();
    assert(m->cleanup_pending && m->dl && m->ctx && m->id==id && s->present()==-1);
    assert(command(h,"status display_fb")==0 && command(h,"start display_fb")==-1);
    assert(command(h,"reload display_fb --force")==-1 && command(h,"unload display_fb --force")==-1);
    assert(unloads==gone && closes==closed && display(h)==s);
    release_worker();retry(h,"reload display_fb");block_worker=0;
    m=oa_host_find_module(h,"display_fb");assert(m && m->id!=id && m->state==OA_ST_STARTED);
    assert(unloads==gone+1 && closes==closed+1 && loads==2);
    assert(pixels[64]==0x67 && var.yoffset==2);
    puts("ok display DSO: control/force keeps code and fd alive during blocked worker; join permits actual dlclose/reload");

    kernel_suspended=1;
    assert(command(h,"suspend")==-1 && m->state==OA_ST_STARTED && !m->cleanup_pending);
    kernel_suspended=0;assert(!command(h,"suspend") && m->state==OA_ST_SUSPENDED);
    closed=closes;unsigned opened=opens;id=m->id;
    assert(!command(h,"reload display_fb"));m=oa_host_find_module(h,"display_fb");
    assert(m && m->id!=id && m->state==OA_ST_SUSPENDED && m->start_on_resume);
    assert(opens==opened && closes==closed && !oa_host_service_exists(h,OA_SVC_DISPLAY));
    assert(!command(h,"resume") && m->state==OA_ST_STARTED && opens==opened+1);
    retry(h,"unload display_fb");
    puts("ok display DSO: firmware suspended-pan error propagates; suspend/reload defers hardware until resume");

    int collision=1;assert(!h->api.publish(h,OA_SVC_DISPLAY_ALPHA,1,&collision));
    m=oa_module_load(h,"display_fb");assert(m);block_next();gone=unloads;closed=closes;
    assert(oa_module_start(h,m)==-1);wait_worker();
    assert(m->cleanup_pending && m->state==OA_ST_LOADED && m->start_attempted && m->dl);
    assert(command(h,"unload display_fb --force")==-1 && unloads==gone && closes==closed);
    assert(!h->api.unpublish(h,OA_SVC_DISPLAY_ALPHA));release_worker();retry(h,"unload display_fb");block_worker=0;
    assert(unloads==gone+1 && closes==closed+1 && !oa_host_service_exists(h,OA_SVC_DISPLAY));
    puts("ok display DSO: failed publication retains partial-start module and backup through asynchronous cleanup");

    m=start(h);block_next();timer=h->api.timer_add(h,1,1,heartbeat,h);assert(timer);
    oa_host_quit(h);assert(!oa_host_run(h));block_worker=0;
    assert(beats && !oa_host_find_module(h,"display_fb") && opens==closes && loads==unloads);
    assert(pixels[64]==0x67 && var.yoffset==2);
    puts("ok display DSO: real shutdown dispatches heartbeat while driver is blocked and only unloads after join");
    close(h->selfpipe[0]);close(h->selfpipe[1]);assert(!pthread_mutex_destroy(&h->post_mu));free(h);return 0;
}
