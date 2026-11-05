/* Firmware-grounded DVB EIT time codec and range algebra. See
 * include/openaurora/eit_time.h for provenance and exact vendor addresses. */
#include "openaurora/eit_time.h"

#include <stddef.h>

unsigned oa_eit_bcd(uint8_t value)
{
    unsigned high = value >> 4;
    unsigned low = value & 0x0fu;
    if (high > 9 || low > 9) return 0;
    return high * 10u + low;
}

int oa_eit_time_valid(const uint8_t data[8])
{
    if (!data) return 0;
    unsigned mjd = ((unsigned)data[0] << 8) | data[1];
    if (mjd < OA_EIT_MJD_UNIX_EPOCH) return 0;
    unsigned hour_hi = data[2] >> 4, hour_lo = data[2] & 0x0fu;
    if (hour_lo > 9 || hour_hi > 2 || (hour_hi == 2 && hour_lo > 3)) return 0;
    if ((data[3] >> 4) > 5 || (data[3] & 0x0fu) > 9) return 0;
    if ((data[4] >> 4) > 5 || (data[4] & 0x0fu) > 9) return 0;
    if ((data[5] >> 4) > 9 || (data[5] & 0x0fu) > 9) return 0;
    if ((data[6] >> 4) > 9 || (data[6] & 0x0fu) > 9) return 0;
    if ((data[7] >> 4) > 5 || (data[7] & 0x0fu) > 9) return 0;
    return 1;
}

void oa_eit_time_decode(const uint8_t data[8], struct oa_eit_range *out)
{
    if (!out) return;
    if (!data) {
        out->begin = 0;
        out->end = 0;
        return;
    }
    unsigned mjd = ((unsigned)data[0] << 8) | data[1];
    uint32_t start = (mjd - OA_EIT_MJD_UNIX_EPOCH) * 86400u +
        oa_eit_bcd(data[2]) * 3600u + oa_eit_bcd(data[3]) * 60u +
        oa_eit_bcd(data[4]);
    uint32_t duration = oa_eit_bcd(data[5]) * 3600u +
        oa_eit_bcd(data[6]) * 60u + oa_eit_bcd(data[7]);
    out->begin = start;
    out->end = start + duration;
}

struct oa_eit_range oa_eit_range_set(uint32_t begin, uint32_t end)
{
    struct oa_eit_range range;
    if (end < begin) {
        range.begin = 0;
        range.end = 0;
    } else {
        range.begin = begin;
        range.end = end;
    }
    return range;
}

void oa_eit_range_clear(struct oa_eit_range *range)
{
    if (!range) return;
    range->begin = 0;
    range->end = 0;
}

void oa_eit_range_make_infinite(struct oa_eit_range *range)
{
    if (!range) return;
    range->begin = 0;
    range->end = OA_EIT_TIME_INFINITE_END;
}

int oa_eit_range_is_infinite(const struct oa_eit_range *range)
{
    return range && range->begin == 0 &&
        range->end == OA_EIT_TIME_INFINITE_END;
}

int oa_eit_range_includes(const struct oa_eit_range *outer,
    const struct oa_eit_range *inner)
{
    if (!outer || !inner) return 0;
    return inner->begin >= outer->begin && inner->end <= outer->end;
}

int oa_eit_range_intersects(const struct oa_eit_range *a,
    const struct oa_eit_range *b)
{
    if (!a || !b) return 0;
    return a->begin < b->end && b->begin < a->end;
}

void oa_eit_range_merge(struct oa_eit_range *range,
    const struct oa_eit_range *other)
{
    if (!range || !other) return;
    if (range->begin == range->end) {
        range->begin = other->begin;
        range->end = other->end;
        return;
    }
    if (range->begin == 0 || other->begin <= range->begin)
        range->begin = other->begin;
    if (!(range->end != 0 && other->end <= range->end))
        range->end = other->end;
}

void oa_eit_range_grow(struct oa_eit_range *range, int32_t seconds)
{
    if (!range) return;
    if (range->begin != 0) range->begin = (uint32_t)((int32_t)range->begin -
        seconds);
    if (range->end != OA_EIT_TIME_INFINITE_END)
        range->end = (uint32_t)((int32_t)range->end + seconds);
}

void oa_eit_range_shift(struct oa_eit_range *range, int32_t seconds)
{
    if (!range) return;
    if (range->begin != 0) range->begin = (uint32_t)((int32_t)range->begin +
        seconds);
    if (range->end != OA_EIT_TIME_INFINITE_END)
        range->end = (uint32_t)((int32_t)range->end + seconds);
}

int oa_eit_range_clip(struct oa_eit_range *range,
    const struct oa_eit_range *other)
{
    if (!range || !other) return 0;
    int includes_other = oa_eit_range_includes(range, other);
    uint32_t other_begin = other->begin;
    uint32_t other_end = other->end;
    if (includes_other && range->begin != other_begin &&
        range->end != other_end)
        return 0;
    if (other_begin != other_end) {
        if (oa_eit_range_includes(other, range)) {
            range->begin = range->end;
            return 1;
        }
        int changed = 0;
        if (range->begin < other_begin && other_begin < range->end) {
            changed = 1;
            range->end = other_begin;
        }
        if (other_end > range->begin && other_end < range->end) {
            range->begin = other_end;
            return 1;
        }
        return changed;
    }
    return 1;
}

void oa_eit_range_move_into(struct oa_eit_range *range,
    const struct oa_eit_range *other)
{
    if (!range || !other) return;
    if (!oa_eit_range_is_infinite(range)) {
        if (range->begin < other->begin) {
            range->end = (uint32_t)(other->begin + range->end - range->begin);
            range->begin = other->begin;
        }
        if (other->end < range->end) {
            range->begin = (uint32_t)(other->end - (range->end - range->begin));
            range->end = other->end;
        }
        if (oa_eit_range_includes(other, range)) return;
    }
    range->begin = other->begin;
    range->end = other->end;
}
