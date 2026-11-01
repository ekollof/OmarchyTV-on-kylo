/* Real worker threads exercise post ownership and joining during stop/reload. */
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "openaurora/module.h"
#include "post_worker.h"

struct ctx {
    const oa_host_api *api;
    uint32_t owner;
    unsigned calls;
    int late_rc, release, spawned;
    pthread_t late_worker;
    pthread_mutex_t mu;
    pthread_cond_t cv;
};
static struct ctx *g;
static void timer(void *unused) { (void)unused; }
static void callback(void *ud)
{
    struct ctx *c = ud;
    c->calls++;
    /* Registrations from a posted callback must belong to its module, not host. */
    c->api->timer_add(c->api->priv, 60000, 0, timer, c);
}
static void *enqueue_worker(void *ud)
{
    struct ctx *c = ud;
    int rc = c->api->post(c->api->priv, c->owner, callback, c);
    return (void *)(long)rc;
}
static int enqueue(void)
{
    pthread_t thread;
    if (pthread_create(&thread, NULL, enqueue_worker, g)) return -99;
    void *result;
    pthread_join(thread, &result);
    return (int)(long)result;
}
static void *late_worker(void *ud)
{
    struct ctx *c = ud;
    pthread_mutex_lock(&c->mu);
    while (!c->release) pthread_cond_wait(&c->cv, &c->mu);
    pthread_mutex_unlock(&c->mu);
    c->late_rc = c->api->post(c->api->priv, c->owner, callback, c);
    return NULL;
}
static unsigned calls(void) { return g->calls; }
static int late_result(void) { return g->late_rc; }
static uint32_t owner(void) { return g->owner; }
static const struct post_worker_iface iface = { enqueue, calls, late_result, owner };
static int init(const oa_host_api *api, void **out)
{
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    *out = g = c;
    c->api = api;
    c->owner = api->owner_id(api->priv);
    pthread_mutex_init(&c->mu, NULL);
    pthread_cond_init(&c->cv, NULL);
#ifdef TEST_FAIL_INIT_POST
    enqueue();
    return -1;
#else
    return 0;
#endif
}
static int start(void *ud)
{
    struct ctx *c = ud;
    c->release = 0;
    c->late_rc = 123;
    if (pthread_create(&c->late_worker, NULL, late_worker, c)) return -1;
    c->spawned = 1;
    if (c->api->publish(c->api->priv, POST_WORKER_SERVICE, 1, &iface)) return -1;
#ifdef TEST_FAIL_START_POST
    enqueue();
    return -1;
#else
    return 0;
#endif
}
static void stop(void *ud)
{
    struct ctx *c = ud;
    if (c->spawned) {
        pthread_mutex_lock(&c->mu);
        c->release = 1;
        pthread_cond_signal(&c->cv);
        pthread_mutex_unlock(&c->mu);
        pthread_join(c->late_worker, NULL);
        c->spawned = 0;
    }
    c->api->unpublish(c->api->priv, POST_WORKER_SERVICE);
}
static void destroy(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    pthread_cond_destroy(&c->cv);
    pthread_mutex_destroy(&c->mu);
    g = NULL;
    free(c);
}
static size_t save(void *ud, void *buf, size_t cap)
{
    if (buf && cap >= sizeof(unsigned)) memcpy(buf, &((struct ctx *)ud)->calls, sizeof(unsigned));
    return sizeof(unsigned);
}
static int restore(void *ud, const void *buf, size_t n)
{
    if (n != sizeof(unsigned)) return -1;
    memcpy(&((struct ctx *)ud)->calls, buf, n);
    return 0;
}
static const oa_module module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "post_worker", .version = "test", .init = init, .start = start,
    .stop = stop, .destroy = destroy, .save = save, .restore = restore,
};
const oa_module *oa_module_entry(void) { return &module; }
