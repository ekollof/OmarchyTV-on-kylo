#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/audio_amp.h"
#include "openaurora/profile.h"

int oa_ad82587_profile_from_hw(const void *data,size_t size,
    oa_ad82587_profile *out,char power[32])
{
    if(!out || !power)return EINVAL;
    const char *base="AudioSettings/Prescales";
    const void *raw;size_t length;int present;
    int rc=oa_profile_data(data,size,base,"DefaultSpeaker",&raw,&length,&present);
    if(rc)return rc;
    char name[32]={0};
    if(present && length<=30) {
        const char *end=length?memchr(raw,0,length):NULL;
        if(length && !end)return EBADMSG;
        if(end)memcpy(name,raw,(size_t)(end-(const char *)raw));
    } else {
        uint32_t index;
        rc=oa_profile_u32(data,size,base,"DefaultSpeaker",255,&index,&present);
        if(rc)return rc;
        static const char *const legacy[]={"8Watt","6Watt","2.5Watt","12Watt","SoundBox4R12Watt"};
        if(index!=255) {
            if(index>=sizeof legacy/sizeof legacy[0])return ERANGE;
            memcpy(name,legacy[index],strlen(legacy[index])+1);
        }
    }
    char section[96],drc[100];
    int n=snprintf(section,sizeof section,"%s/%s",base,name[0]?name:"8Watt");
    if(n<0 || (size_t)n>=sizeof section)return EOVERFLOW;
    n=snprintf(drc,sizeof drc,"%s/DRC",section);
    if(n<0 || (size_t)n>=sizeof drc)return EOVERFLOW;
    oa_ad82587_profile value={0};
    uint32_t *const scalars[]={&value.prescale,&value.channel1,&value.channel2,&value.fine_tune};
    static const char *const keys[]={"Amp_Prescale","Amp_Ch1_Volume","Amp_Ch2_Volume","AmpVolumeFineTune"};
    for(unsigned i=0;i<4;++i) {
        rc=oa_profile_u32(data,size,section,keys[i],0xc0,scalars[i],&present);
        if(rc)return rc;
    }
    uint32_t words[5];
    static const char *const drc_keys[]={"AMPPowerClip","SpeakerDRC","SpeakerStateControl4","DRCLimiterAttackRate","DRCReleaseThreshold"};
    for(unsigned i=0;i<5;++i) {
        rc=oa_profile_u32(data,size,drc,drc_keys[i],UINT32_MAX,&words[i],&present);
        if(rc)return rc;
    }
    value.power_clip=words[0];value.speaker_drc=words[1];
    /* Preserve raw32-bit signed getter results without implementation-defined
     * unsigned-to-signed narrowing for values above INT32_MAX. */
    memcpy(&value.state_control4,&words[2],4);
    memcpy(&value.limiter_attack,&words[3],4);
    memcpy(&value.release_threshold,&words[4],4);
    memcpy(power,name,sizeof name);value.speaker_power=power;*out=value;
    return 0;
}
