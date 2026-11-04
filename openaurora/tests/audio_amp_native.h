/* Included by the offline ARM harness. Executed Init callees are controlled
 * memory fixtures, with GPIO descriptions absent. No HAL, GPIO, MCU, I2C,
 * sleeping or device access occurs; the present-GPIO branches are not covered. */
#include "openaurora/audio_amp.h"
typedef uint32_t (*amp_native_fn)(uint32_t,uint32_t,uint32_t,uint32_t);
static oa_ad82587_profile amp_profile;
static unsigned amp_drc_count,amp_writes,amp_failure,amp_subcalls;
static uint8_t amp_written[OA_AD82587_REGISTERS];
static uint8_t *amp_image;
static uint32_t amp_bits(amp_native_fn fn)
{ uint32_t value;_Static_assert(sizeof fn==4,"ARM function address");memcpy(&value,&fn,4);return value; }
#define AMP_FN(name) static uint32_t name(uint32_t a,uint32_t b,uint32_t c,uint32_t d) { (void)a;(void)b;(void)c;(void)d;
AMP_FN(amp_noop) return 0; }
AMP_FN(amp_power) assert(strlen(amp_profile.speaker_power)<32);strcpy((char *)(uintptr_t)a,amp_profile.speaker_power);return 0; }
AMP_FN(amp_prescale) return amp_profile.prescale; }
AMP_FN(amp_ch1) return amp_profile.channel1; }
AMP_FN(amp_ch2) return amp_profile.channel2; }
AMP_FN(amp_fine) return amp_profile.fine_tune; }
AMP_FN(amp_drc)
    static const char *const keys[]={"AMPPowerClip","SpeakerStateControl4","DRCLimiterAttackRate","SpeakerDRC","DRCReleaseThreshold"};
    const uint32_t values[]={amp_profile.power_clip,(uint32_t)amp_profile.state_control4,
        (uint32_t)amp_profile.limiter_attack,amp_profile.speaker_drc,(uint32_t)amp_profile.release_threshold};
    assert(amp_drc_count<5 && !strcmp((const char *)(uintptr_t)a,keys[amp_drc_count]));
    return values[amp_drc_count++];
}
AMP_FN(amp_write)
    assert(a==0x1234 && b==amp_writes && b<OA_AD82587_REGISTERS && c==1);
    assert((const uint8_t *)(uintptr_t)d==amp_image+b); /* preserve the fourth argument */
    amp_written[amp_writes++]=*(const uint8_t *)(uintptr_t)d;
    return b!=amp_failure;
}
AMP_FN(amp_sub) ++amp_subcalls;return 0; }
#undef AMP_FN

static void amp_stub(void *mapped,uint32_t base,uint32_t address,amp_native_fn fn)
{
    /* Same four-argument preserving trampoline as quad_stub. Using the generic
     * r3-clobbering stub would corrupt _DEV_AMP_WriteIIC's data pointer. */
    unsigned char *p=stub_destination(mapped,base,address,16);
    const uint16_t insn[]={0xb40f,0x4b02,0x469c,0xbc0f,0x4760,0x46c0};
    assert(!(address&3));memcpy(p,insn,sizeof insn);
    uint32_t target=amp_bits(fn);memcpy(p+12,&target,4);
}

static void check_audio_amp(const unsigned char *elf,size_t bytes)
{
    const uint32_t bases[]={0x874000,0x84f000,0x850000,0x91b000,0xc5e000,
        0xc6e000,0x6b4000,0x1356000,0x1369000,0x1764000,0x1791000,0x1978000};
    const size_t sizes[]={12288,4096,4096,4096,8192,4096,4096,4096,4096,4096,4096,4096};
    unsigned char *regions[12];
    for(unsigned i=0;i<12;++i)regions[i]=page(bases[i],sizes[i]);
    size_t off=file_offset(elf,bytes,0x874ef0,598);
    assert(crc32(elf+off,598)==UINT32_C(0x1fba7b2d));
    assert(mapped_range(regions[0],bases[0],0x874ef0,598));
    memcpy(regions[0]+0xef0,elf+off,598);
    const uint32_t string_crc[]={UINT32_C(0xf409ae5b),UINT32_C(0x4ac40d4b)};
    for(unsigned i=7;i<=8;++i) {
        off=file_offset(elf,bytes,bases[i],4096);
        assert(crc32(elf+off,4096)==string_crc[i-7]);
        memcpy(regions[i],elf+off,4096);
    }
    memcpy(regions[10]+0xd68,&guard,4);
    amp_image=regions[9]+0xe8;
    off=file_offset(elf,bytes,0x17640e8,OA_AD82587_REGISTERS);
    assert(crc32(elf+off,OA_AD82587_REGISTERS)==UINT32_C(0xb942ff40));
    uint8_t shipped[OA_AD82587_REGISTERS];memcpy(shipped,elf+off,sizeof shipped);
    /* Executed call-site inventory for the absent-GPIO fixture. */
    amp_stub(regions[2],bases[2],0x85024c,amp_noop);
    amp_stub(regions[1],bases[1],0x84f668,amp_noop); /* absent GPIO resources */
    amp_stub(regions[0],bases[0],0x874e50,amp_noop);
    amp_stub(regions[0],bases[0],0x874dac,amp_noop);
    amp_stub(regions[3],bases[3],0x91bee4,amp_noop);
    amp_stub(regions[4],bases[4],0xc5e858,amp_power);
    amp_stub(regions[4],bases[4],0xc5f090,amp_prescale);
    amp_stub(regions[4],bases[4],0xc5f1f8,amp_ch1);
    amp_stub(regions[4],bases[4],0xc5f2ac,amp_ch2);
    amp_stub(regions[4],bases[4],0xc5f144,amp_fine);
    amp_stub(regions[4],bases[4],0xc5f6f0,amp_drc);
    amp_stub(regions[1],bases[1],0x84fe58,amp_write);
    amp_stub(regions[5],bases[5],0xc6ecb4,amp_noop);
    amp_stub(regions[0],bases[0],0x875dd8,amp_sub);
    amp_stub(regions[0],bases[0],0x8761b4,amp_sub);
    /* strcmp is BLX to an even ARM PLT slot, so install an ARM trampoline. */
    int (*compare)(const char *,const char *)=strcmp;uint32_t compare_address;
    memcpy(&compare_address,&compare,4);
    const uint32_t arm[]={UINT32_C(0xe59fc000),UINT32_C(0xe12fff1c),compare_address};
    assert(mapped_range(regions[6],bases[6],0x6b4c48,sizeof arm));
    memcpy(regions[6]+0xc48,arm,sizeof arm);
    for(unsigned i=0;i<=6;++i)executable(regions[i],sizes[i]);
    amp_native_fn reference;uint32_t entry=0x874ef1;memcpy(&reference,&entry,4);
    const char *const powers[]={"6Watt","8Watt_12V","12Watt","6watt"};
    const uint32_t drcs[]={0,499,500,UINT32_C(0x123456),UINT32_MAX};
    const int32_t overrides[]={-1,-2,0,0x123456,INT32_MIN};
    const int32_t attacks[]={-1,-2,0x123};
    unsigned compared=0;
    for(unsigned seed=0;seed<4;++seed)for(unsigned power=0;power<4;++power)
    for(unsigned drc=0;drc<5;++drc)for(unsigned release=0;release<5;++release)
    for(unsigned state=0;state<5;++state)for(unsigned attack=0;attack<3;++attack) {
        uint8_t original[OA_AD82587_REGISTERS],ours[OA_AD82587_REGISTERS];
        for(unsigned i=0;i<sizeof original;++i)original[i]=seed==0?shipped[i]:
            seed==1?0:seed==2?255:(uint8_t)(0x80+i);
        amp_profile=(oa_ad82587_profile){powers[power],compared,UINT32_MAX-compared,
            compared<<8,compared*257,UINT32_C(0xab123456),drcs[drc],
            overrides[state],attacks[attack],overrides[release]};
        assert(!oa_ad82587_configure(ours,original,&amp_profile));
        memcpy(amp_image,original,sizeof original);amp_image[-1]=0xa5;amp_image[sizeof original]=0x5a;
        amp_drc_count=amp_writes=amp_subcalls=0;amp_failure=38;
        assert(reference(0x1234,0,0,0)==1);
        assert(amp_drc_count==5 && amp_writes==38 && amp_subcalls==2);
        assert(amp_image[-1]==0xa5 && amp_image[sizeof original]==0x5a);
        if(memcmp(ours,amp_image,sizeof ours) || memcmp(ours,amp_written,sizeof ours)) {
            fprintf(stderr,"amp image mismatch case=%u seed=%u power=%s drc=%x release=%x state=%x attack=%x\n",
                compared,seed,powers[power],drcs[drc],(unsigned)overrides[release],
                (unsigned)overrides[state],(unsigned)attacks[attack]);exit(1);
        }
        ++compared;
    }
    /* The native image is fully patched even when a later write fails. Check
     * every failure position and then reuse the retained image as next input. */
    for(unsigned failure=0;failure<38;++failure) {
        uint8_t ours[OA_AD82587_REGISTERS];
        amp_profile.state_control4=failure&1?-1:0x42;
        assert(!oa_ad82587_configure(ours,amp_image,&amp_profile));
        amp_drc_count=amp_writes=amp_subcalls=0;amp_failure=failure;
        assert(reference(0x1234,0,0,0)==0);
        assert(amp_writes==failure+1 && amp_subcalls==0 && amp_drc_count==5);
        assert(!memcmp(ours,amp_image,sizeof ours));
        assert(!memcmp(ours,amp_written,amp_writes));
        assert(amp_image[-1]==0xa5 && amp_image[sizeof ours]==0x5a);
    }
    for(unsigned i=0;i<12;++i)assert(!munmap(regions[i],sizes[i]));
    printf("ok offline shipped ARM amp image: %u native/C profiles, 38 write-failure/retained-image cases; hardware callees controlled\n",compared);
}
