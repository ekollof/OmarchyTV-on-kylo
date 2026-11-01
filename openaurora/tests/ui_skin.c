/* Skin selection over ui.engine: ui.skin selects the registered skin at
 * runtime; the vendor skin and the OmarchyTV skeleton switch by unloading one
 * and starting the other (no rebuild). Recorder display only. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "host.h"
#include "openaurora/events.h"
#include "openaurora/services.h"
#include "ui_nav_trace.h"

static const char *g_skin = "omarchy";

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "ui.skin")) return g_skin;
    if (!strcmp(key, "ui.ok_file")) return "";
    if (!strcmp(key, "ui.fps")) return "0";
    return def;
}

static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[160];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}

static int has_fill(const oa_nav_trace *trace, int x, int y, int w, int h, int r, int g, int b)
{
    int n = trace->fill_count();
    for (int i = 0; i < n; i++) {
        int fx, fy, fw, fh, fr, fg, fb;
        if (trace->fill_at(i, &fx, &fy, &fw, &fh, &fr, &fg, &fb) == 0 &&
            fx == x && fy == y && fw == w && fh == h &&
            fr == r && fg == g && fb == b)
            return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    oa_host host;
    assert(!oa_host_init(&host, argv[1], NULL));
    host.log_level = OA_LOG_ERROR;
    host.api.config_get = config;
    char out[256];

    assert(!command(&host, "load nav_display", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start nav_display", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_engine", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_engine", out, sizeof out) && !strcmp(out, "ok\n"));

    assert(!command(&host, "load ui_omarchy", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_omarchy", out, sizeof out) && !strcmp(out, "ok\n"));
    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    assert(trace);
    assert(has_fill(trace, 0, 0, 14, 1080, 0, 122, 204));      /* omarchy side bar */
    assert(has_fill(trace, 40, 88, 300, 180, 24, 28, 36));      /* skeleton tile */

    /* Switch to the vendor skin: unload omarchy, select vendor, start it. */
    assert(!command(&host, "unload ui_omarchy", out, sizeof out) && strstr(out, "ok"));
    g_skin = "vendor";
    assert(!command(&host, "load ui_vendor", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_vendor", out, sizeof out) && !strcmp(out, "ok\n"));
    /* The focused rail slot at ui.focus=2 is 150x150; the vendor rail paints
     * RAIL[2] = 190,30,45 and the fallback accent 247,148,30 is not used when
     * the highlight frames are absent. */
    assert(has_fill(trace, 11, 451, 150, 150, 190, 30, 45));

    /* Switch back to the skeleton, proving selection is not compile-time. */
    assert(!command(&host, "unload ui_vendor", out, sizeof out) && strstr(out, "ok"));
    g_skin = "omarchy";
    assert(!command(&host, "load ui_omarchy", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_omarchy", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(has_fill(trace, 0, 0, 14, 1080, 0, 122, 204));

    puts("  ok   ui.skin selects the engine's active skin; vendor and omarchy switch by reload, no rebuild");
    return 0;
}
