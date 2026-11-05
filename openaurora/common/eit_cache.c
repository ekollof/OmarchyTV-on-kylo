/* Firmware-grounded EIT schedule cache. See
 * include/openaurora/eit_cache.h for provenance and limits. */
#include "openaurora/eit_cache.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct oa_eit_cache {
    struct oa_eit_record *records;
    size_t count;
    size_t capacity;
};

static int before(const struct oa_eit_record *a,
    const struct oa_eit_record *b)
{
    if (a->service_id != b->service_id)
        return a->service_id < b->service_id;
    if (a->range.begin != b->range.begin)
        return a->range.begin < b->range.begin;
    return a->event_id < b->event_id;
}

static void sort_records(struct oa_eit_cache *cache)
{
    for (size_t i = 1; i < cache->count; i++) {
        struct oa_eit_record record = cache->records[i];
        size_t j = i;
        while (j > 0 && before(&record, &cache->records[j - 1])) {
            cache->records[j] = cache->records[j - 1];
            j--;
        }
        cache->records[j] = record;
    }
}

static struct oa_eit_record *find_key(struct oa_eit_cache *cache,
    uint32_t service_id, uint32_t event_id)
{
    for (size_t i = 0; i < cache->count; i++)
        if (cache->records[i].service_id == service_id &&
            cache->records[i].event_id == event_id)
            return &cache->records[i];
    return NULL;
}

static void copy_record(struct oa_eit_record *to,
    const struct oa_eit_record *from)
{
    *to = *from;
    to->title[sizeof to->title - 1] = 0;
}

struct oa_eit_cache *oa_eit_cache_create(size_t capacity)
{
    if (!capacity || capacity > SIZE_MAX / sizeof(struct oa_eit_record))
        return NULL;
    struct oa_eit_cache *cache = calloc(1, sizeof *cache);
    if (!cache) return NULL;
    cache->records = calloc(capacity, sizeof *cache->records);
    if (!cache->records) {
        free(cache);
        return NULL;
    }
    cache->capacity = capacity;
    return cache;
}

void oa_eit_cache_destroy(struct oa_eit_cache *cache)
{
    if (!cache) return;
    free(cache->records);
    free(cache);
}

size_t oa_eit_cache_count(const struct oa_eit_cache *cache)
{
    return cache ? cache->count : 0;
}

size_t oa_eit_cache_capacity(const struct oa_eit_cache *cache)
{
    return cache ? cache->capacity : 0;
}

int oa_eit_cache_put(struct oa_eit_cache *cache,
    const struct oa_eit_record *record)
{
    if (!cache || !record || !record->service_id ||
        record->range.end < record->range.begin)
        return -EINVAL;
    struct oa_eit_record *existing =
        find_key(cache, record->service_id, record->event_id);
    if (existing) {
        copy_record(existing, record);
        sort_records(cache);
        return 0;
    }
    if (cache->count >= cache->capacity) return -ENOSPC;
    copy_record(&cache->records[cache->count++], record);
    sort_records(cache);
    return 0;
}

int oa_eit_cache_get(const struct oa_eit_cache *cache, uint32_t service_id,
    uint32_t event_id, struct oa_eit_record *out)
{
    if (!cache || !service_id || !out) return -EINVAL;
    for (size_t i = 0; i < cache->count; i++) {
        if (cache->records[i].service_id == service_id &&
            cache->records[i].event_id == event_id) {
            copy_record(out, &cache->records[i]);
            return 0;
        }
    }
    return -ENOENT;
}

int oa_eit_cache_query(const struct oa_eit_cache *cache, uint32_t service_id,
    uint32_t start_sec, uint32_t duration_sec, struct oa_eit_record *out,
    size_t cap, size_t *count)
{
    if (count) *count = 0;
    if (!cache || (cap && !out)) return -EINVAL;
    struct oa_eit_range window =
        oa_eit_range_set(start_sec, start_sec + duration_sec);
    size_t found = 0;
    for (size_t i = 0; i < cache->count; i++) {
        const struct oa_eit_record *record = &cache->records[i];
        if (service_id && record->service_id != service_id) continue;
        if (!oa_eit_range_intersects(&record->range, &window)) continue;
        if (found < cap) copy_record(&out[found], record);
        found++;
    }
    if (count) *count = found;
    return 0;
}

int oa_eit_cache_now_next(const struct oa_eit_cache *cache,
    uint32_t service_id, uint32_t now_sec, struct oa_eit_record *now,
    struct oa_eit_record *next)
{
    if (!cache || !service_id || (!now && !next)) return -EINVAL;
    if (now) memset(now, 0, sizeof *now);
    if (next) memset(next, 0, sizeof *next);
    int found = 0;
    for (size_t i = 0; i < cache->count; i++) {
        const struct oa_eit_record *record = &cache->records[i];
        if (record->service_id != service_id) continue;
        if (!found && now && record->range.begin <= now_sec &&
            now_sec < record->range.end) {
            copy_record(now, record);
            found = 1;
        }
        if (next && record->range.begin > now_sec) {
            copy_record(next, record);
            found = 1;
            break;
        }
    }
    return found ? 0 : -ENOENT;
}
