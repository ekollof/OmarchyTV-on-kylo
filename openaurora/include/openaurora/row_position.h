#ifndef OPENAURORA_ROW_POSITION_H
#define OPENAURORA_ROW_POSITION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pure model of SVGListBoxBase::getRowPosition @0x010bbaa8 (aurora.elf MD5
 * 64c252340d3af6c5d66072611b7bad7a), instruction-checked 2026-11-16.
 *
 * EASIRect packs two EASIPoints as {p1.x,p1.y,p2.x,p2.y}
 * (EASIRect::EASIRect @0x009a8004). The virtual vtable+0x28 query supplies a
 * base rect; the three branches are selected by the bytes at this+0xe8/+0xe9.
 * This helper performs no UI, SVG or host I/O: the caller injects the query
 * and the raw field values. Field assignment from rowPos/mRowM is still
 * unresolved, so this is not a stock-UI parity claim.
 */
typedef struct {
    void *ud;
    /* Fills out[4] with the virtual base rect; 0 success, negative errno. */
    int (*query)(void *ud, int rect[4]);
} oa_row_position_io;

typedef struct {
    int f204, f208, f20c, f210;
    int f1f4, f1f8, f1fc, f200;
    int e8, e9; /* boolean flags at this+0xe8 / +0xe9 */
} oa_row_position_fields;

/* index is the row number. Returns 0 and a 4-int EASIRect, or -EINVAL/-EIO. */
int oa_row_position(const oa_row_position_io *io,
                    const oa_row_position_fields *f, int index, int out[4]);

#ifdef __cplusplus
}
#endif

#endif
