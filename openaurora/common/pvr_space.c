/* PVR disk-space arithmetic and labels. Pure; no I/O. */
#include "openaurora/pvr_space.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>

int oa_pvr_space_calc(const struct oa_pvr_stats *in, oa_pvr_space_values *out)
{
    if (!in || !out)
        return -EINVAL;
    if (in->total_bytes == 0)
        return -ENODATA;
    uint64_t total_kb = in->total_bytes >> 10;
    uint64_t free_kb = in->free_bytes >> 10;
    uint64_t used = in->free_bytes <= in->total_bytes
                        ? (in->total_bytes - in->free_bytes) >> 10
                        : 0;
    uint64_t reserved = in->reserved_bytes >> 10;
    if (reserved > free_kb)
        reserved = free_kb;
    if (used > total_kb)
        used = total_kb;
    uint64_t remaining = total_kb - used;
    uint64_t free_value = reserved < remaining ? remaining - reserved : 0;
    out->used = used;
    out->reserved = reserved;
    out->free = free_value;
    return 0;
}

int oa_pvr_space_label(const char *title, uint64_t value, char *out, size_t cap)
{
    if (!title || !out || cap == 0)
        return -EINVAL;
    int n = snprintf(out, cap, "%s%" PRIu64 " MB", title, value);
    if (n < 0 || (size_t)n >= cap)
        return -EOVERFLOW;
    return 0;
}
