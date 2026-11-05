/* Read-only settingsDB broadcast-service inventory. See service_db.h. */
#include "openaurora/service_db.h"

#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t cap, const char *text)
{
    if (error && cap) snprintf(error, cap, "%s", text ? text : "");
}

static int sql_error(int rc)
{
    if (rc == SQLITE_NOMEM) return -ENOMEM;
    if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) return -EBUSY;
    return -EIO;
}

static void copy_text(char *dst, size_t size, sqlite3_stmt *st, int column)
{
    const unsigned char *src = sqlite3_column_blob(st, column);
    int bytes = sqlite3_column_bytes(st, column);
    size_t length = 0;
    if (src && bytes > 0) {
        while (length < (size_t)bytes && length + 1 < size && src[length])
            length++;
        memcpy(dst, src, length);
    }
    dst[length] = '\0';
}

static int append_row(struct oa_service_list *list,
    const struct oa_service_row *row)
{
    if (!list || !row) return -EINVAL;
    if (list->count >= OA_SERVICE_ROW_MAX) return -EOVERFLOW;
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 64;
        if (capacity > OA_SERVICE_ROW_MAX) capacity = OA_SERVICE_ROW_MAX;
        struct oa_service_row *rows = realloc(list->rows,
            capacity * sizeof *rows);
        if (!rows) return -ENOMEM;
        list->rows = rows;
        list->capacity = capacity;
    }
    list->rows[list->count++] = *row;
    return 0;
}

static int read_dvb(sqlite3 *db, struct oa_service_list *list)
{
    static const char sql[] =
        "SELECT m_id,m_handle,m_channel_no,m_lcn,m_type,m_flags,m_nid,m_onid,"
        "m_tsid,m_service_locator,m_video_pid,m_audio_pid,m_pmt_pid,m_pcr_pid,"
        "m_network_type,m_country,m_name_serialized,m_provider_serialized "
        "FROM DVBSerTable ORDER BY rowid";
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);
    if (rc != SQLITE_OK) return sql_error(rc);
    for (;;) {
        rc = sqlite3_step(st);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) { sqlite3_finalize(st); return sql_error(rc); }
        struct oa_service_row row;
        memset(&row, 0, sizeof row);
        row.source = OA_SERVICE_SOURCE_DVB;
        row.id = sqlite3_column_int64(st, 0);
        row.handle = sqlite3_column_int64(st, 1);
        row.channel_no = sqlite3_column_int64(st, 2);
        row.lcn = sqlite3_column_int64(st, 3);
        row.type = sqlite3_column_int64(st, 4);
        row.flags = sqlite3_column_int64(st, 5);
        row.nid = sqlite3_column_int64(st, 6);
        row.onid = sqlite3_column_int64(st, 7);
        row.tsid = sqlite3_column_int64(st, 8);
        row.service_locator = sqlite3_column_int64(st, 9);
        row.video_pid = sqlite3_column_int64(st, 10);
        row.audio_pid = sqlite3_column_int64(st, 11);
        row.pmt_pid = sqlite3_column_int64(st, 12);
        row.pcr_pid = sqlite3_column_int64(st, 13);
        row.network_type = sqlite3_column_int64(st, 14);
        row.country = sqlite3_column_int64(st, 15);
        copy_text(row.name, sizeof row.name, st, 16);
        copy_text(row.provider, sizeof row.provider, st, 17);
        rc = append_row(list, &row);
        if (rc) { sqlite3_finalize(st); return rc; }
    }
    rc = sqlite3_finalize(st);
    return rc == SQLITE_OK ? 0 : sql_error(rc);
}

static int read_easi(sqlite3 *db, struct oa_service_list *list)
{
    static const char sql[] =
        "SELECT m_handle,m_rsn,m_fine_tune_offset,m_flags,m_name_serialized "
        "FROM EASISerTable ORDER BY rowid";
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);
    if (rc != SQLITE_OK) return sql_error(rc);
    for (;;) {
        rc = sqlite3_step(st);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) { sqlite3_finalize(st); return sql_error(rc); }
        struct oa_service_row row;
        memset(&row, 0, sizeof row);
        row.source = OA_SERVICE_SOURCE_EASI;
        row.handle = sqlite3_column_int64(st, 0);
        row.rsn = sqlite3_column_int64(st, 1);
        row.fine_tune = sqlite3_column_int64(st, 2);
        row.flags = sqlite3_column_int64(st, 3);
        copy_text(row.name, sizeof row.name, st, 4);
        rc = append_row(list, &row);
        if (rc) { sqlite3_finalize(st); return rc; }
    }
    rc = sqlite3_finalize(st);
    return rc == SQLITE_OK ? 0 : sql_error(rc);
}

static int read_analog(sqlite3 *db, struct oa_service_list *list)
{
    static const char sql[] =
        "SELECT m_handle,m_channel_no,m_lcn,m_flags,m_type,m_tv_system,"
        "m_vps_code,m_aps_db_index,m_name_serialized,m_provider_serialized "
        "FROM AnalogSerTable ORDER BY rowid";
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);
    if (rc != SQLITE_OK) return sql_error(rc);
    for (;;) {
        rc = sqlite3_step(st);
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) { sqlite3_finalize(st); return sql_error(rc); }
        struct oa_service_row row;
        memset(&row, 0, sizeof row);
        row.source = OA_SERVICE_SOURCE_ANALOG;
        row.handle = sqlite3_column_int64(st, 0);
        row.channel_no = sqlite3_column_int64(st, 1);
        row.lcn = sqlite3_column_int64(st, 2);
        row.flags = sqlite3_column_int64(st, 3);
        row.type = sqlite3_column_int64(st, 4);
        row.tv_system = sqlite3_column_int64(st, 5);
        row.vps_code = sqlite3_column_int64(st, 6);
        row.aps_db_index = sqlite3_column_int64(st, 7);
        copy_text(row.name, sizeof row.name, st, 8);
        copy_text(row.provider, sizeof row.provider, st, 9);
        rc = append_row(list, &row);
        if (rc) { sqlite3_finalize(st); return rc; }
    }
    rc = sqlite3_finalize(st);
    return rc == SQLITE_OK ? 0 : sql_error(rc);
}

int oa_service_db_read_all(const char *path, struct oa_service_list *out,
    char *error, size_t error_size)
{
    if (!path || !*path || !out) return -EINVAL;
    memset(out, 0, sizeof *out);
    if (error && error_size) error[0] = '\0';

    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        char message[160];
        snprintf(message, sizeof message, "open %s: %s", path,
            db ? sqlite3_errmsg(db) : "unknown error");
        set_error(error, error_size, message);
        if (db) sqlite3_close(db);
        return sql_error(rc);
    }
    sqlite3_busy_timeout(db, 1000);
    rc = read_dvb(db, out);
    if (!rc) rc = read_easi(db, out);
    if (!rc) rc = read_analog(db, out);
    if (rc) {
        char message[200];
        snprintf(message, sizeof message, "read %s: %s", path,
            sqlite3_errmsg(db));
        set_error(error, error_size, message);
        oa_service_db_free(out);
    }
    int close_rc = sqlite3_close(db);
    if (!rc && close_rc != SQLITE_OK && close_rc != SQLITE_BUSY) rc = sql_error(close_rc);
    return rc;
}

void oa_service_db_free(struct oa_service_list *list)
{
    if (!list) return;
    free(list->rows);
    memset(list, 0, sizeof *list);
}

const char *oa_service_source_name(unsigned source)
{
    switch (source) {
    case OA_SERVICE_SOURCE_DVB: return "dvb";
    case OA_SERVICE_SOURCE_EASI: return "easi";
    case OA_SERVICE_SOURCE_ANALOG: return "analog";
    default: return "unknown";
    }
}
