#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "openaurora/audio_amp.h"
#include "openaurora/profile.h"
static void word(unsigned char *p,uint32_t value)
{ for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(value>>(8*i)); }
static void record(unsigned char *b,unsigned slot,const char *name,unsigned type,uint32_t count,uint32_t value)
{
    unsigned char *p=b+0x44+slot*20;
    word(p,oa_profile_hash(name));p[4]=(unsigned char)type;word(p+8,count);word(p+12,value);
}
static void fixture(unsigned char b[512])
{
    memset(b,0,512);memcpy(b,"EFSL",4);word(b+0x38,14);
    record(b,0,"AudioSettings",1,13,0);record(b,1,"Prescales",1,12,0);
    record(b,2,"DefaultSpeaker",4,6,0x180);memcpy(b+0x180,"8Watt",6);
    record(b,3,"8Watt",1,10,0);
    record(b,4,"Amp_Prescale",5,0,0x11);record(b,5,"Amp_Ch1_Volume",5,0,0x22);
    record(b,6,"Amp_Ch2_Volume",5,0,0x33);record(b,7,"AmpVolumeFineTune",5,0,0x44);
    record(b,8,"DRC",1,5,0);record(b,9,"AMPPowerClip",5,0,0x123456);
    record(b,10,"SpeakerDRC",5,0,1200);record(b,11,"SpeakerStateControl4",5,0,UINT32_MAX);
    record(b,12,"DRCLimiterAttackRate",5,0,0x6a);record(b,13,"DRCReleaseThreshold",5,0,UINT32_MAX);
}
static void corpus(const char *path)
{
    FILE *f=fopen(path,"rb");assert(f && !fseek(f,0,SEEK_END));long n=ftell(f);
    assert(n>0 && n<16*1024*1024 && !fseek(f,0,SEEK_SET));
    void *bytes=malloc((size_t)n);assert(bytes && fread(bytes,1,(size_t)n,f)==(size_t)n && !fclose(f));
    oa_ad82587_profile p;char name[32];assert(!oa_ad82587_profile_from_hw(bytes,(size_t)n,&p,name));
    printf("amp hwprofile: power='%s' prescale=%x ch1=%x ch2=%x fine=%x clip=%x drc=%x state=%x attack=%x release=%x\n",
        name,p.prescale,p.channel1,p.channel2,p.fine_tune,p.power_clip,p.speaker_drc,
        (unsigned)p.state_control4,(unsigned)p.limiter_attack,(unsigned)p.release_threshold);
    free(bytes);
}
int main(int argc,char **argv)
{
    assert(argc==1 || argc==2);
    if(argc==2)corpus(argv[1]);
    unsigned char b[512];fixture(b);oa_ad82587_profile p;char name[32];
    assert(!oa_ad82587_profile_from_hw(b,sizeof b,&p,name));
    assert(p.speaker_power==name && !strcmp(name,"8Watt"));
    assert(p.prescale==0x11 && p.channel1==0x22 && p.channel2==0x33 && p.fine_tune==0x44);
    assert(p.power_clip==0x123456 && p.speaker_drc==1200 && p.state_control4==-1 &&
        p.limiter_attack==0x6a && p.release_threshold==-1);
    uint8_t image[38]={0};assert(!oa_ad82587_configure(image,image,&p));
    assert(image[3]==0x11 && image[4]==0x22 && image[5]==0x33 && image[0x17]==0x44);
    assert(image[0x10]==0 && image[0x11]==4 && image[0x12]==0xb0 && image[7]==0x30);
    const char *const legacy[]={"8Watt","6Watt","2.5Watt","12Watt","SoundBox4R12Watt"};
    for(unsigned i=0;i<5;++i) {
        record(b,2,"DefaultSpeaker",5,0,i);
        assert(!oa_ad82587_profile_from_hw(b,sizeof b,&p,name) && !strcmp(name,legacy[i]));
        if(i)assert(p.prescale==0xc0 && p.channel1==0xc0 && p.channel2==0xc0 && p.fine_tune==0xc0 &&
            p.power_clip==UINT32_MAX && p.speaker_drc==UINT32_MAX && p.state_control4==-1);
    }
    record(b,2,"DefaultSpeaker",5,0,255);
    assert(!oa_ad82587_profile_from_hw(b,sizeof b,&p,name) && !name[0] && p.prescale==0x11);
    fixture(b);b[0x180]=0;
    assert(!oa_ad82587_profile_from_hw(b,sizeof b,&p,name) && !name[0] && p.prescale==0x11);
    fixture(b);record(b,2,"DefaultSpeaker",4,31,0x180);
    memset(b+0x180,'x',30);b[0x180+30]=0;
    assert(!oa_ad82587_profile_from_hw(b,sizeof b,&p,name) && !name[0] && p.prescale==0x11);
    puts("ok amp profile: string/legacy enum/empty power, exact sections/defaults and register-image join");

    fixture(b);assert(!oa_ad82587_profile_from_hw(b,sizeof b,&p,name));
    oa_ad82587_profile saved=p;char saved_name[32];memcpy(saved_name,name,32);
    for(size_t n=0;n<0x186;++n) {
        assert(oa_ad82587_profile_from_hw(b,n,&p,name)==EBADMSG);
        assert(!memcmp(&saved,&p,sizeof p) && !memcmp(saved_name,name,32));
    }
    const uint32_t invalid_enums[]={5,254,256,UINT32_MAX};
    for(unsigned i=0;i<4;++i) {
        record(b,2,"DefaultSpeaker",5,0,invalid_enums[i]);
        assert(oa_ad82587_profile_from_hw(b,sizeof b,&p,name)==ERANGE);
        assert(!memcmp(&saved,&p,sizeof p) && !memcmp(saved_name,name,32));
    }
    fixture(b);memset(b+0x180,'x',6);
    assert(oa_ad82587_profile_from_hw(b,sizeof b,&p,name)==EBADMSG);
    fixture(b);word(b+0x20,1);
    assert(oa_ad82587_profile_from_hw(b,sizeof b,&p,name)==ENOTSUP);
    assert(!memcmp(&saved,&p,sizeof p) && !memcmp(saved_name,name,32));
    assert(oa_ad82587_profile_from_hw(b,sizeof b,NULL,name)==EINVAL);
    assert(oa_ad82587_profile_from_hw(b,sizeof b,&p,NULL)==EINVAL);
    puts("ok amp profile: bounded payloads, invalid enum/nontermination/external-base rejection and atomic errors");

    fixture(b);const void *raw;size_t length;int present;
    for(unsigned kind=2;kind<=4;++kind) {
        record(b,2,"DefaultSpeaker",kind,6,0x180);
        assert(!oa_profile_data(b,sizeof b,"AudioSettings/Prescales","DefaultSpeaker",&raw,&length,&present));
        assert(present && raw==b+0x180 && length==6 && !memcmp(raw,"8Watt",6));
    }
    record(b,2,"DefaultSpeaker",5,0,1);
    assert(!oa_profile_data(b,sizeof b,"AudioSettings/Prescales","DefaultSpeaker",&raw,&length,&present));
    assert(!present && !raw && !length);
    raw=(const void *)1;length=42;present=7;
    record(b,2,"DefaultSpeaker",4,1,UINT32_MAX);
    assert(oa_profile_data(b,sizeof b,"AudioSettings/Prescales","DefaultSpeaker",&raw,&length,&present)==EBADMSG);
    assert(raw==(const void *)1 && length==42 && present==7);
    record(b,2,"DefaultSpeaker",4,UINT32_MAX,0x180);
    assert(oa_profile_data(b,sizeof b,"AudioSettings/Prescales","DefaultSpeaker",&raw,&length,&present)==EBADMSG);
    assert(raw==(const void *)1 && length==42 && present==7);
    assert(oa_profile_data(NULL,0,"","",&raw,&length,&present)==EINVAL);
    assert(oa_profile_data(b,sizeof b,"","",NULL,&length,&present)==EINVAL);
    assert(oa_profile_data(b,sizeof b,"","",&raw,NULL,&present)==EINVAL);
    assert(oa_profile_data(b,sizeof b,"","",&raw,&length,NULL)==EINVAL);
    memset(b,0,sizeof b);memcpy(b,"EFSL",4);word(b+0x38,2);
    record(b,0,"Blob",5,0,99);record(b,1,"Blob",4,1,0x180);
    assert(!oa_profile_data(b,sizeof b,"","Blob",&raw,&length,&present));
    assert(!raw && !length && !present); /* first wrong-type duplicate wins */
    puts("ok profile data: borrowed type2/3/4 payloads and wrong-type absence");
    return 0;
}
