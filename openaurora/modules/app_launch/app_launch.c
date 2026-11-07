/* mod_app_launch — consume OA_TOPIC_ACTION SELECT and spawn a launcher helper.
 *
 * The module owns no vendor library and never blocks the host loop on app
 * manager IPC. oa-app-launch (or any configured helper) performs the
 * platform-specific registration/start call in a child process.
 *
 * Config:
 *   app_launch.enable  = 0|1           explicit launch gate (default 0)
 *   app_launch.helper  = PATH          helper executable (default ./oa-app-launch)
 *   app_launch.map     = PATH          display-name<TAB>program<TAB>args file
 *   app_launch.reap_ms = 20..1000      child reap timer (default 100)
 *
 * The map is data, not a service: missing entries are ignored. A request while
 * a helper is running is refused with EBUSY rather than starting a second one.
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/app_launch.h"
#include "openaurora/events.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

extern char **environ;

struct ctx {
    const oa_host_api *api;
    uint32_t id, sub;
    uint64_t timer;
    pid_t child;
    int published, active, enabled;
    char helper[256], map[256];
};

static struct ctx *g_ctx;

static void copy_key(struct ctx *c, char *dst, size_t cap,
                     const char *key, const char *def)
{
    const char *v = c->api->config_get(c->api->priv, key, def);
    if (!v || !*v) v = def;
    snprintf(dst, cap, "%s", v ? v : "");
}

static int map_lookup(struct ctx *c, const char *name,
                      char *program, size_t pcap, char *args, size_t acap)
{
    if (!c->map[0] || !name || !*name) return -1;
    FILE *f = fopen(c->map, "r");
    if (!f) return -1;
    char line[1024];
    int found = -1;
    while (fgets(line, sizeof line, f)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = 0;
        if (!line[0] || line[0] == '#') continue;
        char *t1 = strchr(line, '\t');
        if (!t1) continue;
        *t1++ = 0;
        if (strcmp(line, name)) continue;
        char *t2 = strchr(t1, '\t');
        if (t2) *t2++ = 0;
        snprintf(program, pcap, "%s", t1);
        snprintf(args, acap, "%s", t2 ? t2 : "");
        found = 0;
        break;
    }
    fclose(f);
    return found;
}

static void stop_child(struct ctx *c)
{
    if (!c || c->child <= 0) return;
    pid_t pid = c->child;
    c->child = 0;
    (void)kill(pid, SIGTERM);
    for (int i = 0; i < 50; i++) {
        int status;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid || (r < 0 && errno == ECHILD)) return;
        const struct timespec pause = { .tv_nsec = 10000000 };
        (void)nanosleep(&pause, NULL);
    }
    (void)kill(pid, SIGKILL);
    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {}
}

static int request(const char *name, const char *args)
{
    struct ctx *c = g_ctx;
    if (!c || !c->active || !c->enabled || !name || !*name) {
        errno = ENOTSUP;
        return -1;
    }
    if (c->child > 0) {
        errno = EBUSY;
        return -1;
    }
    char program[128], mapped[512], use[512];
    if (map_lookup(c, name, program, sizeof program, mapped, sizeof mapped) ||
        !program[0]) {
        errno = ENOENT;
        return -1;
    }
    snprintf(use, sizeof use, "%s", args && *args ? args : mapped);
    char *argv[8];
    int n = 0;
    argv[n++] = c->helper;
    argv[n++] = "--name";
    argv[n++] = program;
    if (use[0]) {
        argv[n++] = "--args";
        argv[n++] = use;
    }
    argv[n] = NULL;
    pid_t pid = 0;
    int rc = posix_spawn(&pid, c->helper, NULL, NULL, argv, environ);
    if (rc) {
        errno = rc;
        return -1;
    }
    c->child = pid;
    c->api->log(c->api->priv, OA_LOG_INFO,
                "app_launch: %s -> %s (pid %ld)", name, program, (long)pid);
    return 0;
}

static int iface_request(const char *name, const char *args)
{
    return request(name, args);
}

static const oa_app_launch_iface g_iface = { iface_request };

static void on_action(const void *ev, void *ud)
{
    struct ctx *c = ud;
    const oa_action_event *a = ev;
    if (!c || !a || a->kind != OA_ACTION_SELECT || !a->name) return;
    int saved = 0;
    int rc = request(a->name, NULL);
    if (rc) saved = errno;
    c->api->log(c->api->priv, rc ? OA_LOG_WARN : OA_LOG_INFO,
                "app_launch: action %s -> %s", a->name,
                rc ? strerror(saved) : "spawned");
}

static void reap(void *ud)
{
    struct ctx *c = ud;
    if (!c || c->child <= 0) return;
    int status;
    pid_t r = waitpid(c->child, &status, WNOHANG);
    if (r == c->child) {
        c->child = 0;
        c->api->log(c->api->priv, OA_LOG_INFO,
                    "app_launch: helper exited status %d", status);
    } else if (r < 0 && errno == ECHILD) {
        c->child = 0;
    }
}

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof *api)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api;
    c->id = api->owner_id ? api->owner_id(api->priv) : 0;
    *out = c;
    g_ctx = c;
    return 0;
}

static int m_start(void *ud)
{
    struct ctx *c = ud;
    if (!c || c->active || c->child > 0) return -1;
    copy_key(c, c->helper, sizeof c->helper, "app_launch.helper",
             "./oa-app-launch");
    copy_key(c, c->map, sizeof c->map, "app_launch.map", "/conf/oa_apps.map");
    c->enabled = atoi(c->api->config_get(c->api->priv,
                                         "app_launch.enable", "0")) != 0;
    unsigned reap_ms = (unsigned)atoi(c->api->config_get(c->api->priv,
                                                         "app_launch.reap_ms",
                                                         "100"));
    if (reap_ms < 20) reap_ms = 20;
    if (reap_ms > 1000) reap_ms = 1000;
    if (c->enabled && access(c->helper, X_OK)) {
        c->api->log(c->api->priv, OA_LOG_ERROR,
                    "app_launch: helper '%s' is not executable", c->helper);
        return -1;
    }
    c->sub = c->api->subscribe(c->api->priv, OA_TOPIC_ACTION, on_action, c);
    if (!c->sub) return -1;
    c->timer = c->api->timer_add(c->api->priv, reap_ms, reap_ms, reap, c);
    if (!c->timer) {
        c->api->unsubscribe(c->api->priv, c->sub);
        c->sub = 0;
        return -1;
    }
    if (c->api->publish(c->api->priv, OA_SVC_APP_LAUNCH, 1, &g_iface)) {
        c->api->timer_del(c->api->priv, c->timer);
        c->timer = 0;
        c->api->unsubscribe(c->api->priv, c->sub);
        c->sub = 0;
        return -1;
    }
    c->published = 1;
    c->active = 1;
    c->api->log(c->api->priv, OA_LOG_INFO,
                "app_launch: ready (enable=%d helper=%s map=%s)",
                c->enabled, c->helper, c->map);
    return 0;
}

static void m_stop(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    c->active = 0;
    if (c->sub) {
        c->api->unsubscribe(c->api->priv, c->sub);
        c->sub = 0;
    }
    if (c->timer) {
        c->api->timer_del(c->api->priv, c->timer);
        c->timer = 0;
    }
    if (c->published) {
        c->api->unpublish(c->api->priv, OA_SVC_APP_LAUNCH);
        c->published = 0;
    }
    stop_child(c);
    c->enabled = 0;
}

static void m_destroy(void *ud)
{
    struct ctx *c = ud;
    if (!c) return;
    stop_child(c);
    if (g_ctx == c) g_ctx = NULL;
    free(c);
}

static const oa_module g_module = {
    .magic = OA_MODULE_MAGIC,
    .abi = OA_ABI_VERSION,
    .size = sizeof g_module,
    .name = "app_launch",
    .version = "0.1",
    .requires = { "", },
    .init = m_init,
    .start = m_start,
    .stop = m_stop,
    .destroy = m_destroy,
};

OA_MODULE_EXPORT(g_module)
