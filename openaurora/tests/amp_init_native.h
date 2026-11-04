/* Full Init/Reset/Mute orchestration versus controlled lower callbacks. */
#include "openaurora/amp_init.h"
enum { SQ_PRELUDE,SQ_GPIO,SQ_DIRECTION,SQ_LEVEL,SQ_DELAY,SQ_WRITE,SQ_SUBGET,SQ_SUBENABLE,SQ_SUBINIT };
struct seq_event { unsigned kind;uint32_t a,b; };
static struct seq_event seq_ours[96],seq_vendor[96],*seq_trace;
static unsigned seq_count;
struct seq_fixture {
    unsigned present,mute_queries,levels,writes,fail_mask,fail_write;
    int level_success;uint32_t sub,direction_result;
    oa_amp_gpio gpios[3];uint32_t native_gpios[3][8];
    const uint8_t *image;
};
static struct seq_fixture *seq_current;
static uint32_t seq_object;
static void seq_emit(unsigned kind,uint32_t a,uint32_t b)
{ assert(seq_count<96);seq_trace[seq_count++]=(struct seq_event){kind,a,b}; }
static void seq_prelude(void *v) { (void)v;seq_emit(SQ_PRELUDE,0,0); }
static const oa_amp_gpio *seq_gpio(void *v,unsigned fn)
{
    struct seq_fixture *f=v;assert(fn<2);seq_emit(SQ_GPIO,fn,0);
    unsigned slot=fn?(++f->mute_queries==1?1:2):0;
    return f->present&(1u<<fn)?f->gpios+slot:NULL;
}
static void seq_direction(void *v,uint32_t pin,uint32_t direction)
{ (void)v;seq_emit(SQ_DIRECTION,pin,direction); }
static int seq_level(void *v,uint32_t pin,uint32_t level)
{
    struct seq_fixture *f=v;seq_emit(SQ_LEVEL,pin,level);assert(f->levels<7);
    return (f->fail_mask&(1u<<f->levels++))?0:f->level_success;
}
static void seq_delay(void *v,unsigned ms) { (void)v;seq_emit(SQ_DELAY,ms,0); }
static int seq_write(void *v,unsigned reg,const uint8_t *value)
{
    struct seq_fixture *f=v;assert(reg==f->writes++ && reg<38 && value==f->image+reg);
    seq_emit(SQ_WRITE,reg,*value);return reg==f->fail_write?0:7;
}
static uint32_t seq_subget(void *v)
{ struct seq_fixture *f=v;seq_emit(SQ_SUBGET,0,0);return f->sub; }
static void seq_subenable(void *v,int enabled) { (void)v;seq_emit(SQ_SUBENABLE,(unsigned)enabled,0); }
static void seq_subinit(void *v) { (void)v;seq_emit(SQ_SUBINIT,0,0); }
#define SEQ_FN(name) static uint32_t name(uint32_t a,uint32_t b,uint32_t c,uint32_t d) { (void)a;(void)b;(void)c;(void)d;
SEQ_FN(seq_n_prelude) seq_prelude(seq_current);return 0; }
SEQ_FN(seq_n_gpio)
    assert(a==seq_object);const oa_amp_gpio *g=seq_gpio(seq_current,b);
    return g?(uint32_t)(uintptr_t)seq_current->native_gpios[g-seq_current->gpios]:0;
}
SEQ_FN(seq_n_direction) seq_direction(seq_current,a,b);return seq_current->direction_result; }
SEQ_FN(seq_n_level) return (uint32_t)seq_level(seq_current,a,b); }
SEQ_FN(seq_n_delay) seq_delay(seq_current,a);return 0; }
SEQ_FN(seq_n_write)
    assert(a==seq_object && c==1);return (uint32_t)seq_write(seq_current,b,(const uint8_t *)(uintptr_t)d);
}
SEQ_FN(seq_n_subget) return seq_subget(seq_current); }
SEQ_FN(seq_n_subenable) seq_subenable(seq_current,(int)a);return 0; }
SEQ_FN(seq_n_subinit) seq_subinit(seq_current);return 0; }
#undef SEQ_FN

static void check_amp_init(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0x874000,0x84f000,0x850000,0x91b000,0xc5e000,
        0xc6e000,0x6b4000,0x1356000,0x1369000,0x1764000,0x1791000,0x1978000,
        0x851000,0x8be000};
    const size_t sizes[]={12288,4096,4096,4096,8192,4096,4096,4096,4096,4096,4096,4096,4096,4096};
    unsigned char *regions[14];for(unsigned i=0;i<14;++i)regions[i]=page(bases[i],sizes[i]);
    const struct {uint32_t address,size,crc;} bodies[]={
        {0x874ef0,598,UINT32_C(0x1fba7b2d)},
        {0x874dac,162,UINT32_C(0xef9e3238)},
        {0x874e50,158,UINT32_C(0x0fc39805)}
    };
    for(unsigned i=0;i<3;++i) {
        size_t off=file_offset(elf,bytes,bodies[i].address,bodies[i].size);
        assert(crc32(elf+off,bodies[i].size)==bodies[i].crc);
        memcpy(stub_destination(regions[0],bases[0],bodies[i].address,bodies[i].size),elf+off,bodies[i].size);
    }
    const uint32_t string_crc[]={UINT32_C(0xf409ae5b),UINT32_C(0x4ac40d4b)};
    for(unsigned i=7;i<=8;++i) {
        size_t off=file_offset(elf,bytes,bases[i],4096);assert(crc32(elf+off,4096)==string_crc[i-7]);
        memcpy(regions[i],elf+off,4096);
    }
    memcpy(regions[10]+0xd68,&guard,4);amp_image=regions[9]+0xe8;
    amp_stub(regions[2],bases[2],0x85024c,seq_n_prelude);
    amp_stub(regions[1],bases[1],0x84f668,seq_n_gpio);
    amp_stub(regions[12],bases[12],0x851978,seq_n_direction);
    amp_stub(regions[12],bases[12],0x8516c4,seq_n_level);
    amp_stub(regions[13],bases[13],0x8be754,amp_noop); /* diagnostics excluded from control trace */
    amp_stub(regions[3],bases[3],0x91bee4,seq_n_delay);
    amp_stub(regions[4],bases[4],0xc5e858,amp_power);
    amp_stub(regions[4],bases[4],0xc5f090,amp_prescale);amp_stub(regions[4],bases[4],0xc5f1f8,amp_ch1);
    amp_stub(regions[4],bases[4],0xc5f2ac,amp_ch2);amp_stub(regions[4],bases[4],0xc5f144,amp_fine);
    amp_stub(regions[4],bases[4],0xc5f6f0,amp_drc);
    amp_stub(regions[1],bases[1],0x84fe58,seq_n_write);
    amp_stub(regions[5],bases[5],0xc6ecb4,seq_n_subget);
    amp_stub(regions[0],bases[0],0x875dd8,seq_n_subenable);amp_stub(regions[0],bases[0],0x8761b4,seq_n_subinit);
    int (*compare)(const char *,const char *)=strcmp;uint32_t compare_address;memcpy(&compare_address,&compare,4);
    const uint32_t arm[]={UINT32_C(0xe59fc000),UINT32_C(0xe12fff1c),compare_address};
    memcpy(stub_destination(regions[6],bases[6],0x6b4c48,sizeof arm),arm,sizeof arm);
    for(unsigned i=0;i<14;++i)if(i<=6 || i>=12)executable(regions[i],sizes[i]);
    amp_native_fn reference;uint32_t entry=0x874ef1;memcpy(&reference,&entry,4);
    const uint32_t levels[]={0,1,7,UINT32_MAX},subs[]={0,1,2,UINT32_MAX};
    const unsigned masks[]={0,1,2,4,8,16,32,64,127,24,96,97};unsigned compared=0;
    for(unsigned present=0;present<4;++present)for(unsigned r=0;r<4;++r)
    for(unsigned m=0;m<4;++m)for(unsigned failure=0;failure<12;++failure)
    for(unsigned write_failure=0;write_failure<=38;++write_failure) {
        struct seq_fixture f={.present=present,.fail_mask=masks[failure],.fail_write=write_failure,
            .level_success=compared&1?-1:7,.sub=subs[compared%4],.direction_result=compared&1?0:7,
            .gpios={{114,99,levels[r]},{208,levels[m],levels[(m+1)%4]},
                    {212,levels[(m+2)%4],levels[(m+3)%4]}}};
        for(unsigned i=0;i<3;++i) {
            f.native_gpios[i][1]=f.gpios[i].pin;f.native_gpios[i][4]=f.gpios[i].level_on;
            f.native_gpios[i][5]=f.gpios[i].level_off;
        }
        oa_amp_init_state ours={.reset_cached=7,.mute_cached=9};
        for(unsigned i=0;i<38;++i)ours.image[i]=(uint8_t)(i*17+compared);
        memcpy(amp_image,ours.image,38);amp_image[-1]=0xa5;amp_image[38]=0x5a;
        amp_profile=(oa_ad82587_profile){.speaker_power="12Watt",.prescale=27,.channel1=5,.channel2=5,
            .fine_tune=128,.power_clip=0x630e8,.speaker_drc=0x18000,
            .state_control4=-1,.limiter_attack=-1,.release_threshold=-1};
        oa_amp_init_io io={&f,seq_prelude,seq_gpio,seq_direction,seq_level,seq_delay,seq_write,
            seq_subget,seq_subenable,seq_subinit};
        seq_trace=seq_ours;seq_count=0;f.image=ours.image;
        int result=oa_amp_init(&ours,&amp_profile,&io);unsigned n=seq_count;
        uint32_t object[54]={0};uint8_t cache[8];memset(cache,0xa5,sizeof cache);cache[4]=7;cache[5]=9;
        object[53]=(uint32_t)(uintptr_t)cache;seq_object=(uint32_t)(uintptr_t)object;
        f.mute_queries=f.levels=f.writes=0;f.image=amp_image;
        seq_current=&f;seq_trace=seq_vendor;seq_count=0;amp_drc_count=0;
        unsigned native=reference(seq_object,0,0,0);
        if(result!=(int)native || n!=seq_count || memcmp(seq_ours,seq_vendor,n*sizeof *seq_ours) ||
            memcmp(ours.image,amp_image,38) || ours.reset_cached!=cache[4] || ours.mute_cached!=cache[5]) {
            fprintf(stderr,"amp sequence mismatch case=%u present=%u reset=%x mute=%u mask=%x write=%u counts=%u/%u\n",
                compared,present,levels[r],m,masks[failure],write_failure,n,seq_count);exit(1);
        }
        assert(amp_drc_count==5 && amp_image[-1]==0xa5 && amp_image[38]==0x5a);
        for(unsigned i=0;i<8;++i)if(i!=4 && i!=5)assert(cache[i]==0xa5);
        ++compared;
    }
    for(unsigned i=0;i<14;++i)assert(!munmap(regions[i],sizes[i]));
    printf("ok offline shipped ARM amp init: %u GPIO/failure cases, exact control traces/image/cache/returns; lower I/O controlled\n",compared);
}
