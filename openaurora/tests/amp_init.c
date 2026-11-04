#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/amp_init.h"
struct fixture {
    unsigned calls,writes,levels,delays,subcalls,gets;
    unsigned fail_write;int gpio_success,present;uint32_t sub;
    oa_amp_gpio reset,mute;
};
static void prelude(void *v) { ++((struct fixture *)v)->calls; }
static const oa_amp_gpio *gpio(void *v,unsigned fn)
{
    struct fixture *f=v;++f->gets;assert(fn<2);
    return f->present?(fn?&f->mute:&f->reset):NULL;
}
static void direction(void *v,uint32_t pin,uint32_t value)
{ (void)v;assert((pin==114 || pin==208) && value==1); }
static int level(void *v,uint32_t pin,uint32_t value)
{
    struct fixture *f=v;assert(pin==114 || pin==208);assert(value<=1);++f->levels;return f->gpio_success;
}
static void delay(void *v,unsigned ms) { ((struct fixture *)v)->delays+=ms; }
static int write_reg(void *v,unsigned reg,const uint8_t *value)
{ struct fixture *f=v;assert(reg==f->writes && value);++f->writes;return reg!=f->fail_write; }
static uint32_t sub_get(void *v) { return ((struct fixture *)v)->sub; }
static void sub_enable(void *v,int enabled)
{ struct fixture *f=v;assert(enabled==(f->sub==1));++f->subcalls; }
static void sub_init(void *v) { ++((struct fixture *)v)->subcalls; }
int main(void)
{
    oa_ad82587_profile p={.speaker_power="12Watt",.prescale=27,.channel1=5,.channel2=5,
        .state_control4=-1,.limiter_attack=-1,.release_threshold=-1};
    struct fixture f={.fail_write=38,.gpio_success=7,.present=1,.sub=1,
        .reset={114,1,0},.mute={208,1,0}};
    oa_amp_init_io io={&f,prelude,gpio,direction,level,delay,write_reg,sub_get,sub_enable,sub_init};
    oa_amp_init_state s={.reset_cached=7,.mute_cached=9};
    assert(oa_amp_init(&s,&p,&io)==1);
    assert(f.calls==1 && f.gets==3 && f.levels==7 && f.writes==38 && f.delays==1055 && f.subcalls==2);
    assert(s.reset_cached==0 && s.mute_cached==1 && s.image[3]==27 && s.image[4]==5);
    puts("ok amp init: present GPIO sequence, millisecond delays, image/write/subwoofer and cached flags");
    for(unsigned failure=0;failure<38;++failure) {
        f=(struct fixture){.fail_write=failure,.gpio_success=0,.present=1,.sub=2,
            .reset={114,1,0},.mute={208,1,0}};
        memset(&s,0,sizeof s);s.reset_cached=7;s.mute_cached=9;
        assert(oa_amp_init(&s,&p,&io)==0 && f.writes==failure+1 && !f.subcalls);
        assert(s.reset_cached==7 && s.mute_cached==9 && s.image[3]==27 && f.delays==1055);
    }
    f=(struct fixture){.fail_write=38,.present=0,.sub=UINT32_MAX};
    assert(oa_amp_init(&s,&p,&io)==1 && !f.levels && f.delays==1025 && f.subcalls==2);
    assert(s.reset_cached==7 && s.mute_cached==9);
    puts("ok amp init: all write failures retain image/cache, ignored GPIO failure and absent-resource branches");
    oa_amp_init_state saved=s;unsigned calls=f.calls;
    assert(oa_amp_init(NULL,&p,&io)==-EINVAL && oa_amp_init(&s,NULL,&io)==-EINVAL);
    assert(oa_amp_init(&s,&p,NULL)==-EINVAL);
    io.write=NULL;assert(oa_amp_init(&s,&p,&io)==-EINVAL);
    assert(f.calls==calls && !memcmp(&s,&saved,sizeof s));
    puts("ok amp init: invalid arguments perform no callbacks or state mutation; injection only");
    return 0;
}
