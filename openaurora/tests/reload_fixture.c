/* No devices: exercise actual unload/load and state-handoff callbacks. */
#include <stdlib.h>
#include <string.h>
#include "openaurora/module.h"
#include "reload_fixture.h"

struct ctx {
    const oa_host_api *api;
    struct reload_observer *observer;
    unsigned value;
};

#ifndef RELOAD_WITHOUT_RESTORE
static void unexpected_post(void *ud)
{
    struct ctx *c = ud;
    c->observer->bad_callback++;
}
static void unexpected_timer(void *ud) { unexpected_post(ud); }
static void unexpected_event(const void *ev, void *ud)
{ (void)ev; unexpected_post(ud); }
#endif

static int init(const oa_host_api *api, void **out)
{
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api;
    *out = c;
    c->observer = (struct reload_observer *)api->lookup(api->priv, RELOAD_OBSERVER, 1);
    if (!c->observer) return -1;
    c->observer->init++;
    c->value = c->observer->value;
    return 0;
}
static int start(void *ud)
{
    struct ctx *c = ud;
    c->observer->start++;
    c->observer->active_value = c->value;
    if (c->api->publish(c->api->priv, RELOAD_SERVICE, 1, c)) return -1;
    return c->observer->fail_start ? -1 : 0;
}
static void stop(void *ud)
{
    struct ctx *c = ud;
    c->observer->stop++;
    c->api->unpublish(c->api->priv, RELOAD_SERVICE);
}
static void destroy(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    if (c->observer) c->observer->destroy++;
    c->api->release(c->api->priv, RELOAD_OBSERVER);
    free(c);
}
static int suspend(void *ud)
{
    struct ctx *c = ud;
    c->observer->suspend++;
    c->api->unpublish(c->api->priv, RELOAD_SERVICE);
    return 0;
}
static int resume(void *ud)
{
    struct ctx *c = ud;
    c->observer->resume++;
    if (c->observer->fail_resume) return -1;
    return c->api->publish(c->api->priv, RELOAD_SERVICE, 1, c);
}
static size_t save(void *ud, void *buf, size_t cap)
{
    struct ctx *c = ud;
    if (!buf) return c->observer->save_need;
    if (cap >= sizeof c->value) memcpy(buf, &c->value, sizeof c->value);
    return c->observer->save_result;
}
#ifndef RELOAD_WITHOUT_RESTORE
static int restore(void *ud, const void *buf, size_t size)
{
    struct ctx *c = ud;
    struct reload_observer *o = c->observer;
    o->restore++;
    if (o->fail_restore) {
        /* Even failed restore registrations/posts must not survive dlclose. */
        c->api->publish(c->api->priv, RELOAD_SERVICE, 1, c);
        c->api->timer_add(c->api->priv, 0, 0, unexpected_timer, c);
        c->api->subscribe(c->api->priv, "reload-test", unexpected_event, c);
        c->api->post(c->api->priv, c->api->owner_id(c->api->priv), unexpected_post, c);
        return -7;
    }
    if (size != sizeof c->value) return -1;
    memcpy(&c->value, buf, size);
    o->restored_value = c->value;
    if (o->post_on_restore)
        c->api->post(c->api->priv, c->api->owner_id(c->api->priv), unexpected_post, c);
    return 0;
}
#endif
static const oa_module module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
#ifdef RELOAD_RENAMED
    .name = "reload_renamed",
#else
    .name = "reload_fixture",
#endif
    .version = "test", .init = init, .start = start,
    .stop = stop, .destroy = destroy, .suspend = suspend, .resume = resume,
    .save = save,
#ifndef RELOAD_WITHOUT_RESTORE
    .restore = restore,
#endif
};
OA_MODULE_EXPORT(module)
