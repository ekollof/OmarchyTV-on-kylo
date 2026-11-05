#ifndef OPENAURORA_EIT_SECTION_H
#define OPENAURORA_EIT_SECTION_H

#include <stddef.h>
#include <stdint.h>

#include "openaurora/eit_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DVB EIT section parser (project implementation, EN 300 468 shape).
 *
 * Parses one complete section including the 3-byte header and trailing
 * MPEG-2 CRC32. Supported table ids are 0x4e..0x6f: present/following
 * (0x4e actual, 0x4f other) and schedule (0x50..0x5f actual, 0x60..0x6f
 * other). `actual` is derived from the table id.
 *
 * Event start/duration decoding is delegated to common/eit_time.c, including
 * its MJD/BCD validity rules. Events with invalid time data are skipped; a
 * structurally invalid section or bad CRC fails the whole parse.
 */

#define OA_EIT_NAME_MAX 96
#define OA_EIT_TEXT_MAX 160

struct oa_eit_event_info {
    uint16_t service_id;
    uint16_t event_id;
    struct oa_eit_range range;      /* seconds */
    uint8_t running_status;         /* 0..7 */
    uint8_t free_ca_mode;
    uint8_t actual;
    uint8_t version;                /* section version 0..31 */
    char name[OA_EIT_NAME_MAX];
    char text[OA_EIT_TEXT_MAX];
};

/* Return 0 to continue, nonzero (negative) to stop the parse. */
typedef int (*oa_eit_event_cb)(const struct oa_eit_event_info *event, void *user);

/* Returns the number of events delivered, 0 for a valid non-current section,
 * or -EINVAL for malformed input/bad CRC. Callback errors are returned as-is. */
int oa_eit_parse_section(const uint8_t *data, size_t len,
    oa_eit_event_cb cb, void *user);

#ifdef __cplusplus
}
#endif

#endif
