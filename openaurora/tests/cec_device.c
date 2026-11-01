#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/cec_device.h"
#include "openaurora/vendor_cec.h"

static struct {
    int opens, closes, init, shut, sends, receives;
    int fd, fail_open, fail_init, fail_shut, fail_close, fail_tx, fail_rx;
    int rx_bad, rx_empty;
    uint8_t frame[16]; size_t length;
} fixture;
static int fake_open(void *context, const char *path, int flags)
{
    assert(context == &fixture && !strcmp(path, "/fixture/ntkcec"));
    assert((flags & O_ACCMODE) == O_RDWR && (flags & O_CLOEXEC));
    fixture.opens++;
    if (fixture.fail_open) { errno = EBUSY; return -1; }
    return fixture.fd;
}
static int fake_close(void *context, int fd)
{
    assert(context == &fixture && fd == fixture.fd); fixture.closes++;
    if (fixture.fail_close) { errno = EINTR; return -1; }
    return 0;
}
static int fake_ioctl(void *context, int fd, unsigned long request, void *arg)
{
    assert(context == &fixture && fd == fixture.fd && arg);
    switch (request) {
    case 0x401c4301: {
        const uint32_t expected[] = { 300, 3, 0xedd5c5ad, 0x8b64573f, 0x8b642a12, 0, 0 };
        assert(!memcmp(expected, arg, sizeof expected)); fixture.init++;
        if (fixture.fail_init) { errno = EIO; return -1; }
        return 0;
    }
    case 0x80014302:
        assert(*(const uint8_t *)arg == 0); fixture.shut++;
        if (fixture.fail_shut) { errno = EIO; return -1; }
        return 0;
    case 0xc0104304: {
        const oa_cec_raw_packet *p = arg;
        assert(p->status == 1 && !p->hardware_id && p->bytes && p->length >= 1 && p->length <= 16);
        fixture.sends++; fixture.length = p->length; memcpy(fixture.frame, p->bytes, p->length);
        if (fixture.fail_tx) { errno = EINTR; return -1; }
        return 0;
    }
    case 0x80104303: {
        oa_cec_raw_packet *p = arg;
        assert(!p->status && !p->length && !p->hardware_id && p->bytes);
        for (unsigned i = 0; i < 16; i++) assert(p->bytes[i] == 0);
        fixture.receives++;
        if (fixture.fail_rx) { errno = EIO; return -1; }
        if (fixture.rx_empty) { p->bytes = NULL; return 0; }
        memcpy(p->bytes, fixture.frame, fixture.length);
        p->length = fixture.rx_bad ? 17 : (uint8_t)fixture.length; p->status = 1;
        return 0;
    }
    default: assert(0); return -1;
    }
}
static const oa_cec_device_io io = { &fixture, fake_open, fake_ioctl, fake_close };
static void reset(void) { memset(&fixture, 0, sizeof fixture); }
int main(void)
{
    oa_cec_device d = OA_CEC_DEVICE_INIT;
    reset(); fixture.fd = 0; /* Descriptor zero is valid for the native backend. */
    assert(!oa_cec_device_open(&d, "/fixture/ntkcec", &io));
    assert(d.fd == 0 && d.initialized && fixture.opens == 1 && fixture.init == 1);
    assert(oa_cec_device_open(&d, "/fixture/ntkcec", &io) == -1 && errno == EBUSY);
    assert(fixture.opens == 1);
    oa_cec_message message = { .destination = 5, .opcode = 0x44, .count = 1, .operands = {0x41} };
    assert(!oa_cec_device_send(&d, &message));
    const uint8_t expected[] = { 0x05, 0x44, 0x41 };
    assert(fixture.sends == 1 && fixture.length == sizeof expected && !memcmp(expected, fixture.frame, sizeof expected));
    message.count = 0; message.opcode = -1;
    assert(!oa_cec_device_send(&d, &message) && fixture.length == 1 && fixture.frame[0] == 5);
    message.initiator = 4;
    assert(oa_cec_device_send(&d, &message) == -1 && errno == EADDRNOTAVAIL && fixture.sends == 2);
    assert(!oa_cec_device_close(&d) && d.fd == -1 && !d.initialized && !d.io);
    assert(fixture.shut == 1 && fixture.closes == 1 && !oa_cec_device_close(&d));
    assert(!oa_cec_device_open(&d, "/fixture/ntkcec", &io) && fixture.opens == 2);
    assert(!oa_cec_device_close(&d) && fixture.closes == 2);
    puts("  ok   native fd zero, exact open/close parameters, TV frame/poll transmission and independent reopen");

    reset(); fixture.fd = 7; fixture.fail_open = 1;
    assert(oa_cec_device_open(&d, "/fixture/ntkcec", &io) == -1 && errno == EBUSY);
    assert(d.fd == -1 && !fixture.init && !fixture.closes);
    fixture.fail_open = 0; fixture.fail_init = 1; fixture.fail_close = 1;
    assert(oa_cec_device_open(&d, "/fixture/ntkcec", &io) == -1 && errno == EIO);
    assert(d.fd == -1 && fixture.closes == 1 && !fixture.shut);
    fixture.fail_init = fixture.fail_close = 0;
    assert(!oa_cec_device_open(&d, "/fixture/ntkcec", &io));
    fixture.fail_shut = 1;
    assert(oa_cec_device_close(&d) == -1 && errno == EIO && d.fd == 7 && d.initialized);
    assert(fixture.closes == 1);
    fixture.fail_shut = 0;
    assert(!oa_cec_device_close(&d) && fixture.closes == 2);
    puts("  ok   failed open/init unwind once, preserve primary errno and retain failed hardware-close ownership for retry");

    reset(); fixture.fd = 9;
    assert(!oa_cec_device_open(&d, "/fixture/ntkcec", &io));
    fixture.frame[0] = 0x5f; fixture.frame[1] = 0x72; fixture.frame[2] = 1; fixture.length = 3;
    oa_cec_message out;
    assert(oa_cec_device_receive(&d, &out) == 1 && out.initiator == 5 && out.destination == 15 &&
           out.opcode == 0x72 && out.operands[0] == 1 && oa_cec_validate_audio(&out) == OA_CEC_VALID);
    oa_cec_message original = out;
    fixture.rx_empty = 1;
    assert(oa_cec_device_receive(&d, &out) == 0 && !memcmp(&original, &out, sizeof out));
    fixture.rx_empty = 0; fixture.rx_bad = 1;
    assert(oa_cec_device_receive(&d, &out) == -1 && errno == EPROTO && !memcmp(&original, &out, sizeof out));
    fixture.rx_bad = 0; fixture.fail_rx = 1;
    assert(oa_cec_device_receive(&d, &out) == -1 && errno == EIO && !memcmp(&original, &out, sizeof out));
    fixture.fail_tx = 1; message.initiator = 0;
    assert(oa_cec_device_send(&d, &message) == -1 && errno == EINTR && fixture.sends == 1);
    fixture.fail_close = 1;
    assert(oa_cec_device_close(&d) == -1 && errno == EINTR && d.fd == -1 && !d.io);
    assert(!oa_cec_device_close(&d) && fixture.closes == 1);
    puts("  ok   raw RX accepts spec-valid broadcast audio; empty/error atomicity; TX EINTR is not replayed; final close consumes fd");
    assert(oa_cec_device_send(&d, &message) == -1 && errno == ENODEV);
    assert(oa_cec_device_receive(&d, &out) == -1 && errno == ENODEV);
#if !defined(__arm__) || UINTPTR_MAX != UINT32_MAX
    assert(oa_cec_device_open(&d, "/fixture/ntkcec", NULL) == -1 && errno == ENOTSUP);
#endif
    return 0;
}
