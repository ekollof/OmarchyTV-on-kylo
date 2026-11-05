#include "openaurora/service_db.h"

#include <assert.h>
#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void exec(sqlite3 *db, const char *sql)
{
    char *error = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &error) != SQLITE_OK) {
        fprintf(stderr, "sqlite: %s\n", error ? error : "?");
        sqlite3_free(error);
        exit(1);
    }
}

static void make_synthetic(const char *path)
{
    sqlite3 *db = NULL;
    assert(sqlite3_open(path, &db) == SQLITE_OK);
    exec(db,
        "CREATE TABLE DVBSerTable(m_id integer,m_handle integer,"
        "m_channel_no integer,m_lcn integer,m_type integer,m_flags integer,"
        "m_nid integer,m_onid integer,m_tsid integer,m_service_locator integer,"
        "m_video_pid integer,m_audio_pid integer,m_pmt_pid integer,"
        "m_pcr_pid integer,m_network_type integer,m_country integer,"
        "m_name_serialized blob,m_provider_serialized blob);"
        "CREATE TABLE EASISerTable(m_handle integer,m_rsn integer,"
        "m_fine_tune_offset integer,m_flags integer,m_name_serialized blob);"
        "CREATE TABLE AnalogSerTable(m_handle integer,m_channel_no integer,"
        "m_lcn integer,m_flags integer,m_type integer,m_tv_system integer,"
        "m_vps_code integer,m_aps_db_index integer,m_name_serialized blob,"
        "m_provider_serialized blob);"
        "INSERT INTO DVBSerTable VALUES(10,100,1,7,536870913,7,1,2,3,4,"
        "100,101,102,100,1,31,'DVB One','Provider');"
        "INSERT INTO DVBSerTable VALUES(11,101,2,8,536870914,15,1,2,3,4,"
        "110,111,112,110,2,31,'NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN',"
        "'PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP');"
        "INSERT INTO DVBSerTable VALUES(12,102,3,9,536870915,7,1,2,3,4,"
        "120,121,122,120,1,31,'A'||char(0)||'B','');"
        "INSERT INTO EASISerTable VALUES(200,5,-3,1,'EASI One');"
        "INSERT INTO AnalogSerTable VALUES(300,42,9,135,255,1,4660,7,"
        "'Analog One','analogue');");
    assert(sqlite3_close(db) == SQLITE_OK);
}

static void synthetic(void)
{
    char path[] = "/tmp/oa-service-db-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    remove(path);
    make_synthetic(path);

    struct oa_service_list list;
    char error[200];
    assert(oa_service_db_read_all(path, &list, error, sizeof error) == 0);
    assert(list.count == 5);
    assert(list.rows[0].source == OA_SERVICE_SOURCE_DVB);
    assert(list.rows[0].id == 10 && list.rows[0].handle == 100);
    assert(list.rows[0].channel_no == 1 && list.rows[0].lcn == 7);
    assert(list.rows[0].type == 536870913 && list.rows[0].flags == 7);
    assert(list.rows[0].nid == 1 && list.rows[0].onid == 2 && list.rows[0].tsid == 3);
    assert(list.rows[0].service_locator == 4);
    assert(list.rows[0].video_pid == 100 && list.rows[0].audio_pid == 101);
    assert(list.rows[0].pmt_pid == 102 && list.rows[0].pcr_pid == 100);
    assert(list.rows[0].network_type == 1 && list.rows[0].country == 31);
    assert(!strcmp(list.rows[0].name, "DVB One"));
    assert(!strcmp(list.rows[0].provider, "Provider"));
    assert(oa_service_visible(&list.rows[0]) == 0);
    assert(oa_service_available(&list.rows[0]) == 1);
    assert(oa_service_encrypted(&list.rows[0]) == 0);
    assert(oa_service_numeric_selection(&list.rows[0]) == 0);
    assert(oa_service_not_found(&list.rows[0]) == 0);
    assert(oa_service_visible(&list.rows[3]) == 1); /* EASI flags 1 */

    struct oa_service_query query;
    size_t order[5], matched = 0;
    memset(&query, 0, sizeof query);
    query.source_mask = 0x7;
    query.sort = OA_SERVICE_SORT_NAME;
    assert(!oa_service_list_query(&list, &query, order, 5, &matched));
    assert(matched == 5 && !strcmp(list.rows[order[0]].name, "A"));
    assert(!strcmp(list.rows[order[1]].name, "Analog One"));
    assert(!strcmp(list.rows[order[2]].name, "DVB One"));

    query.sort = OA_SERVICE_SORT_CHANNEL_NO;
    assert(!oa_service_list_query(&list, &query, order, 5, &matched));
    assert(matched == 5 && list.rows[order[0]].source == OA_SERVICE_SOURCE_EASI);
    assert(list.rows[order[0]].channel_no == 0);
    assert(list.rows[order[1]].channel_no == 1);
    assert(list.rows[order[4]].channel_no == 42);

    query.source_mask = 1u << OA_SERVICE_SOURCE_DVB;
    query.available_only = 1;
    assert(!oa_service_list_query(&list, &query, order, 5, &matched));
    assert(matched == 2 && list.rows[order[0]].flags == 7 &&
        list.rows[order[1]].flags == 7);

    query.source_mask = 1u << OA_SERVICE_SOURCE_EASI;
    query.visible_only = 1;
    assert(!oa_service_list_query(&list, &query, order, 5, &matched));
    assert(matched == 1 && order[0] == 3);

    query.source_mask = 0x7;
    query.visible_only = query.available_only = 0;
    query.exclude_encrypted = 1;
    assert(!oa_service_list_query(&list, &query, order, 5, &matched));
    assert(matched == 4);
    assert(!oa_service_list_query(&list, &query, order, 1, &matched));
    assert(matched == 4); /* cap limits output, not the reported match count */

    assert(strlen(list.rows[1].name) == OA_SERVICE_NAME_MAX - 1);
    assert(strlen(list.rows[1].provider) == OA_SERVICE_PROVIDER_MAX - 1);
    assert(!strcmp(list.rows[2].name, "A"));

    assert(list.rows[3].source == OA_SERVICE_SOURCE_EASI);
    assert(list.rows[3].handle == 200 && list.rows[3].rsn == 5);
    assert(list.rows[3].fine_tune == -3 && list.rows[3].flags == 1);
    assert(!strcmp(list.rows[3].name, "EASI One"));

    assert(list.rows[4].source == OA_SERVICE_SOURCE_ANALOG);
    assert(list.rows[4].handle == 300 && list.rows[4].channel_no == 42);
    assert(list.rows[4].lcn == 9 && list.rows[4].flags == 135);
    assert(list.rows[4].type == 255 && list.rows[4].tv_system == 1);
    assert(list.rows[4].vps_code == 4660 && list.rows[4].aps_db_index == 7);
    assert(!strcmp(list.rows[4].name, "Analog One"));
    assert(!strcmp(list.rows[4].provider, "analogue"));
    assert(!strcmp(oa_service_source_name(list.rows[4].source), "analog"));
    assert(!strcmp(oa_service_source_name(99), "unknown"));
    oa_service_db_free(&list);
    assert(list.count == 0 && !list.rows);

    /* A missing table is an error, not a silent empty inventory. */
    sqlite3 *db = NULL;
    assert(sqlite3_open(path, &db) == SQLITE_OK);
    exec(db, "DROP TABLE EASISerTable;");
    assert(sqlite3_close(db) == SQLITE_OK);
    assert(oa_service_db_read_all(path, &list, error, sizeof error) != 0);
    assert(error[0]);
    assert(list.count == 0 && !list.rows);
    unlink(path);

    puts("  ok   service DB inventory: DVB/EASI/analog rows, bounded text, table order, missing-table failure");
}

static void probe(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "skipping: %s unavailable\n", path);
        exit(77);
    }
    fclose(file);
    struct oa_service_list list;
    char error[200];
    int rc = oa_service_db_read_all(path, &list, error, sizeof error);
    if (rc) {
        fprintf(stderr, "service DB probe: %s\n", error);
        exit(1);
    }
    size_t counts[3] = { 0, 0, 0 };
    for (size_t i = 0; i < list.count; i++)
        if (list.rows[i].source < 3) counts[list.rows[i].source]++;
    printf("ok service DB dump: %zu rows (%zu dvb, %zu easi, %zu analog)\n",
        list.count, counts[0], counts[1], counts[2]);
    struct oa_service_query q;
    memset(&q, 0, sizeof q);
    q.source_mask = 0x7;
    q.sort = OA_SERVICE_SORT_NAME;
    size_t *order = malloc(list.count * sizeof *order);
    assert(order);
    size_t matched = 0;
    assert(!oa_service_list_query(&list, &q, order, list.count, &matched));
    assert(matched == list.count && list.rows[order[0]].name[0]);
    free(order);
    assert(list.count == 525);
    assert(counts[0] == 445 && counts[1] == 67 && counts[2] == 13);
    oa_service_db_free(&list);
}

int main(int argc, char **argv)
{
    synthetic();
    if (argc == 3 && !strcmp(argv[2], "probe")) probe(argv[1]);
    return 0;
}
