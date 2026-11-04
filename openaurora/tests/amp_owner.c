/* Deterministic worker, copy/backpressure, fork and TLS-exit lifetime tests. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "openaurora/amp_owner.h"
static struct {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    unsigned entered,released,idle_jobs,destructor_entered,destructor_released;
    unsigned calls,writes,fail_write;
    int block,use_tls,join_error;
    pthread_t main_thread,worker;
    pthread_key_t key;
} hw={.mu=PTHREAD_MUTEX_INITIALIZER,.cv=PTHREAD_COND_INITIALIZER,.fail_write=38};
int __real_pthread_cond_wait(pthread_cond_t *,pthread_mutex_t *);
int __wrap_pthread_cond_wait(pthread_cond_t *cv,pthread_mutex_t *mu)
{
    if(mu!=&hw.mu) {
        /* Observe real owner idle waits; no scheduler sleeps or fake results.
         * A post-I/O idle wait proves completion was published but not taken. */
        pthread_mutex_lock(&hw.mu);hw.idle_jobs=hw.calls;pthread_cond_broadcast(&hw.cv);pthread_mutex_unlock(&hw.mu);
    }
    return __real_pthread_cond_wait(cv,mu);
}
int __real_pthread_tryjoin_np(pthread_t,void **);
int __wrap_pthread_tryjoin_np(pthread_t thread,void **result)
{
    if(hw.join_error){int rc=hw.join_error;hw.join_error=0;return rc;}
    return __real_pthread_tryjoin_np(thread,result);
}
static void wait_for(unsigned *value,unsigned threshold)
{
    struct timespec until;assert(!clock_gettime(CLOCK_REALTIME,&until));until.tv_sec+=5;
    pthread_mutex_lock(&hw.mu);
    while(*value<threshold)assert(!pthread_cond_timedwait(&hw.cv,&hw.mu,&until));
    pthread_mutex_unlock(&hw.mu);
}
static void tls_destructor(void *v)
{
    assert(v==&hw);pthread_mutex_lock(&hw.mu);hw.destructor_entered=1;pthread_cond_broadcast(&hw.cv);
    while(!hw.destructor_released)pthread_cond_wait(&hw.cv,&hw.mu);
    pthread_mutex_unlock(&hw.mu);
}
static void prelude(void *v)
{
    assert(v==&hw && !pthread_equal(pthread_self(),hw.main_thread));
    pthread_mutex_lock(&hw.mu);hw.worker=pthread_self();++hw.entered;++hw.calls;hw.writes=0;
    pthread_cond_broadcast(&hw.cv);
    while(hw.block && !hw.released)pthread_cond_wait(&hw.cv,&hw.mu);
    pthread_mutex_unlock(&hw.mu);
    if(hw.use_tls)assert(!pthread_setspecific(hw.key,&hw));
}
static void check_thread(void *v) { assert(v==&hw && pthread_equal(hw.worker,pthread_self())); }
static const oa_amp_gpio *gpio(void *v,unsigned fn) { check_thread(v);assert(fn<2);return NULL; }
static void direction(void *v,uint32_t pin,uint32_t dir) { check_thread(v);(void)pin;(void)dir; }
static int level(void *v,uint32_t pin,uint32_t value) { check_thread(v);(void)pin;(void)value;return 7; }
static void delay(void *v,unsigned ms) { check_thread(v);assert(ms==5 || ms==10 || ms==1000); }
static int write_reg(void *v,unsigned reg,const uint8_t *value)
{ check_thread(v);assert(reg==hw.writes++ && value);return reg!=hw.fail_write; }
static uint32_t sub_get(void *v) { check_thread(v);return 0; }
static void sub_enable(void *v,int enabled) { check_thread(v);assert(!enabled); }
static void sub_init(void *v) { check_thread(v); }
static void release(void)
{ pthread_mutex_lock(&hw.mu);hw.released=1;pthread_cond_broadcast(&hw.cv);pthread_mutex_unlock(&hw.mu); }
static void finish(oa_amp_owner *o)
{
    for(unsigned i=0;i<5000;++i) {
        int rc=oa_amp_owner_quiesce(o);if(!rc)return;assert(rc==-EBUSY);
        const struct timespec pause={.tv_nsec=1000000};nanosleep(&pause,NULL);
    }
    assert(0);
}
int main(void)
{
    hw.main_thread=pthread_self();assert(!pthread_key_create(&hw.key,tls_destructor));
    oa_amp_init_state initial={.reset_cached=7,.mute_cached=9};
    for(unsigned i=0;i<38;++i)initial.image[i]=(uint8_t)(0xa0+i);
    oa_amp_init_io io={&hw,prelude,gpio,direction,level,delay,write_reg,sub_get,sub_enable,sub_init};
    oa_amp_owner *o=(void *)1;
    assert(oa_amp_owner_create(&o,&initial,NULL)==-ENOTSUP && o==(void *)1);
    oa_amp_init_io bad=io;bad.write=NULL;
    assert(oa_amp_owner_create(&o,&initial,&bad)==-EINVAL && o==(void *)1);
    hw.block=1;assert(!oa_amp_owner_create(&o,&initial,&io));
    memset(&io,0,sizeof io);memset(&initial,0,sizeof initial); /* create owns its copies */
    char power[32]="6Watt";
    oa_ad82587_profile p={.speaker_power=power,.prescale=17,.state_control4=0x42,
        .limiter_attack=-1,.release_threshold=-1,.power_clip=0x123456};
    uint64_t ticket=777;char long_name[32];memset(long_name,'x',sizeof long_name);
    p.speaker_power=long_name;assert(oa_amp_owner_submit(o,&p,&ticket)==-ENAMETOOLONG && ticket==777);
    p.speaker_power=power;assert(!oa_amp_owner_submit(o,&p,&ticket) && ticket==1);
    wait_for(&hw.entered,1);
    p.prescale=999;p.speaker_power=NULL;memset(power,'x',sizeof power);
    oa_amp_completion result,saved;memset(&result,0xa5,sizeof result);saved=result;
    assert(oa_amp_owner_take(o,&result)==0 && !memcmp(&result,&saved,sizeof result));
    p.speaker_power="12Watt";uint64_t rejected=777;
    assert(oa_amp_owner_submit(o,&p,&rejected)==-EBUSY && rejected==777);
    assert(oa_amp_owner_join(o)==-EBUSY && oa_amp_owner_destroy(o)==-EBUSY);
    release();wait_for(&hw.idle_jobs,1);
    assert(oa_amp_owner_submit(o,&p,&rejected)==-EBUSY && rejected==777); /* untaken result */
    assert(oa_amp_owner_take(o,&result)==1 && result.ticket==1 && result.native_result==1);
    assert(result.state.image[0]==0xa0 && result.state.image[3]==17 && result.state.image[7]==0x42);
    assert(result.state.image[0x13]==8 && result.state.reset_cached==7 && result.state.mute_cached==9);
    saved=result;assert(oa_amp_owner_take(o,&result)==0 && !memcmp(&result,&saved,sizeof result));
    puts("ok amp owner: worker-only callbacks, copied table/state/profile/name, pending/result backpressure and value completion");

    result.state.image[7]=0xff;hw.block=0;hw.fail_write=3;
    p.prescale=31;p.state_control4=-1;
    assert(!oa_amp_owner_submit(o,&p,&ticket) && ticket==2);wait_for(&hw.idle_jobs,2);
    assert(oa_amp_owner_take(o,&result)==1 && !result.native_result && result.ticket==2);
    assert(result.state.image[7]==0x72 && result.state.image[3]==31 && result.state.image[0x13]==0x12);
    assert(saved.state.image[7]==0x42 && saved.state.image[3]==17);
    puts("ok amp owner: failed writes retain worker state across requests and caller copies cannot mutate it");

    hw.block=1;hw.released=0;hw.fail_write=38;hw.use_tls=1;p.prescale=47;
    assert(!oa_amp_owner_submit(o,&p,&ticket) && ticket==3);wait_for(&hw.entered,3);
    assert(oa_amp_owner_quiesce(o)==-EBUSY && oa_amp_owner_destroy(o)==-EBUSY);
    assert(oa_amp_owner_submit(o,&p,&rejected)==-ESHUTDOWN && rejected==777);
    pid_t child=fork();assert(child>=0);
    if(!child) {
        int good=oa_amp_owner_submit(o,&p,&rejected)==-ECHILD && oa_amp_owner_take(o,&result)==-ECHILD &&
            oa_amp_owner_quiesce(o)==-ECHILD && oa_amp_owner_join(o)==-ECHILD && oa_amp_owner_destroy(o)==-ECHILD;
        _exit(good?0:1);
    }
    int status;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    release();wait_for(&hw.destructor_entered,1);
    assert(oa_amp_owner_quiesce(o)==-EBUSY && oa_amp_owner_join(o)==-EBUSY && oa_amp_owner_destroy(o)==-EBUSY);
    hw.join_error=EINVAL;assert(oa_amp_owner_join(o)==-EINVAL && oa_amp_owner_destroy(o)==-EBUSY);
    pthread_mutex_lock(&hw.mu);hw.destructor_released=1;pthread_cond_broadcast(&hw.cv);pthread_mutex_unlock(&hw.mu);
    finish(o);assert(!oa_amp_owner_join(o));
    assert(oa_amp_owner_take(o,&result)==1 && result.ticket==3 && result.native_result==1);
    assert(result.state.image[7]==0x72 && result.state.image[3]==47);
    assert(!oa_amp_owner_destroy(o));
    puts("ok amp owner: quiesce drains, inherited handles reject, TLS destructors and join errors retain owner until actual join");

    hw.use_tls=0;unsigned calls=hw.calls;
    oa_amp_init_io fresh={&hw,prelude,gpio,direction,level,delay,write_reg,sub_get,sub_enable,sub_init};
    assert(!oa_amp_owner_create(&o,&result.state,&fresh));finish(o);assert(!oa_amp_owner_destroy(o));
    assert(hw.calls==calls);assert(!pthread_key_delete(hw.key));
    puts("ok amp owner: idle shutdown executes no I/O and frees only after join; native ownership/shutdown remain external");
    return 0;
}
