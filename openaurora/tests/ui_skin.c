/* Skin selection over ui.engine: ui.skin selects the registered skin at
 * runtime; the vendor skin and the OmarchyTV skeleton switch by unloading one
 * and starting the other (no rebuild). Recorder display only. */
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "host.h"
#include "openaurora/events.h"
#include "openaurora/services.h"
#include "ui_nav_trace.h"
#include "openaurora/ui_engine.h"

static const char *g_skin = "omarchy";
static char asset_dir[] = "/tmp/oa-ui-review-XXXXXX";
static int use_fps;

static void pump(oa_host *h)
{
    oa_host_dispatch_posts(h);
    for (int i = 0; i < MAX_TIMERS; i++) if (h->timers[i].active && !h->timers[i].repeat_ms) {
        oa_timer t = h->timers[i]; h->timers[i].active = 0;
        h->cur_module = t.owner; t.cb(t.ud); h->cur_module = 0;
    }
    oa_host_dispatch_posts(h);
}
static void queue_key_id(oa_host *h, int id)
{ oa_key_event e = {.id = (uint8_t)id, .kind = OA_KEY_PRESS}; h->api.emit(h, OA_TOPIC_KEY, &e); }
static void queue_key(oa_host *h) { queue_key_id(h, 19); }
static uint64_t deny_timer(void *p, uint32_t delay, uint32_t repeat, void (*cb)(void *), void *ud)
{ (void)p; (void)delay; (void)repeat; (void)cb; (void)ud; return 0; }

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "ui.skin")) return g_skin;
    if (!strcmp(key, "ui.ok_file")) return "";
    if (!strcmp(key, "ui.fps")) return use_fps ? "20" : "0";
    if (!strcmp(key, "ui.dir")) return asset_dir;
    if (!strcmp(key, "ui.omarchy.wallpaper")) return "wallpaper";
    if (!strcmp(key, "ui.font_index") && getenv("OA_UI_FONT_INDEX"))
        return getenv("OA_UI_FONT_INDEX");
    if (!strcmp(key, "ui.font_pack") && getenv("OA_UI_FONT_PACK"))
        return getenv("OA_UI_FONT_PACK");
    return def;
}

static int action_count;
static oa_action_event last_action;
static char last_action_name[64];
static void on_action(const void *evp, void *ud)
{
    (void)ud;
    const oa_action_event *ev = evp;
    if (!ev) return;
    last_action = *ev;
    snprintf(last_action_name, sizeof last_action_name, "%s",
             ev->name ? ev->name : "");
    last_action.name = last_action_name;
    action_count++;
}
struct oa_launcher_list { int n; };
static const char *fake_app_names[] = { "Netflix", "YouTube", "Amazon" };
static int fake_launcher_read_all(struct oa_launcher_list **out)
{
    if (!out) return -1;
    struct oa_launcher_list *l = calloc(1, sizeof *l);
    if (!l) return -1;
    l->n = 3;
    *out = l;
    return 0;
}
static size_t fake_launcher_count(const struct oa_launcher_list *l)
{
    return l ? (size_t)l->n : 0;
}
static int fake_launcher_get(const struct oa_launcher_list *l, size_t i,
                             struct oa_launcher_row_view *out)
{
    if (!l || !out || i >= (size_t)l->n) return -1;
    memset(out, 0, sizeof *out);
    out->application_name = fake_app_names[i];
    return 0;
}
static int fake_launcher_format(const struct oa_launcher_list *l, size_t i,
                                char *buf, size_t cap)
{
    if (!l || !buf || i >= (size_t)l->n) return -1;
    snprintf(buf, cap, "%s", fake_app_names[i]);
    return 0;
}
static void fake_launcher_release(struct oa_launcher_list *l) { free(l); }
static const oa_launcher_db_iface fake_launcher = {
    fake_launcher_read_all, NULL, fake_launcher_count, fake_launcher_get,
    fake_launcher_format, fake_launcher_release
};

static int fake_source_select;
static int fake_source_current = -1;
static int fake_cec;
static int fake_cec_sets;
static int fake_set_int(const char *col, int v)
{
    if (strcmp(col, "m_cec_enabled")) return -1;
    fake_cec = v;
    fake_cec_sets++;
    return 0;
}
static int fake_get_int(const char *col, int *out)
{
    if (!strcmp(col, "m_current_source") && out) { *out = 0; return 0; }
    if (!strcmp(col, "m_cec_enabled") && out) { *out = fake_cec; return 0; }
    return -1;
}
static int fake_writable(void) { return 1; }
static const oa_settings_iface fake_settings = {
    fake_get_int, fake_set_int, fake_writable
};
static int fake_select_type(int type) { fake_source_select = type; fake_source_current = type; return 0; }
static int fake_current_type(void) { return fake_source_current; }
static int fake_available(void) { return 1; }
static const oa_source_iface fake_source = {
    fake_select_type, fake_current_type, fake_available
};
static int fake_volume = 50;
static int fake_mute;
static int fake_volume_calls;
static int fake_mute_calls;
static int fake_audio_set_volume(int v)
{
    fake_volume = v;
    fake_volume_calls++;
    return 0;
}
static int fake_audio_get_volume(void) { return fake_volume; }
static int fake_audio_set_mute(int on)
{
    fake_mute = on;
    fake_mute_calls++;
    return 0;
}
static int fake_audio_available(void) { return 1; }
static const oa_audio_iface fake_audio = {
    fake_audio_set_volume, fake_audio_get_volume, fake_audio_set_mute,
    fake_audio_available, NULL
};

static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[160];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}

static void capture(const oa_nav_trace *trace, const char *name)
{
    const char *dir = getenv("OA_UI_CAPTURE_DIR");
    if (!dir || !*dir) return;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.rgba", dir, name);
    (void)trace->capture(path);
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

static int has_scaled(const oa_nav_trace *trace, int x, int y, int w, int h,
                      int sw, int sh)
{
    int n = trace->scaled_count();
    for (int i = 0; i < n; i++) {
        int sx, sy, dw, dh, ssw, ssh;
        if (trace->scaled_at(i, &sx, &sy, &dw, &dh, &ssw, &ssh) == 0 &&
            sx == x && sy == y && dw == w && dh == h &&
            ssw == sw && ssh == sh)
            return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    assert(mkdtemp(asset_dir));
    oa_host host;
    assert(!oa_host_init(&host, argv[1], NULL));
    host.log_level = OA_LOG_ERROR;
    host.api.config_get = config;
    assert(!host.api.publish(&host, OA_SVC_SETTINGS, 1, &fake_settings));
    assert(!host.api.publish(&host, OA_SVC_SOURCE, 1, &fake_source));
    assert(!host.api.publish(&host, OA_SVC_LAUNCHER_DB, 1, &fake_launcher));
    assert(!host.api.publish(&host, OA_SVC_AUDIO, 1, &fake_audio));
    assert(host.api.subscribe(&host, OA_TOPIC_ACTION, on_action, NULL));
    char out[256];

    assert(!command(&host, "load nav_display", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start nav_display", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_engine", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_engine", out, sizeof out) && !strcmp(out, "ok\n"));
    const oa_ui_engine_iface *engine = host.api.lookup(&host, OA_SVC_UI_ENGINE, 2);
    assert(engine && engine->register_skin(NULL, NULL) == -1 && errno == ENOTSUP);
    char asset_path[256]; snprintf(asset_path, sizeof asset_path, "%s/bad.rgba", asset_dir);
    const unsigned char blob[] = {'O','A','I','1',1,0,0,0,1,0,0,0,10,20,30,128,99};
    FILE *file = fopen(asset_path, "wb");
    assert(file && fwrite(blob, 1, sizeof blob, file) == sizeof blob && !fclose(file));
    assert(!engine->icon("bad")); /* trailing bytes are malformed, not a cached valid icon */
    file = fopen(asset_path, "wb");
    assert(file && fwrite(blob, 1, 16, file) == 16 && !fclose(file));
    const oa_ui_icon *icon = engine->icon("bad");
    assert(icon && icon->w == 1 && icon->h == 1 && icon->rgba[3] == 128);
    assert(!engine->icon("../bad"));
    char long_name[80]; memset(long_name, 'x', sizeof long_name-1); long_name[79] = 0;
    assert(!engine->icon(long_name));
    assert(!engine->set_scene_transform(4, 0.5f));
    puts("  ok   engine rejects malformed or ambiguous asset names/sizes and keeps RGBA alpha");
    snprintf(asset_path, sizeof asset_path, "%s/wallpaper.rgba", asset_dir);
    const unsigned char wall[] = {
        'O','A','I','1',2,0,0,0,2,0,0,0,
        0,0,0,255, 255,255,255,255,
        255,255,255,255, 0,0,0,255
    };
    file = fopen(asset_path, "wb");
    assert(file && fwrite(wall, 1, sizeof wall, file) == sizeof wall && !fclose(file));

    assert(!command(&host, "load ui_omarchy", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_omarchy", out, sizeof out) && !strcmp(out, "ok\n"));
    const oa_nav_trace *trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    capture(trace, "home");
    assert(trace);
    int transform;
    assert(!trace->transform(&transform, NULL, NULL) && transform == 0);
    assert(has_fill(trace, 0, 0, 1920, 52, 9, 8, 9));            /* top bar */
    assert(has_scaled(trace, 0, 0, 1920, 1080, 2, 2));           /* native wallpaper */
    assert(has_fill(trace, 40, 86, 520, 336, 9, 8, 9));           /* status terminal */
    assert(has_fill(trace, 60, 804, 273, 196, 9, 8, 9));          /* dock tile 0 */
    assert(has_fill(trace, 56, 800, 281, 4, 181, 151, 144));      /* focus frame 0 */
    /* Workspace 2 (Inputs) through the bar, as in the reference. */
    queue_key_id(&host, 20); oa_host_dispatch_posts(&host);
    queue_key_id(&host, 22); oa_host_dispatch_posts(&host);
    queue_key_id(&host, 53); oa_host_dispatch_posts(&host);
    assert(has_fill(trace, 40, 86, 1840, 954, 9, 8, 9));         /* Sources screen */
    assert(has_fill(trace, 70, 202, 430, 260, 9, 8, 9));         /* source tile 0 */
    capture(trace, "sources");
    queue_key_id(&host, 22); oa_host_dispatch_posts(&host);       /* BACK AV */
    queue_key_id(&host, 53); oa_host_dispatch_posts(&host);
    assert(fake_source_select == 15);                             /* BACK AV */
    assert(action_count == 1 && last_action.kind == OA_ACTION_SOURCE &&
           last_action.index == 15 && !strcmp(last_action_name, "BACK AV"));
    queue_key_id(&host, 10); oa_host_dispatch_posts(&host);
    assert(action_count == 1);                                    /* internal back: no action */
    assert(has_fill(trace, 56, 800, 281, 4, 181, 151, 144));      /* dock focus unchanged */
    /* Workspace 3 (Apps) through the bar. */
    queue_key_id(&host, 20); oa_host_dispatch_posts(&host);
    queue_key_id(&host, 22); oa_host_dispatch_posts(&host);
    queue_key_id(&host, 22); oa_host_dispatch_posts(&host);
    queue_key_id(&host, 53); oa_host_dispatch_posts(&host);
    assert(has_fill(trace, 40, 86, 1840, 954, 9, 8, 9));         /* Apps screen */
    assert(has_fill(trace, 70, 202, 237, 200, 9, 8, 9));         /* app tile 0 */
    capture(trace, "apps");
    queue_key_id(&host, 53); oa_host_dispatch_posts(&host);
    assert(action_count == 2 && last_action.kind == OA_ACTION_SELECT &&
           last_action.index == 0 && !strcmp(last_action_name, "Netflix"));
    queue_key_id(&host, 10); oa_host_dispatch_posts(&host);
    /* Workspace 4 (Settings) through the bar. */
    queue_key_id(&host, 20); oa_host_dispatch_posts(&host);
    queue_key_id(&host, 22); oa_host_dispatch_posts(&host);
    queue_key_id(&host, 22); oa_host_dispatch_posts(&host);
    queue_key_id(&host, 22); oa_host_dispatch_posts(&host);
    queue_key_id(&host, 53); oa_host_dispatch_posts(&host);
    assert(has_fill(trace, 40, 86, 1840, 954, 9, 8, 9));         /* Settings screen */
    assert(has_fill(trace, 70, 212, 1780, 76, 9, 8, 9));         /* settings row 0 */
    capture(trace, "settings");
    queue_key_id(&host, 19); oa_host_dispatch_posts(&host);
    queue_key_id(&host, 53); oa_host_dispatch_posts(&host);
    assert(fake_cec == 1 && fake_cec_sets == 1);                  /* CEC toggled on */
    queue_key_id(&host, 19); oa_host_dispatch_posts(&host);
    queue_key_id(&host, 19); oa_host_dispatch_posts(&host);
    queue_key_id(&host, 53); oa_host_dispatch_posts(&host);
    assert(has_fill(trace, 40, 86, 1840, 954, 9, 8, 9));         /* About screen */
    capture(trace, "about");
    queue_key_id(&host, 10); oa_host_dispatch_posts(&host);
    assert(has_fill(trace, 0, 0, 1920, 52, 9, 8, 9));            /* back home */
    assert(!command(&host, "settings-get m_cec_enabled", out, sizeof out) &&
           atoi(out) == 1);
    assert(!command(&host, "settings-set m_cec_enabled 0", out, sizeof out) &&
           !strcmp(out, "ok\n"));
    assert(fake_cec == 0 && fake_cec_sets == 2);
    assert(command(&host, "settings-set m_cec_enabled nope", out,
                   sizeof out) == -1);
    assert(command(&host, "settings-get", out, sizeof out) == -1);
    puts("  ok   settings-get/set expose the snapshot/restore columns");

    /* Volume/mute keys are global to the shell: codes 16/17/13. */
    fake_volume = 50; fake_volume_calls = 0; fake_mute_calls = 0; fake_mute = 0;
    queue_key_id(&host, 16); oa_host_dispatch_posts(&host);
    assert(fake_volume_calls == 1 && fake_volume == 52);
    assert(has_fill(trace, 1520, 944, 360, 96, 18, 21, 28));
    queue_key_id(&host, 13); oa_host_dispatch_posts(&host);
    assert(fake_mute_calls == 1 && fake_mute == 1);
    puts("  ok   volume/mute keys call the audio service and draw the native OSD");
    pump(&host);
    assert(!has_fill(trace, 1520, 944, 360, 96, 18, 21, 28));

    /* The fake launcher is not a real vendor list; drop it before ui_vendor. */
    assert(!host.api.unpublish(&host, OA_SVC_LAUNCHER_DB));
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
    assert(has_fill(trace, 0, 0, 1920, 52, 9, 8, 9));

    puts("  ok   ui.skin selects the engine's active skin; vendor and omarchy switch by reload, no rebuild");
    queue_key(&host);
    assert(host.nposts > 0);
    assert(!oa_module_unload(&host, "ui_engine", 1));
    oa_host_dispatch_posts(&host); /* queued skin trampoline must survive provider dlclose */
    assert(!command(&host, "load ui_engine", out, sizeof out));
    assert(!command(&host, "start ui_engine", out, sizeof out));
    pump(&host);
    queue_key(&host); oa_host_dispatch_posts(&host);
    assert(has_fill(trace, 0, 0, 1920, 52, 9, 8, 9));
    assert(oa_module_unload(&host, "ui_engine", 0) == -1);
    puts("  ok   queued input survives forced engine removal safely; Omarchy rebinds and pins its replacement");
    for (unsigned i = 0; i < 40; i++) {
        queue_key(&host);
        assert(!oa_module_unload(&host, "ui_omarchy", 0));
        oa_mod *m = oa_module_load(&host, "ui_omarchy");
        assert(m && !oa_module_start(&host, m));
    }
    pump(&host);
    queue_key(&host);
    assert(!oa_module_suspend_all(&host));
    oa_host_dispatch_posts(&host); /* old generation cannot call a suspended skin */
    assert(!oa_module_resume_all(&host)); pump(&host);
    puts("  ok   queued-input skin unload/reload and suspend/resume leave no borrowed callbacks or heap posts");
    assert(!oa_module_unload(&host, "ui_omarchy", 0));
    g_skin = "vendor";
    oa_mod *m = oa_module_load(&host, "ui_vendor"); assert(m && !oa_module_start(&host, m));
    pump(&host); queue_key(&host);
    assert(!oa_module_unload(&host, "ui_engine", 1));
    oa_host_dispatch_posts(&host);
    m = oa_module_load(&host, "ui_engine"); assert(m && !oa_module_start(&host, m));
    pump(&host);
    assert(oa_module_unload(&host, "ui_engine", 0) == -1);
    const oa_portal_iface *portal = host.api.lookup(&host, OA_SVC_PORTAL, 1);
    assert(portal);
    queue_key(&host); oa_host_dispatch_posts(&host);
    assert(has_fill(trace, 11, 571, 150, 150, 129, 25, 36));
    puts("  ok   vendor skin re-registers after forced engine replacement and receives keys again");
    assert(!oa_module_unload(&host, "ui_vendor", 0));
    assert(!oa_module_unload(&host, "ui_engine", 0));
    use_fps = 1; g_skin = "omarchy";
    m = oa_module_load(&host, "ui_engine"); assert(m);
    uint64_t (*real_timer)(void *, uint32_t, uint32_t, void (*)(void *), void *) = host.api.timer_add;
    host.api.timer_add = deny_timer;
    assert(oa_module_start(&host, m) == -1);
    host.api.timer_add = real_timer;
    assert(!oa_module_start(&host, m));
    m = oa_module_load(&host, "ui_omarchy"); assert(m && !oa_module_start(&host, m)); pump(&host);
    assert(!oa_module_suspend_all(&host));
    host.api.timer_add = deny_timer;
    assert(oa_module_resume_all(&host) == -1);
    assert(oa_host_find_module(&host, "ui_engine")->state == OA_ST_SUSPENDED);
    host.api.timer_add = real_timer;
    assert(!oa_module_resume_all(&host)); pump(&host);
    puts("  ok   configured repaint timer allocation failures reject start/resume and remain retryable");
    host.stop_requested = 1; assert(!oa_host_run(&host));
    close(host.selfpipe[0]); close(host.selfpipe[1]); pthread_mutex_destroy(&host.post_mu);
    char bad_path[256]; snprintf(bad_path, sizeof bad_path, "%s/bad.rgba", asset_dir);
    assert(!unlink(asset_path) && !unlink(bad_path) && !rmdir(asset_dir));
    return 0;
}
