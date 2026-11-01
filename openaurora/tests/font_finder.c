#include "openaurora/font_finder.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>

int main(void)
{
    struct oa_font_finder f = {0};
    unsigned id = 99;
    assert(oa_font_type_from_markers(0) == OA_FONT_REGULAR);
    assert(oa_font_type_from_markers(1u << OA_FONT_BOLD) == OA_FONT_BOLD);
    assert(oa_font_type_from_markers((1u << OA_FONT_BOLD) | (1u << OA_FONT_ITALIC)) == OA_FONT_BOLD);
    assert(oa_font_type_from_markers(UINT_MAX) == OA_FONT_REGULAR);
    for (unsigned type = 0; type < 6; ++type)
        assert(oa_font_type_from_markers(1u << type) == type);
    assert(!oa_font_finder_select(&f, 0, 0, 0, &id) && !id);
    assert(!oa_font_finder_add(&f, 100, 30, 0));
    assert(!oa_font_finder_add(&f, 101, 20, 1));
    assert(!oa_font_finder_add(&f, 102, 30, 0));
    assert(!oa_font_finder_add(&f, 103, 50, 0));
    assert(!oa_font_finder_select(&f, 29, 1, 0, &id) && id == 101);
    assert(!oa_font_finder_select(&f, 30, 1, 0, &id) && id == 102);
    assert(!oa_font_finder_select(&f, 99, 2, 0, &id) && id == 102);
    assert(!oa_font_finder_select(&f, 100, 2, 0, &id) && id == 103);
    assert(!oa_font_finder_select(&f, 100, 1, 1, &id) && id == 101);
    assert(!oa_font_finder_select(&f, 100, 1, 2, &id) && id == 103);
    assert(!oa_font_finder_select(&f, 100, 1, UINT_MAX, &id) && id == 103);
    assert(!oa_font_finder_select(&f, -100, 1, 0, &id) && id == 101);
    assert(!oa_font_finder_select(&f, -100, -2, 0, &id) && id == 103);
    assert(oa_font_finder_select(&f, 100, 0, 0, &id) == EDOM && id == 103);
    assert(oa_font_finder_select(&f, INT_MIN, -1, 0, &id) == EDOM && id == 103);
    /* Unknown registration type becomes default; equal heights retain order. */
    assert(!oa_font_finder_add(&f, 104, 30, UINT_MAX));
    assert(!oa_font_finder_select(&f, 30, 1, 0, &id) && id == 104);
    assert(!oa_font_finder_add(&f, 105, 0, 0));
    assert(!oa_font_finder_select(&f, 100, 1, 0, &id) && id == 105);
    while (f.count < OA_FONT_FINDER_CAPACITY)
        assert(!oa_font_finder_add(&f, 106, 100, 5));
    assert(oa_font_finder_add(&f, 107, 100, 5) == ENOSPC);
    puts("ok font finder: typed floor, global fallback, stable ties, division, bounds");
    return 0;
}
