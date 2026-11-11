/* Source-switch HAL seam.
 *
 * The sequence is recovered from aurora's PLAT_VIU_ChangeInput /
 * plat_vin_slot_connect and the shipped libNDAL3/libNDAL_EX wrappers. The
 * module must be host-testable without a TV, so every hardware call goes
 * through this table. Production uses common/source_hal_vendor.c; the fake
 * test supplies its own oa_source_hal().
 *
 * All pointers are caller-owned. Get_V4l2Info is 4 words; FdCommon is 9
 * words; Get_V4l2Drm and the V4L2 ext-control payload are 0x1a4 bytes.
 */
#ifndef OPENAURORA_SOURCE_HAL_H
#define OPENAURORA_SOURCE_HAL_H

#include <stdint.h>

struct oa_source_hal {
    int (*vin_open)(int index);            /* 0..4 -> /dev/video20/24/21/28/66 */
    void (*vin_close)(int fd);
    void (*vin_select)(int fd, int id);
    void (*vin_start)(int fd);
    void (*audio_easy_select)(int path, int id, const void *records);
    int (*vpltio_send)(int cmd, int a, void *buf, int b, int c, int d,
                       int e, int f, int g);
    void (*v4l2_info_get)(int index, void *out4);
    void (*v4l2_info_set)(int index, int w0, int w1, int w2, int w3);
    void (*v4l2_info_reset)(int index);
    void (*v4l2_drm_get)(int index, void *out_1a4);
    void (*v4l2_fd_get)(void *out9);
    void (*v4l2_fd_set)(int w0, int w1, int w2, int w3, int w4,
                        int w5, int w6, int w7, int w8);
    int (*drm_device_id)(int index);
    int (*drm_plane_type)(int path);
    void (*drm_plane_info)(int plane, void *out12);
    void (*drm_set_dpsource)(int dev, int w0, int w1, int w2);
    void (*video_ext_open)(int flag, unsigned type);
    void (*video_dec_stop)(int path);      /* PLAT_DEC_StopVideoDecoding */
    void (*video_dec_close)(int path);     /* Hal_VideoDec_Close */
    void (*sd_video_connect)(void);        /* PLAT_DEC_SdVideoConnect, type 1 */
    int (*ioctl)(int fd, unsigned long request, void *arg);
    /* plat_vin_slot_set_auto_color_system: open VIN(0), set the color-system
     * words, close. The vendor calls it from dev_src_init after creating the
     * hotplug task; the two words are table entry 2 = 0x00ff40ff, 0. */
    int (*vin_set_color_system)(int fd, uint32_t v0, uint32_t v1);
};

const struct oa_source_hal *oa_source_hal(void);

#endif
