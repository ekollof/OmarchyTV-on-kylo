#ifndef OPENAURORA_EIT_CACHE_H
#define OPENAURORA_EIT_CACHE_H

#include <stddef.h>
#include <stdint.h>

#include "openaurora/eit_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Firmware-grounded EIT schedule cache (project implementation).
 *
 * Mirrors the recovered vendor shapes:
 *   - EITEventKey identity: (service id, EIT event id);
 *   - ScheduleEvent columns: StartTime/EndTime (uint32 Unix seconds),
 *     ProgramCridId, FirstShowing;
 *   - ScheduleCache::getEvents(DateTime, duration seconds, service, list) and
 *     getEvent(service, event id);
 *   - NowNextCache::getNowNext(service, now, next).
 *
 * It does not reproduce EITEventRecordList::findEvents/removeEventsBetween,
 * whose CompareType argument could not be recovered, and it does not persist.
 * Times are uint32 seconds to match the vendor; callers convert to the
 * contract's milliseconds at the service boundary.
 */

#define OA_EIT_TITLE_MAX 96

struct oa_eit_record {
    uint32_t service_id;
    uint32_t event_id;
    struct oa_eit_range range;      /* begin/end seconds */
    uint32_t program_crid_id;       /* ScheduleEvent.ProgramCridId */
    uint32_t running_status;        /* EITEventKey running status */
    uint8_t first_showing;          /* ScheduleEvent.FirstShowing */
    uint8_t state;                  /* EITEventKey state bits */
    uint8_t version;                /* EITEventKey version 0..31 */
    uint8_t actual;                 /* actual (present/following) flag */
    char title[OA_EIT_TITLE_MAX];
};

struct oa_eit_cache;

struct oa_eit_cache *oa_eit_cache_create(size_t capacity);
void oa_eit_cache_destroy(struct oa_eit_cache *cache);
size_t oa_eit_cache_count(const struct oa_eit_cache *cache);
size_t oa_eit_cache_capacity(const struct oa_eit_cache *cache);
void oa_eit_cache_clear(struct oa_eit_cache *cache);

/* Insert or replace by (service_id, event_id). Returns -ENOSPC when the cache
 * is full and the key is new; -EINVAL for a zero service id or begin > end. */
int oa_eit_cache_put(struct oa_eit_cache *cache,
    const struct oa_eit_record *record);

/* Exact ScheduleCache::getEvent(service, event id). */
int oa_eit_cache_get(const struct oa_eit_cache *cache, uint32_t service_id,
    uint32_t event_id, struct oa_eit_record *out);

/* Project enumeration for persistence/tests, not a recovered vendor API.
 * Records are in the cache's service/begin/event sorted order. */
int oa_eit_cache_get_at(const struct oa_eit_cache *cache, size_t index,
    struct oa_eit_record *out);

/* ScheduleCache::getEvents shape: [start, start + duration) in seconds. A
 * zero service id matches all services; duration 0 matches nothing. Returns 0
 * and the full match count, copying at most cap records. */
int oa_eit_cache_query(const struct oa_eit_cache *cache, uint32_t service_id,
    uint32_t start_sec, uint32_t duration_sec, struct oa_eit_record *out,
    size_t cap, size_t *count);

/* NowNextCache::getNowNext: now is the earliest record with
 * begin <= now < end; next is the earliest record with begin > now. Returns 0
 * when either was found, -ENOENT when neither was. Missing outputs are
 * zeroed. */
int oa_eit_cache_now_next(const struct oa_eit_cache *cache,
    uint32_t service_id, uint32_t now_sec, struct oa_eit_record *now,
    struct oa_eit_record *next);

#ifdef __cplusplus
}
#endif

#endif
