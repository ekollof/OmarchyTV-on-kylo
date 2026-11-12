#ifndef OPENAURORA_SOURCE_HDMI_EVENT_H
#define OPENAURORA_SOURCE_HDMI_EVENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pure decoder for the private HDMI V4L2 event on ndp-hdmi (/dev/video21).
 *
 * Recovered from:
 *   ntkndphdmi.ko _ndp_hdmi_core_vidioc_subscribe_event @ 0x0000052c
 *   ntkndphdmi.ko _ndp_hdmi_core_event_callback         @ 0x0000150c
 *   ntkdriver_nondovi.ko _CTRL_EVENT_Update              @ 0x006394bc
 *   ntkdriver_nondovi.ko VPL_SCLR_CTRL_Event_RegisterCB  @ 0x00637538
 *   ntkdriver_nondovi.ko VPL_SCLR_CTRL_UpdateSCLR        @ 0x0063967c
 *
 * Address convention: the HDMI RX addresses are .text section offsets
 * (objdump/nm); the ntkdriver_nondovi addresses are Ghidra runtime addresses
 * (.text offset + 0x10228 in this import).
 *
 * The V4L2 event type is 0xc000001. The event data is 28 bytes:
 *   [0..3]    source type (little-endian, VPL_SRC_GetSrcType)
 *   [4]       (VPL_SCLR_CTRL_UpdateSCLR arg == 6)
 *   [5]       VPL VidSetting+0x0c
 *   [6..7]    VPL VidSetting+0x86 (little-endian uint16)
 *   [8..0x1b] scaler timing fields from VPL_SCLR_VidSetting(0)
 *
 * This is a scaler update notification, not the stock HDMI hotplug/source-loss
 * provider. Stock polls the HPD GPIOs 200..203 (see source_hpd.h). Do not map
 * update_flag to ExternalSourceStatus or to the NEBULA interface status.
 * The captured live event had source_type 5, update_flag 0 and 0x1770 at
 * [6..7]; the timing fields repeated 3840x2160.
 */
#define OA_SOURCE_HDMI_EVENT_SIZE 28u

struct oa_source_hdmi_event {
    uint32_t source_type;
    uint8_t update_flag;
    uint8_t video_setting_0c;
    uint16_t video_setting_86;
    uint8_t raw[OA_SOURCE_HDMI_EVENT_SIZE];
};

/* Returns 0 on success, -EINVAL for NULL output or a short payload. The
 * output is unchanged on failure. */
int oa_source_hdmi_event_decode(const uint8_t *data, size_t length,
                                struct oa_source_hdmi_event *out);

#ifdef __cplusplus
}
#endif

#endif
