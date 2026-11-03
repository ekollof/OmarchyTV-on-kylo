#include "openaurora/profile.h"
#include "openaurora/tab_policy.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void put(unsigned char *p,uint32_t n)
{ for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(8*i)); }
static void entry(unsigned char *b,size_t p,const char *name,unsigned kind,uint32_t count,uint32_t value)
{ put(b+p,oa_profile_hash(name));b[p+4]=(unsigned char)kind;put(b+p+8,count);put(b+p+12,value); }
static void fixture(unsigned char *b)
{
    memset(b,0,208);memcpy(b,"EFSL",4);put(b+0x38,7);
    entry(b,0x44,"Config",1,6,0);entry(b,0x58,"Connectivity",1,2,0);
    entry(b,0x6c,"Connectivity",5,0,1);entry(b,0x80,"Connectivity",5,0,2);
    entry(b,0x94,"HotelTv",1,2,0);entry(b,0xa8,"HotelMode",5,0,0);
    entry(b,0xbc,"Other",5,0,UINT32_MAX);
}
static void corpus(const char *path)
{
    FILE *f=fopen(path,"rb");assert(f && !fseek(f,0,SEEK_END));long length=ftell(f);
    assert(length>0 && length<16*1024*1024 && !fseek(f,0,SEEK_SET));
    unsigned char *b=malloc((size_t)length);assert(b && fread(b,1,(size_t)length,f)==(size_t)length && !fclose(f));
    struct oa_tab_profile p;assert(!oa_tab_profile_from_sw(b,(size_t)length,&p));
    printf("profile corpus: connectivity=%u/%u present=%u hotel=%u/%u present=%u bytes=%ld\n",
           p.connectivity_raw,p.connectivity_enabled,p.connectivity_present,
           p.hotel_raw,p.hotel_enabled,p.hotel_present,length);
    free(b);
}
int main(int argc,char **argv)
{
    assert(argc==1 || argc==2);if(argc==2)corpus(argv[1]);
    unsigned char b[208];fixture(b);uint32_t value;int present;
    assert(!oa_profile_u32(b,sizeof b,"//Config//Connectivity/","Connectivity",17,&value,&present) && value==1 && present);
    assert(!oa_profile_u32(b,sizeof b,"Config","Connectivity",17,&value,&present) && value==17 && !present);
    assert(!oa_profile_u32(b,sizeof b,"Config/Missing","Connectivity",18,&value,&present) && value==18 && !present);
    assert(!oa_profile_u32(b,sizeof b,"Config/HotelTv","Other",0,&value,&present) && value==UINT32_MAX && present);
    assert(!oa_profile_u32(b,sizeof b,"","Missing",19,&value,&present) && value==19 && !present);
    struct oa_tab_profile profile;assert(!oa_tab_profile_from_sw(b,sizeof b,&profile));
    assert(profile.connectivity_raw==1 && profile.connectivity_enabled && profile.connectivity_present);
    assert(profile.hotel_raw==0 && !profile.hotel_enabled && profile.hotel_present);
    put(b+0x6c+12,2);put(b+0xa8+12,UINT32_MAX);
    assert(!oa_tab_profile_from_sw(b,sizeof b,&profile) && !profile.connectivity_enabled && !profile.hotel_enabled);
    b[0x6c+4]=4;b[0xa8+4]=4;
    assert(!oa_tab_profile_from_sw(b,sizeof b,&profile));
    assert(!profile.connectivity_raw && !profile.connectivity_enabled && !profile.connectivity_present);
    assert(profile.hotel_raw==1 && profile.hotel_enabled && !profile.hotel_present);
    /* Earlier wrong-type matching value wins over a later numeric duplicate. */
    b[0x6c+4]=1;put(b+0x6c+8,0);
    assert(!oa_profile_u32(b,sizeof b,"Config/Connectivity","Connectivity",0,&value,&present) && value==2 && present);
    fixture(b);struct oa_tab_profile saved=profile;
    for(size_t n=0;n<sizeof b;++n) {
        assert(oa_tab_profile_from_sw(b,n,&profile)==EBADMSG);
        assert(!memcmp(&profile,&saved,sizeof profile));
    }
    put(b+0x94+8,UINT32_MAX);value=42;present=7;
    assert(oa_profile_u32(b,sizeof b,"Config/HotelTv","HotelMode",1,&value,&present)==EBADMSG && value==42 && present==7);
    assert(oa_tab_profile_from_sw(b,sizeof b,&profile)==EBADMSG && !memcmp(&profile,&saved,sizeof profile));
    fixture(b);put(b+0x44+8,7);
    assert(oa_profile_u32(b,sizeof b,"Config","HotelMode",1,&value,&present)==EBADMSG);
    fixture(b);b[0]='X';assert(oa_tab_profile_from_sw(b,sizeof b,&profile)==EBADMSG);
    assert(oa_profile_u32(NULL,0,"","",0,&value,&present)==EINVAL);
    assert(oa_profile_u32(b,sizeof b,NULL,"",0,&value,&present)==EINVAL);
    assert(oa_profile_u32(b,sizeof b,"",NULL,0,&value,&present)==EINVAL);
    assert(oa_profile_u32(b,sizeof b,"","",0,NULL,&present)==EINVAL);
    assert(oa_profile_u32(b,sizeof b,"","",0,&value,NULL)==EINVAL);
    assert(oa_tab_profile_from_sw(b,sizeof b,NULL)==EINVAL);
    assert(oa_profile_hash("MODE")==UINT32_C(0x5aa5907f));
    puts("ok profile: nested/duplicate/type/default/exact-one semantics, truncated/escaping descendant spans and atomic errors");
    return 0;
}
