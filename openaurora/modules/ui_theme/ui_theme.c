/* Data-only immutable BinSVG provider. Start/reload reads bounded configured
 * files; service calls use memory only. No renderer or hardware ownership. */
#include "openaurora/module.h"
#include "openaurora/services.h"
#include "openaurora/svg_pack.h"
#include <errno.h>
#include <stdlib.h>

struct ctx { const oa_host_api *api; struct oa_svg_pack *pack; int published; };
static struct ctx *current;
static int record(unsigned id, struct oa_binsvg *out)
{ return current && current->published ? oa_svg_pack_record(current->pack,id,out) : ENODEV; }
static int named(const char *name, struct oa_binsvg *out)
{ return current && current->published ? oa_svg_pack_named(current->pack,name,out) : ENODEV; }
static unsigned count(void)
{ return current && current->published ? oa_svg_pack_count(current->pack) : 0; }
static const oa_ui_theme_iface iface={record,named,count};
static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi!=OA_ABI_VERSION || api->size<sizeof *api) return -1;
    struct ctx *c=calloc(1,sizeof *c);
    if (!c) return -1;
    c->api=api; current=c; *out=c;
    return 0;
}
static int m_start(void *v)
{
    struct ctx *c=v;
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
static void m_stop(void *v)
{
    struct ctx *c=v;
    if (c->published) c->api->unpublish(c->api->priv,OA_SVC_UI_THEME);
    c->published=0;
    oa_svg_pack_free(c->pack); c->pack=NULL;
}
static void m_destroy(void *v)
{
    struct ctx *c=v;
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
    .destroy=m_destroy,.suspend=m_suspend,.resume=m_resume,
};
OA_MODULE_EXPORT(module)
