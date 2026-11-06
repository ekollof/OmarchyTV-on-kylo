/* picture.settings: the firmware's per-source PictureSettingConfiguration.
 * Uses a copy of the shipped settingsDB and checks the decoded field offsets
 * against the values visible in the p0/03 capture (source 1 = HDMI1):
 * mode NATURAL (1), zoom 16:9 (1), energy MAXIMUM (3). */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "host.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

static const char *g_db;
static int g_readonly = 0;

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "settings.db")) return g_db;
    if (!strcmp(key, "settings.readonly")) return g_readonly ? "1" : "0";
    return def;
}

static int copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    char buf[65536];
    size_t n;
    int rc = 0;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { rc = -1; break; }
    if (ferror(in)) rc = -1;
    if (fclose(out) != 0) rc = -1;
    fclose(in);
    return rc;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    char dir[] = "/tmp/oa-picture-settings-XXXXXX";
    assert(mkdtemp(dir));
    char db[256];
    snprintf(db, sizeof db, "%s/settingsDB.db", dir);
    assert(copy_file(argv[1], db) == 0);
    g_db = db;

    oa_host h;
    assert(!oa_host_init(&h, argv[2], NULL));
    h.log_level = OA_LOG_ERROR;
    h.api.config_get = config;
    oa_mod *m = oa_module_load(&h, "settings");
    assert(m && !oa_module_start(&h, m));
    const oa_picture_settings_iface *p =
        h.api.lookup(&h, OA_SVC_PICTURE_SETTINGS, 1);
    assert(p && p->is_writable());

    int v = -1;
    assert(p->get_mode(1, &v) == 0 && v == 1);      /* NATURAL */
    assert(p->get_zoom(1, &v) == 0 && v == 1);      /* 16:9 */
    assert(p->get_energy(1, 1, 0, &v) == 0 && v == 3); /* MAXIMUM */
    assert(p->get_mode(0, &v) == 0 && v == 0);      /* DYNAMIC */
    assert(p->get_energy(0, 0, 0, &v) == 0 && v == 0); /* OFF */

    assert(p->set_mode(1, 2) == 0);                 /* CINEMA */
    assert(p->set_zoom(1, 2) == 0);                 /* SUBTITLE */
    assert(p->set_energy(1, 2, 0, 4) == 0);         /* AUTO */
    assert(p->get_mode(1, &v) == 0 && v == 2);
    assert(p->get_zoom(1, &v) == 0 && v == 2);
    assert(p->get_energy(1, 2, 0, &v) == 0 && v == 4);
    assert(p->set_mode(1, 9) == EINVAL);
    assert(p->set_energy(1, 5, 0, 1) == EINVAL);
    assert(p->set_energy(1, 1, 2, 1) == EINVAL);

    /* Reload must read the persisted blob. */
    assert(!oa_module_stop(&h, m));
    assert(!oa_module_start(&h, m));
    p = h.api.lookup(&h, OA_SVC_PICTURE_SETTINGS, 1);
    assert(p);
    assert(p->get_mode(1, &v) == 0 && v == 2);
    assert(p->get_zoom(1, &v) == 0 && v == 2);
    assert(p->get_energy(1, 2, 0, &v) == 0 && v == 4);
    assert(!oa_module_stop(&h, m));
    oa_module_unload(&h, "settings", 0);

    /* Read-only mode keeps the values but refuses writes. */
    g_readonly = 1;
    m = oa_module_load(&h, "settings");
    assert(m && !oa_module_start(&h, m));
    p = h.api.lookup(&h, OA_SVC_PICTURE_SETTINGS, 1);
    assert(p && !p->is_writable());
    assert(p->get_mode(1, &v) == 0 && v == 2);
    assert(p->set_mode(1, 1) == EROFS);
    assert(!oa_module_stop(&h, m));
    oa_module_unload(&h, "settings", 0);

    close(h.selfpipe[0]);
    close(h.selfpipe[1]);
    pthread_mutex_destroy(&h.post_mu);
    assert(!unlink(db) && !rmdir(dir));
    puts("ok picture.settings: source1 NATURAL/16:9/MAXIMUM, writes persist at 0x240 stride");
    return 0;
}
