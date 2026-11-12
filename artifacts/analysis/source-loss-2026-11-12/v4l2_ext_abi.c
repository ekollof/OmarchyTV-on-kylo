#include <linux/videodev2.h>
#include <stddef.h>
#include <stdio.h>
int main(void) {
    printf("ext_control size=%zu id=%zu size=%zu ptr=%zu reserved2=%zu value64=%zu\n",
        sizeof(struct v4l2_ext_control),
        offsetof(struct v4l2_ext_control, id),
        offsetof(struct v4l2_ext_control, size),
        offsetof(struct v4l2_ext_control, ptr),
        offsetof(struct v4l2_ext_control, reserved2),
        offsetof(struct v4l2_ext_control, value64));
    printf("ext_controls size=%zu class=%zu count=%zu error=%zu controls=%zu\n",
        sizeof(struct v4l2_ext_controls),
        offsetof(struct v4l2_ext_controls, ctrl_class),
        offsetof(struct v4l2_ext_controls, count),
        offsetof(struct v4l2_ext_controls, error_idx),
        offsetof(struct v4l2_ext_controls, controls));
    return 0;
}
