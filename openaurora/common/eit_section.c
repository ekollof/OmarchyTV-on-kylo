/* DVB EIT section parser. See include/openaurora/eit_section.h. */
#include "openaurora/eit_section.h"

#include <errno.h>
#include <string.h>

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

static void copy_bounded(char *out, size_t cap, const uint8_t *src, size_t len)
{
    if (!cap) return;
    if (len > cap - 1) len = cap - 1;
    if (len) memcpy(out, src, len);
    out[len] = 0;
}

static void parse_short_event(const uint8_t *data, size_t len,
    struct oa_eit_event_info *event)
{
    /* language(3), name_length(1), name, text_length(1), text */
    if (len < 5) return;
    size_t name_len = data[3];
    if (4 + name_len + 1 > len) return;
    size_t text_len = data[4 + name_len];
    if (4 + name_len + 1 + text_len > len) return;
    copy_bounded(event->name, sizeof event->name, data + 4, name_len);
    copy_bounded(event->text, sizeof event->text,
        data + 4 + name_len + 1, text_len);
}

int oa_eit_parse_section(const uint8_t *data, size_t len,
    oa_eit_event_cb cb, void *user)
{
    if (!data || !cb || len < 18) return -EINVAL;
    uint8_t table_id = data[0];
    if (table_id < 0x4e || table_id > 0x6f) return -EINVAL;
    if (!(data[1] & 0x80)) return -EINVAL; /* section_syntax_indicator */
    size_t section_length = ((size_t)(data[1] & 0x0fu) << 8) | data[2];
    if (section_length < 15 || section_length > 4093) return -EINVAL;
    size_t total = 3 + section_length;
    if (total > len) return -EINVAL;
    if (crc32_mpeg(data, total) != 0) return -EINVAL;

    /* A non-current section is valid but carries no usable schedule data. */
    if (!(data[5] & 0x01)) return 0;

    uint16_t service_id = (uint16_t)(((uint16_t)data[3] << 8) | data[4]);
    uint8_t version = (uint8_t)((data[5] >> 1) & 0x1f);
    uint8_t actual = (uint8_t)(table_id == 0x4e ||
        (table_id >= 0x50 && table_id <= 0x5f));
    size_t offset = 14;
    size_t end = total - 4; /* CRC */
    int delivered = 0;

    while (offset + 12 <= end) {
        const uint8_t *entry = data + offset;
        uint16_t event_id = (uint16_t)(((uint16_t)entry[0] << 8) | entry[1]);
        size_t descriptors_length =
            ((size_t)(entry[10] & 0x0fu) << 8) | entry[11];
        if (offset + 12 + descriptors_length > end) return -EINVAL;

        struct oa_eit_event_info info;
        memset(&info, 0, sizeof info);
        info.service_id = service_id;
        info.event_id = event_id;
        info.running_status = (uint8_t)(entry[10] >> 5);
        info.free_ca_mode = (uint8_t)((entry[10] >> 4) & 1);
        info.actual = actual;
        info.version = version;

        if (oa_eit_time_valid(entry + 2)) {
            oa_eit_time_decode(entry + 2, &info.range);
            size_t descriptor = offset + 12;
            size_t descriptor_end = descriptor + descriptors_length;
            while (descriptor + 2 <= descriptor_end) {
                uint8_t tag = data[descriptor];
                size_t tag_len = data[descriptor + 1];
                if (descriptor + 2 + tag_len > descriptor_end) break;
                if (tag == 0x4d)
                    parse_short_event(data + descriptor + 2, tag_len, &info);
                descriptor += 2 + tag_len;
            }
            int rc = cb(&info, user);
            if (rc) return rc;
            delivered++;
        }
        offset += 12 + descriptors_length;
    }
    return delivered;
}
