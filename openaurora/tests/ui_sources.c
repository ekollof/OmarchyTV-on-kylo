/* Source-selection screen: rail Select opens DialogSourceSelection's panel,
 * Up/Down moves the row bar, Exit returns. Uses the shipped mod_ui_vendor and the
 * recorder display; no TV, no assets (the fills are checked, not the text). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "host.h"
#include "openaurora/events.h"
#include "openaurora/services.h"
#include "ui_nav_trace.h"

static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[160];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}

static void frame(uint8_t out[9], uint8_t a1, uint8_t cmd, int good)
{
    out[0] = 0xff;
    out[1] = 0xff;
    out[2] = 0x50;
    out[3] = a1;
    out[4] = 0x11;
    out[5] = cmd;
    out[6] = good ? (uint8_t)~cmd : 0;
    out[7] = 0;
    out[8] = 0;
}

static oa_host *g_press_host;

static void press(const oa_input_iface *input, uint8_t a1)
{
    uint8_t bytes[9];
    frame(bytes, a1, 0x00, 1);
    assert(!input->feed(bytes, 9));
    if (g_press_host) oa_host_dispatch_posts(g_press_host);
}

static int has_panel(const oa_nav_trace *trace)
{
    int n = trace->fill_count();
    for (int i = 0; i < n; i++) {
        int x, y, w, h, r, g, b;
        if (trace->fill_at(i, &x, &y, &w, &h, &r, &g, &b) == 0 &&
            x == 0 && y == 0 && w == 780 && h == 1080 &&
            r == 190 && g == 30 && b == 45)
            return 1;
    }
    return 0;
}

static void read_px(FILE *f, int x, int y, unsigned char px[4])
{
    assert(!fseek(f, ((long)y*1920+x)*4, SEEK_SET));
    assert(fread(px, 1, 4, f) == 4);
}
static void check_alpha_capture(const oa_nav_trace *trace)
{
    char path[] = "/tmp/oa-src-alpha.XXXXXX";
    unsigned char px[4];
    FILE *f;
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    assert(!trace->capture(path));
    f = fopen(path, "rb");
    assert(f);
    /* The 780x1080 panel is opaque; the rest of the OSD stays transparent. */
    read_px(f, 1000, 500, px);
    assert(px[0] == 0 && px[1] == 0 && px[2] == 0 && px[3] == 0);
    read_px(f, 100, 500, px);
    assert(px[0] == 190 && px[1] == 30 && px[2] == 45 && px[3] == 255);
    assert(!fclose(f));
    assert(!unlink(path));
}
static void check_alpha_composition(const oa_display_iface *disp,
    const oa_display_alpha_iface *alpha, const oa_nav_trace *trace)
{
    static const unsigned char white_half[4] = {255, 255, 255, 128};
    static const unsigned char green_half[4] = {0, 255, 0, 128};
    char path[] = "/tmp/oa-alpha-mix.XXXXXX";
    unsigned char px[4];
    FILE *f;
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    assert(alpha->clear_rgba(0, 0, 0, 256) < 0);
    assert(!alpha->clear_rgba(0, 0, 0, 0));
    assert(!disp->fill(1000, 500, 1, 1, 0, 0, 255));
    assert(!disp->fill(1001, 500, 1, 1, 0, 0, 255));
    assert(!disp->blit(1001, 500, 1, 1, white_half));
    assert(!disp->blit(1002, 500, 1, 1, green_half));
    assert(!trace->capture(path));
    f = fopen(path, "rb");
    assert(f);
    read_px(f, 1000, 500, px);
    assert(px[0] == 0 && px[1] == 0 && px[2] == 255 && px[3] == 255);
    /* Straight source-over: 50% white over opaque blue, 50% green over
     * transparent keeps its colour and half alpha. */
    read_px(f, 1001, 500, px);
    if (!(px[0] == 128 && px[1] == 128 && px[2] == 255 && px[3] == 255)) {
        fprintf(stderr, "white-over-blue pixel %u,%u,%u,%u\n", px[0], px[1], px[2], px[3]);
        assert(0);
    }
    read_px(f, 1002, 500, px);
    assert(px[0] == 0 && px[1] == 255 && px[2] == 0 && px[3] == 128);
    read_px(f, 1003, 500, px);
    assert(px[0] == 0 && px[1] == 0 && px[2] == 0 && px[3] == 0);
    assert(!fclose(f));
    assert(!unlink(path));
}
static int has_bar(const oa_nav_trace *trace, int y)
{
    int n = trace->fill_count();
    for (int i = 0; i < n; i++) {
        int x, yy, w, h, r, g, b;
        if (trace->fill_at(i, &x, &yy, &w, &h, &r, &g, &b) == 0 &&
            x == 18 && yy == y && w == 748 && h == 66 &&
            r == 254 && g == 148 && b == 25)
            return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    oa_host host;
    assert(!oa_host_init(&host, argv[1], argv[2]));
    g_press_host = &host;
    char out[256];
    assert(!command(&host, "load plat", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start plat", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load nav_display", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start nav_display", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ir", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ir", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ticker", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ticker", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_clock", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_clock", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_anim", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_anim", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load settings", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start settings", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_engine", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_engine", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_vendor", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_vendor", out, sizeof out) && !strcmp(out, "ok\n"));

    const oa_input_iface *input = host.api.lookup(&host, OA_SVC_INPUT, 1);
    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    const oa_portal_iface *portal = host.api.lookup(&host, OA_SVC_PORTAL, 1);
    const oa_display_iface *disp = host.api.lookup(&host, OA_SVC_DISPLAY, 1);
    const oa_display_alpha_iface *alpha = host.api.lookup(&host, OA_SVC_DISPLAY_ALPHA, 1);
    assert(input && trace && portal && disp && alpha && !input->device_open());
    assert(portal->screen() == 0);

    press(input, 20); /* Up from the configured home rail (2) to rail 1, sources */
    press(input, 53); /* Select */
    assert(portal->screen() == 2);
    assert(has_panel(trace));
    assert(has_bar(trace, 152));
    check_alpha_capture(trace);
    check_alpha_composition(disp, alpha, trace);

    press(input, 19); /* Down */
    assert(has_bar(trace, 227));
    press(input, 20); /* Up */
    assert(has_bar(trace, 152));

    press(input, 10); /* Exit */
    assert(portal->screen() == 0);

    puts("  ok   source selection: rail Select opens the 780x1080 panel, Up/Down moves the 748x66 bar, Exit returns");
    return 0;
}
