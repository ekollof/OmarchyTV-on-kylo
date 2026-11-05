/* Generate a small DVB TS replayed by mod_tuner's tuner.ts_path backend.
 * Two EIT events on service 777, split across full-payload packets. This is
 * generated project data, not a vendor capture. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/eit_section.h"

static void packet(unsigned char out[188], unsigned cc, int pusi,
    const unsigned char *payload, size_t len)
{
    memset(out, 0xff, 188);
    out[0] = 0x47;
    out[1] = (unsigned char)(pusi ? 0x40 : 0);
    out[2] = 0x12;
    out[3] = (unsigned char)(0x10 | (cc & 0x0f));
    size_t offset = 4;
    if (pusi) out[offset++] = 0;
    assert(offset + len <= 188);
    memcpy(out + offset, payload, len);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s output.ts\n", argv[0]);
        return 2;
    }
    char name1[101], text1[101], name2[101], text2[101];
    memset(name1, 'A', 100);
    name1[100] = 0;
    memset(text1, 'a', 100);
    text1[100] = 0;
    memset(name2, 'B', 100);
    name2[100] = 0;
    memset(text2, 'b', 100);
    text2[100] = 0;
    const struct oa_eit_build_event events[2] = {
        {
            .event_id = 7001,
            .start = { 0xea, 0x60, 0x12, 0x00, 0x00 },
            .duration = { 0x00, 0x30, 0x00 },
            .running_status = 4,
            .name = name1,
            .text = text1
        },
        {
            .event_id = 7002,
            .start = { 0xea, 0x60, 0x13, 0x00, 0x00 },
            .duration = { 0x00, 0x45, 0x00 },
            .running_status = 1,
            .name = name2,
            .text = text2
        }
    };
    unsigned char section[4096];
    size_t section_len = 0;
    if (oa_eit_build_section(0x4e, 777, 7, 1, events, 2, section,
            sizeof section, &section_len))
        return 3;
    FILE *file = fopen(argv[1], "wb");
    if (!file) return 4;
    unsigned char packet_data[188];
    size_t offset = 0;
    unsigned cc = 0;
    while (offset < section_len) {
        size_t capacity = offset == 0 ? 183 : 184;
        size_t length = section_len - offset;
        if (length > capacity) length = capacity;
        packet(packet_data, cc++, offset == 0, section + offset, length);
        if (fwrite(packet_data, 1, sizeof packet_data, file) !=
                sizeof packet_data)
            return 5;
        offset += length;
    }
    if (fclose(file)) return 6;
    printf("section %zu bytes, %u packets\n", section_len, cc);
    return 0;
}
