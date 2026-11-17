/* PVR disk-space arithmetic/labels: the vendor's KiB-as-MB convention. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/pvr_space.h"

int main(void)
{
    const uint64_t gib = 1024ull * 1024ull * 1024ull;
    struct oa_pvr_stats in = {
        .total_bytes = 10 * gib,
        .free_bytes = 4 * gib,
        .reserved_bytes = 1 * gib
    };
    oa_pvr_space_values v;
    assert(oa_pvr_space_calc(&in, &v) == 0);
    assert(v.used == 6 * 1024 * 1024);
    assert(v.reserved == 1024 * 1024);
    assert(v.free == 3 * 1024 * 1024);

    /* Reserved clamps to free; used cannot exceed total. */
    in.free_bytes = 512 * 1024 * 1024;
    in.reserved_bytes = 2 * gib;
    assert(oa_pvr_space_calc(&in, &v) == 0);
    assert(v.reserved == 512 * 1024);
    assert(v.free == 0);

    in.free_bytes = 12 * gib;           /* bogus free > total */
    in.reserved_bytes = 0;
    assert(oa_pvr_space_calc(&in, &v) == 0);
    assert(v.used == 0 && v.free == 10 * 1024 * 1024);

    in.total_bytes = 0;
    assert(oa_pvr_space_calc(&in, &v) == -ENODATA);
    assert(oa_pvr_space_calc(NULL, &v) == -EINVAL);
    assert(oa_pvr_space_calc(&in, NULL) == -EINVAL);

    char label[64];
    assert(oa_pvr_space_label("Gebruikte ruimte: ", 6291456, label,
                              sizeof label) == 0);
    assert(!strcmp(label, "Gebruikte ruimte: 6291456 MB"));
    assert(oa_pvr_space_label("Used: ", 0, label, sizeof label) == 0);
    assert(!strcmp(label, "Used: 0 MB"));
    assert(oa_pvr_space_label("x", 1, label, 2) == -EOVERFLOW);
    assert(oa_pvr_space_label(NULL, 1, label, sizeof label) == -EINVAL);
    assert(oa_pvr_space_label("x", 1, NULL, 8) == -EINVAL);

    puts("ok pvr space: vendor KiB-as-MB used/reserved/free and labels");
    return 0;
}
