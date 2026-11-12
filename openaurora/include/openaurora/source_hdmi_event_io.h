#ifndef OPENAURORA_SOURCE_HDMI_EVENT_IO_H
#define OPENAURORA_SOURCE_HDMI_EVENT_IO_H

#include <stddef.h>
#include <stdint.h>

#include "openaurora/source_hdmi_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Injectable V4L2 event adapter for the ndp-hdmi private event.
 *
 * Target ABI (ARM32/glibc 2.23, checked with the pinned toolchain):
 *   sizeof(struct v4l2_event) = 128, u at offset 8
 *   sizeof(struct v4l2_event_subscription) = 32
 *   VIDIOC_SUBSCRIBE_EVENT = 0x4020565a
 *   VIDIOC_DQEVENT         = 0x80805659
 * The host test uses the same constants and a fake ioctl; it never opens a
 * device. The live adapter is gated by the caller.
 */
#define OA_SOURCE_HDMI_EVENT_TYPE 0xc000001u
#define OA_SOURCE_HDMI_EVENT_V4L2_SIZE 128u
#define OA_SOURCE_HDMI_EVENT_DATA_OFFSET 8u
#define OA_SOURCE_HDMI_EVENT_SUBSCRIPTION_SIZE 32u
#define OA_SOURCE_HDMI_EVENT_IOC_SUBSCRIBE 0x4020565aUL
#define OA_SOURCE_HDMI_EVENT_IOC_DQEVENT 0x80805659UL

struct oa_source_hdmi_event_io {
    void *context;
    int (*open)(void *context, const char *path, int flags);
    int (*ioctl)(void *context, int fd, unsigned long request, void *arg);
    int (*close)(void *context, int fd);
};

struct oa_source_hdmi_event_subscription {
    uint32_t type;
    uint32_t id;
    uint32_t flags;
    uint32_t reserved[5];
};

/* Target-layout v4l2_event prefix; only type and data are consumed. */
struct oa_source_hdmi_event_buffer {
    uint32_t type;
    uint32_t pad;
    uint8_t data[64];
    uint32_t pending;
    uint32_t sequence;
    uint8_t timestamp[8];
    uint32_t id;
    uint32_t reserved[8];
    uint32_t tail; /* target 8-byte alignment pads the 124-byte fields to 128 */
};

_Static_assert(sizeof(struct oa_source_hdmi_event_subscription) ==
               OA_SOURCE_HDMI_EVENT_SUBSCRIPTION_SIZE,
               "target v4l2_event_subscription size");
_Static_assert(sizeof(struct oa_source_hdmi_event_buffer) ==
               OA_SOURCE_HDMI_EVENT_V4L2_SIZE,
               "target v4l2_event size");
_Static_assert(offsetof(struct oa_source_hdmi_event_buffer, data) ==
               OA_SOURCE_HDMI_EVENT_DATA_OFFSET,
               "target v4l2_event data offset");

/* Opens path and subscribes to 0xc000001. Returns 0 and sets *fd_out, or
 * -errno. The fd is closed on a subscribe failure. */
int oa_source_hdmi_event_subscribe(const struct oa_source_hdmi_event_io *io,
                                   const char *path, int *fd_out);

/* 1 = decoded event, 0 = no event/wrong type, -errno = I/O error.
 * The event buffer is zeroed before the dequeue. */
int oa_source_hdmi_event_read(const struct oa_source_hdmi_event_io *io, int fd,
                              struct oa_source_hdmi_event *out);

/* Closing the fd releases the V4L2 subscription. Returns 0 or -errno. */
int oa_source_hdmi_event_close(const struct oa_source_hdmi_event_io *io, int fd);

#ifdef __cplusplus
}
#endif

#endif
