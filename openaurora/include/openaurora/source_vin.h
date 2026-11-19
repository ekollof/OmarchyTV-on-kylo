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
/* PLAT_VIU_NdVideoSetting @0x00901c28 returns &DAT_019d4bf8 + path*4 for
 * path 0..6 and index 0 for path > 6 (with a debug log when VID_MSG < 0);
 * PLAT_VIU_ChangeInput @0x00902ae4 stores the selected source type into that
 * word (str r5,[r0,#0] @0x00902d60). This replacement tracks the same seven
 * path slots; it never calls the vendor DEV_* code. The following
 * PLAT_VIN_UpdateTDLayerInputSource 13-word copy is dead on this firmware:
 * PLAT_SRCIN_GetVinParam @0x008fd9fc reads table 0x017706d4[type*8+4], whose
 * pointer words are all zero, so the caller's NULL check skips the copy. */
#define OA_SOURCE_VIN_PATHS 7u
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

/* Vendor path->type table (PLAT_VIU_NdVideoSetting + ChangeInput). The
 * context owns seven uint32 slots, zero-initialized. set/get clamp path >6
 * (and negative) to slot 0; -EINVAL for a NULL context or negative type. */
struct oa_source_vin_paths {
    uint32_t type[OA_SOURCE_VIN_PATHS];
};
void oa_source_vin_paths_init(struct oa_source_vin_paths *p);
int oa_source_vin_path_set(struct oa_source_vin_paths *p, int path, int type);
int oa_source_vin_path_get(const struct oa_source_vin_paths *p, int path);

#ifdef __cplusplus
}
#endif

#endif
