/* Pure Bronlijst row/type/picture-index map. No UI, source or settings I/O. */
#include <assert.h>
#include <stdio.h>

#include "openaurora/source_map.h"

int main(void)
{
    const int types[9] = { 0, 15, 17, 18, 19, 20, 16, 29, 30 };
    const int pictures[9] = { 0, 3, 5, 6, 7, 9, 10, 12, 13 };

    for (int row = 0; row < 9; row++) {
        assert(oa_source_type_for_row(row) == types[row]);
        assert(oa_source_picture_index(row) == pictures[row]);
        assert(oa_source_row_for_type(types[row]) == row);
    }
    assert(oa_source_row_for_type(-1) == -1);
    assert(oa_source_row_for_type(1) == -1);
    assert(oa_source_row_for_type(25) == -1);
    assert(oa_source_row_for_type(31) == -1);
    assert(oa_source_type_for_row(-1) == -1);
    assert(oa_source_type_for_row(9) == -1);
    assert(oa_source_picture_index(-1) == -1);
    assert(oa_source_picture_index(9) == -1);

    puts("ok source-map: nine vendor rows, source types, picture indexes and "
         "unknown/out-of-range rejection");
    return 0;
}
