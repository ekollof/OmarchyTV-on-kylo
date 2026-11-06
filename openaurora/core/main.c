/*
 * openaurora — main entry point.
 *
 * Everything the TV does is a module; this process only owns the loop, the
 * registry and the control plane. See docs/ARCHITECTURE.md.
 */
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/module.h"
#include "host.h"

static oa_host g_host;

static void on_signal(int sig)
{
    (void)sig;
    oa_host_signal_stop(&g_host);
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [options]\n"
            "  --modules DIR    module directory (default: %s, else /buffer/openaurora/modules)\n"
            "  --config FILE    key=value config (default: %s if it exists)\n"
            "  --sock PATH      control socket (default /tmp/openaurora.sock, '' disables)\n"
            "  --load NAME      load and start a module (repeatable; config autoload= does the same)\n"
            "  --log 0-3        log level (default 2)\n"
            "  --check          read-only ELF ABI/dependency/symbol check; no module startup\n"
            "  --help\n",
            argv0, "build/modules", "/conf/openaurora.conf");
}

static int exists(const char *p)
{
    return p && *p && access(p, R_OK) == 0;
}

int main(int argc, char **argv)
{
    const char *modules = NULL;
    const char *config = NULL;
    const char *sock = "/tmp/openaurora.sock";
    const char *loads[64];
    int nloads = 0;
    int loglevel = 2;
    int check = 0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--modules") && i + 1 < argc)
            modules = argv[++i];
        else if (!strcmp(a, "--config") && i + 1 < argc)
            config = argv[++i];
        else if (!strcmp(a, "--sock") && i + 1 < argc)
            sock = argv[++i];
        else if (!strcmp(a, "--log") && i + 1 < argc)
            loglevel = atoi(argv[++i]);
        else if (!strcmp(a, "--check"))
            check = 1;
        else if (!strcmp(a, "--load") && i + 1 < argc) {
            if (nloads < 64)
                loads[nloads++] = argv[++i];
        } else if (!strcmp(a, "--help") || !strcmp(a, "-h")) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown option: %s\n", a);
            usage(argv[0]);
            return 2;
        }
    }

    /* On the set there should be nothing to pass: find the deployed config and module directory.
     * Anything on the command line still wins, so a test run can point elsewhere. */
    if (!config && exists("/conf/openaurora.conf"))
        config = "/conf/openaurora.conf";
    if (!modules) {
        if (exists("/buffer/openaurora/modules/mod_ui_vendor.so"))
            modules = "/buffer/openaurora/modules";
        else
            modules = "build/modules";
    }

    /* Check all installed module files before creating the host/control plane.
     * No init/start, sockets, heartbeat writes, HAL or device access. */
    if (check)
        return oa_runtime_check(modules);

    if (oa_host_init(&g_host, modules, config) != 0) {
        fprintf(stderr, "host init failed\n");
        return 1;
    }
    struct sigaction sa = { .sa_handler = on_signal };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    g_host.log_level = loglevel;

    g_host.api.log(&g_host, OA_LOG_INFO, "openaurora starting, modules=%s", modules);

    /* autoload=ir,ui_console from the config file */
    const char *autoload = g_host.api.config_get(&g_host, "autoload", NULL);
    char autobuf[1024];
    if (autoload) {
        snprintf(autobuf, sizeof autobuf, "%s", autoload);
        char *save = NULL;
        for (char *t = strtok_r(autobuf, ", ", &save); t && nloads < 64; t = strtok_r(NULL, ", ", &save))
            loads[nloads++] = strdup(t); /* leaked on purpose: process lifetime */
    }

    if (oa_control_start(&g_host, sock) != 0) {
        g_host.api.log(&g_host, OA_LOG_ERROR, "control startup failed");
        return 1;
    }

    for (int i = 0; i < nloads; i++) {
        oa_mod *m = oa_module_load(&g_host, loads[i]);
        if (m)
            oa_module_start(&g_host, m);
    }
    if (oa_skin_bootstrap(&g_host))
        g_host.api.log(&g_host, OA_LOG_ERROR, "UI selection could not start");

    printf("openaurora: %d module(s) loaded, control on %s\n", nloads,
           (sock && *sock) ? sock : "(stdin only)");
    fflush(stdout);

    int rc = oa_host_run(&g_host);

    g_host.api.log(&g_host, rc == 0 ? OA_LOG_INFO : OA_LOG_ERROR,
                   rc == 0 ? "openaurora stopped" : "openaurora stopped with modules still loaded");
    return rc == 0 ? 0 : 1;
}
