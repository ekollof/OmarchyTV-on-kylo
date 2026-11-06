/* picture.settings: the firmware's per-source PictureSettingConfiguration.
 * Uses a copy of the shipped settingsDB and checks the decoded field offsets
 * and the vendor SettingValue IDs. Raw source-1 fields in the dump are mode 1
 * (NATURAL), zoom 1 (AUTO, not 16:9), energy 3 (MAXIMUM); zoom 5 is 16:9. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>

#include "host.h"
#include "openaurora/module.h"
#include "openaurora/picture_config.h"
#include "openaurora/services.h"

static const char *g_db;
static int g_readonly = 0;
static int g_full_mode = 1;

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "settings.db")) return g_db;
    if (!strcmp(key, "settings.readonly")) return g_readonly ? "1" : "0";
    if (!strcmp(key, "picture.full_mode")) return g_full_mode ? "1" : "0";
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

/* Read one 32-bit field from the persisted blob. */
static int raw_word(const char *db, int offset, int *out)
{
    sqlite3 *sq = NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_open_v2(db, &sq, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (sq) sqlite3_close(sq);
        return -1;
    }
    int rc = sqlite3_prepare_v2(sq,
        "SELECT m_picture_setting_configuration FROM Settings LIMIT 1;",
        -1, &st, NULL);
    if (rc != SQLITE_OK) goto done;
    rc = sqlite3_step(st);
    if (rc != SQLITE_ROW) { rc = -1; goto done; }
    const unsigned char *blob = sqlite3_column_blob(st, 0);
    int n = sqlite3_column_bytes(st, 0);
    if (!blob || offset < 0 || offset + 4 > n) { rc = -1; goto done; }
    memcpy(out, blob + offset, sizeof *out);
    rc = 0;
done:
    if (st) sqlite3_finalize(st);
    sqlite3_close(sq);
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

    int v = -1, raw = -1;
    assert(p->get_mode(1, &v) == 0 && v == 1);      /* NATURAL */
    assert(p->get_zoom(1, &v) == 0 && v == 1);      /* AUTO (vendor id 1) */
    assert(p->get_energy(1, 1, 0, &v) == 0 && v == 3); /* MAXIMUM */
    assert(p->get_mode(0, &v) == 0 && v == 0);      /* DYNAMIC */
    assert(p->get_energy(0, 0, 0, &v) == 0 && v == 0); /* OFF */
    assert(raw_word(db, 0x240 + 0x08, &raw) == 0 && raw == 1);
    assert(raw_word(db, 0x240 + 0x0c, &raw) == 0 && raw == 1);

    assert(p->set_mode(1, 2) == 0);                 /* CINEMA */
    assert(p->set_zoom(1, 2) == 0);                 /* 4:3 */
    assert(p->set_energy(1, 2, 0, 4) == 0);         /* AUTO */
    assert(p->get_mode(1, &v) == 0 && v == 2);
    assert(p->get_zoom(1, &v) == 0 && v == 2);
    assert(p->get_energy(1, 2, 0, &v) == 0 && v == 4);
    assert(raw_word(db, 0x240 + 0x0c, &raw) == 0 && raw == 2);
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

    /* Format 0: full_mode=0 reads/writes +0x08, except ZOOM_MODE_FULL (9)
     * which setZoomMode stores in the full field +0x0c. Reading after that
     * still uses +0x08, matching the vendor asymmetry. */
    g_readonly = 0;
    g_full_mode = 0;
    m = oa_module_load(&h, "settings");
    assert(m && !oa_module_start(&h, m));
    p = h.api.lookup(&h, OA_SVC_PICTURE_SETTINGS, 1);
    assert(p);
    assert(p->get_zoom(1, &v) == 0 && v == 1);      /* +0x08, the full-mode write was +0x0c */
    assert(p->set_zoom(1, 5) == 0);                 /* 16:9 -> +0x08 */
    assert(p->get_zoom(1, &v) == 0 && v == 5);
    assert(raw_word(db, 0x240 + 0x08, &raw) == 0 && raw == 5);
    assert(raw_word(db, 0x240 + 0x0c, &raw) == 0 && raw == 2);
    assert(p->set_zoom(1, 9) == 0);                 /* FULL -> +0x0c */
    assert(p->get_zoom(1, &v) == 0 && v == 5);      /* still +0x08 */
    assert(raw_word(db, 0x240 + 0x0c, &raw) == 0 && raw == 9);
    assert(!oa_module_stop(&h, m));
    oa_module_unload(&h, "settings", 0);

    close(h.selfpipe[0]);
    close(h.selfpipe[1]);
    pthread_mutex_destroy(&h.post_mu);
    assert(!unlink(db) && !rmdir(dir));
    puts("ok picture.settings: vendor field IDs, 0x240 stride, full-mode zoom rule");
    return 0;
}
