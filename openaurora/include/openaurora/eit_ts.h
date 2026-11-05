#ifndef OPENAURORA_EIT_TS_H
#define OPENAURORA_EIT_TS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DVB MPEG-2 transport-stream EIT demux (project implementation).
 *
 * Filters one PID (DVB EIT is normally PID 0x12), handles PUSI pointer fields,
 * adaptation fields, continuity errors/duplicates and sections split across
 * packets. Completed sections are passed to the callback; EIT table ids
 * 0x4e..0x6f are forwarded, other table ids are assembled only to be skipped.
 * Section CRC/table validation remains with oa_eit_parse_section().
 */

#define OA_EIT_TS_PACKET_SIZE 188
#define OA_EIT_TS_SECTION_MAX 4096

struct oa_eit_ts {
    uint16_t pid;
    uint8_t section[OA_EIT_TS_SECTION_MAX];
    size_t length;
    size_t expected;
    int discard;
    int have_cc;
    uint8_t cc;
};

void oa_eit_ts_init(struct oa_eit_ts *ts, uint16_t pid);
void oa_eit_ts_reset(struct oa_eit_ts *ts);

/* Return 0 to continue, negative to stop the feed. */
typedef int (*oa_eit_ts_section_cb)(const uint8_t *section, size_t len,
    void *user);

/* Feed exactly one 188-byte packet. Returns the number of completed sections
 * (possibly more than one), or -EINVAL for a malformed packet and a negative
 * callback error as-is. Transport/continuity errors drop only the partial
 * section. */
int oa_eit_ts_feed(struct oa_eit_ts *ts, const uint8_t *packet, size_t len,
    oa_eit_ts_section_cb cb, void *user);

#ifdef __cplusplus
}
#endif

#endif
