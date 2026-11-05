/* Offline NIM stub tests: recovered StartTune/MonitorTune/signal shape and the
 * synthetic EIT section source. No hardware or vendor code. */
#include "openaurora/nim.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "openaurora/eit_dvb.h"
#include "openaurora/eit_section.h"

struct capture {
    int count;
    struct oa_eit_event_info last;
};

static int capture_event(const struct oa_eit_event_info *event, void *user)
{
    struct capture *c = user;
    c->last = *event;
    c->count++;
    return 0;
}

static void test_tune_signal(void)
{
    const struct oa_tuner_backend *backend = oa_nim_stub_backend();
    assert(backend && !strcmp(backend->name, "nim-stub") && backend->nim);
    const uint32_t params[2] = { 0, 474000 };
    struct oa_nim_tune_result result;
    struct oa_nim_signal signal;

    assert(backend->nim->start(OA_NIM_PATH_TERRESTRIAL, NULL, 0, &result) ==
        -EINVAL);
    assert(backend->nim->start(9, params, 2, &result) == -EINVAL);
    assert(backend->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &result) ==
        0);
    assert(result.state == OA_NIM_TUNE_UNLOCKED && result.frequency_khz == 474000);

    assert(backend->nim->monitor(OA_NIM_PATH_CABLE, &result) == -EINVAL);
    assert(backend->nim->signal(OA_NIM_PATH_TERRESTRIAL, &signal) == 0);
    assert(signal.locked == 0 && signal.strength == 20 &&
        signal.quality == 0 && signal.ber == 0.0f && signal.snr == 0.0f &&
        signal.unc == OA_NIM_UNC_UNKNOWN);
    assert(backend->nim->monitor(OA_NIM_PATH_TERRESTRIAL, &result) == 0);
    assert(result.state == OA_NIM_TUNE_UNLOCKED);
    assert(backend->nim->monitor(OA_NIM_PATH_TERRESTRIAL, &result) == 0);
    assert(result.state == OA_NIM_TUNE_LOCKED);
    assert(backend->nim->signal(OA_NIM_PATH_TERRESTRIAL, &signal) == 0);
    assert(signal.locked == 1 && signal.strength == 80 &&
        signal.quality == 75 && signal.unc == 0 && signal.snr == 20.5f);

    assert(backend->nim->stop(OA_NIM_PATH_CABLE) == -EINVAL);
    assert(backend->nim->stop(OA_NIM_PATH_TERRESTRIAL) == 0);
    assert(backend->nim->signal(OA_NIM_PATH_TERRESTRIAL, &signal) == 0);
    assert(signal.locked == 0 && signal.strength == 0 &&
        signal.unc == OA_NIM_UNC_UNKNOWN);
    assert(backend->nim->stop(OA_NIM_PATH_TERRESTRIAL) == -EINVAL);
}

static void test_eit_source(void)
{
    const struct oa_tuner_backend *backend = oa_nim_stub_backend();
    assert(backend->next_eit);
    uint8_t section[4096];
    size_t len = 0;
    struct capture capture;

    /* Not tuned: no output. */
    assert(backend->next_eit(section, sizeof section, &len) == 0);

    const uint32_t params[2] = { 0, OA_NIM_DEFAULT_FREQUENCY_KHZ };
    struct oa_nim_tune_result result;
    assert(backend->nim->start(OA_NIM_PATH_CABLE, params, 2, &result) == 0);
    for (unsigned i = 0; i < 12; i++) {
        assert(backend->next_eit(section, sizeof section, &len) == 1);
        memset(&capture, 0, sizeof capture);
        assert(oa_eit_parse_section(section, len, capture_event, &capture) == 1);
        assert(capture.last.service_id == 101 + i);
        assert(capture.last.event_id == 1001 + i);
        assert(capture.last.actual == 1 && capture.last.running_status == 4);
        assert(capture.last.range.begin == 1677326400u);
        assert(capture.last.range.end == 1677328200u);
        char expected[32];
        snprintf(expected, sizeof expected, "Stub Service %u", i + 1);
        assert(!strcmp(capture.last.name, expected));
    }
    assert(backend->next_eit(section, sizeof section, &len) == 0);
    assert(backend->nim->stop(OA_NIM_PATH_CABLE) == 0);

    /* A new tune resets the source. */
    assert(backend->nim->start(OA_NIM_PATH_QPSK, params, 2, &result) == 0);
    assert(backend->next_eit(section, sizeof section, &len) == 1);
    memset(&capture, 0, sizeof capture);
    assert(oa_eit_parse_section(section, len, capture_event, &capture) == 1);
    assert(capture.last.service_id == 101);
    assert(backend->nim->stop(OA_NIM_PATH_QPSK) == 0);
}

struct ndal_mock {
    int opens, closes, tunes, scans, frequencies, status_calls, metrics_calls;
    int fail_open, fail_tune, fail_status, fail_metrics;
    int fd;
    int locked;
    uint8_t path;
    uint32_t frequency;
    struct oa_nim_signal signal;
};

static int mock_open(void *ctx, const char *path)
{
    struct ndal_mock *m = ctx;
    (void)path;
    m->opens++;
    return m->fail_open ? -EACCES : m->fd;
}

static int mock_close(void *ctx, int fd)
{
    struct ndal_mock *m = ctx;
    assert(fd == m->fd);
    m->closes++;
    return 0;
}

static int mock_tune(void *ctx, int fd, uint8_t path, uint32_t frequency)
{
    struct ndal_mock *m = ctx;
    assert(fd == m->fd);
    m->tunes++;
    m->path = path;
    m->frequency = frequency;
    return m->fail_tune ? -EIO : 0;
}

static int mock_scan(void *ctx, int fd, uint32_t mode)
{
    struct ndal_mock *m = ctx;
    assert(fd == m->fd);
    assert(mode == OA_NIM_PATH_TERRESTRIAL);
    m->scans++;
    return 0;
}

static int mock_frequency(void *ctx, int fd, uint32_t *khz)
{
    struct ndal_mock *m = ctx;
    assert(fd == m->fd);
    m->frequencies++;
    *khz = 474000;
    return 0;
}

static int mock_delivery(void *ctx, int fd, uint8_t *system)
{
    struct ndal_mock *m = ctx;
    assert(fd == m->fd);
    *system = 1;
    return 0;
}

static int mock_symbol(void *ctx, int fd, uint32_t *ksym)
{
    struct ndal_mock *m = ctx;
    assert(fd == m->fd);
    *ksym = 6900;
    return 0;
}

static int mock_status(void *ctx, int fd, uint32_t *status)
{
    struct ndal_mock *m = ctx;
    assert(fd == m->fd);
    m->status_calls++;
    if (m->fail_status) return -EIO;
    *status = m->locked ? 0x10 : 0;
    return 0;
}

static int mock_metrics(void *ctx, int fd, struct oa_nim_signal *out)
{
    struct ndal_mock *m = ctx;
    assert(fd == m->fd);
    m->metrics_calls++;
    if (m->fail_metrics) return -EIO;
    *out = m->signal;
    return 0;
}

static int mock_set_one(void *ctx, int fd, uint32_t value)
{
    struct ndal_mock *m = ctx;
    assert(fd == m->fd);
    (void)value;
    return 0;
}

static int mock_set_standby(void *ctx, int fd, int on)
{
    struct ndal_mock *m = ctx;
    assert(fd == m->fd);
    (void)on;
    return 0;
}

static void test_ndal_mock(void)
{
    struct ndal_mock mock;
    memset(&mock, 0, sizeof mock);
    mock.fd = 42;
    mock.locked = 1;
    mock.signal.locked = 1;
    mock.signal.strength = 80;
    mock.signal.quality = 75;
    mock.signal.ber = 1.5f;
    mock.signal.snr = 20.5f;
    mock.signal.unc = 0;
    const struct oa_ndal_nim_ops ops = {
        mock_open, mock_close, mock_tune, mock_scan, mock_frequency,
        mock_delivery, mock_symbol, mock_status, mock_metrics, mock_set_one,
        mock_set_standby
    };
    oa_nim_ndal_set_ops(&ops, &mock);
    const struct oa_tuner_backend *backend =
        oa_nim_ndal_backend("/mock/frontend0");
    assert(backend && !strcmp(backend->name, "nim-ndal"));
    assert(backend->nim && backend->next_eit);

    struct oa_nim_tune_result result;
    const uint32_t params[2] = { 0, 482000 };
    assert(backend->nim->start(OA_NIM_PATH_TERRESTRIAL, NULL, 0, &result) ==
        -EINVAL);
    assert(backend->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &result) ==
        0);
    assert(mock.opens == 1 && mock.tunes == 1 && mock.scans == 1);
    assert(mock.path == OA_NIM_PATH_TERRESTRIAL && mock.frequency == 482000);
    assert(result.state == OA_NIM_TUNE_UNLOCKED &&
        result.frequency_khz == 474000);

    assert(backend->nim->monitor(OA_NIM_PATH_TERRESTRIAL, &result) == 0);
    assert(result.state == OA_NIM_TUNE_LOCKED &&
        result.frequency_khz == 474000);
    struct oa_nim_signal signal;
    assert(backend->nim->signal(OA_NIM_PATH_TERRESTRIAL, &signal) == 0);
    assert(signal.strength == 80 && signal.quality == 75 &&
        signal.snr == 20.5f && signal.unc == 0);
    assert(backend->nim->stop(OA_NIM_PATH_TERRESTRIAL) == 0);
    assert(mock.closes == 1);

    mock.fail_tune = 1;
    assert(backend->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &result) ==
        -EIO);
    assert(mock.opens == 2 && mock.closes == 2);
    mock.fail_tune = 0;
    mock.fail_status = 1;
    assert(backend->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &result) ==
        0);
    assert(backend->nim->monitor(OA_NIM_PATH_TERRESTRIAL, &result) == -EIO);
    assert(backend->nim->stop(OA_NIM_PATH_TERRESTRIAL) == 0);
    mock.fail_status = 0;
    mock.fail_metrics = 1;
    assert(backend->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &result) ==
        0);
    assert(backend->nim->signal(OA_NIM_PATH_TERRESTRIAL, &signal) == -EIO);
    assert(backend->nim->stop(OA_NIM_PATH_TERRESTRIAL) == 0);
    mock.fail_metrics = 0;
    mock.fail_open = 1;
    assert(backend->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &result) ==
        -EACCES);
    oa_nim_ndal_set_ops(NULL, NULL);
}

static void ts_packet(uint8_t out[188], uint16_t pid, uint8_t cc, int pusi,
    const uint8_t *payload, size_t len)
{
    memset(out, 0xff, 188);
    out[0] = 0x47;
    out[1] = (uint8_t)((pusi ? 0x40 : 0) | ((pid >> 8) & 0x1f));
    out[2] = (uint8_t)pid;
    out[3] = (uint8_t)(0x10 | (cc & 0x0f));
    size_t offset = 4;
    if (pusi) out[offset++] = 0;
    assert(offset + len <= 188);
    memcpy(out + offset, payload, len);
}

static void test_ts_replay(void)
{
    char name[120], text[120];
    memset(name, 'N', sizeof name - 1);
    name[sizeof name - 1] = 0;
    memset(text, 'T', sizeof text - 1);
    text[sizeof text - 1] = 0;
    const struct oa_eit_build_event event = {
        .event_id = 41,
        .start = { 0xea, 0x60, 0x11, 0x00, 0x00 },
        .duration = { 0x00, 0x30, 0x00 },
        .running_status = 1,
        .name = name,
        .text = text
    };
    uint8_t section[512];
    size_t section_len = 0;
    assert(oa_eit_build_section(0x4e, 401, 6, 1, &event, 1, section,
        sizeof section, &section_len) == 0);
    assert(section_len > 184 && section_len < sizeof section);

    char path[] = "/tmp/openaurora-nim-ts-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    FILE *file = fdopen(fd, "wb");
    assert(file);
    uint8_t packet[188];
    size_t offset = 0;
    uint8_t cc = 0;
    while (offset < section_len) {
        size_t chunk = offset == 0 ? 183 : 184;
        size_t n = section_len - offset;
        if (n > chunk) n = chunk;
        ts_packet(packet, 0x12, cc++, offset == 0, section + offset, n);
        assert(fwrite(packet, 1, sizeof packet, file) == sizeof packet);
        offset += n;
    }
    assert(fclose(file) == 0);

    assert(oa_nim_ts_open(path) == 0);
    const struct oa_tuner_backend *backend = oa_nim_ts_backend();
    assert(backend && !strcmp(backend->name, "nim-ts-replay"));
    assert(backend->nim == oa_nim_stub_ops() && backend->next_eit);
    uint8_t got[4096];
    size_t got_len = 0;
    assert(backend->next_eit(got, sizeof got, &got_len) == 1);
    assert(got_len == section_len && !memcmp(got, section, section_len));
    struct capture capture;
    memset(&capture, 0, sizeof capture);
    assert(oa_eit_parse_section(got, got_len, capture_event, &capture) == 1);
    assert(capture.last.service_id == 401 && capture.last.event_id == 41);
    assert(backend->next_eit(got, sizeof got, &got_len) == 0);
    assert(backend->next_eit(NULL, 0, &got_len) == -EINVAL);
    oa_nim_ts_close();
    assert(backend->next_eit(got, sizeof got, &got_len) == 0);
    assert(oa_nim_ts_open("/nonexistent/openaurora-eit.ts") == -ENOENT);
    assert(unlink(path) == 0);
}

struct eit_mock {
    int opens, filters, reads, closes;
    int fail_open, fail_filter, fail_read;
    int fd;
    int fed;
    uint16_t pid;
    uint8_t section[512];
    size_t section_len;
};

static int eit_open(void *ctx, int adapter, int index)
{
    struct eit_mock *m = ctx;
    assert(adapter == 0 && index == 0);
    m->opens++;
    return m->fail_open ? -EACCES : m->fd;
}

static int eit_filter(void *ctx, int fd, uint16_t pid)
{
    struct eit_mock *m = ctx;
    assert(fd == m->fd);
    m->filters++;
    m->pid = pid;
    return m->fail_filter ? -EIO : 0;
}

static long eit_read(void *ctx, int fd, uint8_t *buf, size_t cap)
{
    struct eit_mock *m = ctx;
    assert(fd == m->fd);
    m->reads++;
    if (m->fail_read) return -EIO;
    if (m->fed) return 0;
    if (m->section_len > cap) return -E2BIG;
    memcpy(buf, m->section, m->section_len);
    m->fed = 1;
    return (long)m->section_len;
}

static int eit_close(void *ctx, int fd)
{
    struct eit_mock *m = ctx;
    assert(fd == m->fd);
    m->closes++;
    return 0;
}

struct eit_hal_seen {
    int opens, filters, closes;
    int adapter, index;
    int fd;
    uint16_t pid;
    size_t filter_len;
    void *filter, *mask, *mode_ptr;
    int flags;
    int open_mode;
    int fail_filter;
};

static struct eit_hal_seen eit_hal;

static int eit_hal_open(int adapter, int index)
{
    eit_hal.opens++;
    eit_hal.adapter = adapter;
    eit_hal.index = index;
    if (eit_hal.open_mode == 1) {
        errno = 0;
        return -1;
    }
    if (eit_hal.open_mode == 2) {
        errno = EACCES;
        return -1;
    }
    return 81;
}

static int eit_hal_filter(int fd, uint16_t pid, size_t filter_len, void *filter,
    void *mask, void *mode, char flags)
{
    eit_hal.filters++;
    eit_hal.fd = fd;
    eit_hal.pid = pid;
    eit_hal.filter_len = filter_len;
    eit_hal.filter = filter;
    eit_hal.mask = mask;
    eit_hal.mode_ptr = mode;
    eit_hal.flags = flags;
    return eit_hal.fail_filter ? -1 : 0;
}

static int eit_hal_close(int fd)
{
    assert(fd == 81);
    eit_hal.closes++;
    return 0;
}

static void test_eit_dvb(void)
{
    const struct oa_eit_build_event event = {
        .event_id = 51,
        .start = { 0xea, 0x60, 0x12, 0x00, 0x00 },
        .duration = { 0x00, 0x30, 0x00 },
        .running_status = 4,
        .name = "DVB section",
        .text = "Filtered"
    };
    struct eit_mock eit;
    memset(&eit, 0, sizeof eit);
    eit.fd = 77;
    assert(oa_eit_build_section(0x4e, 501, 2, 1, &event, 1, eit.section,
        sizeof eit.section, &eit.section_len) == 0);
    const struct oa_eit_dvb_ops ops = {
        eit_open, eit_filter, eit_read, eit_close
    };

    uint8_t section[512];
    size_t len = 0;
    assert(oa_eit_dvb_next(section, sizeof section, &len) == 0);
    assert(oa_eit_dvb_open(0, 0, 0x12, &ops, &eit) == 0);
    assert(eit.opens == 1 && eit.filters == 1 && eit.pid == 0x12);
    assert(oa_eit_dvb_next(section, sizeof section, &len) == 1);
    assert(len == eit.section_len &&
        !memcmp(section, eit.section, len));
    struct capture capture;
    memset(&capture, 0, sizeof capture);
    assert(oa_eit_parse_section(section, len, capture_event, &capture) == 1);
    assert(capture.last.service_id == 501 && capture.last.event_id == 51);
    assert(oa_eit_dvb_next(section, sizeof section, &len) == 0);
    oa_eit_dvb_close();
    assert(eit.closes == 1);

    eit.fail_filter = 1;
    assert(oa_eit_dvb_open(0, 0, 0x12, &ops, &eit) == -EIO);
    assert(eit.closes == 2);
    eit.fail_filter = 0;
    eit.fail_open = 1;
    assert(oa_eit_dvb_open(0, 0, 0x12, &ops, &eit) == -EACCES);
    eit.fail_open = 0;
    eit.fail_read = 1;
    assert(oa_eit_dvb_open(0, 0, 0x12, &ops, &eit) == 0);
    assert(oa_eit_dvb_next(section, sizeof section, &len) == -EIO);
    oa_eit_dvb_close();
    eit.fail_read = 0;

    /* A positive read is one section only when the length field matches and
     * is inside the same bounds the TS demux uses. */
    uint8_t saved[512];
    size_t full_len = eit.section_len;
    assert(full_len > 4 && full_len + 1 <= sizeof saved);
    memcpy(saved, eit.section, full_len);
    eit.fed = 0;
    eit.section_len = full_len - 1;
    assert(oa_eit_dvb_open(0, 0, 0x12, &ops, &eit) == 0);
    len = 99;
    assert(oa_eit_dvb_next(section, sizeof section, &len) == -EINVAL);
    assert(len == 0);
    oa_eit_dvb_close();
    eit.fed = 0;
    eit.section_len = 2;
    assert(oa_eit_dvb_open(0, 0, 0x12, &ops, &eit) == 0);
    assert(oa_eit_dvb_next(section, sizeof section, &len) == -EINVAL);
    assert(len == 0);
    oa_eit_dvb_close();
    eit.fed = 0;
    eit.section_len = full_len + 1;
    eit.section[full_len] = 0;
    assert(oa_eit_dvb_open(0, 0, 0x12, &ops, &eit) == 0);
    assert(oa_eit_dvb_next(section, sizeof section, &len) == -EINVAL);
    assert(len == 0);
    oa_eit_dvb_close();
    memset(eit.section, 0, 8);
    eit.section[0] = 0x4e;
    eit.section[1] = 0x00;
    eit.section[2] = 0x03;
    eit.section_len = 6;
    eit.fed = 0;
    assert(oa_eit_dvb_open(0, 0, 0x12, &ops, &eit) == 0);
    assert(oa_eit_dvb_next(section, sizeof section, &len) == -EINVAL);
    assert(len == 0);
    oa_eit_dvb_close();
    memcpy(eit.section, saved, full_len);
    eit.section_len = full_len;
    eit.fed = 0;

    /* Integrated next_eit path on nim-ndal. */
    struct ndal_mock nim;
    memset(&nim, 0, sizeof nim);
    nim.fd = 42;
    const struct oa_ndal_nim_ops nim_ops = {
        mock_open, mock_close, mock_tune, mock_scan, mock_frequency,
        mock_delivery, mock_symbol, mock_status, mock_metrics, mock_set_one,
        mock_set_standby
    };
    oa_nim_ndal_set_ops(&nim_ops, &nim);
    const struct oa_tuner_backend *backend =
        oa_nim_ndal_backend("/mock/frontend0");
    assert(backend && backend->next_eit);
    assert(oa_nim_ndal_enable_eit(0, 0, 0x12, &ops, &eit) == 0);
    const uint32_t params[2] = { 0, 474000 };
    struct oa_nim_tune_result result;
    assert(backend->nim->start(OA_NIM_PATH_TERRESTRIAL, params, 2, &result) ==
        0);
    eit.fed = 0;
    assert(backend->next_eit(section, sizeof section, &len) == 1);
    assert(len == eit.section_len &&
        !memcmp(section, eit.section, len));
    assert(backend->next_eit(section, sizeof section, &len) == 0);
    assert(backend->nim->stop(OA_NIM_PATH_TERRESTRIAL) == 0);
    assert(eit.closes >= 3);

    /* next_eit can open the filter before the frontend. stop still closes it. */
    eit.fed = 0;
    int closes = eit.closes;
    int frontend_opens = nim.opens;
    assert(backend->next_eit(section, sizeof section, &len) == 1);
    assert(nim.opens == frontend_opens);
    assert(backend->nim->stop(OA_NIM_PATH_TERRESTRIAL) == 0);
    assert(eit.closes == closes + 1);
    assert(backend->nim->stop(OA_NIM_PATH_TERRESTRIAL) == -EINVAL);
    oa_nim_ndal_set_ops(NULL, NULL);

    /* Default linux ops call the resolved HAL close, including after a
     * failed filter, and a -1 open with errno 0 is not a live fd. */
    memset(&eit_hal, 0, sizeof eit_hal);
    const struct oa_eit_dvb_hal hal = {
        eit_hal_open, eit_hal_filter, eit_hal_close
    };
    oa_eit_dvb_set_hal(&hal);
    const struct oa_eit_dvb_ops *linux_ops = oa_eit_dvb_linux_ops();
    eit_hal.open_mode = 1;
    assert(oa_eit_dvb_open(0, 1, 0x12, linux_ops, NULL) == -EIO);
    assert(eit_hal.opens == 1 && eit_hal.index == 1 && eit_hal.closes == 0);
    eit_hal.open_mode = 2;
    assert(oa_eit_dvb_open(0, 1, 0x12, linux_ops, NULL) == -EACCES);
    assert(eit_hal.closes == 0);
    eit_hal.open_mode = 0;
    eit_hal.fail_filter = 1;
    assert(oa_eit_dvb_open(0, 1, 0x12, linux_ops, NULL) == -EIO);
    assert(eit_hal.closes == 1 && eit_hal.fd == 81 && eit_hal.pid == 0x12 &&
        eit_hal.filter_len == 0 && eit_hal.filter == NULL &&
        eit_hal.mask == NULL && eit_hal.mode_ptr == NULL &&
        eit_hal.flags == 1);
    eit_hal.fail_filter = 0;
    assert(oa_eit_dvb_open(0, 1, 0x12, linux_ops, NULL) == 0);
    oa_eit_dvb_close();
    assert(eit_hal.closes == 2);
    assert(linux_ops->close_demux(NULL, -1) == -EBADF);
    assert(eit_hal.closes == 2);
    oa_eit_dvb_set_hal(NULL);
}

int main(void)
{
    test_tune_signal();
    test_eit_source();
    test_ts_replay();
    test_ndal_mock();
    test_eit_dvb();
    puts("ok nim stub: stub/replay/NDAL/EIT-DVB mock backends");
    return 0;
}
