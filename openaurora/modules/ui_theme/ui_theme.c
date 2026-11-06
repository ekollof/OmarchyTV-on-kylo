/* Data-only immutable BinSVG provider. Start/reload reads bounded configured
 * files; service calls use memory only. No renderer or hardware ownership. */
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/svg_pack.h"
#include <errno.h>
#include <stdlib.h>

struct ctx {
    const oa_host_api *api;
    struct oa_svg_pack *pack;
    int published;
    /* ABI 3 quiesce: the parsed pack is detached and freed by a scheduler
     * task on the host loop, not a dedicated thread. */
    uint32_t cleanup_task;
    struct oa_svg_pack *cleanup_pack;
    unsigned cleanup_delay_ms;
};
static struct ctx *current;
static int record(unsigned id, struct oa_binsvg *out)
{ return current && current->published && current->pack ? oa_svg_pack_record(current->pack,id,out) : ENODEV; }
static int named(const char *name, struct oa_binsvg *out)
{ return current && current->published && current->pack ? oa_svg_pack_named(current->pack,name,out) : ENODEV; }
static unsigned count(void)
{ return current && current->published && current->pack ? oa_svg_pack_count(current->pack) : 0; }
static const oa_ui_theme_iface iface={record,named,count};
static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi!=OA_ABI_VERSION || api->size<sizeof *api) return -1;
    struct ctx *c=calloc(1,sizeof *c);
    if (!c) return -1;
    c->api=api; current=c; *out=c;
    return 0;
}
/* Runs as one nonblocking host scheduler slice; frees the detached pack. */
static int cleanup_step(void *v, uint32_t *delay_ms)
{
    (void)delay_ms;
    struct ctx *c=v;
    struct oa_svg_pack *pack=c->cleanup_pack;
    c->cleanup_pack=NULL;
    c->cleanup_task=0;
    oa_svg_pack_free(pack);
    return OA_SCHED_DONE;
}

/* Cancel a queued free and release any pack the step did not consume. */
static void cancel_cleanup(struct ctx *c)
{
    if(c->cleanup_task) {
        const oa_sched_iface *s=c->api->lookup(c->api->priv,OA_SVC_SCHED,1);
        if(s) {
            (void)s->cancel(s->context,c->cleanup_task);
            c->api->release(c->api->priv,OA_SVC_SCHED);
        }
        c->cleanup_task=0;
    }
    if(c->cleanup_pack){oa_svg_pack_free(c->cleanup_pack);c->cleanup_pack=NULL;}
}

static int m_start(void *v)
{
    struct ctx *c=v;
    if(c->published || c->cleanup_task || c->cleanup_pack || c->pack) return -EBUSY;
    long delay=atol(c->api->config_get(c->api->priv,
        "ui_theme.cleanup_delay_ms","0"));
    c->cleanup_delay_ms=delay>0?(unsigned)delay:0;
    const char *index=c->api->config_get(c->api->priv,"ui_theme.index",NULL);
    const char *binary=c->api->config_get(c->api->priv,"ui_theme.binary",NULL);
    int rc=oa_svg_pack_load(&c->pack,index,binary);
    if (rc) {
        c->api->log(c->api->priv,OA_LOG_ERROR,"ui_theme: invalid/unreadable BinSVG index or binary (%d)",rc);
        return -1;
    }
    rc=c->api->publish(c->api->priv,OA_SVC_UI_THEME,1,&iface);
    if (rc) { oa_svg_pack_free(c->pack); c->pack=NULL; return rc; }
    c->published=1;
    return 0;
}
/* ABI 3 gate: unpublish ui.theme synchronously, detach the parsed pack, then
 * free it synchronously or from one host scheduler slice after
 * ui_theme.cleanup_delay_ms. The scheduler keeps cleanup on the loop and
 * serializes it with other module slices. */
static int m_quiesce(void *v)
{
    struct ctx *c=v;
    if(!c)return 0;
    if(c->cleanup_task || c->cleanup_pack)return -EAGAIN;
    int published=c->published;
    c->published=0;
    if(published)c->api->unpublish(c->api->priv,OA_SVC_UI_THEME);
    struct oa_svg_pack *pack=c->pack;
    c->pack=NULL;
    if(!pack)return 0;
    if(!c->cleanup_delay_ms){oa_svg_pack_free(pack);return 0;}
    c->cleanup_pack=pack;
    const oa_sched_iface *s=c->api->lookup(c->api->priv,OA_SVC_SCHED,1);
    if(!s){c->cleanup_pack=NULL;oa_svg_pack_free(pack);return 0;}
    uint32_t id=0;
    int rc=s->submit(s->context,cleanup_step,c,c->cleanup_delay_ms,&id);
    c->api->release(c->api->priv,OA_SVC_SCHED);
    if(rc){c->cleanup_pack=NULL;oa_svg_pack_free(pack);return -EIO;}
    c->cleanup_task=id;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c=v;
    if(c->published)c->api->unpublish(c->api->priv,OA_SVC_UI_THEME);
    c->published=0;
    cancel_cleanup(c);
    oa_svg_pack_free(c->pack);c->pack=NULL;
}

static void m_destroy(void *v)
{
    struct ctx *c=v;
    cancel_cleanup(c);
    oa_svg_pack_free(c->pack);
    if (current==c) current=NULL;
    free(c);
}

/* Held immutable views remain available during ordinary paired standby.
 * A replacement loaded while suspended defers loading/publication to start. */
static int m_suspend(void *v) { (void)v; return 0; }
static int m_resume(void *v) { (void)v; return 0; }
static const oa_module module={
    .magic=OA_MODULE_MAGIC,.abi=OA_ABI_VERSION,.size=sizeof(oa_module),
    .name="ui_theme",.version="0.1",.init=m_init,.start=m_start,.stop=m_stop,
    .destroy=m_destroy,.suspend=m_suspend,.resume=m_resume,.quiesce=m_quiesce,
};
OA_MODULE_EXPORT(module)
