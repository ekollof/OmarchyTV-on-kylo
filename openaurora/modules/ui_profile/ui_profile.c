/* Immutable profile facts only; no telemetry, settings writes or hardware. */
#include "openaurora/module.h"
#include "openaurora/services.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
struct ctx {
    const oa_host_api *api;
    struct oa_tab_profile *profile;
    int published;
    /* ABI 3 quiesce: the copied profile is detached and freed on a worker. */
    pthread_t cleanup_thread;
    int cleanup_running;
    struct oa_tab_profile *cleanup_profile;
    unsigned cleanup_delay_ms;
};
static struct ctx *current;
static int copy(struct oa_tab_profile *out)
{
    if(!out)return EINVAL;
    if(!current || !current->published || !current->profile)return ENODEV;
    *out=*current->profile;return 0;
}
static const oa_ui_profile_iface iface={copy};
static int load(const char *path,struct oa_tab_profile *out)
{
    if(!path || !*path)return EINVAL;
    int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);if(fd<0)return errno;
    struct stat st;int rc=0;unsigned char *data=NULL;size_t n=0,at=0;
    if(fstat(fd,&st)){rc=errno;goto done;}
    /* Project allocation bound, not a claim about the vendor format limit. */
    if(!S_ISREG(st.st_mode) || st.st_size<=0 || (uintmax_t)st.st_size>16u*1024u*1024u) {
        rc=EINVAL;goto done;
    }
    n=(size_t)st.st_size;data=malloc(n+1);if(!data){rc=ENOMEM;goto done;}
    while(at<n+1) {
        ssize_t got=read(fd,data+at,n+1-at);
        if(got<0){if(errno==EINTR)continue;rc=errno;goto done;}
        if(!got)break;
        at+=(size_t)got;
    }
    if(at!=n)rc=EINVAL;
done:
    if(close(fd) && !rc)rc=errno;
    if(!rc)rc=oa_tab_profile_from_sw(data,n,out);
    free(data);return rc;
}
static int m_init(const oa_host_api *api,void **out)
{
    if(!api || api->abi!=OA_ABI_VERSION || api->size<sizeof *api)return -1;
    struct ctx *c=calloc(1,sizeof *c);if(!c)return -1;
    c->api=api;current=c;*out=c;return 0;
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
    struct oa_tab_profile *profile=c->cleanup_profile;
    c->cleanup_profile=NULL;
    free(profile);
    return NULL;
}

static int m_start(void *v)
{
    struct ctx *c=v;
    if(c->published || c->cleanup_running) return -EBUSY;
    c->cleanup_running=0;
    long delay=atol(c->api->config_get(c->api->priv,
        "ui_profile.cleanup_delay_ms","0"));
    c->cleanup_delay_ms=delay>0?(unsigned)delay:0;
    struct oa_tab_profile profile;
    const char *path=c->api->config_get(c->api->priv,"ui_profile.swprofile",NULL);
    int rc=load(path,&profile);
    if(rc) {
        c->api->log(c->api->priv,OA_LOG_ERROR,"ui_profile: invalid/unreadable software profile (%d)",rc);
        return -1;
    }
    if(!c->profile) {
        c->profile=malloc(sizeof *c->profile);
        if(!c->profile)return -1;
    }
    *c->profile=profile;
    rc=c->api->publish(c->api->priv,OA_SVC_UI_PROFILE,1,&iface);
    if(rc)return rc;
    c->published=1;return 0;
}
/* ABI 3 gate: unpublish ui.profile synchronously, detach the profile copy,
 * then free it synchronously or on a worker after
 * ui_profile.cleanup_delay_ms. */
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
    if(published)c->api->unpublish(c->api->priv,OA_SVC_UI_PROFILE);
    struct oa_tab_profile *profile=c->profile;
    c->profile=NULL;
    if(!profile)return 0;
    if(!c->cleanup_delay_ms){free(profile);return 0;}
    c->cleanup_profile=profile;
    int rc=pthread_create(&c->cleanup_thread,NULL,cleanup_worker,c);
    if(rc){c->cleanup_profile=NULL;free(profile);return -EIO;}
    c->cleanup_running=1;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c=v;
    int published=c->published;c->published=0;
    if(published)c->api->unpublish(c->api->priv,OA_SVC_UI_PROFILE);
    if(c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread,NULL);
        c->cleanup_running=0;
    }
    free(c->cleanup_profile);c->cleanup_profile=NULL;
    free(c->profile);c->profile=NULL;
}

static void m_destroy(void *v)
{
    struct ctx *c=v;
    if(c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread,NULL);
        c->cleanup_running=0;
    }
    free(c->cleanup_profile);
    free(c->profile);
    if(current==c)current=NULL;
    free(c);
}

static int m_suspend(void *v) { (void)v;return 0; }
static int m_resume(void *v) { (void)v;return 0; }
static const oa_module module={
    .magic=OA_MODULE_MAGIC,.abi=OA_ABI_VERSION,.size=sizeof(oa_module),
    .name="ui_profile",.version="0.1",.init=m_init,.start=m_start,.stop=m_stop,
    .destroy=m_destroy,.suspend=m_suspend,.resume=m_resume,.quiesce=m_quiesce,
};
OA_MODULE_EXPORT(module)
