/* Fake V4L2 device test for the ndp-hdmi event adapter. No /dev, TV or HAL. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/source_hdmi_event_io.h"

struct fake {
    int open_rc;
    int open_errno;
    int subscribe_rc;
    int subscribe_errno;
    int dq_rc;
    int dq_errno;
    uint32_t dq_type;
    uint8_t payload[OA_SOURCE_HDMI_EVENT_SIZE];
    int open_calls;
    int subscribe_calls;
    int dq_calls;
    int close_calls;
    char path[64];
    int flags;
    uint32_t subscribed_type;
};

static int fake_open(void *context, const char *path, int flags)
{
    struct fake *f = context;
    f->open_calls++;
    snprintf(f->path, sizeof f->path, "%s", path);
    f->flags = flags;
    if (f->open_rc < 0) {
        errno = f->open_errno;
        return f->open_rc;
    }
    return 9;
}

static int fake_ioctl(void *context, int fd, unsigned long request, void *arg)
{
    struct fake *f = context;
    assert(fd == 9);
    assert(arg != NULL);
    if (request == OA_SOURCE_HDMI_EVENT_IOC_SUBSCRIBE) {
        struct oa_source_hdmi_event_subscription *sub = arg;
        f->subscribe_calls++;
        assert(sizeof *sub == OA_SOURCE_HDMI_EVENT_SUBSCRIPTION_SIZE);
        f->subscribed_type = sub->type;
        assert(sub->id == 0 && sub->flags == 0);
        if (f->subscribe_rc < 0) {
            errno = f->subscribe_errno;
            return f->subscribe_rc;
        }
        return 0;
    }
    if (request == OA_SOURCE_HDMI_EVENT_IOC_DQEVENT) {
        struct oa_source_hdmi_event_buffer *event = arg;
        f->dq_calls++;
        memset(event, 0, sizeof *event);
        if (f->dq_rc < 0) {
            errno = f->dq_errno;
            return f->dq_rc;
        }
        event->type = f->dq_type;
        memcpy(event->data, f->payload, sizeof f->payload);
        return 0;
    }
    errno = EINVAL;
    return -1;
}

static int fake_close(void *context, int fd)
{
    struct fake *f = context;
    assert(fd == 9);
    f->close_calls++;
    return 0;
}

static void fill_payload(struct fake *f)
{
    memset(f->payload, 0, sizeof f->payload);
    f->payload[0] = 0x15;
    f->payload[4] = 1;
    f->payload[5] = 0x70;
    f->payload[6] = 0x70;
    f->payload[7] = 0x17;
    for (int i = 8; i < (int)OA_SOURCE_HDMI_EVENT_SIZE; i++)
        f->payload[i] = (uint8_t)i;
}

static void reset(struct fake *f)
{
    memset(f, 0, sizeof *f);
    f->dq_type = OA_SOURCE_HDMI_EVENT_TYPE;
}

static const struct oa_source_hdmi_event_io io = {
    .open = fake_open,
    .ioctl = fake_ioctl,
    .close = fake_close,
};

static void test_subscribe_read_close(void)
{
    struct fake f;
    reset(&f);
    fill_payload(&f);
    struct oa_source_hdmi_event_io fio = io;
    fio.context = &f;
    int fd = -1;
    assert(oa_source_hdmi_event_subscribe(&fio, "/dev/video21", &fd) == 0);
    assert(fd == 9);
    assert(f.open_calls == 1);
    assert(!strcmp(f.path, "/dev/video21"));
    assert((f.flags & O_RDWR) && (f.flags & O_NONBLOCK) && (f.flags & O_CLOEXEC));
    assert(f.subscribe_calls == 1);
    assert(f.subscribed_type == OA_SOURCE_HDMI_EVENT_TYPE);

    struct oa_source_hdmi_event event;
    assert(oa_source_hdmi_event_read(&fio, fd, &event) == 1);
    assert(event.source_type == 0x15);
    assert(event.update_flag == 1);
    assert(event.video_setting_0c == 0x70);
    assert(event.video_setting_86 == 0x1770);
    assert(event.raw[4] == 1 && event.raw[27] == 27);

    assert(oa_source_hdmi_event_close(&fio, fd) == 0);
    assert(f.close_calls == 1);
}

static void test_no_event_and_wrong_type(void)
{
    struct fake f;
    reset(&f);
    struct oa_source_hdmi_event_io fio = io;
    fio.context = &f;
    struct oa_source_hdmi_event event;

    f.dq_rc = -1;
    f.dq_errno = EAGAIN;
    assert(oa_source_hdmi_event_read(&fio, 9, &event) == 0);

    f.dq_rc = 0;
    f.dq_type = 0x12345678;
    assert(oa_source_hdmi_event_read(&fio, 9, &event) == 0);

    f.dq_type = OA_SOURCE_HDMI_EVENT_TYPE;
    f.dq_rc = -1;
    f.dq_errno = EIO;
    assert(oa_source_hdmi_event_read(&fio, 9, &event) == -EIO);
}

static void test_failures(void)
{
    struct fake f;
    reset(&f);
    struct oa_source_hdmi_event_io fio = io;
    fio.context = &f;
    int fd = 42;

    f.open_rc = -1;
    f.open_errno = ENOENT;
    assert(oa_source_hdmi_event_subscribe(&fio, "/dev/video21", &fd) == -ENOENT);
    assert(fd == 42);
    assert(f.subscribe_calls == 0 && f.close_calls == 0);

    reset(&f);
    fio.context = &f;
    f.subscribe_rc = -1;
    f.subscribe_errno = EPERM;
    fd = 42;
    assert(oa_source_hdmi_event_subscribe(&fio, "/dev/video21", &fd) == -EPERM);
    assert(fd == 42);
    assert(f.close_calls == 1);

    struct oa_source_hdmi_event event;
    assert(oa_source_hdmi_event_read(&fio, -1, &event) == -EINVAL);
    assert(oa_source_hdmi_event_close(&fio, -1) == -EINVAL);

    assert(oa_source_hdmi_event_subscribe(NULL, "/dev/video21", &fd) == -EINVAL);
    assert(oa_source_hdmi_event_subscribe(&fio, NULL, &fd) == -EINVAL);
    assert(oa_source_hdmi_event_subscribe(&fio, "", &fd) == -EINVAL);
    assert(oa_source_hdmi_event_subscribe(&fio, "/dev/video21", NULL) == -EINVAL);
    assert(oa_source_hdmi_event_read(&fio, 9, NULL) == -EINVAL);
}

int main(void)
{
    test_subscribe_read_close();
    test_no_event_and_wrong_type();
    test_failures();
    puts("ok source-hdmi-event-io: target 128/8 ABI, subscribe 0xc000001, "
         "dequeue, decoded payload, EAGAIN/wrong-type/error and cleanup gates");
    return 0;
}
