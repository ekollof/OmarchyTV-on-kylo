#ifndef OPENAURORA_PVR_SPACE_H
#define OPENAURORA_PVR_SPACE_H

#include <stddef.h>
#include <stdint.h>

#include "openaurora/broadcast.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PVR disk-space labels, recovered from PlayListDialog::refreshSpaceUsage
 * 0x00df14e8. The vendor works in 1024-byte blocks and labels them "MB":
 *   used     = (total - free) >> 10
 *   reserved = reserved_bytes >> 10, clamped to free >> 10
 *   free     = (total >> 10) - used - reserved, floored at 0
 * The labels are `USED_SPACE_INFO_TITLE`, `RESERVED_SPACE_INFO_TITLE` and
 * `FREE_SPACE_INFO_TITLE` followed by the number and `" MB"` (dut titles
 * "Gebruikte ruimte: ", "Gereserveerd: ", "Vrije ruimte: "). The vendor gates
 * the whole block on total != 0.
 *
 * This helper performs no I/O. It reproduces the vendor unit convention
 * (KiB shown with an MB label), not a corrected IEC display.
 */
typedef struct {
    uint64_t used;
    uint64_t reserved;
    uint64_t free;
} oa_pvr_space_values;

/* Returns 0 and fills *out, -EINVAL for NULL, -ENODATA when total_bytes is 0
 * (the vendor then paints nothing). */
int oa_pvr_space_calc(const struct oa_pvr_stats *in, oa_pvr_space_values *out);

/* Writes "<title><value> MB" (title used verbatim, so the dut title already
 * ends in ": "). Returns 0 or -EINVAL/-EOVERFLOW. */
int oa_pvr_space_label(const char *title, uint64_t value, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
