/*
 * host.c — the only place module code ever runs.
 *
 * Single-threaded by design: timers, fd watches, events and service calls are all
 * dispatched from oa_host_run(). That is what makes dlclose() safe — a module can
 * only be unloaded between two dispatches, never underneath a callback that is
 * still on the stack.
 *
 * Threads are allowed, but a module must not call the host from them except post().
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/events.h"
#include "openaurora/module.h"
#include "host.h"

/* ------------------------------------------------------------------ util */

uint64_t oa_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static uint32_t next_id(oa_host *h)
{
    if (++h->next_id == 0)
        h->next_id = 1;
    return h->next_id;
}

/* --------------------------------------------------------------- logging */

static void api_log(void *priv, int level, const char *fmt, ...)
{
    oa_host *h = priv;
    if (level > h->log_level)
        return;
    static const char *tag[] = { "ERR ", "WARN", "INFO", "DBG " };
    const char *mod = "host";
    if (h->cur_module) {
        oa_mod *m = oa_host_find_module_by_id(h, h->cur_module);
        if (m)
            mod = m->name;
    }
    fprintf(stderr, "[%6llu] %s %-12s ", (unsigned long long)(oa_now_ms() % 1000000), tag[level], mod);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void api_emit(void *priv, const char *topic, const void *ev);

/* -------------------------------------------------------------- services */

static int api_publish(void *priv, const char *name, uint32_t version, const void *iface)
{
    oa_host *h = priv;
    for (int i = 0; i < MAX_SVCS; i++)
        if (h->svcs[i].used && !strcmp(h->svcs[i].name, name)) {
            api_log(h, OA_LOG_ERROR, "publish %s: already published by module %u", name, h->svcs[i].owner);
            return -1;
        }
    for (int i = 0; i < MAX_SVCS; i++)
        if (!h->svcs[i].used) {
            snprintf(h->svcs[i].name, OA_NAME_MAX, "%s", name);
            h->svcs[i].version = version;
            h->svcs[i].iface = iface;
            h->svcs[i].owner = h->cur_module;
            h->svcs[i].used = 1;
            api_log(h, OA_LOG_DEBUG, "service up: %s v%u", name, version);
            return 0;
        }
    return -1;
}

static int api_unpublish(void *priv, const char *name)
{
    oa_host *h = priv;
    for (int i = 0; i < MAX_SVCS; i++)
        if (h->svcs[i].used && !strcmp(h->svcs[i].name, name)) {
            h->svcs[i].used = 0;
            for (int j = 0; j < MAX_HOLDS; j++)
                if (h->holds[j].used && !strcmp(h->holds[j].name, name))
                    h->holds[j].used = 0;
            api_log(h, OA_LOG_DEBUG, "service down: %s", name);
            oa_service_removed_event ev = { name };
            api_emit(h, OA_TOPIC_SERVICE_REMOVED, &ev);
            return 0;
        }
    return -1;
}

static const void *api_lookup(void *priv, const char *name, uint32_t min_version)
{
    oa_host *h = priv;
    for (int i = 0; i < MAX_SVCS; i++) {
        if (!h->svcs[i].used || strcmp(h->svcs[i].name, name))
            continue;
        if (h->svcs[i].version < min_version) {
            api_log(h, OA_LOG_WARN, "lookup %s: v%u < required v%u", name, h->svcs[i].version, min_version);
            return NULL;
        }
        if (h->cur_module && h->cur_module != h->svcs[i].owner) {
            /* one hold per (holder, service): repeated lookups must not fill the table */
            int held = 0;
            for (int j = 0; j < MAX_HOLDS; j++)
                if (h->holds[j].used && h->holds[j].holder == h->cur_module &&
                    !strcmp(h->holds[j].name, name))
                    held = 1;
            if (!held) {
                int slot = -1;
                for (int j = 0; j < MAX_HOLDS; j++)
                    if (!h->holds[j].used) {
                        slot = j;
                        break;
                    }
                if (slot < 0) {
                    api_log(h, OA_LOG_ERROR, "lookup %s: hold table full, refusing unprotected pointer", name);
                    return NULL;
                } else {
                    snprintf(h->holds[slot].name, OA_NAME_MAX, "%s", name);
                    h->holds[slot].holder = h->cur_module;
                    h->holds[slot].used = 1;
                }
            }
        }
        return h->svcs[i].iface;
    }
    return NULL;
}

static void api_release(void *priv, const char *name)
{
    oa_host *h = priv;
    for (int j = 0; j < MAX_HOLDS; j++)
        if (h->holds[j].used && h->holds[j].holder == h->cur_module && !strcmp(h->holds[j].name, name))
            h->holds[j].used = 0;
}

int oa_host_service_held_by_others(oa_host *h, const char *name, uint32_t owner)
{
    for (int j = 0; j < MAX_HOLDS; j++)
        if (h->holds[j].used && h->holds[j].holder != owner && !strcmp(h->holds[j].name, name))
            return 1;
    return 0;
}

int oa_host_service_exists(oa_host *h, const char *name)
{
    for (int i = 0; i < MAX_SVCS; i++)
        if (h->svcs[i].used && !strcmp(h->svcs[i].name, name))
            return 1;
    return 0;
}

/* ---------------------------------------------------------------- events */

static void api_emit(void *priv, const char *topic, const void *ev)
{
    oa_host *h = priv;
    for (int i = 0; i < MAX_SUBS; i++) {
        oa_sub *s = &h->subs[i];
        if (!s->active || strcmp(s->topic, topic))
            continue;
        /* copy first: the callee may unsubscribe or add subscribers */
        void (*cb)(const void *, void *) = s->cb;
        void *ud = s->ud;
        uint32_t prev = h->cur_module;
        h->cur_module = s->owner;
        cb(ev, ud);
        h->cur_module = prev;
    }
}

static uint32_t api_subscribe(void *priv, const char *topic, void (*cb)(const void *, void *), void *ud)
{
    oa_host *h = priv;
    for (int i = 0; i < MAX_SUBS; i++)
        if (!h->subs[i].active) {
            h->subs[i].id = next_id(h);
            snprintf(h->subs[i].topic, TOPIC_MAX, "%s", topic);
            h->subs[i].cb = cb;
            h->subs[i].ud = ud;
            h->subs[i].owner = h->cur_module;
            h->subs[i].active = 1;
            return h->subs[i].id;
        }
    return 0;
}

static void api_unsubscribe(void *priv, uint32_t id)
{
    oa_host *h = priv;
    for (int i = 0; i < MAX_SUBS; i++)
        if (h->subs[i].active && h->subs[i].id == id)
            h->subs[i].active = 0;
}

/* ---------------------------------------------------------------- timers */

static uint64_t api_timer_add(void *priv, uint32_t delay_ms, uint32_t repeat_ms, void (*cb)(void *), void *ud)
{
    oa_host *h = priv;
    for (int i = 0; i < MAX_TIMERS; i++)
        if (!h->timers[i].active) {
            h->timers[i].id = next_id(h);
            h->timers[i].due = oa_now_ms() + delay_ms;
            h->timers[i].repeat_ms = repeat_ms;
            h->timers[i].cb = cb;
            h->timers[i].ud = ud;
            h->timers[i].owner = h->cur_module;
            h->timers[i].active = 1;
            return h->timers[i].id;
        }
    api_log(h, OA_LOG_ERROR, "timer table full");
    return 0;
}

static void api_timer_del(void *priv, uint64_t id)
{
    oa_host *h = priv;
    for (int i = 0; i < MAX_TIMERS; i++)
        if (h->timers[i].active && h->timers[i].id == id)
            h->timers[i].active = 0;
}

/* --------------------------------------------------------------- watches */

static uint32_t api_watch_add(void *priv, int fd, unsigned events,
                              void (*cb)(void *ud, int fd, unsigned revents), void *ud)
{
    oa_host *h = priv;
    for (int i = 0; i < MAX_WATCHES; i++)
        if (!h->watches[i].active) {
            h->watches[i].id = next_id(h);
            h->watches[i].fd = fd;
            h->watches[i].events = events;
            h->watches[i].cb = cb;
            h->watches[i].ud = ud;
            h->watches[i].owner = h->cur_module;
            h->watches[i].active = 1;
            return h->watches[i].id;
        }
    return 0;
}

static void api_watch_del(void *priv, uint32_t id)
{
    oa_host *h = priv;
    for (int i = 0; i < MAX_WATCHES; i++)
        if (h->watches[i].active && h->watches[i].id == id)
            h->watches[i].active = 0;
}

/* --------------------------------------------------------------- posting */

static int api_post(void *priv, uint32_t owner, void (*cb)(void *), void *ud)
{
    oa_host *h = priv;
    if (!cb)
        return -1;
    int ok = -2;
    pthread_mutex_lock(&h->post_mu);
    for (int i = 0; owner && i < MAX_MODULES; i++) {
        if (h->post_owners[i] != owner)
            continue;
        ok = -1;
        if (h->nposts < MAX_POSTS) {
            h->posts[h->nposts++] = (oa_post){ cb, ud, owner };
            ok = 0;
            unsigned char b = 1;
            (void)!write(h->selfpipe[1], &b, 1);
        }
        break;
    }
    pthread_mutex_unlock(&h->post_mu);
    return ok;
}

static uint32_t api_owner_id(void *priv)
{
    return ((oa_host *)priv)->cur_module;
}

int oa_host_enable_posts(oa_host *h, uint32_t owner)
{
    if (!owner)
        return -1;
    int slot = -1;
    pthread_mutex_lock(&h->post_mu);
    for (int i = 0; i < MAX_MODULES; i++) {
        if (h->post_owners[i] == owner) {
            slot = i;
            break;
        }
        if (!h->post_owners[i] && slot < 0)
            slot = i;
    }
    if (slot >= 0)
        h->post_owners[slot] = owner;
    pthread_mutex_unlock(&h->post_mu);
    return slot < 0 ? -1 : 0;
}

void oa_host_disable_posts(oa_host *h, uint32_t owner)
{
    pthread_mutex_lock(&h->post_mu);
    for (int i = 0; i < MAX_MODULES; i++)
        if (h->post_owners[i] == owner)
            h->post_owners[i] = 0;
    int keep = 0;
    for (int i = 0; i < h->nposts; i++)
        if (h->posts[i].owner != owner)
            h->posts[keep++] = h->posts[i];
    h->nposts = keep;
    pthread_mutex_unlock(&h->post_mu);
}

void oa_host_dispatch_posts(oa_host *h)
{
    /* Pop one callback at a time. An earlier callback can stop/unload another
     * owner and cancel its queued work; a detached batch would keep stale code.
     * Bound dispatch so a busy producer cannot starve timers and fd watches. */
    for (int dispatched = 0; dispatched < MAX_POSTS; dispatched++) {
        pthread_mutex_lock(&h->post_mu);
        if (!h->nposts) {
            pthread_mutex_unlock(&h->post_mu);
            break;
        }
        oa_post p = h->posts[0];
        h->nposts--;
        memmove(h->posts, h->posts + 1, (size_t)h->nposts * sizeof h->posts[0]);
        pthread_mutex_unlock(&h->post_mu);
        uint32_t prev = h->cur_module;
        h->cur_module = p.owner;
        p.cb(p.ud);
        h->cur_module = prev;
    }
}

/* ---------------------------------------------------------------- config */

static void config_load(oa_host *h, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return;
    char line[256];
    while (fgets(line, sizeof line, f) &&
           h->config_count < (int)(sizeof h->config / sizeof h->config[0])) {
        char *s = line;
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s == '#' || *s == '\n' || !*s)
            continue;
        char *eq = strchr(s, '=');
        if (!eq)
            continue;
        *eq = 0;
        char *k = s, *v = eq + 1;
        /* "key = value" is a natural way to write a config file, so skip the value's leading
         * whitespace too - forgetting this made display.fb resolve to " /dev/fb1" and every
         * open() fail with ENOENT while sysfs lookups (basename-based) still succeeded. */
        while (*v == ' ' || *v == '\t')
            v++;
        char *e = v + strlen(v);
        while (e > v && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' '))
            *--e = 0;
        e = k + strlen(k);
        while (e > k && (e[-1] == ' ' || e[-1] == '\t'))
            *--e = 0;
        int slot = h->config_count++;
        snprintf(h->config[slot].k, sizeof h->config[slot].k, "%.*s",
                 (int)sizeof h->config[slot].k - 1, k);
        snprintf(h->config[slot].v, sizeof h->config[slot].v, "%.*s",
                 (int)sizeof h->config[slot].v - 1, v);
    }
    fclose(f);
}

static const char *api_config_get(void *priv, const char *key, const char *def)
{
    oa_host *h = priv;
    if (!h || !key) return def;
    if (!strcmp(key, "ui.skin") && h->skin_selected[0]) return h->skin_selected;
    for (int i = 0; i < h->config_count; i++)
        if (!strcmp(h->config[i].k, key))
            return h->config[i].v;
    return def;
}

/* ------------------------------------------------------------------ init */

static int api_sleep_cycle(void *context, uint32_t owner, int (*enter)(void *),
                            void *ud, oa_sleep_result *result)
{
    return oa_module_standby_cycle(context, owner, enter, ud, result);
}

static const oa_host_api g_api_template = {
    .size = sizeof(oa_host_api),
    .abi = OA_ABI_VERSION,
    .priv = NULL,
    .log = api_log,
    .publish = api_publish,
    .unpublish = api_unpublish,
    .lookup = api_lookup,
    .release = api_release,
    .subscribe = api_subscribe,
    .unsubscribe = api_unsubscribe,
    .emit = api_emit,
    .timer_add = api_timer_add,
    .timer_del = api_timer_del,
    .watch_add = api_watch_add,
    .watch_del = api_watch_del,
    .post = api_post,
    .config_get = api_config_get,
    .owner_id = api_owner_id,
};

int oa_host_init(oa_host *h, const char *modules_dir, const char *config)
{
    memset(h, 0, sizeof *h);
    h->api = g_api_template;
    h->api.priv = h;
    h->log_level = OA_LOG_INFO;
    h->running = 1;
    snprintf(h->modules_dir, sizeof h->modules_dir, "%s", modules_dir ? modules_dir : "modules");
    if (config) {
        snprintf(h->config_path, sizeof h->config_path, "%s", config);
        config_load(h, config);
    }
    /* pipe2 rather than pipe+fcntl: fcntl is versioned GLIBC_2.28 and the TV has 2.23 */
    if (pipe2(h->selfpipe, O_NONBLOCK | O_CLOEXEC) != 0)
        return -1;
    pthread_mutex_init(&h->post_mu, NULL);
    const char *heartbeat = api_config_get(h, "host.heartbeat", "");
    if (strlen(heartbeat) >= sizeof h->heartbeat_path - 32)
        return -1;
    snprintf(h->heartbeat_path, sizeof h->heartbeat_path, "%s", heartbeat);
    h->sleep_iface = (oa_sleep_iface){ h, api_sleep_cycle };
    if (api_publish(h, OA_SVC_SLEEP, 1, &h->sleep_iface) != 0) return -1;
    return 0;
}

void oa_host_signal_stop(oa_host *h)
{
    int saved = errno;
    h->stop_requested = 1;
    unsigned char b = 1;
    (void)!write(h->selfpipe[1], &b, 1);
    errno = saved;
}

void oa_host_quit(oa_host *h)
{
    h->running = 0;
    unsigned char b = 1;
    ssize_t r = write(h->selfpipe[1], &b, 1);
    (void)r;
}

oa_mod *oa_host_find_module(oa_host *h, const char *name)
{
    for (int i = 0; i < MAX_MODULES; i++)
        if (h->mods[i].state != OA_ST_EMPTY && !strcmp(h->mods[i].name, name))
            return &h->mods[i];
    return NULL;
}

oa_mod *oa_host_find_module_by_id(oa_host *h, uint32_t id)
{
    for (int i = 0; i < MAX_MODULES; i++)
        if (h->mods[i].state != OA_ST_EMPTY && h->mods[i].id == id)
            return &h->mods[i];
    return NULL;
}

/* Drop everything a module registered. Safe after stop()/destroy(). */
void oa_host_cleanup_owned(oa_host *h, uint32_t module_id)
{
    oa_host_disable_posts(h, module_id);
    for (int i = 0; i < MAX_TIMERS; i++)
        if (h->timers[i].active && h->timers[i].owner == module_id)
            h->timers[i].active = 0;
    for (int i = 0; i < MAX_WATCHES; i++)
        if (h->watches[i].active && h->watches[i].owner == module_id)
            h->watches[i].active = 0;
    for (int i = 0; i < MAX_SUBS; i++)
        if (h->subs[i].active && h->subs[i].owner == module_id)
            h->subs[i].active = 0;
    for (int i = 0; i < MAX_SVCS; i++)
        if (h->svcs[i].used && h->svcs[i].owner == module_id)
            api_unpublish(h, h->svcs[i].name);
    for (int i = 0; i < MAX_HOLDS; i++)
        if (h->holds[i].used && h->holds[i].holder == module_id)
            h->holds[i].used = 0;
}

/* ------------------------------------------------------------------- loop */

/* Only the dispatch thread writes this receipt. A blocked module callback must
 * stop it advancing. Keep it on tmpfs, not /conf (flash), and publish atomically. */
static int heartbeat_write(oa_host *h)
{
    uint64_t now = oa_now_ms();
    if (!h->heartbeat_path[0] || now < h->heartbeat_due)
        return 0;
    char tmp[288];
    snprintf(tmp, sizeof tmp, "%s.%ld.tmp", h->heartbeat_path, (long)getpid());
    FILE *f = fopen(tmp, "w");
    if (!f)
        return -1;
    int failed = fprintf(f, "%ld %llu\n", (long)getpid(), (unsigned long long)now) < 0;
    if (fclose(f) != 0)
        failed = 1;
    if (!failed && rename(tmp, h->heartbeat_path) == 0) {
        h->heartbeat_due = now + 1000;
        return 0;
    }
    unlink(tmp);
    return -1;
}

static int next_timeout_ms(oa_host *h)
{
    uint64_t now = oa_now_ms(), best = 0;
    int have = 0;
    for (int i = 0; i < MAX_TIMERS; i++)
        if (h->timers[i].active) {
            uint64_t d = h->timers[i].due;
            if (!have || d < best) {
                best = d;
                have = 1;
            }
        }
    if (!have)
        return -1;
    if (best <= now)
        return 0;
    return (int)(best - now);
}

static void fire_due_timers(oa_host *h)
{
    uint64_t now = oa_now_ms();
    for (int i = 0; i < MAX_TIMERS; i++) {
        oa_timer *t = &h->timers[i];
        if (!t->active || t->due > now)
            continue;
        void (*cb)(void *) = t->cb;
        void *ud = t->ud;
        uint32_t owner = t->owner;
        if (t->repeat_ms)
            t->due = now + t->repeat_ms;
        else
            t->active = 0;
        uint32_t prev = h->cur_module;
        h->cur_module = owner;
        cb(ud);
        h->cur_module = prev;
    }
}

/* One wait. EINTR is not fatal: the caller loops. A real poll error is -1. */
static int poll_once(oa_host *h)
{
    struct pollfd pfds[MAX_WATCHES + 1];
    uint32_t map[MAX_WATCHES + 1];
    int n = 0;
    pfds[n].fd = h->selfpipe[0];
    pfds[n].events = POLLIN;
    pfds[n].revents = 0;
    map[n] = 0;
    n++;
    for (int i = 0; i < MAX_WATCHES; i++) {
        oa_watch *w = &h->watches[i];
        if (!w->active)
            continue;
        pfds[n].fd = w->fd;
        pfds[n].events = (short)(((w->events & OA_WATCH_READ) ? POLLIN : 0) |
                                 ((w->events & OA_WATCH_WRITE) ? POLLOUT : 0));
        pfds[n].revents = 0;
        map[n] = w->id;
        n++;
    }

    int timeout = next_timeout_ms(h);
    if (h->heartbeat_path[0]) {
        uint64_t now = oa_now_ms();
        int hb_timeout = h->heartbeat_due > now ? (int)(h->heartbeat_due - now) : 0;
        if (timeout < 0 || hb_timeout < timeout)
            timeout = hb_timeout;
    }
    int r = poll(pfds, (nfds_t)n, timeout);
    if (r < 0)
        return errno == EINTR ? 0 : -1;

    if (pfds[0].revents & POLLIN) {
        unsigned char buf[64];
        while (read(h->selfpipe[0], buf, sizeof buf) > 0) {
        }
    }

    for (int i = 1; i < n && r > 0; i++) {
        if (!pfds[i].revents)
            continue;
        /* re-find by id: an earlier callback may have removed it */
        oa_watch *w = NULL;
        for (int j = 0; j < MAX_WATCHES; j++)
            if (h->watches[j].active && h->watches[j].id == map[i]) {
                w = &h->watches[j];
                break;
            }
        if (!w)
            continue;
        unsigned rev = 0;
        if (pfds[i].revents & (POLLIN | POLLHUP))
            rev |= OA_WATCH_READ;
        if (pfds[i].revents & POLLOUT)
            rev |= OA_WATCH_WRITE;
        if (pfds[i].revents & (POLLERR | POLLNVAL))
            rev |= OA_WATCH_ERR;
        void (*cb)(void *, int, unsigned) = w->cb;
        void *ud = w->ud;
        int fd = w->fd;
        uint32_t prev = h->cur_module;
        h->cur_module = w->owner;
        cb(ud, fd, rev);
        h->cur_module = prev;
    }
    return 0;
}

static int modules_loaded(const oa_host *h)
{
    for (int i = 0; i < MAX_MODULES; i++)
        if (h->mods[i].state != OA_ST_EMPTY)
            return 1;
    return 0;
}

static int cleanup_blocked(const oa_host *h)
{
    for (int i = 0; i < MAX_MODULES; i++)
        if (h->mods[i].state != OA_ST_EMPTY && h->mods[i].cleanup_pending)
            return 1;
    return 0;
}

/* A module timer or fd watch can still finish worker/broker release. Host-owned
 * watches (owner 0), including the control plane, do not count. */
static int module_has_loop_work(const oa_host *h)
{
    for (int i = 0; i < MAX_TIMERS; i++)
        if (h->timers[i].active && h->timers[i].owner)
            return 1;
    for (int i = 0; i < MAX_WATCHES; i++)
        if (h->watches[i].active && h->watches[i].owner)
            return 1;
    return 0;
}

static int posts_queued(oa_host *h)
{
    pthread_mutex_lock(&h->post_mu);
    int n = h->nposts;
    pthread_mutex_unlock(&h->post_mu);
    return n;
}

/* Reverse slot order matches the historical load-order shutdown. Errors from a
 * busy gate are retained for another pass; this never uses --force. */
static int release_pass(oa_host *h)
{
    int failed = 0;
    for (int i = MAX_MODULES - 1; i >= 0; i--)
        if (h->mods[i].state != OA_ST_EMPTY && oa_module_stop(h, &h->mods[i]) != 0)
            failed = 1;
    for (int i = MAX_MODULES - 1; i >= 0; i--)
        if (h->mods[i].state != OA_ST_EMPTY &&
            oa_module_unload(h, h->mods[i].name, 0) != 0)
            failed = 1;
    return failed;
}

static void log_shutdown_blocked(oa_host *h)
{
    for (int i = 0; i < MAX_MODULES; i++) {
        oa_mod *m = &h->mods[i];
        if (m->state == OA_ST_EMPTY)
            continue;
        api_log(h, OA_LOG_ERROR, "%s: shutdown left module loaded (state %s, cleanup_pending %d)",
                m->name, oa_state_name(m->state), m->cleanup_pending);
    }
}

/* Stay on the loop thread until every module is gone, or until a failure has
 * no completion post, timer or module watch that could make progress. A busy
 * gate blocks here instead of dlclose. */
static int shutdown_modules(oa_host *h)
{
    h->shutting_down = 1;
    for (;;) {
        oa_host_dispatch_posts(h);
        fire_due_timers(h);
        if (!modules_loaded(h))
            return 0;
        (void)release_pass(h);
        if (!modules_loaded(h))
            return 0;
        /* A gate may have queued its own completion. Run that before sleeping,
         * then retry stop/unload on the next pass. */
        if (posts_queued(h))
            continue;
        if (!cleanup_blocked(h) && !module_has_loop_work(h)) {
            log_shutdown_blocked(h);
            return -1;
        }
        if (heartbeat_write(h) != 0) {
            api_log(h, OA_LOG_ERROR, "heartbeat %s: %s", h->heartbeat_path, strerror(errno));
            return -1;
        }
        if (poll_once(h) != 0)
            return -1;
    }
}

int oa_host_run(oa_host *h)
{
    int rc = 0;
    while (h->running && !h->stop_requested) {
        oa_host_dispatch_posts(h);
        fire_due_timers(h);
        if (!h->running || h->stop_requested)
            break;
        if (heartbeat_write(h) != 0) {
            api_log(h, OA_LOG_ERROR, "heartbeat %s: %s", h->heartbeat_path, strerror(errno));
            rc = -1;
            break;
        }
        if (poll_once(h) != 0) {
            rc = -1;
            break;
        }
    }
    if (shutdown_modules(h) != 0)
        rc = -1;
    if (h->heartbeat_path[0])
        unlink(h->heartbeat_path);
    oa_control_stop(h);
    return rc;
}
