/* Firmware bootstrap: DemoApp::checkUIDesign enables UIAnimationController
 * when SVGDesignChecker::checkUsabilityStyle finds the "animation" rectangle in
 * DesignStyle.svg. Exercise ui_vendor with synthetic packs with/without it. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "openaurora/services.h"

static char g_dir[256];
static char g_index[320], g_binary[320];
static const char *g_override;

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "ui_theme.index")) return g_index;
    if (!strcmp(key, "ui_theme.binary")) return g_binary;
    if (!strcmp(key, "ui.anim_enable") && g_override) return g_override;
    if (!strcmp(key, "ui.paint")) return "0";
    if (!strcmp(key, "ui.ok_file")) return "";
    return def;
}

static void pack(int animation)
{
    unsigned char rec[64];
    size_t n = 0;
    rec[n++] = 1; /* rect */
    const char *id = animation ? "animation" : "other";
    size_t idlen = strlen(id);
    memcpy(rec + n, id, idlen); n += idlen;
    rec[n++] = 0;
    const unsigned char fixed[11] = { 0,0, 0,0, 0x80,0x07, 0x38,0x04, 0,0, 0 };
    memcpy(rec + n, fixed, sizeof fixed); n += sizeof fixed;
    for (int i = 0; i < 4; i++) rec[n++] = 0; /* stroke/fill/animation/animate */
    rec[n++] = 0; rec[n++] = 0;               /* begin */
    snprintf(g_index, sizeof g_index, "%s/index", g_dir);
    snprintf(g_binary, sizeof g_binary, "%s/binary", g_dir);
    FILE *f = fopen(g_binary, "wb");
    assert(f && fwrite(rec, 1, n, f) == n && !fclose(f));
    f = fopen(g_index, "w");
    assert(f && fprintf(f, "1\n0x131669a4 %zu\n", n) > 0 && !fclose(f));
}

static void run(const char *modules, int animation, const char *override, int expected)
{
    snprintf(g_dir, sizeof g_dir, "/tmp/oa-anim-bootstrap-XXXXXX");
    assert(mkdtemp(g_dir));
    pack(animation);
    g_override = override;
    oa_host h;
    assert(!oa_host_init(&h, modules, NULL));
    h.log_level = OA_LOG_ERROR;
    h.api.config_get = config;
    const char *order[] = { "nav_display", "ui_engine", "fake_clock", "ui_anim",
                            "ui_theme", "ui_vendor" };
    for (unsigned i = 0; i < sizeof order / sizeof order[0]; i++) {
        oa_mod *m = oa_module_load(&h, order[i]);
        assert(m && !oa_module_start(&h, m));
    }
    const oa_ui_anim_iface *anim = h.api.lookup(&h, OA_SVC_UI_ANIM, 2);
    assert(anim && anim->enabled() == expected);
    h.stop_requested = 1;
    assert(!oa_host_run(&h));
    close(h.selfpipe[0]); close(h.selfpipe[1]); pthread_mutex_destroy(&h.post_mu);
    unlink(g_index); unlink(g_binary); rmdir(g_dir);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    run(argv[1], 1, NULL, 1);
    run(argv[1], 0, NULL, 0);
    run(argv[1], 0, "1", 1);
    run(argv[1], 1, "0", 0);
    puts("ok ui_anim bootstrap: DesignStyle.svg animation marker and explicit override");
    return 0;
}
