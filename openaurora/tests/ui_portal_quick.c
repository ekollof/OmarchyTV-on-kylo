/* Real-asset capture of the Startpagina portal (screen 0). Uses the workbench
 * title/rail/card assets and the recorder display; no TV, fb1 or GPU. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "openaurora/services.h"
#include "ui_nav_trace.h"

static char g_dir[512];

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "ui.dir")) return g_dir;
    if (!strcmp(key, "ui.ok_file")) return "";
    if (!strcmp(key, "ui.fps")) return "0";
    if (!strcmp(key, "ui.focus")) return "2";
    if (!strcmp(key, "ui.live_cards")) return "0";
    if (!strcmp(key, "ui.rail_hl")) return "0";
    return def;
}

static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[160];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}

static int blit_at(const oa_nav_trace *trace, int x, int y)
{
    int n = trace->blit_count();
    for (int i = 0; i < n; i++) {
        int bx, by, bw, bh;
        if (trace->blit_at(i, &bx, &by, &bw, &bh) == 0 &&
            bx == x && by == y && bw > 0 && bh > 0)
            return 1;
    }
    return 0;
}

static int fill_at(const oa_nav_trace *trace, int x, int y)
{
    int n = trace->fill_count();
    for (int i = 0; i < n; i++) {
        int bx, by, bw, bh, r, g, b;
        if (trace->fill_at(i, &bx, &by, &bw, &bh, &r, &g, &b) == 0 &&
            bx == x && by == y && bw > 0 && bh > 0)
            return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    snprintf(g_dir, sizeof g_dir, "%s", argv[2]);
    oa_host host;
    assert(!oa_host_init(&host, argv[1], NULL));
    host.log_level = OA_LOG_ERROR;
    host.api.config_get = config;
    char out[256];

    assert(!command(&host, "load nav_display", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start nav_display", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_engine", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_engine", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_vendor", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_vendor", out, sizeof out) && !strcmp(out, "ok\n"));

    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    const oa_portal_iface *portal = host.api.lookup(&host, OA_SVC_PORTAL, 1);
    assert(trace && portal && portal->screen() == 0);
    assert(blit_at(trace, 29, 80));   /* title */
    assert(fill_at(trace, 24, 211));  /* first rail slot */
    const char *cap = getenv("OA_NAV_CAPTURE");
    if (cap) {
        char path[512];
        snprintf(path, sizeof path, "%s.portal", cap);
        assert(!trace->capture(path));
    }
    puts("ok Startpagina real-asset capture: title at 29,80 and rail at 0,211");
    return 0;
}
