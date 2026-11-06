/* End-to-end JSON action -> portal dispatcher: a menu entry loaded from JSON
 * opens a real vendor-skin screen. No display device is opened. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "host.h"
#include "openaurora/menu_model.h"
#include "openaurora/services.h"

static const char *g_menu_path;

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "ui_menu.file")) return g_menu_path;
    if (!strcmp(key, "ui.paint")) return "0";
    if (!strcmp(key, "ui.text")) return "0";
    if (!strcmp(key, "ui.ok_file")) return "";
    if (!strcmp(key, "ui.fps")) return "0";
    if (!strcmp(key, "ui.screen")) return "portal";
    return def;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    char dir[] = "/tmp/oa-menu-action-XXXXXX";
    assert(mkdtemp(dir));
    char path[256];
    snprintf(path, sizeof path, "%s/menu.json", dir);
    FILE *f = fopen(path, "w");
    assert(f);
    const char *json =
        "{\"dialogs\":[{\"id\":\"action_test\",\"class\":\"Test\","
        "\"name\":\"Test\",\"svg\":\"\",\"list\":[0,0,1,1],"
        "\"item\":{\"w\":1,\"h\":1},\"branches\":[{\"launcher_type\":99,"
        "\"name\":\"test\",\"items\":[{\"id\":0,\"key\":\"OPEN_SETTINGS\","
        "\"action\":\"open:settings\",\"values\":[]}]}]},"
        "{\"id\":\"sub_launcher\",\"class\":\"SubLauncherDialog\","
        "\"name\":\"SubLauncher\",\"svg\":\"\",\"list\":[189,255,1774,222],"
        "\"item\":{\"w\":240,\"h\":140},\"branches\":[{\"launcher_type\":2,"
        "\"name\":\"picture\",\"items\":["
        "{\"id\":0,\"key\":\"MODE\",\"action\":\"setting:MODE\","
        "\"values\":["
        "{\"key\":\"DYNAMIC\"},{\"key\":\"NATURAL\"},"
        "{\"key\":\"CINEMA\"},{\"key\":\"GAME\"},{\"key\":\"SPORT\"}]},"
        "{\"id\":1,\"key\":\"ENERGY_SAVING\","
        "\"action\":\"setting:ENERGY_SAVING\",\"values\":["
        "{\"key\":\"OFF\",\"action\":\"set:ENERGY_SAVING=OFF\"},"
        "{\"key\":\"MAXIMUM_STR\",\"action\":\"set:ENERGY_SAVING=MAXIMUM_STR\"}]},"
        "{\"id\":2,\"key\":\"PICTURE_ZOOM\","
        "\"action\":\"setting:PICTURE_ZOOM\",\"values\":["
        "{\"key\":\"ZOOM_MODE_AUTO\",\"action\":\"set:PICTURE_ZOOM=ZOOM_MODE_AUTO\"},"
        "{\"key\":\"ZOOM_MODE_16_9\",\"action\":\"set:PICTURE_ZOOM=ZOOM_MODE_16_9\"},"
        "{\"key\":\"ZOOM_MODE_SUBTITLE\",\"action\":\"set:PICTURE_ZOOM=ZOOM_MODE_SUBTITLE\"}]}"
        "]}]}]}";
    assert(fwrite(json, 1, strlen(json), f) == strlen(json) && !fclose(f));
    g_menu_path = path;

    oa_host h;
    assert(!oa_host_init(&h, argv[1], NULL));
    h.log_level = OA_LOG_ERROR;
    h.api.config_get = config;
    const char *order[] = { "nav_display", "ui_engine", "fake_clock", "ui_anim",
                            "ui_menu", "ui_vendor" };
    for (unsigned i = 0; i < sizeof order / sizeof order[0]; i++) {
        oa_mod *m = oa_module_load(&h, order[i]);
        assert(m && !oa_module_start(&h, m));
    }

    const oa_ui_menu_iface *menu = h.api.lookup(&h, OA_SVC_UI_MENU, 1);
    const oa_portal_iface *portal = h.api.lookup(&h, OA_SVC_PORTAL, 2);
    assert(menu && portal);
    /* v1 consumers keep the prefix; v3 does not exist. */
    assert(h.api.lookup(&h, OA_SVC_PORTAL, 1));
    assert(!h.api.lookup(&h, OA_SVC_PORTAL, 3));
    const struct oa_menu_dialog *d = menu->dialog("action_test");
    assert(d);
    const struct oa_menu_branch *b = menu->branch(d, 99);
    assert(b && b->item_count == 1);
    assert(!strcmp(b->items[0].action, "open:settings"));

    assert(portal->screen() == 0);
    assert(portal->menu_action(b->items[0].action) == 0);
    assert(portal->screen() == 3); /* SCREEN_SETTINGS */
    assert(portal->menu_action("open:portal") == 0 && portal->screen() == 0);
    assert(portal->menu_action("open:apps") == 0 && portal->screen() == 5);
    assert(portal->menu_action("open:channels") == 0 && portal->screen() == 6);
    assert(portal->menu_action("open:nope") == ENOENT);
    assert(portal->menu_action("setting:MODE") == 0);
    assert(portal->screen() == 7); /* SCREEN_SUBLAUNCHER */
    assert(portal->menu_action("setting:ENERGY_SAVING") == 0);
    assert(portal->menu_action("set:PICTURE_ZOOM=ZOOM_MODE_SUBTITLE") == 0);
    assert(portal->menu_action("set:PICTURE_ZOOM=nope") == ENOENT);
    assert(portal->menu_action("set:MODE=NATURAL") == 0);
    assert(portal->menu_action("set:MODE=NOPE") == ENOENT);
    assert(portal->menu_action("setting:NOPE") == ENOENT);
    assert(portal->menu_action("set:nope") == EINVAL);
    assert(portal->menu_action(NULL) == EINVAL);

    h.stop_requested = 1;
    assert(!oa_host_run(&h));
    close(h.selfpipe[0]);
    close(h.selfpipe[1]);
    pthread_mutex_destroy(&h.post_mu);
    assert(!unlink(path) && !rmdir(dir));
    puts("ok ui.menu action: JSON entry invokes open/setting/set");
    return 0;
}
