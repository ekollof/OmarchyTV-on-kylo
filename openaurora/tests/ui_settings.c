/* Captured picture-settings state over ui.engine: ui.screen=settings paints the
 * three vendor bands; Exit returns to the portal. Synthetic 4x4 blobs stand in
 * for the real bands; the recorder checks the blit positions, not pixels. */
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
    if (!strcmp(key, "ui.screen")) return "settings";
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

int main(int argc, char **argv)
{
    assert(argc == 3);
    snprintf(g_dir, sizeof g_dir, "%s", argv[2]);
    blob("settings_top");
    blob("settings_cat");
    blob("settings_rail");

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
    assert(portal->screen() == 3);
    assert(blit_at(trace, 0, 240));
    assert(blit_at(trace, 0, 440));
    assert(blit_at(trace, 0, 180));

    oa_key_event key = { .id = 10, .kind = OA_KEY_PRESS };
    host.api.emit(&host, OA_TOPIC_KEY, &key);
    oa_host_dispatch_posts(&host);
    assert(portal->screen() == 0);

    /* Real entry path: two Downs reach rail 4 (the wrench), Select shows the
     * captured state. Focus starts at ui.focus default 2. */
    for (int i = 0; i < 2; i++) {
        oa_key_event down = { .id = 19, .kind = OA_KEY_PRESS };
        host.api.emit(&host, OA_TOPIC_KEY, &down);
        oa_host_dispatch_posts(&host);
    }
    key.id = 53;
    host.api.emit(&host, OA_TOPIC_KEY, &key);
    oa_host_dispatch_posts(&host);
    assert(portal->screen() == 3);
    assert(blit_at(trace, 0, 240) && blit_at(trace, 0, 440) && blit_at(trace, 0, 180));

    puts("  ok   captured picture-settings state paints rail/top/category bands; rail-4 Select enters, Exit returns");
    return 0;
}
