#include <linux/videodev2.h>
#include <stddef.h>
#include <stdio.h>
_Static_assert(sizeof(struct v4l2_event) == 128, "event size");
_Static_assert(offsetof(struct v4l2_event, u) == 8, "event u offset");
_Static_assert(sizeof(struct v4l2_event_subscription) == 32, "subscription size");
int main(void) {
    printf("event=%zu u=%zu sub=%zu subscribe=0x%lx dq=0x%lx\n",
        sizeof(struct v4l2_event), offsetof(struct v4l2_event, u),
        sizeof(struct v4l2_event_subscription),
        (unsigned long)VIDIOC_SUBSCRIBE_EVENT,
        (unsigned long)VIDIOC_DQEVENT);
    return 0;
}
