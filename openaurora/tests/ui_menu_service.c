/* ui.menu service: load the JSON tree, query it, unload/reload, and prove a
 * broken path fails without publishing a stale service. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "host.h"
#include "openaurora/menu_model.h"
#include "openaurora/services.h"

static const char *g_path;

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "ui_menu.file")) return g_path;
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
    g_path = argv[2];
    oa_host h;
    assert(!oa_host_init(&h, argv[1], NULL));
    h.log_level = OA_LOG_ERROR;
    h.api.config_get = config;

    assert(!cmd(&h, "load ui_menu"));
    assert(!cmd(&h, "start ui_menu"));
    const oa_ui_menu_iface *menu = h.api.lookup(&h, OA_SVC_UI_MENU, 1);
    assert(menu && menu->document() && menu->document()->dialog_count == 1);
    const struct oa_menu_dialog *d = menu->dialog("sub_launcher");
    assert(d && d->branch_count == 5);
    const struct oa_menu_branch *picture = menu->branch(d, 2);
    assert(picture && picture->item_count == 3);
    assert(!strcmp(picture->items[0].key, "MODE"));
    assert(!strcmp(picture->items[0].action, "setting:MODE"));
    const struct oa_menu_branch *sound = menu->branch(d, 3);
    assert(sound && sound->item_count == 6);
    assert(!menu->branch(d, 52)); /* excluded by the JSON model */

    assert(!cmd(&h, "unload ui_menu"));
    assert(!h.api.lookup(&h, OA_SVC_UI_MENU, 1));

    g_path = "/tmp/oa-no-such-menu.json";
    assert(!cmd(&h, "load ui_menu"));
    assert(cmd(&h, "start ui_menu") != 0);
    assert(!h.api.lookup(&h, OA_SVC_UI_MENU, 1));

    g_path = argv[2];
    assert(!cmd(&h, "start ui_menu"));
    menu = h.api.lookup(&h, OA_SVC_UI_MENU, 1);
    assert(menu && menu->dialog("sub_launcher"));
    assert(!cmd(&h, "unload ui_menu"));
    assert(!h.api.lookup(&h, OA_SVC_UI_MENU, 1));

    h.stop_requested = 1;
    assert(!oa_host_run(&h));
    close(h.selfpipe[0]);
    close(h.selfpipe[1]);
    pthread_mutex_destroy(&h.post_mu);
    puts("ok ui.menu service: JSON tree, reload, and failed-load isolation");
    return 0;
}
