/* i18n service: real langprofile, 41 locales, Dutch/English selection and
 * reload. The profile is an explicit input, not an optional fixture. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "host.h"
#include "openaurora/services.h"

static const char *g_profile;
static const char *g_language = "dut";

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "i18n.profile")) return g_profile;
    if (!strcmp(key, "i18n.language")) return g_language;
    return def;
}

static int cmd(oa_host *h, const char *text)
{
    char line[128], out[512];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(h, line, out, sizeof out);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    g_profile = argv[2];
    oa_host h;
    assert(!oa_host_init(&h, argv[1], NULL));
    h.log_level = OA_LOG_ERROR;
    h.api.config_get = config;

    assert(!cmd(&h, "load i18n"));
    assert(!cmd(&h, "start i18n"));
    const oa_i18n_iface *i18n = h.api.lookup(&h, OA_SVC_I18N, 1);
    assert(i18n);
    assert(i18n->locale_count() == 41);
    assert(!strcmp(i18n->current_locale(), "dut"));
    assert(i18n->find_locale("eng") == 0);
    assert(i18n->find_locale("dut") == 10);
    assert(!strcmp(i18n->translate("MODE", "dut"), "Modus"));
    assert(!strcmp(i18n->translate("MODE", "eng"), "Mode"));
    assert(!strcmp(i18n->translate("MODE", NULL), "Modus"));
    assert(!strcmp(i18n->translate("ENERGY_SAVING", "dut"),
                   "Energiebesparing"));
    assert(!strcmp(i18n->translate("ZOOM_MODE_16_9", "dut"), "16:9"));
    assert(!strcmp(i18n->translate("MODE", "nope"), "MODE"));

    assert(!cmd(&h, "unload i18n"));
    assert(!h.api.lookup(&h, OA_SVC_I18N, 1));

    g_language = "eng";
    assert(!cmd(&h, "load i18n"));
    assert(!cmd(&h, "start i18n"));
    i18n = h.api.lookup(&h, OA_SVC_I18N, 1);
    assert(i18n && !strcmp(i18n->current_locale(), "eng"));
    assert(!strcmp(i18n->translate("MODE", NULL), "Mode"));
    assert(!cmd(&h, "unload i18n"));

    g_language = "dut";
    g_profile = "/tmp/oa-no-such-langprofile.bin";
    assert(!cmd(&h, "load i18n"));
    assert(cmd(&h, "start i18n") != 0);
    assert(!h.api.lookup(&h, OA_SVC_I18N, 1));
    g_profile = argv[2];
    assert(!cmd(&h, "start i18n"));
    i18n = h.api.lookup(&h, OA_SVC_I18N, 1);
    assert(i18n && !strcmp(i18n->translate("MODE", NULL), "Modus"));
    assert(!cmd(&h, "unload i18n"));

    h.stop_requested = 1;
    assert(!oa_host_run(&h));
    close(h.selfpipe[0]);
    close(h.selfpipe[1]);
    pthread_mutex_destroy(&h.post_mu);
    puts("ok i18n service: 41 locales, Dutch/English reload and failed-load isolation");
    return 0;
}
