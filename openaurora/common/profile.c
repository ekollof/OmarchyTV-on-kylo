#include "openaurora/profile.h"
#include <errno.h>
#include <string.h>
static uint32_t word(const unsigned char *p)
{ return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint32_t hash(const char *name,size_t length)
{
    uint32_t h=UINT32_C(0xf1d3b597)^((uint32_t)length<<6),sum=0,rolling=0;
    int32_t previous=0;
    for(size_t i=0;i<length;++i) {
        unsigned byte=(unsigned char)name[i];
        int32_t c=byte<128?(int32_t)byte:(int32_t)byte-256;
        h=(h+2u*~(uint32_t)previous)^(sum<<9);
        sum+=(uint32_t)c;
        h=(h+((uint32_t)c<<17))^rolling;
        rolling=(uint32_t)c+(rolling<<8);
        /* ARM register shifts use low byte; negative signed remainders become
         * >=32 and produce zero, rather than C's undefined negative shift. */
        unsigned shift=(unsigned)(c%3)&255u;
        if(shift<32)h+=h<<shift;
        shift=(unsigned)(previous%5)&255u;
        if(shift<32)h+=h>>shift;
        previous=c;
    }
    return h;
}
uint32_t oa_profile_hash(const char *name)
{ return hash(name,strlen(name)); }
/* Return ENOENT for no match. Each parent count includes all descendants. */
static int find(const unsigned char *b,size_t n,size_t parent,uint32_t id,int section,size_t *out)
{
    if(parent>n || n-parent<20)return EBADMSG;
    size_t count=word(b+parent+8),start=parent+20;
    if(count>(n-start)/20)return EBADMSG;
    size_t end=start+20*count;
    for(size_t p=start;p<end;) {
        int is_section=b[p+4]==1;
        size_t skip=1;
        if(is_section) {
            size_t descendants=word(b+p+8);
            if(descendants>(end-p)/20-1)return EBADMSG;
            skip+=descendants;
        }
        if(is_section==section && word(b+p)==id){*out=p;return 0;}
        p+=20*skip;
    }
    return ENOENT;
}
int oa_profile_u32(const void *data,size_t size,const char *section,const char *key,
                   uint32_t default_value,uint32_t *out,int *present)
{
    if(!data || !section || !key || !out || !present)return EINVAL;
    const unsigned char *b=data;
    if(size<0x44 || memcmp(b,"EFSL",4))return EBADMSG;
    size_t parent=0x30;
    if(word(b+parent+8)>(size-0x44)/20)return EBADMSG;
    const char *p=section;
    int found=0;uint32_t value=default_value;
    while(*p) {
        if(*p=='/'){++p;continue;}
        const char *end=strchr(p,'/');size_t length=end?(size_t)(end-p):strlen(p);
        size_t next;int rc=find(b,size,parent,hash(p,length),1,&next);
        if(rc==ENOENT)goto done;
        if(rc)return rc;
        parent=next;p+=length;
    }
    size_t entry;int rc=find(b,size,parent,oa_profile_hash(key),0,&entry);
    if(rc!=ENOENT && rc)return rc;
    if(!rc && b[entry+4]==5){value=word(b+entry+12);found=1;}
done:
    *out=value;*present=found;return 0;
}
