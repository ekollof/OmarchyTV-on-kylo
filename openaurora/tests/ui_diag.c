/* Diagnostic skin over ui.engine: service presence rows, selection and reload.
 * Recorder display only; no TV/HAL/device access. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "host.h"
#include "openaurora/events.h"
#include "openaurora/services.h"
#include "openaurora/ui_engine.h"
#include "ui_nav_trace.h"

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "ui.skin")) return "diag";
    if (!strcmp(key, "ui.paint")) return "1";
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

static int fill_at(const oa_nav_trace *trace, int x, int y, int w, int h,
                   int r, int g, int b)
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

static void key(oa_host *host, int id)
{
    oa_key_event ev = { .id = (uint8_t)id, .kind = OA_KEY_PRESS };
    host->api.emit(host, OA_TOPIC_KEY, &ev);
    oa_host_dispatch_posts(host);
}

static void status(oa_host *host, oa_ui_skin_status *out)
{
    const oa_ui_skin_status_iface *iface =
        host->api.lookup(host, OA_SVC_UI_SKIN, 1);
    assert(iface && !iface->get(out));
    host->api.release(host, OA_SVC_UI_SKIN);
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
    assert(!command(&host, "load ui_diag", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_diag", out, sizeof out) && !strcmp(out, "ok\n"));
    oa_host_dispatch_posts(&host);
    oa_ui_skin_status st;
    status(&host, &st);
    assert(!strcmp(st.name, "diag") && st.entered && st.ready && !st.error);

    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    assert(trace);
    assert(fill_at(trace, 0, 0, 1920, 120, 0, 110, 190));
    assert(fill_at(trace, 48, 150, 1824, 58, 24, 86, 58)); /* ui.engine */
    assert(fill_at(trace, 48, 216, 1824, 58, 24, 86, 58)); /* display */
    assert(fill_at(trace, 48, 282, 1824, 58, 88, 30, 44)); /* settings absent */
    assert(fill_at(trace, 40, 146, 1840, 66, 247, 148, 30));

    key(&host, 19); /* Down */
    assert(fill_at(trace, 40, 212, 1840, 66, 247, 148, 30));
    key(&host, 20); /* Up */
    key(&host, 53); /* Select refresh */
    assert(!command(&host, "reload ui_diag", out, sizeof out));
    oa_host_dispatch_posts(&host);
    status(&host, &st);
    assert(!strcmp(st.name, "diag") && st.entered && st.ready && !st.error);
    assert(!command(&host, "stop ui_diag", out, sizeof out));
    assert(!command(&host, "unload ui_diag", out, sizeof out));
    assert(command(&host, "start ui_diag", out, sizeof out) != 0);
    puts("ok ui_diag: service rows, selection, refresh, reload and teardown");
    return 0;
}
