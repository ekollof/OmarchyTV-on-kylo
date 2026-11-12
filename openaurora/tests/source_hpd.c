/* Pure HPD provider model test. No /dev, TV, GPIO or HAL access. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/source_hpd.h"

static void test_mapping(void)
{
    assert(oa_source_hpd_pin(1) == 203);
    assert(oa_source_hpd_pin(2) == 200);
    assert(oa_source_hpd_pin(3) == 201);
    assert(oa_source_hpd_pin(4) == 202);
    assert(oa_source_hpd_pin(0) == 0);
    assert(oa_source_hpd_pin(5) == 0);

    assert(oa_source_hpd_bulk_index(1) == 0);
    assert(oa_source_hpd_bulk_index(2) == 2);
    assert(oa_source_hpd_bulk_index(3) == 3);
    assert(oa_source_hpd_bulk_index(4) == 1);
    assert(oa_source_hpd_bulk_index(0) == 0xff);
    assert(oa_source_hpd_bulk_index(5) == 0xff);
}

static void test_transitions(void)
{
    struct oa_source_hpd_state state;
    struct oa_source_hpd_event events[2];

    oa_source_hpd_init(&state);
    for (unsigned i = 0; i < OA_SOURCE_HPD_PORTS; i++)
        assert(state.level[i] == 0);

    /* First sample 0 is the stock BSS default: no event. */
    assert(oa_source_hpd_update(&state, 1, 0, events, 2) == 0);

    /* First sample 1 looks like a plug at boot. */
    assert(oa_source_hpd_update(&state, 1, 1, events, 2) == 1);
    assert(events[0].port == 1);
    assert(events[0].status == OA_SOURCE_HPD_CONNECTED);
    assert(state.level[0] == 1);

    /* No change: no event. */
    assert(oa_source_hpd_update(&state, 1, 1, events, 2) == 0);

    /* Unplug emits disconnected then attributes changed. */
    assert(oa_source_hpd_update(&state, 1, 0, events, 2) == 2);
    assert(events[0].port == 1);
    assert(events[0].status == OA_SOURCE_HPD_DISCONNECTED);
    assert(events[1].port == 1);
    assert(events[1].status == OA_SOURCE_HPD_ATTRIBUTES_CHANGED);
    assert(state.level[0] == 0);

    /* Each port has independent state. */
    assert(oa_source_hpd_update(&state, 2, 1, events, 2) == 1);
    assert(events[0].port == 2);
    assert(state.level[0] == 0 && state.level[1] == 1);
    assert(oa_source_hpd_update(&state, 3, 1, events, 2) == 1);
    assert(oa_source_hpd_update(&state, 4, 1, events, 2) == 1);
}

static void test_buffer_and_errors(void)
{
    struct oa_source_hpd_state state;
    struct oa_source_hpd_event events[2];

    oa_source_hpd_init(&state);
    assert(oa_source_hpd_update(&state, 1, 1, events, 1) == 1);
    assert(state.level[0] == 1);

    /* Too small for the two-event unplug sequence: state must not change. */
    assert(oa_source_hpd_update(&state, 1, 0, events, 1) == -ENOSPC);
    assert(state.level[0] == 1);
    assert(oa_source_hpd_update(&state, 1, 0, NULL, 2) == -ENOSPC);
    assert(state.level[0] == 1);

    /* A retry with room succeeds. */
    assert(oa_source_hpd_update(&state, 1, 0, events, 2) == 2);
    assert(state.level[0] == 0);

    assert(oa_source_hpd_update(NULL, 1, 1, events, 2) == -EINVAL);
    assert(oa_source_hpd_update(&state, 0, 1, events, 2) == -EINVAL);
    assert(oa_source_hpd_update(&state, 5, 1, events, 2) == -EINVAL);
    assert(oa_source_hpd_update(&state, 1, 2, events, 2) == -EINVAL);
    assert(state.level[0] == 0);
}

struct fake {
    int rc;
    int errno_value;
    uint8_t bytes[OA_SOURCE_HPD_BULK_BYTES];
    int calls;
};

static int fake_read_bulk(void *context,
                          uint8_t bytes[OA_SOURCE_HPD_BULK_BYTES])
{
    struct fake *fake = context;
    fake->calls++;
    if (fake->rc < 0) {
        errno = fake->errno_value;
        return -fake->errno_value;
    }
    if (fake->rc > 0) return fake->rc;
    memcpy(bytes, fake->bytes, sizeof fake->bytes);
    return 0;
}

static void test_adapter(void)
{
    struct fake fake;
    memset(&fake, 0, sizeof fake);
    /* Live sample 01 01 00 01 00 with the Deck on HDMI1:
     * 0=pin203/HDMI1, 1=pin202/HDMI4, 2=pin200/HDMI2, 3=pin201/HDMI3,
     * 4=pin204. */
    fake.bytes[0] = 0x01;
    fake.bytes[1] = 0x01;
    fake.bytes[2] = 0x00;
    fake.bytes[3] = 0x01;
    fake.bytes[4] = 0x7f;
    struct oa_source_hpd_io io;
    memset(&io, 0, sizeof io);
    io.context = &fake;
    io.read_bulk = fake_read_bulk;

    uint8_t level = 0xff;
    assert(oa_source_hpd_sample(&io, 1, &level) == 0 && level == 1);
    assert(oa_source_hpd_sample(&io, 2, &level) == 0 && level == 0);
    assert(oa_source_hpd_sample(&io, 3, &level) == 0 && level == 1);
    assert(oa_source_hpd_sample(&io, 4, &level) == 0 && level == 1);
    assert(fake.calls == 4);

    level = 0x5a;
    assert(oa_source_hpd_sample(&io, 0, &level) == -EINVAL);
    assert(oa_source_hpd_sample(&io, 5, &level) == -EINVAL);
    assert(level == 0x5a);

    fake.rc = -1;
    fake.errno_value = EIO;
    level = 0x5a;
    assert(oa_source_hpd_sample(&io, 1, &level) == -EIO);
    assert(level == 0x5a);

    fake.rc = 1;
    assert(oa_source_hpd_sample(&io, 1, &level) == -EPROTO);
    assert(level == 0x5a);

    assert(oa_source_hpd_sample(NULL, 1, &level) == -EINVAL);
    assert(oa_source_hpd_sample(&io, 1, NULL) == -EINVAL);
    io.read_bulk = NULL;
    assert(oa_source_hpd_sample(&io, 1, &level) == -EINVAL);
}

int main(void)
{
    test_mapping();
    test_transitions();
    test_buffer_and_errors();
    test_adapter();
    puts("ok source-hpd: GPIO 200..203 mapping, 0x6712 index table, "
         "plug/unplug sequence, independent ports, buffer/error gates, "
         "bulk-getter adapter");
    return 0;
}
