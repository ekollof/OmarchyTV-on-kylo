/* DVB EIT section parser tests. Sections are built locally with a correct
 * MPEG-2 CRC; no vendor code or TV data is used. */
#include "openaurora/eit_section.h"

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

int main(void)
{
    test_valid_section();
    test_table_ids();
    test_validation();
    test_skips();
    test_bounded_text_and_abort();
    test_builder_roundtrip();
    puts("ok eit section: EIT header/CRC, BCD times, descriptors and skips");
    return 0;
}
