/* DVB EIT section parser tests. Sections are built locally with a correct
 * MPEG-2 CRC; no vendor code or TV data is used. */
#include "openaurora/eit_section.h"
#include "openaurora/eit_ts.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

struct build {
    uint8_t buf[2048];
    size_t len;
};

static void emit8(struct build *b, uint8_t value)
{
    assert(b->len < sizeof b->buf);
    b->buf[b->len++] = value;
}

static void emit16(struct build *b, uint16_t value)
{
    emit8(b, (uint8_t)(value >> 8));
    emit8(b, (uint8_t)value);
}

static void emit_bytes(struct build *b, const uint8_t *data, size_t len)
{
    assert(b->len + len <= sizeof b->buf);
    memcpy(b->buf + b->len, data, len);
    b->len += len;
}

static uint32_t crc32_mpeg(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i] << 24;
        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 0x80000000u) ? (crc << 1) ^ 0x04c11db7u : crc << 1;
    }
    return crc;
}

static void section_header(struct build *b, uint8_t table_id,
    uint16_t service_id, uint8_t version, int current)
{
    emit8(b, table_id);
    emit8(b, 0xb0);
    emit8(b, 0);
    emit16(b, service_id);
    emit8(b, (uint8_t)((version << 1) | (current ? 1 : 0)));
    emit8(b, 0);
    emit8(b, 0);
    emit16(b, 0x1234);
    emit16(b, 0x5678);
    emit8(b, 0);
    emit8(b, table_id);
}

static void section_finish(struct build *b)
{
    size_t section_length = b->len - 3 + 4;
    b->buf[1] = (uint8_t)(0xb0 | ((section_length >> 8) & 0x0fu));
    b->buf[2] = (uint8_t)section_length;
    uint32_t crc = crc32_mpeg(b->buf, b->len);
    emit8(b, (uint8_t)(crc >> 24));
    emit8(b, (uint8_t)(crc >> 16));
    emit8(b, (uint8_t)(crc >> 8));
    emit8(b, (uint8_t)crc);
}

static void add_event(struct build *b, uint16_t event_id,
    const uint8_t start[5], const uint8_t duration[3], uint8_t running_status,
    uint8_t free_ca_mode, const uint8_t *descriptors, size_t descriptors_length)
{
    emit16(b, event_id);
    emit_bytes(b, start, 5);
    emit_bytes(b, duration, 3);
    emit8(b, (uint8_t)((running_status << 5) | ((free_ca_mode & 1) << 4) |
        ((descriptors_length >> 8) & 0x0fu)));
    emit8(b, (uint8_t)descriptors_length);
    emit_bytes(b, descriptors, descriptors_length);
}

static size_t short_event(uint8_t *out, const char *name, const char *text)
{
    size_t name_len = strlen(name), text_len = strlen(text);
    assert(name_len <= 255 && text_len <= 255);
    out[0] = 'e'; out[1] = 'n'; out[2] = 'g';
    out[3] = (uint8_t)name_len;
    memcpy(out + 4, name, name_len);
    out[4 + name_len] = (uint8_t)text_len;
    memcpy(out + 5 + name_len, text, text_len);
    return 5 + name_len + text_len;
}

static size_t descriptor(uint8_t *out, uint8_t tag, const uint8_t *payload,
    size_t payload_len)
{
    out[0] = tag;
    out[1] = (uint8_t)payload_len;
    memcpy(out + 2, payload, payload_len);
    return 2 + payload_len;
}

static const uint8_t START_60000_1200[5] = { 0xea, 0x60, 0x12, 0x00, 0x00 };
static const uint8_t DUR_1H[3] = { 0x01, 0x00, 0x00 };

struct capture {
    struct oa_eit_event_info events[4];
    int count;
    int abort_after;
};

static int capture_event(const struct oa_eit_event_info *event, void *user)
{
    struct capture *c = user;
    if (c->abort_after >= 0 && c->count >= c->abort_after)
        return -ENOSPC;
    if (c->count < 4) c->events[c->count] = *event;
    c->count++;
    return 0;
}

static void test_valid_section(void)
{
    struct build b = { .len = 0 };
    uint8_t payload[256], desc[300];
    size_t payload_len = short_event(payload, "Hello", "World");
    size_t desc_len = descriptor(desc, 0x4d, payload, payload_len);
    section_header(&b, 0x4e, 7, 3, 1);
    add_event(&b, 100, START_60000_1200, DUR_1H, 4, 1, desc, desc_len);
    section_finish(&b);

    struct capture c = { .abort_after = -1 };
    assert(oa_eit_parse_section(b.buf, b.len, capture_event, &c) == 1);
    assert(c.count == 1);
    const struct oa_eit_event_info *event = &c.events[0];
    assert(event->service_id == 7 && event->event_id == 100);
    assert(event->range.begin == 1677326400u &&
        event->range.end == 1677330000u);
    assert(event->running_status == 4 && event->free_ca_mode == 1);
    assert(event->actual == 1 && event->version == 3);
    assert(!strcmp(event->name, "Hello") && !strcmp(event->text, "World"));
}

static void test_table_ids(void)
{
    const uint8_t start[5] = { 0xea, 0x60, 0x00, 0x00, 0x00 };
    const uint8_t duration[3] = { 0x00, 0x01, 0x00 };
    const uint8_t tables[] = { 0x4e, 0x4f, 0x50, 0x5f, 0x60, 0x6f };
    const uint8_t actual[] = { 1, 0, 1, 1, 0, 0 };
    for (size_t i = 0; i < sizeof tables; i++) {
        struct build b = { .len = 0 };
        section_header(&b, tables[i], 1, 0, 1);
        add_event(&b, 10, start, duration, 1, 0, NULL, 0);
        section_finish(&b);
        struct capture c = { .abort_after = -1 };
        assert(oa_eit_parse_section(b.buf, b.len, capture_event, &c) == 1);
        assert(c.events[0].actual == actual[i]);
        assert(!strcmp(c.events[0].name, ""));
    }
}

static void test_validation(void)
{
    struct build b = { .len = 0 };
    section_header(&b, 0x4e, 1, 0, 1);
    add_event(&b, 10, START_60000_1200, DUR_1H, 0, 0, NULL, 0);
    section_finish(&b);
    struct capture c = { .abort_after = -1 };

    assert(oa_eit_parse_section(NULL, b.len, capture_event, &c) == -EINVAL);
    assert(oa_eit_parse_section(b.buf, b.len, NULL, &c) == -EINVAL);
    assert(oa_eit_parse_section(b.buf, 17, capture_event, &c) == -EINVAL);
    assert(oa_eit_parse_section(b.buf, b.len - 1, capture_event, &c) == -EINVAL);

    struct build bad_crc = b;
    bad_crc.buf[bad_crc.len - 1] ^= 1;
    assert(oa_eit_parse_section(bad_crc.buf, bad_crc.len, capture_event, &c) ==
        -EINVAL);

    struct build no_syntax = b;
    no_syntax.buf[1] &= 0x7f;
    uint32_t crc = crc32_mpeg(no_syntax.buf, no_syntax.len - 4);
    no_syntax.buf[no_syntax.len - 4] = (uint8_t)(crc >> 24);
    no_syntax.buf[no_syntax.len - 3] = (uint8_t)(crc >> 16);
    no_syntax.buf[no_syntax.len - 2] = (uint8_t)(crc >> 8);
    no_syntax.buf[no_syntax.len - 1] = (uint8_t)crc;
    assert(oa_eit_parse_section(no_syntax.buf, no_syntax.len, capture_event,
        &c) == -EINVAL);

    struct build bad_table = { .len = 0 };
    section_header(&bad_table, 0x40, 1, 0, 1);
    add_event(&bad_table, 10, START_60000_1200, DUR_1H, 0, 0, NULL, 0);
    section_finish(&bad_table);
    assert(oa_eit_parse_section(bad_table.buf, bad_table.len, capture_event,
        &c) == -EINVAL);

    struct build truncated_event = { .len = 0 };
    section_header(&truncated_event, 0x4e, 1, 0, 1);
    add_event(&truncated_event, 10, START_60000_1200, DUR_1H, 0, 0, NULL, 0);
    section_finish(&truncated_event);
    /* Grow the descriptor loop beyond the section end and fix the CRC. */
    truncated_event.buf[24] = 0x00;
    truncated_event.buf[25] = 0x10;
    uint32_t crc2 = crc32_mpeg(truncated_event.buf, truncated_event.len - 4);
    truncated_event.buf[truncated_event.len - 4] = (uint8_t)(crc2 >> 24);
    truncated_event.buf[truncated_event.len - 3] = (uint8_t)(crc2 >> 16);
    truncated_event.buf[truncated_event.len - 2] = (uint8_t)(crc2 >> 8);
    truncated_event.buf[truncated_event.len - 1] = (uint8_t)crc2;
    assert(oa_eit_parse_section(truncated_event.buf, truncated_event.len,
        capture_event, &c) == -EINVAL);
}

static void test_skips(void)
{
    struct build b = { .len = 0 };
    const uint8_t bad_start[5] = { 0xea, 0x60, 0x24, 0x00, 0x00 };
    section_header(&b, 0x4e, 1, 0, 1);
    add_event(&b, 10, bad_start, DUR_1H, 0, 0, NULL, 0);
    add_event(&b, 11, START_60000_1200, DUR_1H, 0, 0, NULL, 0);
    section_finish(&b);
    struct capture c = { .abort_after = -1 };
    assert(oa_eit_parse_section(b.buf, b.len, capture_event, &c) == 1);
    assert(c.count == 1 && c.events[0].event_id == 11);

    struct build not_current = { .len = 0 };
    section_header(&not_current, 0x4e, 1, 0, 0);
    add_event(&not_current, 10, START_60000_1200, DUR_1H, 0, 0, NULL, 0);
    section_finish(&not_current);
    c.count = 0;
    assert(oa_eit_parse_section(not_current.buf, not_current.len,
        capture_event, &c) == 0);
    assert(c.count == 0);
}

static void test_bounded_text_and_abort(void)
{
    struct build b = { .len = 0 };
    char long_name[160];
    memset(long_name, 'N', sizeof long_name - 1);
    long_name[sizeof long_name - 1] = 0;
    uint8_t payload[512], desc[520];
    size_t payload_len = short_event(payload, long_name, "Text");
    size_t desc_len = descriptor(desc, 0x4d, payload, payload_len);
    section_header(&b, 0x4e, 1, 0, 1);
    add_event(&b, 10, START_60000_1200, DUR_1H, 0, 0, desc, desc_len);
    section_finish(&b);

    struct capture c = { .abort_after = -1 };
    assert(oa_eit_parse_section(b.buf, b.len, capture_event, &c) == 1);
    assert(strlen(c.events[0].name) == OA_EIT_NAME_MAX - 1);

    struct capture aborting = { .abort_after = 0 };
    assert(oa_eit_parse_section(b.buf, b.len, capture_event, &aborting) ==
        -ENOSPC);
}

static void test_builder_roundtrip(void)
{
    const struct oa_eit_build_event event = {
        .event_id = 77,
        .start = { 0xea, 0x60, 0x08, 0x30, 0x00 },
        .duration = { 0x00, 0x45, 0x00 },
        .running_status = 2,
        .free_ca_mode = 1,
        .name = "Builder",
        .text = "Roundtrip"
    };
    uint8_t section[512];
    size_t len = 0;
    assert(oa_eit_build_section(0x4e, 9, 5, 1, &event, 1, section,
        sizeof section, &len) == 0);
    struct capture c = { .abort_after = -1 };
    assert(oa_eit_parse_section(section, len, capture_event, &c) == 1);
    assert(c.events[0].service_id == 9 && c.events[0].event_id == 77);
    assert(c.events[0].range.begin == 1677283200u + 8u * 3600u + 30u * 60u);
    assert(c.events[0].range.end == c.events[0].range.begin + 45u * 60u);
    assert(c.events[0].running_status == 2 && c.events[0].free_ca_mode == 1);
    assert(c.events[0].version == 5 && !strcmp(c.events[0].name, "Builder") &&
        !strcmp(c.events[0].text, "Roundtrip"));
    assert(oa_eit_build_section(0x40, 9, 0, 1, &event, 1, section,
        sizeof section, &len) == -EINVAL);
    assert(oa_eit_build_section(0x4e, 9, 0, 1, &event, 1, section, 8,
        &len) == -ENOSPC);
}

struct ts_capture {
    int count;
    uint8_t section[4096];
    size_t length;
};

static int ts_capture_section(const uint8_t *section, size_t len, void *user)
{
    struct ts_capture *c = user;
    if (len > sizeof c->section) return -E2BIG;
    memcpy(c->section, section, len);
    c->length = len;
    c->count++;
    return 0;
}

static void ts_packet(uint8_t out[188], uint16_t pid, uint8_t cc, int pusi,
    const uint8_t *payload, size_t len, unsigned pointer)
{
    memset(out, 0xff, 188);
    out[0] = 0x47;
    out[1] = (uint8_t)((pusi ? 0x40 : 0) | ((pid >> 8) & 0x1f));
    out[2] = (uint8_t)pid;
    out[3] = (uint8_t)(0x10 | (cc & 0x0f));
    size_t offset = 4;
    if (pusi) {
        assert(pointer <= 183);
        out[offset++] = (uint8_t)pointer;
        offset += pointer;
    }
    assert(offset + len <= 188);
    memcpy(out + offset, payload, len);
}

static void ts_packet_adaptation(uint8_t out[188], uint16_t pid, uint8_t cc,
    int pusi, const uint8_t *payload, size_t len)
{
    memset(out, 0xff, 188);
    out[0] = 0x47;
    out[1] = (uint8_t)((pusi ? 0x40 : 0) | ((pid >> 8) & 0x1f));
    out[2] = (uint8_t)pid;
    out[3] = (uint8_t)(0x30 | (cc & 0x0f));
    out[4] = 3;
    out[5] = 0;
    out[6] = 0;
    out[7] = 0;
    size_t offset = 8;
    if (pusi) out[offset++] = 0;
    assert(offset + len <= 188);
    memcpy(out + offset, payload, len);
}

static void test_ts_demux(void)
{
    const struct oa_eit_build_event events[2] = {
        {
            .event_id = 21,
            .start = { 0xea, 0x60, 0x08, 0x30, 0x00 },
            .duration = { 0x00, 0x30, 0x00 },
            .running_status = 1,
            .name = "TS one",
            .text = "First"
        },
        {
            .event_id = 22,
            .start = { 0xea, 0x60, 0x09, 0x00, 0x00 },
            .duration = { 0x00, 0x45, 0x00 },
            .running_status = 2,
            .name = "TS two",
            .text = "Second"
        }
    };
    uint8_t section[512], second[512], both[1024];
    size_t section_len = 0, second_len = 0;
    assert(oa_eit_build_section(0x4e, 301, 3, 1, &events[0], 1, section,
        sizeof section, &section_len) == 0);
    assert(oa_eit_build_section(0x4f, 302, 4, 0, &events[1], 1, second,
        sizeof second, &second_len) == 0);

    uint8_t packet[188];
    struct oa_eit_ts ts;
    struct ts_capture capture;

    oa_eit_ts_init(&ts, 0x12);
    memset(&capture, 0, sizeof capture);
    ts_packet(packet, 0x12, 0, 1, section, section_len, 0);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 1);
    assert(capture.count == 1 && capture.length == section_len &&
        !memcmp(capture.section, section, section_len));

    /* A section larger than one packet, split at full 184-byte payloads. */
    char long_name[200], long_text[200];
    memset(long_name, 'N', sizeof long_name - 1);
    long_name[sizeof long_name - 1] = 0;
    memset(long_text, 'T', sizeof long_text - 1);
    long_text[sizeof long_text - 1] = 0;
    const struct oa_eit_build_event big_event = {
        .event_id = 31,
        .start = { 0xea, 0x60, 0x0a, 0x00, 0x00 },
        .duration = { 0x00, 0x30, 0x00 },
        .running_status = 1,
        .name = long_name,
        .text = long_text
    };
    uint8_t big[512];
    size_t big_len = 0;
    assert(oa_eit_build_section(0x4e, 303, 5, 1, &big_event, 1, big,
        sizeof big, &big_len) == 0);
    assert(big_len > 184 && big_len < sizeof big);
    oa_eit_ts_reset(&ts);
    memset(&capture, 0, sizeof capture);
    size_t offset = 0;
    uint8_t cc = 0;
    while (offset < big_len) {
        size_t chunk = offset == 0 ? 183 : 184;
        size_t n = big_len - offset;
        if (n > chunk) n = chunk;
        ts_packet(packet, 0x12, cc++, offset == 0, big + offset, n, 0);
        int emitted = oa_eit_ts_feed(&ts, packet, 188, ts_capture_section,
            &capture);
        assert(emitted == (offset + n == big_len ? 1 : 0));
        offset += n;
    }
    assert(capture.count == 1 && capture.length == big_len &&
        !memcmp(capture.section, big, big_len));

    /* Pointer field before a section. */
    oa_eit_ts_reset(&ts);
    memset(&capture, 0, sizeof capture);
    ts_packet(packet, 0x12, 0, 1, section, section_len, 4);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 1);
    assert(capture.count == 1 && capture.length == section_len);

    /* Two complete sections in one packet. */
    assert(section_len + second_len <= 183);
    memcpy(both, section, section_len);
    memcpy(both + section_len, second, second_len);
    oa_eit_ts_reset(&ts);
    memset(&capture, 0, sizeof capture);
    ts_packet(packet, 0x12, 0, 1, both, section_len + second_len, 0);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 2);
    assert(capture.count == 2 && capture.length == second_len &&
        !memcmp(capture.section, second, second_len));

    /* Wrong PID is ignored. */
    oa_eit_ts_reset(&ts);
    memset(&capture, 0, sizeof capture);
    ts_packet(packet, 0x11, 0, 1, section, section_len, 0);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 0);
    assert(capture.count == 0);

    /* Duplicate packet is ignored. */
    oa_eit_ts_reset(&ts);
    memset(&capture, 0, sizeof capture);
    ts_packet(packet, 0x12, 0, 1, section, section_len, 0);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 1);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 0);
    assert(capture.count == 1);

    /* Continuity error drops the partial section; a new PUSI recovers. */
    oa_eit_ts_reset(&ts);
    memset(&capture, 0, sizeof capture);
    ts_packet(packet, 0x12, 0, 1, big, 183, 0);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 0);
    ts_packet(packet, 0x12, 2, 0, big + 183, 184, 0);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 0);
    ts_packet(packet, 0x12, 3, 1, section, section_len, 0);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 1);
    assert(capture.count == 1 && capture.length == section_len);

    /* Transport error drops the partial section. */
    oa_eit_ts_reset(&ts);
    memset(&capture, 0, sizeof capture);
    ts_packet(packet, 0x12, 0, 1, big, 183, 0);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 0);
    ts_packet(packet, 0x12, 1, 0, big + 183, 184, 0);
    packet[1] |= 0x80;
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 0);
    ts_packet(packet, 0x12, 2, 1, section, section_len, 0);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 1);

    /* Adaptation field plus payload. */
    oa_eit_ts_reset(&ts);
    memset(&capture, 0, sizeof capture);
    ts_packet_adaptation(packet, 0x12, 0, 1, section, section_len);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 1);

    /* Non-EIT table is assembled but not delivered. */
    memcpy(both, section, section_len);
    both[0] = 0x00;
    oa_eit_ts_reset(&ts);
    memset(&capture, 0, sizeof capture);
    ts_packet(packet, 0x12, 0, 1, both, section_len, 0);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 0);
    ts_packet(packet, 0x12, 1, 1, section, section_len, 0);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 1);

    /* Malformed packets and impossible section lengths. */
    oa_eit_ts_reset(&ts);
    assert(oa_eit_ts_feed(&ts, packet, 187, ts_capture_section, &capture) ==
        -EINVAL);
    ts_packet(packet, 0x12, 0, 1, section, section_len, 0);
    packet[0] = 0x46;
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) ==
        -EINVAL);
    packet[0] = 0x47;
    packet[3] = 0x00;
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) ==
        -EINVAL);
    const uint8_t huge[3] = { 0x4e, 0xbf, 0xff };
    oa_eit_ts_reset(&ts);
    memset(&capture, 0, sizeof capture);
    ts_packet(packet, 0x12, 0, 1, huge, sizeof huge, 0);
    assert(oa_eit_ts_feed(&ts, packet, 188, ts_capture_section, &capture) == 0);
    assert(capture.count == 0);
}

int main(void)
{
    test_valid_section();
    test_table_ids();
    test_validation();
    test_skips();
    test_bounded_text_and_abort();
    test_builder_roundtrip();
    test_ts_demux();
    puts("ok eit section: header/CRC, BCD times, descriptors, skips and TS demux");
    return 0;
}
