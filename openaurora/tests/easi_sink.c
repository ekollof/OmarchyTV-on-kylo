/* Pure EASI effect consumer test. No tuner, TV, HAL or UI. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/easi_sink.h"

struct recorder {
    enum oa_easi_effect ev[32];
    int arg[32];
    int n;
    int fail_mute;
};

static int apply(void *ud, enum oa_easi_effect effect, int arg)
{
    struct recorder *r = ud;
    if (r->n < (int)(sizeof r->ev / sizeof r->ev[0])) {
        r->ev[r->n] = effect;
        r->arg[r->n] = arg;
        r->n++;
    }
    if ((effect == OA_EASI_MUTE_ON || effect == OA_EASI_MUTE_OFF) &&
        r->fail_mute)
        return OA_EASI_FAILED;
    if (effect == OA_EASI_MUTE_ON || effect == OA_EASI_MUTE_OFF)
        return OA_EASI_APPLIED;
    return OA_EASI_UNSUPPORTED;
}

static void test_dispatch_counts(void)
{
    struct recorder r;
    oa_easi_sink s;
    oa_easi_sink_state st;
    memset(&r, 0, sizeof r);
    oa_easi_sink_init(&s, &(oa_easi_sink_io){ &r, apply });

    assert(oa_easi_sink_dispatch(&s, OA_EASI_MUTE_ON, 1) == OA_EASI_APPLIED);
    assert(oa_easi_sink_dispatch(&s, OA_EASI_MUTE_OFF, 0) == OA_EASI_APPLIED);
    assert(oa_easi_sink_dispatch(&s, OA_EASI_HIDE_VIDEO, 0) == OA_EASI_UNSUPPORTED);
    assert(oa_easi_sink_dispatch(&s, OA_EASI_SHOW_VIDEO, 0) == OA_EASI_UNSUPPORTED);
    assert(oa_easi_sink_dispatch(&s, OA_EASI_DECODER_SELECT, 1) == OA_EASI_UNSUPPORTED);

    assert(oa_easi_sink_snapshot(&s, &st) == 0);
    assert(st.dispatched == 5 && st.applied == 2 && st.unsupported == 3);
    assert(st.failed == 0 && st.no_backend == 0);
    assert(st.last_status == OA_EASI_UNSUPPORTED);
    assert(st.last_effect == OA_EASI_DECODER_SELECT);
    assert(r.n == 5);
    assert(r.ev[0] == OA_EASI_MUTE_ON && r.arg[0] == 1);
    assert(r.ev[1] == OA_EASI_MUTE_OFF && r.arg[1] == 0);
}

static void test_failure_and_no_backend(void)
{
    struct recorder r;
    oa_easi_sink s;
    oa_easi_sink_state st;
    memset(&r, 0, sizeof r);
    r.fail_mute = 1;
    oa_easi_sink_init(&s, &(oa_easi_sink_io){ &r, apply });
    assert(oa_easi_sink_dispatch(&s, OA_EASI_MUTE_ON, 1) == OA_EASI_FAILED);
    assert(oa_easi_sink_snapshot(&s, &st) == 0 && st.failed == 1 && st.applied == 0);

    /* No callback: effects are counted as no_backend, never applied. */
    oa_easi_sink_init(&s, NULL);
    assert(oa_easi_sink_dispatch(&s, OA_EASI_MUTE_ON, 1) == OA_EASI_NO_BACKEND);
    assert(oa_easi_sink_snapshot(&s, &st) == 0 && st.no_backend == 1);
    assert(oa_easi_sink_dispatch(NULL, OA_EASI_MUTE_ON, 1) == OA_EASI_NO_BACKEND);

    assert(oa_easi_sink_snapshot(NULL, &st) == -EINVAL);
    assert(oa_easi_sink_snapshot(&s, NULL) == -EINVAL);
}

int main(void)
{
    test_dispatch_counts();
    test_failure_and_no_backend();
    puts("ok easi sink: real-effect consumer dispatch, applied/unsupported/"
         "failure accounting and no-backend safety");
    return 0;
}
