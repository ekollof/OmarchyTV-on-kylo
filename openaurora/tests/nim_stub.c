/* Offline NIM stub tests: recovered StartTune/MonitorTune/signal shape and the
 * synthetic EIT section source. No hardware or vendor code. */
#include "openaurora/nim.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

int main(void)
{
    test_tune_signal();
    test_eit_source();
    test_ts_replay();
    puts("ok nim stub: tune/signal defaults, synthetic EIT and TS replay demux");
    return 0;
}
