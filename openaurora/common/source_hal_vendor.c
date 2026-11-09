/* Production source HAL table: direct libNDAL3/libNDAL_EX calls. */
#include "openaurora/source_hal.h"

#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef OA_NO_VENDOR
static int no_open(int index) { (void)index; return -1; }
static void no_close(int fd) { (void)fd; }
static void no_select(int fd, int id) { (void)fd; (void)id; }
static void no_start(int fd) { (void)fd; }
static void no_audio(int path, int id, const void *rec)
{
    (void)path; (void)id; (void)rec;
}
static int no_vplt(int cmd, int a, void *buf, int b, int c, int d, int e, int f, int g)
{
    (void)cmd; (void)a; (void)buf; (void)b; (void)c; (void)d;
    (void)e; (void)f; (void)g;
    return -1;
}
static void no_info_get(int index, void *out) { (void)index; (void)out; }
static void no_info_set(int index, int w0, int w1, int w2, int w3)
{
    (void)index; (void)w0; (void)w1; (void)w2; (void)w3;
}
static void no_info_reset(int index) { (void)index; }
static void no_drm_get(int index, void *out) { (void)index; (void)out; }
static void no_fd_get(void *out) { (void)out; }
static void no_fd_set(int w0, int w1, int w2, int w3, int w4,
                      int w5, int w6, int w7, int w8)
{
    (void)w0; (void)w1; (void)w2; (void)w3; (void)w4;
    (void)w5; (void)w6; (void)w7; (void)w8;
}
static int no_device(int index) { (void)index; return -1; }
static int no_plane(int path) { (void)path; return -1; }
static void no_plane_info(int plane, void *out) { (void)plane; (void)out; }
static void no_dp(int dev, int w0, int w1, int w2)
{
    (void)dev; (void)w0; (void)w1; (void)w2;
}
static void no_video_ext(int flag, unsigned type) { (void)flag; (void)type; }
static void no_dec_stop(int path) { (void)path; }
static void no_dec_close(int path) { (void)path; }
static void no_sd(void) {}
static int no_ioctl(int fd, unsigned long request, void *arg)
{
    (void)fd; (void)request; (void)arg;
    return -1;
}
static const struct oa_source_hal g_hal = {
    no_open, no_close, no_select, no_start, no_audio, no_vplt,
    no_info_get, no_info_set, no_info_reset, no_drm_get, no_fd_get, no_fd_set,
    no_device, no_plane, no_plane_info, no_dp, no_video_ext,
    no_dec_stop, no_dec_close, no_sd, no_ioctl
};
#else
extern int Hal_Vin_Open(int index);
extern void Hal_Vin_Close(int fd);
extern void Hal_Vin_SelectInput(int fd, int id);
extern void Hal_Vin_SetStreamStart(int fd);
extern void Hal_Audio_EasySelect(int path, int id, const void *records);
extern int Hal_Video_Vpltio_SendCmd(int cmd, int a, void *buf, int b, int c,
                                    int d, int e, int f, int g);
extern void Hal_Video_Get_V4l2Info(int index, void *out);
extern void Hal_Video_Set_V4l2Info(int index, int w0, int w1, int w2, int w3);
extern void Hal_Video_Reset_V4l2Info(int index);
extern void Hal_Video_Get_V4l2Drm(int index, void *out);
extern void Hal_Video_Get_V4l2FdCommon(void *out);
extern void Hal_Video_Set_V4l2FdCommon(int w0, int w1, int w2, int w3, int w4,
                                       int w5, int w6, int w7, int w8);
extern int Hal_Video_Get_DrmDeviceId(int index);
extern int Hal_Video_VideoPath_to_PlaneType(int path);
extern void Hal_Video_Get_DrmPlaneInfo(int plane, void *out);
extern void Hal_Video_Drm_Set_DpSource(int dev, int w0, int w1, int w2);
extern void NDAL_VideoExt_OpenDevice(int flag, unsigned type);
extern void Hal_VideoDec_Close(int path);

static int hal_ioctl(int fd, unsigned long request, void *arg)
{
    return ioctl(fd, request, arg);
}
static void hal_dec_stop(int path)
{
    /* PLAT_DEC_StopVideoDecoding is internal to aurora; Hal_VideoDec_Streamoff
     * is part of its body but only with the decoder state. Keep the stage
     * explicit without inventing the missing state machine. */
    (void)path;
}
static void hal_dec_close(int path) { Hal_VideoDec_Close(path); }
static void hal_sd_video_connect(void)
{
    /* PLAT_DEC_SdVideoConnect is internal to aurora; the low-level type-1
     * path is not used by this set's Bronlijst. Keep the slot explicit. */
}
static const struct oa_source_hal g_hal = {
    Hal_Vin_Open, Hal_Vin_Close, Hal_Vin_SelectInput, Hal_Vin_SetStreamStart,
    Hal_Audio_EasySelect, Hal_Video_Vpltio_SendCmd,
    Hal_Video_Get_V4l2Info, Hal_Video_Set_V4l2Info, Hal_Video_Reset_V4l2Info,
    Hal_Video_Get_V4l2Drm, Hal_Video_Get_V4l2FdCommon,
    Hal_Video_Set_V4l2FdCommon,
    Hal_Video_Get_DrmDeviceId, Hal_Video_VideoPath_to_PlaneType,
    Hal_Video_Get_DrmPlaneInfo, Hal_Video_Drm_Set_DpSource,
    NDAL_VideoExt_OpenDevice, hal_dec_stop, hal_dec_close,
    hal_sd_video_connect, hal_ioctl
};
#endif

const struct oa_source_hal *oa_source_hal(void)
{
    return &g_hal;
}
