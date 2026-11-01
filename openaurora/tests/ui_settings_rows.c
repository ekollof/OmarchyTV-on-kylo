/* Picture-settings row selection over ui.engine: Up/Down moves the FA row
 * highlight (settings_focus_N at 20, row_y), skips the disabled "Beeld zoomen"
 * row 6, clamps at both ends, and Exit returns to the quick tiles. Synthetic
 * 4x4 blobs stand in for the real assets; the recorder checks blit positions. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "host.h"
#include "openaurora/events.h"
#include "openaurora/services.h"
#include "ui_nav_trace.h"

static char g_dir[256];

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "ui.screen")) return "settings_list";
    if (!strcmp(key, "ui.dir")) return g_dir;
    if (!strcmp(key, "ui.ok_file")) return "";
    if (!strcmp(key, "ui.fps")) return "0";
    if (!strcmp(key, "ui.rail_hl")) return "0";
    return def;
}

static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[160];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}

static void blob(const char *name)
{
    char path[320];
    snprintf(path, sizeof path, "%s/%s.rgba", g_dir, name);
    FILE *f = fopen(path, "wb");
    assert(f);
    unsigned char hdr[12] = { 'O', 'A', 'I', '1', 4, 0, 0, 0, 4, 0, 0, 0 };
    assert(fwrite(hdr, 1, sizeof hdr, f) == sizeof hdr);
    for (int i = 0; i < 4 * 4; i++) {
        unsigned char px[4] = { 10, 20, 30, 255 };
        assert(fwrite(px, 1, sizeof px, f) == sizeof px);
    }
    assert(fclose(f) == 0);
}

static int blit_at(const oa_nav_trace *trace, int x, int y)
{
    int n = trace->blit_count();
    for (int i = 0; i < n; i++) {
        int bx, by, bw, bh;
        if (trace->blit_at(i, &bx, &by, &bw, &bh) == 0 && bx == x && by == y && bw == 4 && bh == 4)
            return 1;
    }
    return 0;
}

static int last_blit_at(const oa_nav_trace *trace, int x, int y)
{
    int n = trace->blit_count();
    if (n <= 0) return 0;
    int bx, by, bw, bh;
    return trace->blit_at(n - 1, &bx, &by, &bw, &bh) == 0 &&
           bx == x && by == y && bw == 4 && bh == 4;
}

static void key(oa_host *host, int id)
{
    oa_key_event ev = { .id = (uint8_t)id, .kind = OA_KEY_PRESS };
    host->api.emit(host, OA_TOPIC_KEY, &ev);
    oa_host_dispatch_posts(host);
}

static int row_y(int sel) { return 183 + 70 * sel; }

int main(int argc, char **argv)
{
    assert(argc == 3);
    snprintf(g_dir, sizeof g_dir, "%s", argv[2]);
    blob("settings_panel");
    for (int i = 0; i < 9; i++) {
        char name[32];
        snprintf(name, sizeof name, "settings_focus_%d", i);
        blob(name);
    }

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
    assert(trace && portal);
    assert(portal->screen() == 4);
    assert(blit_at(trace, 0, 0) && last_blit_at(trace, 0, 0)); /* panel, no row */
    assert(!blit_at(trace, 20, row_y(0))); /* no row focused at start */

    key(&host, 19);                               /* Down: first row */
    assert(last_blit_at(trace, 20, row_y(0)));
    key(&host, 19);
    assert(last_blit_at(trace, 20, row_y(1)));
    key(&host, 19);
    key(&host, 19);
    key(&host, 19);
    assert(last_blit_at(trace, 20, row_y(4)));
    key(&host, 19);
    assert(last_blit_at(trace, 20, row_y(5)));
    key(&host, 19);                               /* skip disabled row 6 */
    assert(last_blit_at(trace, 20, row_y(7)));
    assert(!last_blit_at(trace, 20, row_y(6)));
    key(&host, 19);
    assert(last_blit_at(trace, 20, row_y(8)));
    key(&host, 19);                               /* clamp */
    assert(last_blit_at(trace, 20, row_y(8)));

    key(&host, 20);                               /* Up */
    assert(last_blit_at(trace, 20, row_y(7)));
    key(&host, 20);
    assert(last_blit_at(trace, 20, row_y(5)));
    for (int i = 0; i < 5; i++) key(&host, 20);
    assert(last_blit_at(trace, 20, row_y(0)));
    key(&host, 20);                               /* clamp at the top */
    assert(last_blit_at(trace, 20, row_y(0)));

    key(&host, 10);                               /* Exit -> quick tiles */
    assert(portal->screen() == 3);

    puts("  ok   Up/Down selects settings rows at the 70 px pitch, skips row 6 and clamps; Exit returns");
    return 0;
}
