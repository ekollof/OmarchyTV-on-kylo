/* Reloadable, data-only amplifier inputs. Firmware rules live in the shared
 * decoder. No vendor library, GPIO, MCU, I2C, device ownership or audio writes. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    oa_ad82587_profile profile;
    char power[32];
    int published;
};
static struct ctx *current;
static int profile_copy(oa_ad82587_profile *out,char power[32])
{
    struct ctx *c=current;
    if(!out || !power)return -EINVAL;
    if(!c || !c->published)return -ENODEV;
    oa_ad82587_profile copy=c->profile;
    memcpy(power,c->power,sizeof c->power);copy.speaker_power=power;*out=copy;
    return 0;
}
static const oa_audio_amp_profile_iface iface={profile_copy};
static int read_profile(struct ctx *c,oa_ad82587_profile *profile,char power[32])
{
    const char *path=c->api->config_get(c->api->priv,"amp_profile.hwprofile",NULL);
    if(!path || !*path)return -EINVAL;
    int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    if(fd<0)return -errno;
    struct stat st;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<0x44 || st.st_size>16*1024*1024) {
        close(fd);return -EINVAL;
    }
    size_t size=(size_t)st.st_size;
    unsigned char *data=malloc(size+1);
    if(!data){close(fd);return -ENOMEM;}
    FILE *f=fdopen(fd,"rb");
    if(!f){free(data);close(fd);return -EIO;}
    size_t got=fread(data,1,size+1,f);
    int failed=ferror(f);if(fclose(f))failed=1;
    int rc=failed || got!=size?-EIO:-oa_ad82587_profile_from_hw(data,size,profile,power);
    free(data);return rc;
}
static int m_init(const oa_host_api *api,void **out)
{
    if(!api || api->abi!=OA_ABI_VERSION || api->size<sizeof *api)return -EINVAL;
    struct ctx *c=calloc(1,sizeof *c);if(!c)return -ENOMEM;
    c->api=api;*out=c;current=c;return 0;
}
static int m_start(void *v)
{
    struct ctx *c=v;oa_ad82587_profile p;char power[32];
    int rc=read_profile(c,&p,power);
    if(rc)return rc;
    memcpy(c->power,power,sizeof power);c->profile=p;c->profile.speaker_power=c->power;
    rc=c->api->publish(c->api->priv,OA_SVC_AUDIO_AMP_PROFILE,1,&iface);
    c->published=rc==0;return rc;
}
static void m_stop(void *v)
{
    struct ctx *c=v;int published=c->published;c->published=0;
    if(published)c->api->unpublish(c->api->priv,OA_SVC_AUDIO_AMP_PROFILE);
}
static void m_destroy(void *v)
{ if(current==v)current=NULL;free(v); }
static int m_suspend(void *v) { (void)v;return 0; }
static int m_resume(void *v) { (void)v;return 0; }
static const oa_module module={
    .magic=OA_MODULE_MAGIC,.abi=OA_ABI_VERSION,.size=sizeof(oa_module),
    .name="audio_amp_profile",.version="1",.init=m_init,.start=m_start,
    .stop=m_stop,.destroy=m_destroy,.suspend=m_suspend,.resume=m_resume
};
OA_MODULE_EXPORT(module)
