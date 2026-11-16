/* libNDAL3-backed video mute io. Compiled only for vendor (ARM) builds. */
#include "openaurora/video_mute.h"

#include <stddef.h>

#ifndef OA_NO_VENDOR

#include <errno.h>

/* Symbols exported by libNDAL3.so (verified with nm -D and Ghidra):
 *   Hal_Video_Get_DrmDeviceId        0x4e7c3201
 *   Hal_Video_VideoPath_to_PlaneType 0x4e7c468d
 *   Hal_Video_Get_DrmPlaneInfo       0x4e7c3465
 *   Hal_Video_Drm_Set_Mute           0x4e7c4a89
 * Hal_Video_Get_DrmPlaneInfo returns void in the shipped library, so the
 * wrapper reports success unconditionally, matching plat_drm_set_mute's
 * (ignored) return handling. */
extern int Hal_Video_Get_DrmDeviceId(int index);
extern int Hal_Video_VideoPath_to_PlaneType(int path);
extern void Hal_Video_Get_DrmPlaneInfo(int plane_type, uint32_t *info);
extern int Hal_Video_Drm_Set_Mute(int fd, unsigned plane, int enable, int on);

static int vendor_drm_device_id(void *ud, int index)
{
    (void)ud;
    return Hal_Video_Get_DrmDeviceId(index);
}

static int vendor_video_path_to_plane(void *ud, int path)
{
    (void)ud;
    return Hal_Video_VideoPath_to_PlaneType(path);
}

static int vendor_get_plane_info(void *ud, int plane_type,
                                 uint32_t info[OA_VIDEO_MUTE_INFO_WORDS])
{
    (void)ud;
    Hal_Video_Get_DrmPlaneInfo(plane_type, info);
    return 0;
}

static int vendor_drm_set_mute(void *ud, int fd, unsigned plane, int enable,
                               int on)
{
    (void)ud;
    return Hal_Video_Drm_Set_Mute(fd, plane, enable, on);
}

static const oa_video_mute_io g_vendor_io = {
    NULL, vendor_drm_device_id, vendor_video_path_to_plane,
    vendor_get_plane_info, vendor_drm_set_mute
};

const oa_video_mute_io *oa_video_mute_vendor_io(void)
{
    return &g_vendor_io;
}

#else /* OA_NO_VENDOR */

const oa_video_mute_io *oa_video_mute_vendor_io(void)
{
    return NULL; /* host builds supply injected fakes; no vendor library */
}

#endif
