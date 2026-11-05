#include <errno.h>
#include <string.h>

#include "openaurora/eit_ts.h"

void oa_eit_ts_reset(struct oa_eit_ts *ts)
{
    if (!ts) return;
    ts->length = 0;
    ts->expected = 0;
    ts->discard = 0;
    ts->have_cc = 0;
    ts->cc = 0;
}

void oa_eit_ts_init(struct oa_eit_ts *ts, uint16_t pid)
{
    if (!ts) return;
    memset(ts, 0, sizeof *ts);
    ts->pid = pid;
}

/* Parse as many complete sections as the buffer holds. A non-EIT table is
 * assembled only to be skipped; CRC/table validation stays in eit_section. */
static int parse_sections(struct oa_eit_ts *ts, oa_eit_ts_section_cb cb,
    void *user, int *count)
{
    while (ts->length >= 3) {
        if (ts->expected == 0) {
            if (ts->section[0] == 0xff) { /* end-of-payload stuffing */
                ts->length = 0;
                return 0;
            }
            unsigned section_length = ((unsigned)(ts->section[1] & 0x0f) << 8) |
                ts->section[2];
            unsigned table_id = ts->section[0];
            if (section_length < 4 ||
                section_length > OA_EIT_TS_SECTION_MAX - 3) {
                ts->length = 0;
                return 0;
            }
            ts->expected = 3 + section_length;
            ts->discard = table_id < 0x4e || table_id > 0x6f;
        }
        if (ts->length < ts->expected) return 0;
        if (!ts->discard) {
            int rc = cb(ts->section, ts->expected, user);
            if (rc < 0) return rc;
            (*count)++;
        }
        size_t remaining = ts->length - ts->expected;
        if (remaining)
            memmove(ts->section, ts->section + ts->expected, remaining);
        ts->length = remaining;
        ts->expected = 0;
        ts->discard = 0;
    }
    return 0;
}

static int append(struct oa_eit_ts *ts, const uint8_t *data, size_t len,
    oa_eit_ts_section_cb cb, void *user, int *count)
{
    if (!len) return 0;
    if (len > sizeof ts->section - ts->length) {
        ts->length = 0;
        ts->expected = 0;
        ts->discard = 0;
        return 0;
    }
    memcpy(ts->section + ts->length, data, len);
    ts->length += len;
    return parse_sections(ts, cb, user, count);
}

int oa_eit_ts_feed(struct oa_eit_ts *ts, const uint8_t *packet, size_t len,
    oa_eit_ts_section_cb cb, void *user)
{
    if (!ts || !packet || len != OA_EIT_TS_PACKET_SIZE || packet[0] != 0x47)
        return -EINVAL;
    uint16_t pid = (uint16_t)(((packet[1] & 0x1f) << 8) | packet[2]);
    if (pid != ts->pid) return 0;
    if (packet[1] & 0x80) { /* transport_error_indicator */
        ts->length = 0;
        ts->expected = 0;
        ts->discard = 0;
        return 0;
    }
    unsigned afc = (packet[3] >> 4) & 0x3;
    if (afc == 0) return -EINVAL;
    int pusi = (packet[1] & 0x40) != 0;
    size_t offset = 4;
    if (afc == 2) {
        if (packet[4] > 183) return -EINVAL;
        return 0;
    }
    if (afc == 3) {
        unsigned adaptation_length = packet[4];
        if (adaptation_length > 183) return -EINVAL;
        offset = 5 + adaptation_length;
        if (offset >= OA_EIT_TS_PACKET_SIZE) return 0;
    }
    if (packet[3] & 0xc0) { /* scrambled payload */
        ts->length = 0;
        ts->expected = 0;
        ts->discard = 0;
        return 0;
    }

    uint8_t cc = packet[3] & 0x0f;
    if (ts->have_cc) {
        if (cc == ts->cc) return 0; /* duplicate packet */
        if (cc != (uint8_t)((ts->cc + 1) & 0x0f)) {
            ts->length = 0;
            ts->expected = 0;
            ts->discard = 0;
        }
    }
    ts->cc = cc;
    ts->have_cc = 1;

    const uint8_t *payload = packet + offset;
    size_t payload_len = OA_EIT_TS_PACKET_SIZE - offset;
    int count = 0;
    int rc;

    if (pusi) {
        if (payload_len == 0) {
            ts->length = 0;
            ts->expected = 0;
            ts->discard = 0;
            return 0;
        }
        unsigned pointer = payload[0];
        if (pointer > payload_len - 1) {
            ts->length = 0;
            ts->expected = 0;
            ts->discard = 0;
            return 0;
        }
        if (ts->length > 0) {
            size_t need = ts->expected > ts->length ?
                ts->expected - ts->length : 0;
            size_t take = pointer < need ? pointer : need;
            rc = append(ts, payload + 1, take, cb, user, &count);
            if (rc < 0) return rc;
            ts->length = 0;
            ts->expected = 0;
            ts->discard = 0;
        }
        payload += 1 + pointer;
        payload_len -= 1 + pointer;
    } else if (ts->length == 0) {
        /* Continuation without a known section start: no assembly. */
        return 0;
    }

    rc = append(ts, payload, payload_len, cb, user, &count);
    if (rc < 0) return rc;
    return count;
}
