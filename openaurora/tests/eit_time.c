/* Firmware-grounded EIT time codec/range tests. Expected values come from the
 * formulas and edge behaviour recovered from aurora.elf; no vendor code is
 * executed. */
#include "openaurora/eit_time.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_bcd(void)
{
    assert(oa_eit_bcd(0x00) == 0);
    assert(oa_eit_bcd(0x09) == 9);
    assert(oa_eit_bcd(0x10) == 10);
    assert(oa_eit_bcd(0x23) == 23);
    assert(oa_eit_bcd(0x59) == 59);
    assert(oa_eit_bcd(0x99) == 99);
    /* The shipped table maps invalid bytes to zero, not to a partial value. */
    assert(oa_eit_bcd(0x0a) == 0);
    assert(oa_eit_bcd(0x1a) == 0);
    assert(oa_eit_bcd(0xa0) == 0);
    assert(oa_eit_bcd(0x9a) == 0);
    assert(oa_eit_bcd(0xff) == 0);
}

static void test_decode(void)
{
    struct oa_eit_range range;
    /* 1970-01-01 00:00:00 + 60s. */
    const uint8_t epoch[8] = { 0x9e, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 };
    assert(oa_eit_time_valid(epoch));
    oa_eit_time_decode(epoch, &range);
    assert(range.begin == 0 && range.end == 60);

    /* MJD 60000 = 2023-02-25; 23:59:58 + 01:02:03. */
    const uint8_t sample[8] = { 0xea, 0x60, 0x23, 0x59, 0x58, 0x01, 0x02, 0x03 };
    assert(oa_eit_time_valid(sample));
    oa_eit_time_decode(sample, &range);
    assert(range.begin == 1677369598u);
    assert(range.end == 1677373321u);

    /* Invalid BCD still decodes through the vendor table (invalid bytes -> 0),
     * which is why validity must be checked separately. */
    const uint8_t bad_bcd[8] = { 0xea, 0x60, 0x1a, 0x59, 0x58, 0x00, 0x00, 0x00 };
    assert(!oa_eit_time_valid(bad_bcd));
    oa_eit_time_decode(bad_bcd, &range);
    assert(range.begin == 1677283200u + 59u * 60u + 58u);

    oa_eit_time_decode(NULL, &range);
    assert(range.begin == 0 && range.end == 0);
}

static void test_validity(void)
{
    uint8_t data[8] = { 0x9e, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    assert(oa_eit_time_valid(data));
    data[0] = 0x9e; data[1] = 0x8a; /* MJD 40586 */
    assert(!oa_eit_time_valid(data));
    data[1] = 0x8b;

    data[2] = 0x23; assert(oa_eit_time_valid(data));
    data[2] = 0x24; assert(!oa_eit_time_valid(data));
    data[2] = 0x19; assert(oa_eit_time_valid(data));
    data[2] = 0x1a; assert(!oa_eit_time_valid(data));
    data[2] = 0x00;
    data[3] = 0x59; assert(oa_eit_time_valid(data));
    data[3] = 0x60; assert(!oa_eit_time_valid(data));
    data[3] = 0x00;
    data[4] = 0x59; assert(oa_eit_time_valid(data));
    data[4] = 0x60; assert(!oa_eit_time_valid(data));
    data[4] = 0x00;
    /* Duration hours/minutes use the full BCD 00..99 table in the shipped
     * validity check; duration seconds are capped at 59. */
    data[5] = 0x99; assert(oa_eit_time_valid(data));
    data[6] = 0x99; assert(oa_eit_time_valid(data));
    data[7] = 0x59; assert(oa_eit_time_valid(data));
    data[7] = 0x60; assert(!oa_eit_time_valid(data));
    assert(!oa_eit_time_valid(NULL));
}

static void test_set_clear_infinite(void)
{
    struct oa_eit_range range = oa_eit_range_set(10, 20);
    assert(range.begin == 10 && range.end == 20);
    range = oa_eit_range_set(500, 400);
    assert(range.begin == 0 && range.end == 0);
    range = oa_eit_range_set(400, 400);
    assert(range.begin == 400 && range.end == 400);

    oa_eit_range_clear(&range);
    assert(range.begin == 0 && range.end == 0);
    assert(!oa_eit_range_is_infinite(&range));
    oa_eit_range_make_infinite(&range);
    assert(range.begin == 0 && range.end == 0xffffffffu);
    assert(oa_eit_range_is_infinite(&range));
}

static void test_predicates(void)
{
    struct oa_eit_range outer = { 10, 20 };
    struct oa_eit_range inner = { 11, 19 };
    struct oa_eit_range same = { 10, 20 };
    struct oa_eit_range over = { 9, 21 };
    struct oa_eit_range after = { 20, 30 };
    struct oa_eit_range overlap = { 19, 21 };
    struct oa_eit_range before = { 0, 10 };
    assert(oa_eit_range_includes(&outer, &inner));
    assert(oa_eit_range_includes(&outer, &same));
    assert(!oa_eit_range_includes(&outer, &over));
    assert(!oa_eit_range_includes(&inner, &outer));
    assert(oa_eit_range_intersects(&outer, &inner));
    assert(oa_eit_range_intersects(&outer, &overlap));
    assert(!oa_eit_range_intersects(&outer, &after));
    assert(!oa_eit_range_intersects(&outer, &before));
    /* Intersections are half-open: touching edges do not overlap, equal
     * non-empty ranges do. */
    assert(oa_eit_range_intersects(&outer, &same));
    assert(oa_eit_range_intersects(&after, &after));
}

static void test_merge(void)
{
    struct oa_eit_range range = { 0, 0 };
    struct oa_eit_range other = { 10, 20 };
    oa_eit_range_merge(&range, &other);
    assert(range.begin == 10 && range.end == 20);

    range = oa_eit_range_set(10, 20);
    other = oa_eit_range_set(5, 15);
    oa_eit_range_merge(&range, &other);
    assert(range.begin == 5 && range.end == 20);

    range = oa_eit_range_set(10, 20);
    other = oa_eit_range_set(15, 30);
    oa_eit_range_merge(&range, &other);
    assert(range.begin == 10 && range.end == 30);

    /* A zero begin is an unknown lower edge and is replaced. */
    range = oa_eit_range_set(0, 20);
    other = oa_eit_range_set(10, 30);
    oa_eit_range_merge(&range, &other);
    assert(range.begin == 10 && range.end == 30);

    /* A zero end is not 0xffffffff, so it is replaced by the other end. */
    range = oa_eit_range_set(10, 0);
    other = oa_eit_range_set(5, 15);
    oa_eit_range_merge(&range, &other);
    assert(range.begin == 5 && range.end == 15);
}

static void test_grow_shift(void)
{
    struct oa_eit_range range = oa_eit_range_set(100, 200);
    oa_eit_range_grow(&range, 5);
    assert(range.begin == 95 && range.end == 205);
    oa_eit_range_shift(&range, -10);
    assert(range.begin == 85 && range.end == 195);

    range = oa_eit_range_set(0, 200);
    oa_eit_range_grow(&range, 5);
    assert(range.begin == 0 && range.end == 205);
    range = oa_eit_range_set(100, 0xffffffffu);
    oa_eit_range_grow(&range, 5);
    assert(range.begin == 95 && range.end == 0xffffffffu);
    oa_eit_range_shift(&range, 7);
    assert(range.begin == 102 && range.end == 0xffffffffu);
}

static void test_clip(void)
{
    struct oa_eit_range range = oa_eit_range_set(10, 20);
    struct oa_eit_range other = oa_eit_range_set(10, 20);
    assert(oa_eit_range_clip(&range, &other) == 1);
    assert(range.begin == 20 && range.end == 20);

    range = oa_eit_range_set(10, 20);
    other = oa_eit_range_set(30, 40);
    assert(oa_eit_range_clip(&range, &other) == 0);
    assert(range.begin == 10 && range.end == 20);

    /* A range that strictly contains the other is left untouched. */
    range = oa_eit_range_set(10, 30);
    other = oa_eit_range_set(15, 20);
    assert(oa_eit_range_clip(&range, &other) == 0);
    assert(range.begin == 10 && range.end == 30);

    /* Trimming the end edge is reported. */
    range = oa_eit_range_set(10, 20);
    other = oa_eit_range_set(15, 30);
    assert(oa_eit_range_clip(&range, &other) == 1);
    assert(range.begin == 10 && range.end == 15);

    /* Trimming the start edge is reported as a change. */
    range = oa_eit_range_set(10, 20);
    other = oa_eit_range_set(5, 15);
    assert(oa_eit_range_clip(&range, &other) == 1);
    assert(range.begin == 15 && range.end == 20);

    /* An empty other strictly inside the range is the early no-op branch. */
    range = oa_eit_range_set(10, 20);
    other = oa_eit_range_set(15, 15);
    assert(oa_eit_range_clip(&range, &other) == 0);
    assert(range.begin == 10 && range.end == 20);

    /* An empty other outside the range reports 1 without changing it. */
    range = oa_eit_range_set(10, 20);
    other = oa_eit_range_set(30, 30);
    assert(oa_eit_range_clip(&range, &other) == 1);
    assert(range.begin == 10 && range.end == 20);
}

static void test_move_into(void)
{
    struct oa_eit_range range = oa_eit_range_set(10, 20);
    struct oa_eit_range other = oa_eit_range_set(5, 15);
    oa_eit_range_move_into(&range, &other);
    assert(range.begin == 5 && range.end == 15);

    range = oa_eit_range_set(10, 20);
    other = oa_eit_range_set(30, 40);
    oa_eit_range_move_into(&range, &other);
    assert(range.begin == 30 && range.end == 40);

    range = oa_eit_range_set(10, 20);
    other = oa_eit_range_set(5, 30);
    oa_eit_range_move_into(&range, &other);
    assert(range.begin == 10 && range.end == 20);

    oa_eit_range_make_infinite(&range);
    other = oa_eit_range_set(30, 40);
    oa_eit_range_move_into(&range, &other);
    assert(range.begin == 30 && range.end == 40);
}

int main(void)
{
    test_bcd();
    test_decode();
    test_validity();
    test_set_clear_infinite();
    test_predicates();
    test_merge();
    test_grow_shift();
    test_clip();
    test_move_into();
    puts("ok eit time: firmware BCD/MJD decode, range algebra and sentinel edges");
    return 0;
}
