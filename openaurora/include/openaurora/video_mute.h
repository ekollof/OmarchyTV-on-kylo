#ifndef OPENAURORA_VIDEO_MUTE_H
#define OPENAURORA_VIDEO_MUTE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Replacement for the vendor main-video mute used by EASI hideVideo/showVideo.
 *
 * Recovered from the shipped aurora.elf (MD5 64c252340d3af6c5d66072611b7bad7a)
 * and libNDAL3.so:
 *   plat_drm_set_mute(u8Path, b8isMuteOn) @ 0x0092cec8 calls, in order,
 *     Hal_Video_Get_DrmDeviceId(0)                       @ libNDAL3 0x4e7c3200
 *     Hal_Video_VideoPath_to_PlaneType(path)             @ libNDAL3 0x4e7c468c
 *     Hal_Video_Get_DrmPlaneInfo(plane_type, info)       @ libNDAL3 0x4e7c3464
 *     Hal_Video_Drm_Set_Mute(fd, info[0], 1, on)         @ libNDAL3 0x4e7c4a88
 *   Hal_Video_Drm_Set_Mute issues Hal_Video_Libdrm_drmIoctl(fd, 0xc00c6455,
 *   &{plane, enable=1, on}) — a real DRM plane mute.
 *   ICE_VideoHide @ 0x007d8cb0 calls PLAT_VOUT_EnableBlueScreen(0, 1) with
 *   path 0 for the main video (_ice_ApplyVideoPatterns @ 0x77a2ee), so path 0
 *   is the recovered default. The vendor EASI methods need live C++ objects;
 *   this HAL-level path is the callable replacement seam.
 *
 * This model performs no I/O itself: each HAL step is injected. The vendor
 * adapter supplies the real libNDAL3 functions on ARM; host tests supply fakes.
 * A successful call is a DRM ioctl acceptance, not visual/scanout proof.
 */
#define OA_VIDEO_MUTE_IOCTL 0xc00c6455u
#define OA_VIDEO_MUTE_PATH_MAIN 0
#define OA_VIDEO_MUTE_INFO_WORDS 4

typedef struct {
    void *ud;
    /* Returns the DRM device fd, or a negative errno. */
    int (*drm_device_id)(void *ud, int index);
    /* Maps a video path to a plane type. */
    int (*video_path_to_plane)(void *ud, int path);
    /* Fills up to OA_VIDEO_MUTE_INFO_WORDS words; info[0] is the plane id.
     * Returns 0 on success, negative errno on failure. */
    int (*get_plane_info)(void *ud, int plane_type,
                          uint32_t info[OA_VIDEO_MUTE_INFO_WORDS]);
    /* Issues the DRM mute ioctl. Returns 0 on success, negative errno. */
    int (*drm_set_mute)(void *ud, int fd, unsigned plane, int enable, int on);
} oa_video_mute_io;

typedef struct {
    int hidden;        /* -1 unknown, 0 shown, 1 muted/hidden */
    int calls;         /* number of attempted mute transactions */
    int last_rc;       /* 0 or the last negative errno */
    int last_fd;
    unsigned last_plane;
    int last_on;
} oa_video_mute_state;

typedef struct {
    oa_video_mute_io io;
    oa_video_mute_state state;
    int path;          /* video path; default OA_VIDEO_MUTE_PATH_MAIN */
} oa_video_mute;

void oa_video_mute_init(oa_video_mute *m, const oa_video_mute_io *io, int path);
/* Run the recovered chain with the given mute state. Returns 0 on success,
 * -EINVAL for a null/incomplete io, or the failing HAL step's negative errno.
 * State is updated only for a completed transaction (-1 hidden unknown on a
 * failed ioctl). */
int oa_video_mute_set(oa_video_mute *m, int on);
int oa_video_mute_snapshot(const oa_video_mute *m, oa_video_mute_state *out);

/* libNDAL3-backed io (no-op stubs under OA_NO_VENDOR). */
const oa_video_mute_io *oa_video_mute_vendor_io(void);

#ifdef __cplusplus
}
#endif

#endif
