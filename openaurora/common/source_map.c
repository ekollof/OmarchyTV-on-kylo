#include "openaurora/source_map.h"

static const int source_types[9] = {
    0, 15, 17, 18, 19, 20, 16, 29, 30
};
static const int picture_indexes[9] = {
    0, 3, 5, 6, 7, 9, 10, 12, 13
};

int oa_source_row_for_type(int type)
{
    for (int row = 0; row < 9; row++)
        if (source_types[row] == type) return row;
    return -1;
}

int oa_source_type_for_row(int row)
{
    if (row < 0 || row >= 9) return -1;
    return source_types[row];
}

int oa_source_picture_index(int row)
{
    if (row < 0 || row >= 9) return -1;
    return picture_indexes[row];
}
