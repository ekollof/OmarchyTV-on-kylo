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
    /* _CTRL_EVENT_Update stores the second argument, (UpdateSCLR arg == 6),
     * with strb at payload[4]. It is a scaler update reason, not a source
     * status. */
    event.update_flag = data[4];
    event.video_setting_0c = data[5];
    event.video_setting_86 = (uint16_t)((uint16_t)data[6] |
                                        ((uint16_t)data[7] << 8));
    *out = event;
    return 0;
}
