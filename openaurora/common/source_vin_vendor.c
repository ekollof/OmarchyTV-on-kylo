/* Production VIN status read: /dev/video21 GET 0xa00967.
 * A non-ARM host gets an unavailable table; the fake module supplies its own
 * oa_source_vin_io() in tests/source_fake.c. */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "openaurora/source_vin.h"

#if defined(__arm__) && UINTPTR_MAX == UINT32_MAX
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

_Static_assert(sizeof(struct v4l2_ext_control) == 20,
               "target v4l2_ext_control size");
_Static_assert(offsetof(struct v4l2_ext_control, ptr) == 12,
               "target v4l2_ext_control ptr offset");
_Static_assert(sizeof(struct v4l2_ext_controls) == 24,
               "target v4l2_ext_controls size");

static int vin_open(void *context)
{
    (void)context;
    return open("/dev/video21", O_RDONLY | O_CLOEXEC);
}

static int vin_read(void *context, int fd,
                    uint32_t words[OA_SOURCE_VIN_WORDS])
{
    (void)context;
    if (fd < 0 || !words) return -EINVAL;

    struct v4l2_ext_control control;
    memset(&control, 0, sizeof control);
    control.id = OA_SOURCE_VIN_CTRL_ID;
    control.size = OA_SOURCE_VIN_SIZE;
    control.ptr = words;

    struct v4l2_ext_controls controls;
    memset(&controls, 0, sizeof controls);
    controls.ctrl_class = OA_SOURCE_VIN_CTRL_CLASS;
    controls.count = 1;
    controls.controls = &control;

    errno = 0;
    if (ioctl(fd, VIDIOC_G_EXT_CTRLS, &controls) < 0)
        return -(errno ? errno : EIO);
    /* Hal_Vin_GetHdmiStatus checks only the ioctl return. The target kernel
     * leaves error_idx == count on success; do not reject that sentinel. */
    return 0;
}

static int vin_close(void *context, int fd)
{
    (void)context;
    if (fd < 0) return -EINVAL;
    if (close(fd) < 0) return -(errno ? errno : EIO);
    return 0;
}
#else
static int vin_open(void *context)
{
    (void)context;
    return -1;
}

static int vin_read(void *context, int fd,
                    uint32_t words[OA_SOURCE_VIN_WORDS])
{
    (void)context;
    (void)fd;
    (void)words;
    return -1;
}

static int vin_close(void *context, int fd)
{
    (void)context;
    (void)fd;
    return -1;
}
#endif

static const struct oa_source_vin_io g_io = {
    NULL, vin_open, vin_read, vin_close
};

const struct oa_source_vin_io *oa_source_vin_io(void)
{
    return &g_io;
}
