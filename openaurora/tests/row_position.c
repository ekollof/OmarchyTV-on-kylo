/* Pure row-position model tests. No UI, SVG or firmware I/O. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/row_position.h"

static int query_calls;
static int query_fail;
static int query_rect[4] = { 10, 20, 810, 290 };

static int fake_query(void *ud, int rect[4])
{
    (void)ud;
    query_calls++;
    if (query_fail)
        return -1;
    memcpy(rect, query_rect, sizeof query_rect);
    return 0;
}

static oa_row_position_fields fields(int e8, int e9)
{
    oa_row_position_fields f;
    memset(&f, 0, sizeof f);
    f.e8 = e8; f.e9 = e9;
    f.f204 = 30; f.f208 = 5; f.f20c = 40; f.f210 = 7;
    f.f1f4 = 100; f.f1f8 = 3; f.f1fc = 20; f.f200 = 4;
    return f;
}

int main(void)
{
    oa_row_position_io io = { NULL, fake_query };
    oa_row_position_fields f;
    int out[4] = { -1, -1, -1, -1 };

    query_calls = 0; query_fail = 0;

    /* Branch +0xe8 == 0: x=+0x204, step=+0x1f4*i + +0x1fc.
     * Horizontal term is (rect.x1 - rect.x0) - +0x208; the +0x204 addend
     * cancels the +0x204 subtrahend. Nonzero step and index make the old
     * "add the row step" defect visible (it would yield 985, not 795). */
    f = fields(0, 0);
    assert(oa_row_position(&io, &f, 2, out) == 0);
    assert(out[0] == 30 && out[1] == 220);
    assert(out[2] == (810 - 10) - 5 && out[3] == 223);

    /* Same branch at index 0: p2.x must not depend on the row step. */
    assert(oa_row_position(&io, &f, 0, out) == 0);
    assert(out[0] == 30 && out[1] == 20);
    assert(out[2] == (810 - 10) - 5 && out[3] == 23);

    /* Branch +0xe8 != 0, +0xe9 == 0: step=+0x20c*i + +0x204. */
    f = fields(1, 0);
    assert(oa_row_position(&io, &f, 2, out) == 0);
    assert(out[0] == 110 && out[1] == 20 && out[2] == 117);
    assert(out[3] == (290 - 20) - 4);

    /* Branch +0xe8 != 0, +0xe9 != 0: right-anchored step. */
    f = fields(1, 1);
    assert(oa_row_position(&io, &f, 2, out) == 0);
    assert(out[0] == 810 - 30 - (40 * 2 + 40));
    assert(out[1] == 20 && out[2] == out[0] + 7);

    assert(query_calls == 4);

    /* Rejections and query failure preserve outputs. */
    memset(out, 0x55, sizeof out);
    assert(oa_row_position(NULL, &f, 0, out) == -EINVAL);
    assert(oa_row_position(&io, NULL, 0, out) == -EINVAL);
    assert(oa_row_position(&io, &f, -1, out) == -EINVAL);
    assert(oa_row_position(&io, &f, 0, NULL) == -EINVAL);
    query_fail = 1;
    assert(oa_row_position(&io, &f, 0, out) == -EIO);
    query_fail = 0;
    for (size_t i = 0; i < sizeof out; i++)
        assert(((unsigned char *)out)[i] == 0x55);

    puts("ok row position: three instruction-recovered branches, EASIRect "
         "point packing and query failure");
    return 0;
}
