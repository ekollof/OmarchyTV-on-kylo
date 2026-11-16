/* Pure model of the recovered plat_drm_set_mute main-video mute chain. */
#include "openaurora/video_mute.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

/* The vendor HAL writes 48 bytes unconditionally; a smaller buffer corrupts
 * the caller's stack (this was the 2026-11-16 on-TV SIGSEGV). */
_Static_assert(OA_VIDEO_MUTE_INFO_WORDS >= OA_VIDEO_MUTE_INFO_WRITTEN_WORDS,
               "video mute info buffer too small for Hal_Video_Get_DrmPlaneInfo");

void oa_video_mute_init(oa_video_mute *m, const oa_video_mute_io *io, int path)
{
    if (!m)
        return;
    memset(m, 0, sizeof(*m));
    m->state.hidden = -1;
    m->path = path;
    if (io)
        m->io = *io;
}

int oa_video_mute_set(oa_video_mute *m, int on)
{
    uint32_t info[OA_VIDEO_MUTE_INFO_WORDS];
    int fd, plane_type, rc;

    if (!m || !m->io.drm_device_id || !m->io.video_path_to_plane ||
        !m->io.get_plane_info || !m->io.drm_set_mute)
        return -EINVAL;

    m->state.calls++;
    memset(info, 0, sizeof info);

    fd = m->io.drm_device_id(m->io.ud, 0);
    if (fd < 0) {
        m->state.last_rc = fd;
        return fd;
    }
    plane_type = m->io.video_path_to_plane(m->io.ud, m->path);
    if (plane_type < 0) {
        m->state.last_rc = plane_type;
        return plane_type;
    }
    /* The getter does not validate the index; the record table has 9 slots. */
    if (plane_type >= OA_VIDEO_MUTE_PLANE_SLOTS) {
        m->state.last_rc = -ERANGE;
        return -ERANGE;
    }
    rc = m->io.get_plane_info(m->io.ud, plane_type, info);
    if (rc < 0) {
        m->state.last_rc = rc;
        return rc;
    }
    /* Firmware passes plane index 0, enable literal 1, and the requested
     * state. Hal_Video_Drm_Set_Mute builds {plane,1,on} for ioctl 0xc00c6455. */
    rc = m->io.drm_set_mute(m->io.ud, fd, info[0], 1, on ? 1 : 0);
    m->state.last_rc = rc;
    m->state.last_fd = fd;
    m->state.last_plane = info[0];
    m->state.last_on = on ? 1 : 0;
    if (rc == 0)
        m->state.hidden = on ? 1 : 0;
    return rc;
}

int oa_video_mute_snapshot(const oa_video_mute *m, oa_video_mute_state *out)
{
    if (!m || !out)
        return -EINVAL;
    *out = m->state;
    return 0;
}
