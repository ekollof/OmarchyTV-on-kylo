/* Offline NIM stub tests: recovered StartTune/MonitorTune/signal shape and the
 * synthetic EIT section source. No hardware or vendor code. */
#include "openaurora/nim.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

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

int main(void)
{
    test_tune_signal();
    test_eit_source();
    puts("ok nim stub: tune/monitor/signal defaults and synthetic EIT source");
    return 0;
}
