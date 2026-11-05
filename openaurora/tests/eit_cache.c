/* Firmware-grounded EIT schedule-cache tests. Expected shapes follow the
 * recovered EITEventKey/ScheduleEvent/NowNextCache semantics; this is project
 * code, not vendor execution. */
#include "openaurora/eit_cache.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static struct oa_eit_record record(uint32_t service_id, uint32_t event_id,
    uint32_t begin, uint32_t end, uint32_t crid, const char *title)
{
    struct oa_eit_record r;
    memset(&r, 0, sizeof r);
    r.service_id = service_id;
    r.event_id = event_id;
    r.range = oa_eit_range_set(begin, end);
    r.program_crid_id = crid;
    r.running_status = 0;
    r.first_showing = 1;
    r.state = 1;
    r.version = 2;
    r.actual = 1;
    snprintf(r.title, sizeof r.title, "%s", title);
    return r;
}

static void test_lifecycle(void)
{
    assert(!oa_eit_cache_create(0));
    struct oa_eit_cache *cache = oa_eit_cache_create(4);
    assert(cache);
    assert(oa_eit_cache_count(cache) == 0);
    assert(oa_eit_cache_capacity(cache) == 4);
    oa_eit_cache_destroy(cache);
    oa_eit_cache_destroy(NULL);
}

static void test_put_get(void)
{
    struct oa_eit_cache *cache = oa_eit_cache_create(5);
    assert(cache);
    struct oa_eit_record a = record(1, 100, 1000, 2000, 5001, "Alpha News");
    struct oa_eit_record b = record(1, 101, 2000, 3000, 5001, "Alpha Movie");
    struct oa_eit_record c = record(2, 200, 1500, 2500, 5002, "Beta Show");
    assert(oa_eit_cache_put(cache, &a) == 0);
    assert(oa_eit_cache_put(cache, &b) == 0);
    assert(oa_eit_cache_put(cache, &c) == 0);
    assert(oa_eit_cache_count(cache) == 3);

    struct oa_eit_record out;
    assert(oa_eit_cache_get(cache, 1, 100, &out) == 0);
    assert(out.event_id == 100 && out.program_crid_id == 5001 &&
        out.first_showing == 1 && out.state == 1 && out.version == 2 &&
        out.actual == 1 && !strcmp(out.title, "Alpha News"));
    assert(oa_eit_cache_get(cache, 1, 999, &out) == -ENOENT);
    assert(oa_eit_cache_get(cache, 0, 100, &out) == -EINVAL);
    assert(oa_eit_cache_get(NULL, 1, 100, &out) == -EINVAL);
    assert(oa_eit_cache_get(cache, 1, 100, NULL) == -EINVAL);
    assert(oa_eit_cache_get_at(cache, 0, &out) == 0 && out.event_id == 100);
    assert(oa_eit_cache_get_at(cache, 1, &out) == 0 && out.event_id == 101);
    assert(oa_eit_cache_get_at(cache, 2, &out) == 0 && out.service_id == 2);
    assert(oa_eit_cache_get_at(cache, 3, &out) == -ENOENT);
    assert(oa_eit_cache_get_at(NULL, 0, &out) == -EINVAL);
    assert(oa_eit_cache_get_at(cache, 0, NULL) == -EINVAL);

    struct oa_eit_record invalid = record(0, 1, 1, 2, 0, "bad");
    assert(oa_eit_cache_put(cache, &invalid) == -EINVAL);
    invalid = record(1, 1, 5, 4, 0, "bad");
    invalid.range.begin = 5;
    invalid.range.end = 4;
    assert(oa_eit_cache_put(cache, &invalid) == -EINVAL);
    assert(oa_eit_cache_put(cache, NULL) == -EINVAL);

    /* Same (service,event) replaces in place and re-sorts. */
    a = record(1, 100, 900, 1900, 5001, "Alpha News HD");
    a.state = 3;
    assert(oa_eit_cache_put(cache, &a) == 0);
    assert(oa_eit_cache_count(cache) == 3);
    assert(oa_eit_cache_get(cache, 1, 100, &out) == 0);
    assert(out.range.begin == 900 && out.range.end == 1900 &&
        out.state == 3 && !strcmp(out.title, "Alpha News HD"));

    /* Non-terminated titles are bounded on copy. */
    struct oa_eit_record raw = record(3, 300, 5000, 6000, 5003, "x");
    memset(raw.title, 'x', sizeof raw.title);
    assert(oa_eit_cache_put(cache, &raw) == 0);
    assert(oa_eit_cache_get(cache, 3, 300, &out) == 0);
    assert(out.title[sizeof out.title - 1] == 0);

    struct oa_eit_record full = record(4, 400, 7000, 8000, 5004, "Fourth");
    assert(oa_eit_cache_put(cache, &full) == 0);
    struct oa_eit_record extra = record(5, 500, 9000, 10000, 5005, "Fifth");
    assert(oa_eit_cache_put(cache, &extra) == -ENOSPC);
    oa_eit_cache_clear(cache);
    assert(oa_eit_cache_count(cache) == 0);
    assert(oa_eit_cache_get(cache, 1, 100, &out) == -ENOENT);
    assert(oa_eit_cache_put(cache, &extra) == 0);
    assert(oa_eit_cache_count(cache) == 1);
    oa_eit_cache_destroy(cache);
}

static void test_query(void)
{
    struct oa_eit_cache *cache = oa_eit_cache_create(4);
    assert(cache);
    struct oa_eit_record a = record(1, 100, 1000, 2000, 5001, "Alpha News");
    struct oa_eit_record b = record(1, 101, 2000, 3000, 5001, "Alpha Movie");
    struct oa_eit_record c = record(2, 200, 1500, 2500, 5002, "Beta Show");
    assert(oa_eit_cache_put(cache, &a) == 0);
    assert(oa_eit_cache_put(cache, &b) == 0);
    assert(oa_eit_cache_put(cache, &c) == 0);

    struct oa_eit_record out[4];
    size_t count = 99;
    /* Absolute start + duration, vendor getEvents shape. */
    assert(oa_eit_cache_query(cache, 1, 0, 4000, out, 4, &count) == 0 &&
        count == 2);
    assert(out[0].event_id == 100 && out[1].event_id == 101);
    assert(oa_eit_cache_query(cache, 1, 2000, 1000, out, 4, &count) == 0 &&
        count == 1 && out[0].event_id == 101);
    assert(oa_eit_cache_query(cache, 0, 1500, 500, out, 4, &count) == 0 &&
        count == 2);
    assert(oa_eit_cache_query(cache, 1, 1000, 0, out, 4, &count) == 0 &&
        count == 0);
    assert(oa_eit_cache_query(cache, 1, 10000, 100, out, 4, &count) == 0 &&
        count == 0);
    /* Full count is reported even when only one row is copied. */
    assert(oa_eit_cache_query(cache, 1, 0, 4000, out, 1, &count) == 0 &&
        count == 2 && out[0].event_id == 100);
    assert(oa_eit_cache_query(cache, 1, 0, 4000, NULL, 1, &count) == -EINVAL);
    assert(oa_eit_cache_query(cache, 1, 0, 4000, out, 4, NULL) == 0);
    assert(oa_eit_cache_query(NULL, 1, 0, 4000, out, 4, &count) == -EINVAL);
    oa_eit_cache_destroy(cache);
}

static void test_now_next(void)
{
    struct oa_eit_cache *cache = oa_eit_cache_create(4);
    assert(cache);
    struct oa_eit_record a = record(1, 100, 1000, 2000, 5001, "Alpha News");
    struct oa_eit_record b = record(1, 101, 2000, 3000, 5001, "Alpha Movie");
    struct oa_eit_record empty = record(3, 300, 5000, 5000, 5003, "Empty");
    assert(oa_eit_cache_put(cache, &a) == 0);
    assert(oa_eit_cache_put(cache, &b) == 0);
    assert(oa_eit_cache_put(cache, &empty) == 0);

    struct oa_eit_record now, next;
    assert(oa_eit_cache_now_next(cache, 1, 500, &now, &next) == 0);
    assert(now.event_id == 0 && next.event_id == 100);
    assert(oa_eit_cache_now_next(cache, 1, 1000, &now, &next) == 0);
    assert(now.event_id == 100 && next.event_id == 101);
    assert(oa_eit_cache_now_next(cache, 1, 1999, &now, &next) == 0);
    assert(now.event_id == 100 && next.event_id == 101);
    /* End is exclusive. */
    assert(oa_eit_cache_now_next(cache, 1, 2000, &now, &next) == 0);
    assert(now.event_id == 101 && next.event_id == 0);
    assert(oa_eit_cache_now_next(cache, 1, 2999, &now, &next) == 0);
    assert(now.event_id == 101 && next.event_id == 0);
    assert(oa_eit_cache_now_next(cache, 1, 3000, &now, &next) == -ENOENT);
    assert(now.event_id == 0 && next.event_id == 0);
    /* An empty event is never now and does not start after itself. */
    assert(oa_eit_cache_now_next(cache, 3, 5000, &now, &next) == -ENOENT);
    assert(oa_eit_cache_now_next(cache, 3, 4999, &now, &next) == 0);
    assert(now.event_id == 0 && next.event_id == 300);

    assert(oa_eit_cache_now_next(cache, 0, 1000, &now, &next) == -EINVAL);
    assert(oa_eit_cache_now_next(cache, 1, 1000, NULL, NULL) == -EINVAL);
    assert(oa_eit_cache_now_next(cache, 1, 1000, &now, NULL) == 0);
    assert(now.event_id == 100);
    assert(oa_eit_cache_now_next(cache, 1, 500, NULL, &next) == 0);
    assert(next.event_id == 100);
    oa_eit_cache_destroy(cache);
}

int main(void)
{
    test_lifecycle();
    test_put_get();
    test_query();
    test_now_next();
    puts("ok eit cache: keyed schedule events, duration query and now/next");
    return 0;
}
