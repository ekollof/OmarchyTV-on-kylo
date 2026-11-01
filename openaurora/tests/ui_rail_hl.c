/* Main-launcher focus highlight: the vendor APNG (11 frames, 30 ms, loop) is drawn at the
 * recovered 160x160 button window, which is (focused_x - 5, focused_y + 10) relative to the
 * 150x150 focused tile. This drives the shipped mod_ui_fb with the recording display. */
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "host.h"
#include "openaurora/services.h"
#include "ui_nav_trace.h"

static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[160];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}

static int timer_count(const oa_host *h, uint32_t owner, uint32_t repeat_ms)
{
    int n = 0;
    for (int i = 0; i < MAX_TIMERS; i++)
        if (h->timers[i].active && h->timers[i].owner == owner &&
            h->timers[i].repeat_ms == repeat_ms)
            n++;
    return n;
}

static int timer_fire(oa_host *h, uint32_t owner, uint32_t repeat_ms)
{
    for (int i = 0; i < MAX_TIMERS; i++) {
        oa_timer *t = &h->timers[i];
        if (!t->active || t->owner != owner || t->repeat_ms != repeat_ms)
            continue;
        void (*cb)(void *) = t->cb;
        void *ud = t->ud;
        uint32_t prev = h->cur_module;
        h->cur_module = t->owner;
        cb(ud);
        h->cur_module = prev;
        return 1;
    }
    return 0;
}

/* The rail highlight is the 160x160 scaled blit; returns its frame index (red channel - 1). */
static int hl_find(const oa_nav_trace *t, int *x, int *y, int *w, int *h, int *sw, int *sh)
{
    int n = t->scaled_count();
    for (int i = 0; i < n; i++) {
        int rx = 0, ry = 0, rw = 0, rh = 0, rsw = 0, rsh = 0, r = -1, g = 0, b = 0, a = 0;
        if (t->scaled_at(i, &rx, &ry, &rw, &rh, &rsw, &rsh) != 0) continue;
        if (rw != 160 || rh != 160) continue;
        if (t->scaled_first(i, &r, &g, &b, &a) != 0) continue;
        if (x) *x = rx;
        if (y) *y = ry;
        if (w) *w = rw;
        if (h) *h = rh;
        if (sw) *sw = rsw;
        if (sh) *sh = rsh;
        return r - 1;
    }
    return -999;
}

int main(int argc, char **argv)
{
    assert(argc == 4);   /* module dir, config file, ui asset dir */
    oa_host host;
    char out[512];

    /* A complete normal bundle must not crowd the last highlight frames out of
     * the icon cache. The old test loaded only highlights and missed this. */
    const char *base[] = {"title", "rail1", "rail2", "rail3", "rail4", "rail5",
        "card_netflix", "card_prime", "card_youtube", "card_tabii", "card_canaldigitaal",
        "card_tvvlaanderen", "card_telesat", "card_digiturk", "card_gain"};
    for (unsigned i = 0; i < sizeof base/sizeof base[0]; i++) {
        char path[512];
        snprintf(path, sizeof path, "%s/%s.rgba", argv[3], base[i]);
        FILE *fp = fopen(path, "wb");
        const unsigned char blob[] = {'O','A','I','1',1,0,0,0,1,0,0,0,255,255,255,255};
        assert(fp && fwrite(blob, 1, sizeof blob, fp) == sizeof blob);
        assert(!fclose(fp));
    }

    /* 11 frames, 210x210, red channel = frame number. */
    for (int f = 1; f <= 11; f++) {
        char path[512];
        snprintf(path, sizeof path, "%s/rail_hl%02d.rgba", argv[3], f);
        FILE *fp = fopen(path, "wb");
        assert(fp);
        unsigned char hdr[12] = { 'O', 'A', 'I', '1' };
        unsigned w = 210, h = 210;
        memcpy(hdr + 4, &w, 4);
        memcpy(hdr + 8, &h, 4);
        assert(fwrite(hdr, 1, sizeof hdr, fp) == sizeof hdr);
        for (unsigned p = 0; p < w * h; p++) {
            unsigned char px[4] = { (unsigned char)f, 0, 0, 255 };
            assert(fwrite(px, 1, sizeof px, fp) == sizeof px);
        }
        assert(fclose(fp) == 0);
    }

    assert(!oa_host_init(&host, argv[1], argv[2]));
    host.log_level = OA_LOG_ERROR;
    assert(!command(&host, "load nav_display", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start nav_display", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_fb", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_fb", out, sizeof out) && !strcmp(out, "ok\n"));

    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    oa_mod *fb = oa_host_find_module(&host, "ui_fb");
    assert(trace && fb);

    /* ui.focus defaults to 2; the focused 150x150 tile is at (11,451) with this layout. */
    int x = 0, y = 0, w = 0, h = 0, sw = 0, sh = 0;
    assert(hl_find(trace, &x, &y, &w, &h, &sw, &sh) == 0);   /* frame 1 */
    assert(x == 6 && y == 461);                              /* 160x160 button window */
    assert(w == 160 && h == 160 && sw == 210 && sh == 210);

    assert(timer_count(&host, fb->id, 30) == 1);             /* APNG delay */
    assert(timer_fire(&host, fb->id, 30));
    assert(hl_find(trace, NULL, NULL, NULL, NULL, NULL, NULL) == 1);  /* frame 2 */

    for (int i = 2; i < 11; i++) {
        assert(timer_fire(&host, fb->id, 30));
        assert(hl_find(trace, NULL, NULL, NULL, NULL, NULL, NULL) == i);
    }
    assert(timer_fire(&host, fb->id, 30));
    assert(hl_find(trace, NULL, NULL, NULL, NULL, NULL, NULL) == 0);  /* 11-frame wrap */

    host.stop_requested = 1;
    assert(oa_host_run(&host) == 0);
    close(host.selfpipe[0]); close(host.selfpipe[1]); pthread_mutex_destroy(&host.post_mu);
    printf("  ok   rail highlight plays 11 frames at 30 ms in the vendor button window\n");
    return 0;
}
