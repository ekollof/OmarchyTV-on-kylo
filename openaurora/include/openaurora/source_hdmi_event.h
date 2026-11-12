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
 *   [0..3]   source type (little-endian, VPL_SRC_GetSrcType)
 *   [4..5]   VPL VidSetting+0x86
 *   [6]      VPL VidSetting+0x0c
 *   [7]      status byte; the recovered producer writes (UpdateSCLR arg == 6)
 *   [8..0x1b] scaler timing fields from VPL_SCLR_VidSetting(0)
 *
 * The status byte's mapping to the NEBULA ExternalInterfaceStatus or the
 * ExternalAVSourceManager role (video ready/lost/connected/disconnected) is
 * not proven. This decoder preserves the raw bytes and the recovered offsets;
 * it does not guess the role.
 */
#define OA_SOURCE_HDMI_EVENT_SIZE 28u

struct oa_source_hdmi_event {
    uint32_t source_type;
    uint8_t status;
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
