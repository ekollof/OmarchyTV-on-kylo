/* Pure decoder test for the ndp-hdmi private V4L2 event payload. No device,
 * TV, V4L2 or HAL access. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/source_hdmi_event.h"

static void fill(uint8_t data[OA_SOURCE_HDMI_EVENT_SIZE], uint32_t type,
                 uint8_t status)
{
    memset(data, 0, OA_SOURCE_HDMI_EVENT_SIZE);
    data[0] = (uint8_t)type;
    data[1] = (uint8_t)(type >> 8);
    data[2] = (uint8_t)(type >> 16);
    data[3] = (uint8_t)(type >> 24);
    data[4] = 0x86;
    data[5] = 0x00;
    data[6] = 0x0c;
    data[7] = status;
    for (int i = 8; i < (int)OA_SOURCE_HDMI_EVENT_SIZE; i++)
        data[i] = (uint8_t)i;
}

static void test_decode(void)
{
    uint8_t data[OA_SOURCE_HDMI_EVENT_SIZE];
    struct oa_source_hdmi_event event;

    fill(data, 0x15, 1);
    assert(oa_source_hdmi_event_decode(data, sizeof data, &event) == 0);
    assert(event.source_type == 0x15);
    assert(event.status == 1);
    assert(memcmp(event.raw, data, sizeof data) == 0);
    assert(event.raw[4] == 0x86 && event.raw[5] == 0x00 &&
           event.raw[6] == 0x0c && event.raw[7] == 1);

    fill(data, 0x12345678, 0);
    assert(oa_source_hdmi_event_decode(data, sizeof data, &event) == 0);
    assert(event.source_type == 0x12345678);
    assert(event.status == 0);

    /* Extra bytes are not part of the 28-byte payload. */
    uint8_t longer[30];
    memset(longer, 0, sizeof longer);
    fill(longer, 0x0f, 1);
    longer[28] = 0xff;
    longer[29] = 0xff;
    assert(oa_source_hdmi_event_decode(longer, sizeof longer, &event) == 0);
    assert(event.source_type == 0x0f);
    assert(event.status == 1);
    assert(event.raw[27] == 27);
}

static void test_rejections(void)
{
    uint8_t data[OA_SOURCE_HDMI_EVENT_SIZE] = {0};
    struct oa_source_hdmi_event event;
    memset(&event, 0xa5, sizeof event);
    struct oa_source_hdmi_event before = event;

    assert(oa_source_hdmi_event_decode(NULL, sizeof data, &event) == -EINVAL);
    assert(memcmp(&event, &before, sizeof event) == 0);
    assert(oa_source_hdmi_event_decode(data, sizeof data - 1, &event) == -EINVAL);
    assert(memcmp(&event, &before, sizeof event) == 0);
    assert(oa_source_hdmi_event_decode(data, sizeof data, NULL) == -EINVAL);
}

int main(void)
{
    test_decode();
    test_rejections();
    puts("ok source-hdmi-event: 28-byte payload, source type, status byte, "
         "raw preservation, short/null rejection");
    return 0;
}
