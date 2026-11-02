/* Regression for the 2026-11-02 adversarial review: ui_vendor m_start must
 * fail (not warn-and-continue) when a lifecycle subscription is refused.
 * Otherwise a later forced ui_theme unload leaves a stale c->theme that
 * dialog_fs_type would call after dlclose. Host-only; real DSOs, no TV. */
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "host.h"
#include "openaurora/events.h"
#include "openaurora/services.h"

static void fill_cb(const void *ev, void *ud)
{
    (void)ev;
    (void)ud;
}

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "ui.paint") || !strcmp(key, "ui.rail_hl"))
        return "0";
    if (!strcmp(key, "ui.dir"))
        return "/nonexistent/oa-vendor-subs-assets";
    return def;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    oa_host host;
    assert(!oa_host_init(&host, argv[1], NULL));
    host.log_level = OA_LOG_ERROR;
    host.api.config_get = config;

    char line[160], out[256];
    snprintf(line, sizeof line, "load nav_display");
    assert(!oa_control_command(&host, line, out, sizeof out) && strstr(out, "ok"));
    snprintf(line, sizeof line, "start nav_display");
    assert(!oa_control_command(&host, line, out, sizeof out));
    snprintf(line, sizeof line, "load ui_engine");
    assert(!oa_control_command(&host, line, out, sizeof out) && strstr(out, "ok"));
    snprintf(line, sizeof line, "start ui_engine");
    assert(!oa_control_command(&host, line, out, sizeof out));
    snprintf(line, sizeof line, "load ui_vendor");
    assert(!oa_control_command(&host, line, out, sizeof out) && strstr(out, "ok"));
    oa_mod *vendor = oa_host_find_module(&host, "ui_vendor");
    assert(vendor);

    /* Exhaust the subscription table from outside any module. */
    uint32_t fill[MAX_SUBS];
    int nfill = 0;
    uint32_t id;
    while (nfill < MAX_SUBS && (id = host.api.subscribe(&host, "fill", fill_cb, NULL)) != 0)
        fill[nfill++] = id;
    assert(nfill > 0); /* table actually filled, not already full */

    /* Start must fail instead of continuing with a missing removal subscription. */
    assert(oa_module_start(&host, vendor) != 0);
    assert(vendor->state != OA_ST_STARTED);
    for (int i = 0; i < MAX_SUBS; i++)
        assert(!host.subs[i].active || host.subs[i].owner != vendor->id);
    for (int i = 0; i < MAX_HOLDS; i++)
        assert(!host.holds[i].used || host.holds[i].holder != vendor->id);

    /* One slot free: module/changed succeeds but service/removed fails.
     * Failed-start cleanup must remove that partial registration as well. */
    host.api.unsubscribe(&host, fill[0]);
    fill[0] = 0;
    assert(oa_module_start(&host, vendor) != 0);
    assert(vendor->state != OA_ST_STARTED);
    for (int i = 0; i < MAX_SUBS; i++)
        assert(!host.subs[i].active || host.subs[i].owner != vendor->id);
    for (int i = 0; i < MAX_HOLDS; i++)
        assert(!host.holds[i].used || host.holds[i].holder != vendor->id);

    for (int i = 0; i < nfill; i++)
        if (fill[i]) host.api.unsubscribe(&host, fill[i]);

    /* With table space back, start succeeds and cleans up normally. */
    assert(!oa_module_start(&host, vendor));
    assert(!oa_module_unload(&host, "ui_vendor", 0));
    assert(!oa_module_unload(&host, "ui_engine", 0));
    assert(!oa_module_unload(&host, "nav_display", 0));
    close(host.selfpipe[0]);
    close(host.selfpipe[1]);
    pthread_mutex_destroy(&host.post_mu);
    puts("  ok   full/one-slot subscription table fails vendor start with no leaked holds/subs; retry starts");
    return 0;
}
