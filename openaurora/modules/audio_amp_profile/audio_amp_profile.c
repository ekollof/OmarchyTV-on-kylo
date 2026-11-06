/* Reloadable, data-only amplifier inputs. Firmware rules live in the shared
 * decoder. No vendor library, GPIO, MCU, I2C, device ownership or audio writes. */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "openaurora/module.h"
#include "openaurora/services.h"

struct profile_data {
    oa_ad82587_profile profile;
    char power[32];
};
struct ctx {
    const oa_host_api *api;
    struct profile_data *data;
    int published;
    /* ABI 3 quiesce: profile+power are detached and freed on a worker. */
    pthread_t cleanup_thread;
    int cleanup_running;
    struct profile_data *cleanup_data;
    unsigned cleanup_delay_ms;
};
static struct ctx *current;
static int profile_copy(oa_ad82587_profile *out,char power[32])
{
    struct ctx *c=current;
    if(!out || !power)return -EINVAL;
    if(!c || !c->published || !c->data)return -ENODEV;
    oa_ad82587_profile copy=c->data->profile;
    memcpy(power,c->data->power,sizeof c->data->power);copy.speaker_power=power;*out=copy;
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
/* No host API calls after this returns. */
static void *cleanup_worker(void *v)
{
    struct ctx *c=v;
    if(c->cleanup_delay_ms) {
        const struct timespec pause={
            .tv_sec=c->cleanup_delay_ms/1000u,
            .tv_nsec=(long)(c->cleanup_delay_ms%1000u)*1000000L
        };
        (void)nanosleep(&pause,NULL);
    }
    struct profile_data *data=c->cleanup_data;
    c->cleanup_data=NULL;
    free(data);
    return NULL;
}

static int m_start(void *v)
{
    struct ctx *c=v;
    if(c->published || c->cleanup_running) return -EBUSY;
    c->cleanup_running=0;
    long delay=atol(c->api->config_get(c->api->priv,
        "audio_amp_profile.cleanup_delay_ms","0"));
    c->cleanup_delay_ms=delay>0?(unsigned)delay:0;
    oa_ad82587_profile p;char power[32];
    int rc=read_profile(c,&p,power);
    if(rc)return rc;
    if(!c->data) {
        c->data=malloc(sizeof *c->data);
        if(!c->data)return -ENOMEM;
    }
    memcpy(c->data->power,power,sizeof power);
    c->data->profile=p;
    c->data->profile.speaker_power=c->data->power;
    rc=c->api->publish(c->api->priv,OA_SVC_AUDIO_AMP_PROFILE,1,&iface);
    c->published=rc==0;return rc;
}
/* ABI 3 gate: unpublish audio.amp_profile synchronously, detach profile+power,
 * then free them synchronously or on a worker after
 * audio_amp_profile.cleanup_delay_ms. */
static int m_quiesce(void *v)
{
    struct ctx *c=v;
    if(!c)return 0;
    if(c->cleanup_running) {
        for(int i=0;i<10;i++) {
            int rc=pthread_tryjoin_np(c->cleanup_thread,NULL);
            if(rc==0){c->cleanup_running=0;return 0;}
            if(rc!=EBUSY){c->cleanup_running=0;return -EIO;}
            const struct timespec pause={.tv_nsec=5000000};
            (void)nanosleep(&pause,NULL);
        }
        return -EAGAIN;
    }
    int published=c->published;
    c->published=0;
    if(published)c->api->unpublish(c->api->priv,OA_SVC_AUDIO_AMP_PROFILE);
    struct profile_data *data=c->data;
    c->data=NULL;
    if(!data)return 0;
    if(!c->cleanup_delay_ms){free(data);return 0;}
    c->cleanup_data=data;
    int rc=pthread_create(&c->cleanup_thread,NULL,cleanup_worker,c);
    if(rc){c->cleanup_data=NULL;free(data);return -EIO;}
    c->cleanup_running=1;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c=v;
    int published=c->published;c->published=0;
    if(published)c->api->unpublish(c->api->priv,OA_SVC_AUDIO_AMP_PROFILE);
    if(c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread,NULL);
        c->cleanup_running=0;
    }
    free(c->cleanup_data);c->cleanup_data=NULL;
    free(c->data);c->data=NULL;
}

static void m_destroy(void *v)
{
    struct ctx *c=v;
    if(c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread,NULL);
        c->cleanup_running=0;
    }
    free(c->cleanup_data);
    free(c->data);
    if(current==c)current=NULL;
    free(c);
}

static int m_suspend(void *v) { (void)v;return 0; }
static int m_resume(void *v) { (void)v;return 0; }
static const oa_module module={
    .magic=OA_MODULE_MAGIC,.abi=OA_ABI_VERSION,.size=sizeof(oa_module),
    .name="audio_amp_profile",.version="1",.init=m_init,.start=m_start,
    .stop=m_stop,.destroy=m_destroy,.suspend=m_suspend,.resume=m_resume,
    .quiesce=m_quiesce
};
OA_MODULE_EXPORT(module)
