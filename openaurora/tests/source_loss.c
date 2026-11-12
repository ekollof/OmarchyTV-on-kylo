/* Pure source-loss policy test from InputManager @0x00fb1f30/0x00fb1fbc.
 * No TV, HAL, tuner, UI or device access. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>

#include "openaurora/source_loss.h"

static void test_init(void)
{
    struct oa_source_loss s;
    oa_source_loss_init(&s);
    assert(s.previous_external == -1);
    assert(s.tuner_position == -1);
    assert(s.auto_switched == 0);
    assert(s.flag_88 == 0);
}

static void test_from_tv_uses_tuner_position(void)
{
    struct oa_source_loss s;
    int target = -1;
    oa_source_loss_init(&s);

    /* TV (current_external -1) to HDMI1, tuner position 3. */
    assert(oa_source_loss_connected(&s, -1, 4, 3, &target) ==
           OA_SOURCE_LOSS_SELECT_EXTERNAL);
    assert(target == 4);
    assert(s.previous_external == -1);
    assert(s.tuner_position == 3);
    assert(s.auto_switched == 1);

    /* HDMI1 disconnect: no previous external, so restore the tuner position. */
    target = -1;
    assert(oa_source_loss_disconnected(&s, 1, 1, &target) ==
           OA_SOURCE_LOSS_TUNER_POSITION);
    assert(target == 3);
    assert(s.previous_external == -1);
}

static void test_previous_external_restored_first(void)
{
    struct oa_source_loss s;
    int target = -1;
    oa_source_loss_init(&s);

    /* HDMI2 (5) to HDMI1 (4), tuner position 3. */
    assert(oa_source_loss_connected(&s, 5, 4, 3, &target) ==
           OA_SOURCE_LOSS_SELECT_EXTERNAL);
    assert(target == 4);
    assert(s.previous_external == 5);

    target = -1;
    assert(oa_source_loss_disconnected(&s, 1, 1, &target) ==
           OA_SOURCE_LOSS_PREVIOUS_EXTERNAL);
    assert(target == 5);
    assert(s.previous_external == -1);

    /* A second disconnect with no previous external falls back to the tuner. */
    target = -1;
    assert(oa_source_loss_disconnected(&s, 1, 1, &target) ==
           OA_SOURCE_LOSS_TUNER_POSITION);
    assert(target == 3);
}

static void test_gates(void)
{
    struct oa_source_loss s;
    int target = -1;

    /* this[0x98] not set: disconnect does nothing. */
    oa_source_loss_init(&s);
    assert(oa_source_loss_disconnected(&s, 1, 1, &target) ==
           OA_SOURCE_LOSS_NONE);

    /* this[0x88] set and matching group: only clear it. */
    oa_source_loss_init(&s);
    s.auto_switched = 1;
    s.tuner_position = 3;
    s.flag_88 = 1;
    assert(oa_source_loss_disconnected(&s, 1, 1, &target) ==
           OA_SOURCE_LOSS_NONE);
    assert(s.flag_88 == 0);
    assert(oa_source_loss_disconnected(&s, 1, 1, &target) ==
           OA_SOURCE_LOSS_TUNER_POSITION);
    assert(target == 3);

    /* Group mismatch changes nothing. */
    oa_source_loss_init(&s);
    s.auto_switched = 1;
    s.tuner_position = 3;
    s.flag_88 = 1;
    assert(oa_source_loss_disconnected(&s, 0, 1, &target) ==
           OA_SOURCE_LOSS_NONE);
    assert(s.flag_88 == 1);
    assert(s.tuner_position == 3);

    /* No previous source and no tuner availability: no action. */
    target = -1;
    s.flag_88 = 0;
    assert(oa_source_loss_disconnected(&s, 1, 0, &target) ==
           OA_SOURCE_LOSS_NONE);
    assert(target == -1);

    /* A negative tuner position is not a selectable target. */
    s.tuner_position = -1;
    assert(oa_source_loss_disconnected(&s, 1, 1, &target) ==
           OA_SOURCE_LOSS_NONE);
}

static void test_connect_is_idempotent(void)
{
    struct oa_source_loss s;
    int target = -1;
    oa_source_loss_init(&s);
    s.previous_external = 7;
    s.tuner_position = 2;
    s.auto_switched = 1;

    /* Same source: no state change and no switch. */
    assert(oa_source_loss_connected(&s, 4, 4, 9, &target) ==
           OA_SOURCE_LOSS_NONE);
    assert(s.previous_external == 7);
    assert(s.tuner_position == 2);
    assert(s.auto_switched == 1);
}

static void test_invalid_arguments(void)
{
    struct oa_source_loss s;
    int target = 42;
    oa_source_loss_init(&s);

    assert(oa_source_loss_connected(NULL, -1, 4, 3, &target) ==
           OA_SOURCE_LOSS_NONE);
    assert(oa_source_loss_connected(&s, -1, -1, 3, &target) ==
           OA_SOURCE_LOSS_NONE);
    assert(target == 42);
    assert(s.auto_switched == 0);
    assert(oa_source_loss_connected(&s, -1, 4, 3, NULL) ==
           OA_SOURCE_LOSS_NONE);
    assert(s.auto_switched == 0);
    assert(oa_source_loss_disconnected(NULL, 1, 1, &target) ==
           OA_SOURCE_LOSS_NONE);
    assert(oa_source_loss_disconnected(&s, 1, 1, NULL) ==
           OA_SOURCE_LOSS_NONE);
}

struct fake_worker {
    int dtv_calls;
    int tuner_calls;
    int last_position;
    int dtv_rc;
    int tuner_rc;
};

static int fake_dtv(void *context)
{
    struct fake_worker *f = context;
    f->dtv_calls++;
    return f->dtv_rc;
}

static int fake_tuner(void *context, int position)
{
    struct fake_worker *f = context;
    f->tuner_calls++;
    f->last_position = position;
    return f->tuner_rc;
}

static void test_inactive_source_worker(void)
{
    struct fake_worker f = { 0 };
    struct oa_source_loss_worker w = {
        .context = &f, .select_dtv = fake_dtv,
        .select_tuner_position = fake_tuner
    };

    assert(oa_source_loss_inactive_source(&w, 0, 1, 3) ==
           OA_SOURCE_LOSS_NONE);
    assert(oa_source_loss_inactive_source(&w, 1, 0, 3) ==
           OA_SOURCE_LOSS_NONE);
    assert(oa_source_loss_inactive_source(&w, 1, 1, -1) ==
           OA_SOURCE_LOSS_NONE);
    assert(f.dtv_calls == 0 && f.tuner_calls == 0);

    assert(oa_source_loss_inactive_source(&w, 1, 1, 3) ==
           OA_SOURCE_LOSS_DTV_TUNER);
    assert(f.dtv_calls == 1 && f.tuner_calls == 1);
    assert(f.last_position == 3);

    f.dtv_calls = f.tuner_calls = 0;
    f.dtv_rc = -EIO;
    assert(oa_source_loss_inactive_source(&w, 1, 1, 3) == -EIO);
    assert(f.dtv_calls == 1 && f.tuner_calls == 0);

    f.dtv_calls = f.tuner_calls = 0;
    f.dtv_rc = 0;
    f.tuner_rc = -ENODEV;
    assert(oa_source_loss_inactive_source(&w, 1, 1, 3) == -ENODEV);
    assert(f.dtv_calls == 1 && f.tuner_calls == 1);

    assert(oa_source_loss_inactive_source(NULL, 1, 1, 3) == -EINVAL);
    w.select_tuner_position = NULL;
    assert(oa_source_loss_inactive_source(&w, 1, 1, 3) == -EINVAL);
    w.select_tuner_position = fake_tuner;
    w.select_dtv = NULL;
    assert(oa_source_loss_inactive_source(&w, 1, 1, 3) == -EINVAL);
}

int main(void)
{
    test_init();
    test_from_tv_uses_tuner_position();
    test_previous_external_restored_first();
    test_gates();
    test_connect_is_idempotent();
    test_invalid_arguments();
    test_inactive_source_worker();
    puts("ok source-loss policy: connect/disconnect, previous external, "
         "tuner position, flag gates, inactive-source worker order/gates, "
         "invalid arguments");
    return 0;
}
