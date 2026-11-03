/* Immutable profile facts only; no telemetry, settings writes or hardware. */
#include "openaurora/module.h"
#include "openaurora/services.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
struct ctx { const oa_host_api *api;struct oa_tab_profile profile;int published; };
static struct ctx *current;
static int copy(struct oa_tab_profile *out)
{
    if(!out)return EINVAL;
    if(!current || !current->published)return ENODEV;
    *out=current->profile;return 0;
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
static int m_start(void *v)
{
    struct ctx *c=v;struct oa_tab_profile profile;
    const char *path=c->api->config_get(c->api->priv,"ui_profile.swprofile",NULL);
    int rc=load(path,&profile);
    if(rc) {
        c->api->log(c->api->priv,OA_LOG_ERROR,"ui_profile: invalid/unreadable software profile (%d)",rc);
        return -1;
    }
    c->profile=profile;
    rc=c->api->publish(c->api->priv,OA_SVC_UI_PROFILE,1,&iface);
    if(rc)return rc;
    c->published=1;return 0;
}
static void m_stop(void *v)
{
    struct ctx *c=v;int published=c->published;c->published=0;
    if(published)c->api->unpublish(c->api->priv,OA_SVC_UI_PROFILE);
}
static void m_destroy(void *v)
{ struct ctx *c=v;if(current==c)current=NULL;free(c); }
static int m_suspend(void *v) { (void)v;return 0; }
static int m_resume(void *v) { (void)v;return 0; }
static const oa_module module={
    .magic=OA_MODULE_MAGIC,.abi=OA_ABI_VERSION,.size=sizeof(oa_module),
    .name="ui_profile",.version="0.1",.init=m_init,.start=m_start,.stop=m_stop,
    .destroy=m_destroy,.suspend=m_suspend,.resume=m_resume,
};
OA_MODULE_EXPORT(module)
