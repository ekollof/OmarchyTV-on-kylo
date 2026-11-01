/* OpenAurora policy, not a vendor screen effect: choose a reloadable skin and
 * fall back to ui_vendor only after the failed candidate has safely unloaded. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "openaurora/ui_engine.h"

enum { IDLE, REMOVE, LOAD, WAIT, CLEAN, DONE, FAILED };
static int snapshot(oa_host *h, oa_ui_skin_status *s)
{
    for (int i = 0; i < MAX_SVCS; i++) if (h->svcs[i].used && !strcmp(h->svcs[i].name, OA_SVC_UI_SKIN)) {
        oa_mod *m = oa_host_find_module_by_id(h, h->svcs[i].owner);
        if (!m || m->state != OA_ST_STARTED || m->cleanup_pending) break;
        const oa_ui_skin_status_iface *iface = h->svcs[i].iface;
        return iface->get(s);
    }
    errno = ENODEV; return -1;
}
static void finish(oa_host *h, int phase)
{
    h->skin_phase = phase;
    if (h->skin_timer) h->api.timer_del(h, h->skin_timer);
    h->skin_timer = 0;
}
static void candidate(oa_host *h, const char *name)
{
    snprintf(h->skin_selected, sizeof h->skin_selected, "%s", name);
    snprintf(h->skin_module, sizeof h->skin_module, "ui_%.*s", OA_NAME_MAX-4, name);
    h->skin_owner = 0;
    h->skin_phase = LOAD;
}
static void tick(void *ud)
{
    oa_host *h = ud;
    if (h->shutting_down || h->stop_requested) { h->skin_error = ECANCELED; finish(h, FAILED); return; }
    if (h->sleep_cycle) return;
    if (h->skin_phase == REMOVE || h->skin_phase == CLEAN) {
        oa_mod *m = oa_host_find_module_by_id(h, h->skin_owner);
        if (m && oa_module_unload(h, m->name, 0)) {
            /* Never overlap a fallback with pending device/code cleanup. */
            return;
        }
        if (h->skin_phase == REMOVE) candidate(h, h->skin_requested);
        else if (!strcmp(h->skin_selected, "vendor")) { finish(h, FAILED); return; }
        else { h->skin_fallback = 1; candidate(h, "vendor"); }
    }
    if (h->skin_phase == LOAD) {
        oa_mod *m = oa_host_find_module(h, h->skin_module);
        if (!m) {
            uint32_t ids[MAX_MODULES];
            for (int i = 0; i < MAX_MODULES; i++) ids[i] = h->mods[i].id;
            m = oa_module_load(h, h->skin_module);
            /* Failed init can return NULL while retaining a busy DSO. It is
             * still our candidate and must drain before any fallback starts. */
            if (!m) for (int i = 0; i < MAX_MODULES; i++)
                if (h->mods[i].id != ids[i] && h->mods[i].cleanup_pending) { m = &h->mods[i]; break; }
        }
        h->skin_owner = m ? m->id : 0;
        if (!m || oa_module_start(h, m)) {
            if (!h->skin_error) h->skin_error = EIO;
            h->skin_phase = CLEAN;
            return;
        }
        unsigned timeout = 5000;
        const char *text = h->api.config_get(h, "ui.skin_timeout_ms", "5000");
        char *end; errno = 0; unsigned long parsed = strtoul(text, &end, 10);
        if (!errno && text[0] && !*end && parsed >= 100 && parsed <= 60000) timeout = (unsigned)parsed;
        h->skin_deadline = oa_now_ms()+timeout;
        h->skin_phase = WAIT;
    }
    if (h->skin_phase == WAIT) {
        oa_ui_skin_status s;
        int rc = snapshot(h, &s);
        if (!rc && s.owner == h->skin_owner && !strcmp(s.name, h->skin_selected) && !s.error && s.entered && s.ready) {
            finish(h, DONE); return;
        }
        if (rc || s.error || oa_now_ms() >= h->skin_deadline) {
            if (!h->skin_error) h->skin_error = rc ? ENODEV : s.error ? s.error : ETIMEDOUT;
            h->skin_phase = CLEAN;
        }
    }
}
int oa_skin_request(oa_host *h, const char *name)
{
    if (!h || !name || !*name || strlen(name) > OA_NAME_MAX-4) { errno = EINVAL; return -1; }
    for (const char *p = name; *p; p++) if (!((*p >= 'a' && *p <= 'z') ||
        (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) { errno = EINVAL; return -1; }
    if (h->cur_module || h->sleep_cycle || h->shutting_down || h->stop_requested) { errno = EBUSY; return -1; }
    if (h->skin_timer) { errno = EBUSY; return -1; }
    oa_ui_skin_status s;
    if (snapshot(h, &s)) return -1;
    uint64_t timer = h->api.timer_add(h, 0, 10, tick, h);
    if (!timer) { errno = ENOSPC; return -1; }
    h->skin_timer = timer; h->skin_error = h->skin_fallback = 0;
    snprintf(h->skin_requested, sizeof h->skin_requested, "%s", name);
    if (s.owner && strcmp(s.name, name)) { h->skin_owner = s.owner; h->skin_phase = REMOVE; }
    else candidate(h, name);
    return 0;
}
void oa_skin_status(oa_host *h, char *out, size_t cap)
{
    oa_ui_skin_status s = {0}; (void)snapshot(h, &s);
    const char *phases[] = {"idle", "removing", "loading", "waiting", "cleanup", "ready", "failed"};
    snprintf(out, cap, "requested %s active %s state %s fallback %d error %d\n",
        h->skin_requested[0] ? h->skin_requested : "-", s.name[0] ? s.name : "-",
        phases[h->skin_phase], h->skin_fallback, h->skin_error);
}
int oa_skin_bootstrap(oa_host *h)
{
    oa_ui_skin_status s;
    if (snapshot(h, &s)) return 0; /* no UI engine in this process */
    const char *name = h->api.config_get(h, "ui.skin", "");
    name = *name ? name : s.name[0] ? s.name : "vendor";
    int rc = oa_skin_request(h, name);
    if (rc && errno == EINVAL) {
        char requested[OA_NAME_MAX]; snprintf(requested, sizeof requested, "%s", name);
        rc = oa_skin_request(h, "vendor");
        if (!rc) {
            snprintf(h->skin_requested, sizeof h->skin_requested, "%s", requested);
            h->skin_error = EINVAL; h->skin_fallback = 1;
        }
    }
    return rc;
}
