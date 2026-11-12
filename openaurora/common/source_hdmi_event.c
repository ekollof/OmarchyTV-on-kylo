#include <errno.h>
#include <string.h>

#include "openaurora/source_hdmi_event.h"

int oa_source_hdmi_event_decode(const uint8_t *data, size_t length,
                                struct oa_source_hdmi_event *out)
{
    if (!data || !out || length < OA_SOURCE_HDMI_EVENT_SIZE) return -EINVAL;
    struct oa_source_hdmi_event event;
    memset(&event, 0, sizeof event);
    memcpy(event.raw, data, sizeof event.raw);
    event.source_type = (uint32_t)data[0] |
                        ((uint32_t)data[1] << 8) |
                        ((uint32_t)data[2] << 16) |
                        ((uint32_t)data[3] << 24);
    /* _CTRL_EVENT_Update packs (UpdateSCLR arg == 6) at byte 7. */
    event.status = data[7];
    *out = event;
    return 0;
}
