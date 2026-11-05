#ifndef OPENAURORA_SERVICE_DB_H
#define OPENAURORA_SERVICE_DB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * settingsDB broadcast-service inventory.
 *
 * The shipped SQLiteStreamStorage maps table types to these tables:
 *   0xc9 DVBSerTable, 0xca EASISerTable, 0xcb AnalogSerTable
 * (write() @0x9b4754, open() @0x9b4b9c). This adapter reads the ordinary
 * columns by name and intentionally does not invent service-list filtering,
 * LCN sorting or tuner state. Those belong to the later service-list/tuner
 * layer, not to this raw inventory.
 */
enum oa_service_source {
    OA_SERVICE_SOURCE_DVB = 0,
    OA_SERVICE_SOURCE_EASI = 1,
    OA_SERVICE_SOURCE_ANALOG = 2
};

#define OA_SERVICE_NAME_MAX 128
#define OA_SERVICE_PROVIDER_MAX 64
#define OA_SERVICE_ROW_MAX 100000u

struct oa_service_row {
    uint8_t source;             /* enum oa_service_source */
    uint8_t reserved[7];
    int64_t id;                 /* m_id (DVB only; zero elsewhere) */
    int64_t handle;             /* m_handle */
    int64_t channel_no;         /* m_channel_no */
    int64_t lcn;                /* m_lcn */
    int64_t type;               /* m_type */
    int64_t flags;              /* m_flags */
    int64_t nid;
    int64_t onid;
    int64_t tsid;
    int64_t service_locator;
    int64_t video_pid;
    int64_t audio_pid;
    int64_t pmt_pid;
    int64_t pcr_pid;
    int64_t network_type;
    int64_t country;
    int64_t rsn;                /* EASI m_rsn */
    int64_t fine_tune;          /* EASI m_fine_tune_offset */
    int64_t tv_system;          /* analog m_tv_system */
    int64_t vps_code;           /* analog m_vps_code */
    int64_t aps_db_index;       /* analog m_aps_db_index */
    char name[OA_SERVICE_NAME_MAX];
    char provider[OA_SERVICE_PROVIDER_MAX];
};

struct oa_service_list {
    struct oa_service_row *rows;
    size_t count;
    size_t capacity;
};

/* Reads all three tables in table order. The caller passes a zero-initialized
 * list; on error the partial list is freed and `error` describes the failure.
 * Text blobs are bounded and NUL-terminated; an embedded NUL ends the string.
 * A database with a missing required table is an error, not an empty list. */
int oa_service_db_read_all(const char *path, struct oa_service_list *out,
    char *error, size_t error_size);
void oa_service_db_free(struct oa_service_list *list);
const char *oa_service_source_name(unsigned source);

/* Exact raw predicates from StoredDvbServiceData (flags word at +0x14):
 *   visibleService     ((flags ^ 2) & 3) >> 1
 *   availableService   ((flags ^ 8) & 0xf) >> 3
 *   encryptedService   ((flags ^ 4) & 7) >> 2
 *   numericSelection   (~flags) & 1
 *   notFoundInLastScan (flags & 0x1f) >> 4
 * These are stored-DB predicates. The vendor's active service list additionally
 * consults runtime EASI visibility/HD-simulcast state, so do not call these a
 * complete channel-list filter. Inline so the sqlite-free host core can evaluate
 * them without linking the database adapter. */
static inline uint16_t oa_service_flags(const struct oa_service_row *row)
{
    return row ? (uint16_t)row->flags : 0;
}
static inline int oa_service_visible(const struct oa_service_row *row)
{
    uint16_t flags = oa_service_flags(row);
    return ((flags ^ 2u) & 3u) >> 1;
}
static inline int oa_service_available(const struct oa_service_row *row)
{
    uint16_t flags = oa_service_flags(row);
    return ((flags ^ 8u) & 0xfu) >> 3;
}
static inline int oa_service_encrypted(const struct oa_service_row *row)
{
    uint16_t flags = oa_service_flags(row);
    return ((flags ^ 4u) & 7u) >> 2;
}
static inline int oa_service_numeric_selection(const struct oa_service_row *row)
{
    return (~oa_service_flags(row)) & 1u;
}
static inline int oa_service_not_found(const struct oa_service_row *row)
{
    return (oa_service_flags(row) & 0x1fu) >> 4;
}

#ifdef __cplusplus
}
#endif

#endif
