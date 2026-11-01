/*
 * module.c — discovery, lifecycle, unload and reload.
 *
 * The contract that makes hot reload safe:
 *   - every entry point into a module sets h->cur_module, so registrations and
 *     service lookups are attributed to the right owner;
 *   - unload refuses while another module holds its services or actively requires them;
 *   - anything the module registered is dropped by the host afterwards, so a
 *     module that forgets to clean up cannot leak host table entries;
 *   - dlclose() only ever runs from here, i.e. between two loop dispatches.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "openaurora/events.h"
#include "openaurora/module.h"
#include "host.h"

const char *oa_state_name(int st)
{
    switch (st) {
    case OA_ST_EMPTY: return "empty";
    case OA_ST_LOADED: return "loaded";
    case OA_ST_STARTED: return "started";
    case OA_ST_SUSPENDED: return "suspended";
    default: return "?";
    }
}

static void emit_module_event(oa_host *h, const char *name, uint8_t what)
{
    oa_module_event ev = { name, what };
    h->api.emit(h, OA_TOPIC_MODULE_CHANGED, &ev);
}

/* Resolve "ticker", "mod_ticker.so", "mod_ticker" or a full path. */
static int resolve_path(oa_host *h, const char *in, char *out, size_t cap)
{
    struct stat st;
    if (strchr(in, '/')) {
        if (!stat(in, &st)) {
            snprintf(out, cap, "%s", in);
            return 0;
        }
        return -1;
    }
    const char *cands[] = { "%s/%s", "%s/%s.so", "%s/mod_%s.so" };
    for (unsigned i = 0; i < sizeof cands / sizeof cands[0]; i++) {
        snprintf(out, cap, cands[i], h->modules_dir, in);
        if (!stat(out, &st) && S_ISREG(st.st_mode))
            return 0;
    }
    return -1;
}

oa_mod *oa_module_load(oa_host *h, const char *path_or_name)
{
    if (h->sleep_cycle || h->shutting_down) {
        h->api.log(h, OA_LOG_ERROR, "load '%s': host is not accepting modules", path_or_name);
        return NULL;
    }
    char path[256];
    if (resolve_path(h, path_or_name, path, sizeof path) != 0) {
        h->api.log(h, OA_LOG_ERROR, "load '%s': not found in %s", path_or_name, h->modules_dir);
        return NULL;
    }

    oa_mod *slot = NULL;
    for (int i = 0; i < MAX_MODULES; i++)
        if (h->mods[i].state == OA_ST_EMPTY) {
            slot = &h->mods[i];
            break;
        }
    if (!slot) {
        h->api.log(h, OA_LOG_ERROR, "load '%s': module table full", path);
        return NULL;
    }

    /* Function binding is lazy during normal loading. TV modules still link the
     * complete vendor closure; --check verifies eager relocations separately.
     * Lazy binding is not evidence that unresolved imports are safe. */
    void *dl = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (!dl) {
        h->api.log(h, OA_LOG_ERROR, "load '%s': %s", path, dlerror());
        return NULL;
    }

    /* POSIX dlsym supplies a function address as void *. Copy its representation
     * instead of using an object/function pointer cast forbidden by ISO C. */
    void *symbol = dlsym(dl, OA_MODULE_SYMBOL);
    oa_module_entry_fn entry;
    _Static_assert(sizeof entry == sizeof symbol, "dlsym function pointer size");
    memcpy(&entry, &symbol, sizeof entry);
    if (!entry) {
        h->api.log(h, OA_LOG_ERROR, "load '%s': no symbol %s", path, OA_MODULE_SYMBOL);
        dlclose(dl);
        return NULL;
    }
    const oa_module *desc = entry();
    if (!desc || desc->magic != OA_MODULE_MAGIC) {
        h->api.log(h, OA_LOG_ERROR, "load '%s': bad magic", path);
        dlclose(dl);
        return NULL;
    }
    if (desc->abi != OA_ABI_VERSION) {
        h->api.log(h, OA_LOG_ERROR, "load '%s': abi %u != %u", path, desc->abi, OA_ABI_VERSION);
        dlclose(dl);
        return NULL;
    }
    if (desc->size < sizeof(oa_module)) {
        h->api.log(h, OA_LOG_ERROR, "load '%s': descriptor size %u < %zu", path,
                   desc->size, sizeof(oa_module));
        dlclose(dl);
        return NULL;
    }
    if (!desc->name || !desc->init || !desc->start || !desc->stop || !desc->destroy) {
        h->api.log(h, OA_LOG_ERROR, "load '%s': incomplete descriptor", path);
        dlclose(dl);
        return NULL;
    }
    if (oa_host_find_module(h, desc->name)) {
        h->api.log(h, OA_LOG_ERROR, "load '%s': '%s' already loaded", path, desc->name);
        dlclose(dl);
        return NULL;
    }

    memset(slot, 0, sizeof *slot);
    snprintf(slot->name, sizeof slot->name, "%s", desc->name);
    snprintf(slot->path, sizeof slot->path, "%s", path);
    slot->dl = dl;
    slot->desc = desc;
    slot->id = h->next_id + 1;
    h->next_id++;
    if (oa_host_enable_posts(h, slot->id) != 0) {
        h->api.log(h, OA_LOG_ERROR, "load '%s': cannot register post owner", path);
        dlclose(dl);
        memset(slot, 0, sizeof *slot);
        return NULL;
    }

    uint32_t prev = h->cur_module;
    h->cur_module = slot->id;
    int rc = desc->init(&h->api, &slot->ctx);
    h->cur_module = prev;
    if (rc != 0) {
        h->api.log(h, OA_LOG_ERROR, "load '%s': init() failed (%d)", path, rc);
        slot->state = OA_ST_LOADED;
        slot->init_failed = 1;
        h->cur_module = slot->id;
        int busy = desc->quiesce ? desc->quiesce(slot->ctx) : 0;
        h->cur_module = prev;
        if (busy) {
            slot->cleanup_pending = 1;
            h->api.log(h, OA_LOG_ERROR, "%s: init cleanup pending; retry unload", slot->name);
            return NULL;
        }
        oa_host_disable_posts(h, slot->id);
        h->cur_module = slot->id;
        slot->desc->destroy(slot->ctx);
        h->cur_module = prev;
        oa_host_cleanup_owned(h, slot->id);
        dlclose(dl);
        memset(slot, 0, sizeof *slot);
        return NULL;
    }

    slot->state = OA_ST_LOADED;
    h->api.log(h, OA_LOG_INFO, "loaded %s v%s (%s)", slot->name, desc->version ? desc->version : "?", path);
    emit_module_event(h, slot->name, OA_MOD_LOADED_EV);
    return slot;
}

/* Keep service names and stable provider names, not slot indices or reload tokens.
 * Published services take precedence over the cached owner, allowing replacement
 * providers. The cache survives suspend callbacks that release/unpublish services. */
static oa_mod *service_provider(oa_host *h, const oa_phase_dep *dep)
{
    for (int i = 0; i < MAX_SVCS; i++)
        if (h->svcs[i].used && !strcmp(h->svcs[i].name, dep->service))
            return oa_host_find_module_by_id(h, h->svcs[i].owner);
    return *dep->provider ? oa_host_find_module(h, dep->provider) : NULL;
}

static int add_phase_dependency(oa_host *h, oa_mod *m, oa_phase_dep *deps,
                                unsigned *count, const char *service)
{
    for (unsigned i = 0; i < *count; i++)
        if (!strcmp(deps[i].service, service)) return 0;
    if (*count == MAX_PHASE_DEPS) return -1;
    oa_phase_dep *d = &deps[(*count)++];
    snprintf(d->service, sizeof d->service, "%s", service);
    for (unsigned i = 0; i < m->ndeps; i++)
        if (!strcmp(m->deps[i].service, service)) {
            snprintf(d->provider, sizeof d->provider, "%s", m->deps[i].provider);
            break;
        }
    oa_mod *p = service_provider(h, d);
    if (p) snprintf(d->provider, sizeof d->provider, "%s", p->name);
    return 0;
}

static int capture_phase_dependencies(oa_host *h, oa_mod *m)
{
    oa_phase_dep deps[MAX_PHASE_DEPS] = {0};
    unsigned count = 0;
    for (int i = 0; i < OA_REQUIRES_MAX; i++) {
        const char *r = m->desc->requires[i];
        if (!r || !*r) break;
        if (add_phase_dependency(h, m, deps, &count, r) != 0) return -1;
    }
    for (int i = 0; i < MAX_HOLDS; i++)
        if (h->holds[i].used && h->holds[i].holder == m->id &&
            add_phase_dependency(h, m, deps, &count, h->holds[i].name) != 0)
            return -1;
    memcpy(m->deps, deps, sizeof deps);
    m->ndeps = count;
    return 0;
}

static int requirements_ready(oa_host *h, oa_mod *m)
{
    for (int i = 0; i < OA_REQUIRES_MAX; i++) {
        const char *r = m->desc->requires[i];
        if (!r || !*r) break;
        oa_phase_dep d = {0};
        snprintf(d.service, sizeof d.service, "%s", r);
        oa_mod *p = service_provider(h, &d);
        if (!oa_host_service_exists(h, r)) {
            h->api.log(h, OA_LOG_ERROR, "%s: missing required service '%s'", m->name, r);
            return -1;
        }
        if (p && p != m && (p->state != OA_ST_STARTED || p->cleanup_pending)) {
            h->api.log(h, OA_LOG_ERROR, "%s: required service '%s' is not ready", m->name, r);
            return -1;
        }
    }
    return 0;
}

static int resume_module(oa_host *h, oa_mod *m);

static int quiesce_module(oa_host *h, oa_mod *m)
{
    uint32_t prev = h->cur_module;
    h->cur_module = m->id;
    int rc = m->desc->quiesce ? m->desc->quiesce(m->ctx) : 0;
    h->cur_module = prev;
    m->cleanup_pending = rc != 0;
    if (rc) h->api.log(h, OA_LOG_ERROR, "%s: cleanup pending; retry stop/unload", m->name);
    return rc ? -1 : 0;
}

int oa_module_start(oa_host *h, oa_mod *m)
{
    if (h->sleep_cycle || h->shutting_down) return -1;
    if (!m || m->state == OA_ST_EMPTY)
        return -1;
    if (m->cleanup_pending || m->init_failed || m->rejected) return -1;
    if (m->state == OA_ST_STARTED)
        return 0;
    if (m->state == OA_ST_SUSPENDED)
        return resume_module(h, m);

    if (requirements_ready(h, m) != 0) return -1;

    if (oa_host_enable_posts(h, m->id) != 0)
        return -1;
    uint32_t prev = h->cur_module;
    h->cur_module = m->id;
    m->start_attempted = 1;
    int rc = m->desc->start(m->ctx);
    h->cur_module = prev;
    if (rc == 0) rc = capture_phase_dependencies(h, m);
    if (rc != 0) {
        h->api.log(h, OA_LOG_ERROR, "%s: start() failed (%d)", m->name, rc);
        if (quiesce_module(h, m) != 0) return -1;
        oa_host_disable_posts(h, m->id);
        /* a module that opened something and then returned -1 relies on stop() to close it */
        h->cur_module = m->id;
        if (m->desc->stop)
            m->desc->stop(m->ctx);
        h->cur_module = prev;
        oa_host_cleanup_owned(h, m->id);
        m->start_attempted = 0;
        return -1;
    }
    m->state = OA_ST_STARTED;
    h->api.log(h, OA_LOG_INFO, "started %s", m->name);
    emit_module_event(h, m->name, OA_MOD_STARTED_EV);
    return 0;
}

static int resume_module(oa_host *h, oa_mod *m)
{
    if (m->cleanup_pending || m->init_failed || m->rejected) return -1;
    if (requirements_ready(h, m) != 0) return -1;
    for (unsigned i = 0; i < m->ndeps; i++) {
        oa_mod *p = service_provider(h, &m->deps[i]);
        if (p && p != m && p->state != OA_ST_STARTED) {
            h->api.log(h, OA_LOG_ERROR, "%s: provider '%s' has not resumed", m->name, p->name);
            return -1;
        }
    }
    if (m->start_on_resume) {
        /* This instance has never started. Restore must not wake hardware, so
         * activation was deferred rather than running start -> suspend at reload. */
        m->state = OA_ST_LOADED;
        m->start_on_resume = 0;
        int rc = oa_module_start(h, m);
        if (rc != 0) {
            m->state = OA_ST_SUSPENDED; /* keep deferred activation retryable */
            if (!m->cleanup_pending) m->start_on_resume = 1;
        }
        return rc;
    }
    if (!m->desc->resume)
        return -1;
    uint32_t prev = h->cur_module;
    h->cur_module = m->id;
    int rc = m->desc->resume(m->ctx);
    h->cur_module = prev;
    if (rc == 0) rc = capture_phase_dependencies(h, m);
    if (rc == 0)
        m->state = OA_ST_STARTED;
    return rc;
}

static int stop_module(oa_host *h, oa_mod *m, int force)
{
    if (!m || m->state == OA_ST_EMPTY)
        return 0;
    if (m->state == OA_ST_LOADED && !m->cleanup_pending) return 0;
    if (!force) {
        for (int i = 0; i < MAX_MODULES; i++) {
            oa_mod *c = &h->mods[i];
            if (c == m || c->state != OA_ST_STARTED) continue;
            for (int j = 0; j < OA_REQUIRES_MAX; j++) {
                const char *r = c->desc->requires[j];
                if (!r || !*r) break;
                oa_phase_dep d = {0};
                snprintf(d.service, sizeof d.service, "%s", r);
                if (service_provider(h, &d) == m) {
                    h->api.log(h, OA_LOG_ERROR, "%s: required by active module '%s'", m->name, c->name);
                    return -1;
                }
            }
        }
        for (int i = 0; i < MAX_SVCS; i++)
            if (h->svcs[i].used && h->svcs[i].owner == m->id &&
                oa_host_service_held_by_others(h, h->svcs[i].name, m->id)) {
                h->api.log(h, OA_LOG_ERROR, "%s: refusing to stop, service '%s' still held",
                           m->name, h->svcs[i].name);
                return -1;
            }
    }
    if (quiesce_module(h, m) != 0) return -1;
    oa_host_disable_posts(h, m->id);
    uint32_t prev = h->cur_module;
    h->cur_module = m->id;
    /* A deferred replacement has only run init/restore, not start. */
    if (m->start_attempted)
        m->desc->stop(m->ctx);
    h->cur_module = prev;
    /* registrations made by the module belong to this run: drop them, or a stop/start cycle
     * double-registers timers, watches and services */
    oa_host_cleanup_owned(h, m->id);
    m->start_on_resume = 0;
    m->start_attempted = 0;
    m->ndeps = 0;
    m->state = OA_ST_LOADED;
    h->api.log(h, OA_LOG_INFO, "stopped %s", m->name);
    emit_module_event(h, m->name, OA_MOD_STOPPED_EV);
    return 0;
}

int oa_module_stop(oa_host *h, oa_mod *m)
{
    if (h->sleep_cycle) return -1;
    return stop_module(h, m, 0);
}

int oa_module_unload(oa_host *h, const char *name, int force)
{
    if (h->sleep_cycle) return -1;
    oa_mod *m = oa_host_find_module(h, name);
    if (!m)
        return -1;

    if (!force) {
        for (int i = 0; i < MAX_SVCS; i++)
            if (h->svcs[i].used && h->svcs[i].owner == m->id &&
                oa_host_service_held_by_others(h, h->svcs[i].name, m->id)) {
                h->api.log(h, OA_LOG_ERROR, "%s: refusing to unload, service '%s' still held (use --force)",
                           name, h->svcs[i].name);
                return -1;
            }
    } else {
        h->api.log(h, OA_LOG_WARN, "%s: force unload, consumers may hold stale pointers", name);
    }

    if (stop_module(h, m, force) != 0)
        return -1;

    /* init() can own workers even when start() never ran. The gate must be
     * idempotent; a successful stop can therefore be followed by this call. */
    if (quiesce_module(h, m) != 0) return -1;

    oa_host_disable_posts(h, m->id);
    uint32_t prev = h->cur_module;
    h->cur_module = m->id;
    m->desc->destroy(m->ctx);
    h->cur_module = prev;
    m->ctx = NULL;

    oa_host_cleanup_owned(h, m->id);

    char saved[OA_NAME_MAX];
    snprintf(saved, sizeof saved, "%s", m->name);
    if (m->dl)
        dlclose(m->dl);
    free(m->blob);
    memset(m, 0, sizeof *m);
    h->api.log(h, OA_LOG_INFO, "unloaded %s", saved);
    emit_module_event(h, saved, OA_MOD_UNLOADED_EV);
    return 0;
}

int oa_module_reload(oa_host *h, const char *name, int force)
{
    if (h->sleep_cycle || h->shutting_down) return -1;
    oa_mod *m = oa_host_find_module(h, name);
    if (!m)
        return -1;
    /* Retrying reload may drive the existing quiescence gate to completion.
     * It still cannot unload/recreate an instance while that gate is busy. */
    if (m->init_failed || m->rejected) return -1;

    int prior_state = m->state;
    unsigned ndeps = m->ndeps;
    oa_phase_dep deps[MAX_PHASE_DEPS];
    memcpy(deps, m->deps, sizeof deps);
    char path[256], module_name[OA_NAME_MAX];
    snprintf(path, sizeof path, "%s", m->path);
    snprintf(module_name, sizeof module_name, "%s", m->name);

    /* optional state handoff: save, reload, restore */
    unsigned char *blob = NULL;
    size_t bloblen = 0;
    if (m->desc->save) {
        uint32_t prev = h->cur_module;
        h->cur_module = m->id;
        size_t need = m->desc->save(m->ctx, NULL, 0);
        h->cur_module = prev;
        if (need > MAX_STATE) {
            h->api.log(h, OA_LOG_ERROR, "%s: state size %zu exceeds %d; reload canceled",
                       module_name, need, MAX_STATE);
            return -1;
        }
        if (need) {
            blob = malloc(need);
            if (!blob) {
                h->api.log(h, OA_LOG_ERROR, "%s: cannot allocate saved state; reload canceled",
                           module_name);
                return -1;
            }
            prev = h->cur_module;
            h->cur_module = m->id;
            size_t got = m->desc->save(m->ctx, blob, need);
            h->cur_module = prev;
            if (got > need) {
                h->api.log(h, OA_LOG_ERROR, "%s: saved state grew beyond capacity; reload canceled",
                           module_name);
                free(blob);
                return -1;
            }
            bloblen = got;
        }
    }

    if (oa_module_unload(h, name, force) != 0) {
        free(blob);
        return -1;
    }

    oa_mod *n = oa_module_load(h, path);
    if (!n) {
        free(blob);
        return -1;
    }
    if (strcmp(n->name, module_name) != 0) {
        h->api.log(h, OA_LOG_ERROR, "%s: replacement name '%s' differs", module_name, n->name);
        free(blob);
        goto failed_candidate;
    }
    if (bloblen) {
        uint32_t prev = h->cur_module;
        h->cur_module = n->id;
        int rc = n->desc->restore ? n->desc->restore(n->ctx, blob, bloblen) : -1;
        h->cur_module = prev;
        if (rc != 0) {
            h->api.log(h, OA_LOG_ERROR, "%s: restore() failed or absent (%d)", n->name, rc);
            free(blob);
            goto failed_candidate;
        }
        h->api.log(h, OA_LOG_INFO, "%s: restored %zu bytes of state", n->name, bloblen);
    }
    free(blob);

    if (prior_state == OA_ST_SUSPENDED) {
        oa_host_disable_posts(h, n->id);
        memcpy(n->deps, deps, sizeof deps);
        n->ndeps = ndeps;
        n->start_on_resume = 1;
        n->state = OA_ST_SUSPENDED;
    } else if (prior_state == OA_ST_STARTED && oa_module_start(h, n) != 0) {
        goto failed_candidate;
    }
    h->api.log(h, OA_LOG_INFO, "%s: reloaded (%s)", n->name, oa_state_name(n->state));
    return 0;

failed_candidate:
    /* init/restore may have registered work before failing. No replacement
     * callback may survive dlclose, and failed state must never be started. */
    n->rejected = 1; /* a retained rejected replacement cannot activate */
    (void)oa_module_unload(h, n->name, 1);
    return -1;
}

/* Stable topological order over active modules. Missing/unpublished services
 * resolve through their snapshot until the provider republishes after resume. */
static int phase_order(oa_host *h, int order[MAX_MODULES])
{
    unsigned char active[MAX_MODULES] = {0}, placed[MAX_MODULES] = {0};
    int total = 0;
    for (int i = 0; i < MAX_MODULES; i++) {
        oa_mod *m = &h->mods[i];
        if (m->cleanup_pending) return -1;
        if (m->state == OA_ST_STARTED && capture_phase_dependencies(h, m) != 0)
            return -1;
        if (m->state == OA_ST_STARTED || m->state == OA_ST_SUSPENDED) {
            active[i] = 1;
            total++;
        }
    }
    for (int n = 0; n < total; n++) {
        int next = -1;
        for (int i = 0; i < MAX_MODULES; i++) {
            if (!active[i] || placed[i]) continue;
            oa_mod *m = &h->mods[i];
            int blocked = 0;
            for (unsigned j = 0; j < m->ndeps; j++) {
                oa_mod *p = service_provider(h, &m->deps[j]);
                if (p && p != m) {
                    int slot = (int)(p - h->mods);
                    if (active[slot] && !placed[slot]) blocked = 1;
                }
            }
            if (!blocked) { next = i; break; }
        }
        if (next < 0) {
            h->api.log(h, OA_LOG_ERROR, "module dependency cycle; lifecycle transition canceled");
            return -1;
        }
        placed[next] = 1;
        order[n] = next;
    }
    return total;
}

static int suspend_order(oa_host *h, const int *order, int count)
{
    for (int n = count - 1; n >= 0; n--) {
        oa_mod *m = &h->mods[order[n]];
        if (m->state != OA_ST_STARTED || !m->desc->suspend)
            continue;
        /* A consumer without a hook remains active: do not sleep under it. */
        for (int i = 0; i < MAX_MODULES; i++) {
            oa_mod *c = &h->mods[i];
            if (c == m || c->state != OA_ST_STARTED) continue;
            for (unsigned j = 0; j < c->ndeps; j++)
                if (service_provider(h, &c->deps[j]) == m) {
                    h->api.log(h, OA_LOG_ERROR, "%s: consumer '%s' is still active", m->name, c->name);
                    return -1;
                }
        }
        uint32_t prev = h->cur_module;
        h->cur_module = m->id;
        int rc = m->desc->suspend(m->ctx);
        h->cur_module = prev;
        if (rc == 0)
            m->state = OA_ST_SUSPENDED;
        else {
            h->api.log(h, OA_LOG_WARN, "%s: suspend failed (%d)", m->name, rc);
            return -1;
        }
    }
    return 0;
}

int oa_module_suspend_all(oa_host *h)
{
    if (h->skin_timer) return -1; /* finish/cancel the UI transaction before standby */
    if (h->sleep_cycle || h->shutting_down) return -1;
    int order[MAX_MODULES];
    int count = phase_order(h, order);
    return count < 0 ? -1 : suspend_order(h, order, count);
}

int oa_module_resume_all(oa_host *h)
{
    if (h->sleep_cycle || h->shutting_down) return -1;
    int pending = 0;
    for (int i = 0; i < MAX_MODULES; i++)
        if (h->mods[i].state == OA_ST_SUSPENDED) pending = 1;
    if (!pending) return 0;
    int order[MAX_MODULES];
    int count = phase_order(h, order);
    if (count < 0) return -1;
    for (int n = 0; n < count; n++) {
        oa_mod *m = &h->mods[order[n]];
        if (m->state != OA_ST_SUSPENDED)
            continue;
        int rc = resume_module(h, m);
        if (rc != 0) {
            h->api.log(h, OA_LOG_WARN, "%s: resume failed (%d)", m->name, rc);
            return -1;
        }
    }
    return 0;
}

static int sleep_cycle(oa_host *h, uint32_t entry_owner, int (*enter)(void *),
                        void *ud, oa_sleep_result *r, int notify)
{
    if (!r) return -1;
    memset(r, 0, sizeof *r);
    r->prepare_rc = -1;
    if (!enter || h->skin_timer || h->sleep_cycle || h->shutting_down || !h->running || h->stop_requested) return -1;
    oa_mod *owner = entry_owner ? oa_host_find_module_by_id(h, entry_owner) : NULL;
    if (entry_owner && (!owner || owner->state != OA_ST_STARTED)) return -1;
    int order[MAX_MODULES];
    int count = phase_order(h, order);
    if (count < 0) return -1;
    uint32_t active[MAX_MODULES] = {0};
    for (int i = 0; i < count; i++) {
        oa_mod *m = &h->mods[order[i]];
        if (m->state != OA_ST_STARTED) continue;
        /* An absent hook is not proof that workers/devices are quiescent.
         * Refuse before any callback rather than silently skipping owners. */
        if (!m->desc->suspend || !m->desc->resume) {
            h->api.log(h, OA_LOG_ERROR, "%s: sleep cycle requires suspend and resume hooks", m->name);
            return -1;
        }
        active[order[i]] = m->id;
    }
    uint32_t previous = h->cur_module;
    h->sleep_cycle = 1;
    if (notify) {
        oa_power_event ev = { OA_POWER_STANDBY };
        h->api.emit(h, OA_TOPIC_POWER, &ev);
    }
    r->prepare_rc = (!h->running || h->stop_requested) ? -1 : suspend_order(h, order, count);
    if (r->prepare_rc == 0 && (!h->running || h->stop_requested)) r->prepare_rc = -1;
    if (r->prepare_rc == 0) {
        r->entry_attempted = 1;
        h->cur_module = entry_owner;
        r->entry_rc = enter(ud); /* synchronous; returns after wake or entry failure */
        h->cur_module = previous;
    }
    for (int i = 0; i < count; i++) {
        oa_mod *m = &h->mods[order[i]];
        if (active[order[i]] && m->id == active[order[i]] && m->state == OA_ST_SUSPENDED) {
            /* Continue independent restores after a failure. resume_module
             * still refuses consumers whose provider failed to restore. */
            int rc = resume_module(h, m);
            if (rc != 0) {
                r->restore_rc = -1;
                h->api.log(h, OA_LOG_WARN, "%s: sleep-cycle restore failed (%d)", m->name, rc);
            }
        }
    }
    if (notify) {
        /* ON means the cycle's prior state has been restored. A failed restore
         * remains RESUMING; never announce readiness with sleeping consumers.
         * Notifications run while lifecycle mutation is still pinned. */
        oa_power_event ev = { r->restore_rc == 0 ? OA_POWER_ON : OA_POWER_RESUMING };
        h->api.emit(h, OA_TOPIC_POWER, &ev);
    }
    h->cur_module = previous;
    h->sleep_cycle = 0;
    return r->prepare_rc == 0 && r->entry_attempted &&
           r->entry_rc == 0 && r->restore_rc == 0 ? 0 : -1;
}

int oa_module_sleep_cycle(oa_host *h, uint32_t owner, int (*enter)(void *),
                          void *ud, oa_sleep_result *result)
{
    return sleep_cycle(h, owner, enter, ud, result, 0);
}

int oa_module_standby_cycle(oa_host *h, uint32_t owner, int (*enter)(void *),
                            void *ud, oa_sleep_result *result)
{
    return sleep_cycle(h, owner, enter, ud, result, 1);
}
