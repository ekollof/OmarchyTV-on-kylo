#ifndef OPENAURORA_SOURCE_VIN_H
#define OPENAURORA_SOURCE_VIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * HDMI RX video status from the vendor GET ext-control 0xa00967.
 *
 * Recovered from aurora.elf Hal_Vin_GetHdmiStatus, called by
 * plat_vin_slot_get_status @ 0x0092ea68 for the VIN slot:
 *   class 0xa00000, count 1
 *   control id 0xa00967, size 0x14, five output words
 *   VIDIOC_G_EXT_CTRLS = 0xc0185647 on /dev/video21
 *
 * The vendor getter checks only the ioctl return; the target kernel leaves
 * error_idx == count on success, so callers must not turn that sentinel into a
 * failure. The first word is the connected/ready flag:
 *   1       active HDMI signal
 *   0 or 5  no signal / lost (seen during Deck suspend)
 *   w1/w2   video format words (not decoded here)
 *   w4      0xffffffff on a transition
 *
 * This is the suspend detector, not the HPD cable plug/unplug provider
 * (source_hpd.h) and not the private V4L2 scaler event (source_hdmi_event.h).
 */
#define OA_SOURCE_VIN_WORDS 5u
#define OA_SOURCE_VIN_CTRL_CLASS 0xa00000u
#define OA_SOURCE_VIN_CTRL_ID 0xa00967u
#define OA_SOURCE_VIN_SIZE 0x14u

/* Returns 1 for an active signal, 0 for lost, -EINVAL for a NULL pointer. */
int oa_source_vin_ready(const uint32_t words[OA_SOURCE_VIN_WORDS]);

/* Injectable read seam. The production table opens /dev/video21 and issues
 * GET 0xa00967; the fake host module supplies a table without any device. */
struct oa_source_vin_io {
    void *context;
    int (*open)(void *context);
    int (*read)(void *context, int fd, uint32_t words[OA_SOURCE_VIN_WORDS]);
    int (*close)(void *context, int fd);
};

const struct oa_source_vin_io *oa_source_vin_io(void);

/* Reads one status word set. Returns 1 ready, 0 lost, or -errno. */
int oa_source_vin_poll(const struct oa_source_vin_io *io, int fd,
                       uint32_t words[OA_SOURCE_VIN_WORDS]);

#ifdef __cplusplus
}
#endif

#endif
