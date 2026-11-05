#ifndef OPENAURORA_EIT_TIME_H
#define OPENAURORA_EIT_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Firmware-grounded DVB EIT time handling.
 *
 * Recovered from aurora.elf:
 *   EITEventTimeRange::set(unsigned char const*)        @0x00accaf9
 *   EITEventTimeRange::isValidTimeRangeData(...)        @0x00acca71
 *   EITEventTimeRange::set(unsigned int, unsigned int)  @0x00accc4d
 *   EITEventTimeRange::clear/makeInfinite/mergeWith/... @0x00acccc5+
 *
 * The vendor range stores absolute times as uint32 seconds since
 * 1970-01-01 UTC. EIT sections carry an MJD (big-endian, days since
 * 1858-11-17) plus BCD hh:mm:ss. 40587 is the MJD of the Unix epoch:
 *
 *   start = (mjd - 40587) * 86400 + hh*3600 + mm*60 + ss
 *   end   = start + duration_hh*3600 + duration_mm*60 + duration_ss
 *
 * 0/0 is empty, 0/0xffffffff is infinite. The functions below reproduce the
 * shipped arithmetic, including its sentinel handling and range-clip edges.
 * They are project code, not vendor ABI; callers exchange seconds here and
 * convert to milliseconds at the broadcast service boundary.
 */

#define OA_EIT_MJD_UNIX_EPOCH 40587u
#define OA_EIT_TIME_INFINITE_END UINT32_MAX

struct oa_eit_range {
    uint32_t begin; /* seconds since 1970-01-01 UTC; 0 with end 0xffffffff is
                       the infinite marker */
    uint32_t end;
};

/* Vendor BCD table decode: each nibble must be 0..9, otherwise 0. */
unsigned oa_eit_bcd(uint8_t value);

/* Validity rules from isValidTimeRangeData: MJD >= 40587, start hh<24 and
 * mm/ss<60; duration hours/minutes accept BCD 00..99, duration seconds <60. */
int oa_eit_time_valid(const uint8_t data[8]);

/* Unconditional decode matching EITEventTimeRange::set(data); callers that
 * need the vendor validity gate must check oa_eit_time_valid first. A NULL
 * pointer yields the empty range. */
void oa_eit_time_decode(const uint8_t data[8], struct oa_eit_range *out);

/* set(begin,end): end < begin normalises to the empty range 0/0. */
struct oa_eit_range oa_eit_range_set(uint32_t begin, uint32_t end);
void oa_eit_range_clear(struct oa_eit_range *range);
void oa_eit_range_make_infinite(struct oa_eit_range *range);
int oa_eit_range_is_infinite(const struct oa_eit_range *range);

/* Vendor predicates use plain comparisons; they do not special-case the
 * sentinels. */
int oa_eit_range_includes(const struct oa_eit_range *outer,
    const struct oa_eit_range *inner);
int oa_eit_range_intersects(const struct oa_eit_range *a,
    const struct oa_eit_range *b);

/* Union with the vendor's sentinel-aware begin/end rules. */
void oa_eit_range_merge(struct oa_eit_range *range,
    const struct oa_eit_range *other);

/* grow() subtracts from begin and adds to end unless that edge is a sentinel;
 * shift() moves both non-sentinel edges. */
void oa_eit_range_grow(struct oa_eit_range *range, int32_t seconds);
void oa_eit_range_shift(struct oa_eit_range *range, int32_t seconds);

/* Vendor clip returns 1 on most branches, including an empty `other` outside
 * the range and start-edge trims. It returns 0 for a range that strictly
 * contains `other` (including an empty `other` inside) and for disjoint
 * ranges. Identical or covered ranges collapse to empty and report 1. */
int oa_eit_range_clip(struct oa_eit_range *range,
    const struct oa_eit_range *other);

/* Vendor moveInto: clip into `other` when finite, else become `other`. */
void oa_eit_range_move_into(struct oa_eit_range *range,
    const struct oa_eit_range *other);

#ifdef __cplusplus
}
#endif

#endif
