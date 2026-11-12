/* Pure TunerManager view-position -> handle test. No tuner, TV or database. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/tuner_view.h"

int main(void)
{
    struct oa_channel_entry entries[3];
    memset(entries, 0, sizeof entries);
    entries[0].handle = 100;
    entries[1].handle = 200;
    entries[2].handle = 300;
    struct oa_channel_snapshot channels = {
        .entries = entries, .count = 3, .source = 1
    };
    uint32_t handle = 0;

    struct oa_tuner_view view = { .channels = &channels };
    assert(oa_tuner_handle_at(&view, 0, &handle) == 0 && handle == 100);
    assert(oa_tuner_handle_at(&view, 1, &handle) == 0 && handle == 200);
    assert(oa_tuner_handle_at(&view, 2, &handle) == 0 && handle == 300);
    assert(oa_tuner_handle_at(&view, 3, &handle) == -ERANGE);
    assert(oa_tuner_handle_at(&view, -1, &handle) == -EINVAL);
    assert(oa_tuner_handle_at(NULL, 0, &handle) == -EINVAL);
    assert(oa_tuner_handle_at(&view, 0, NULL) == -EINVAL);

    entries[1].handle = 0;
    assert(oa_tuner_handle_at(&view, 1, &handle) == -EINVAL);

    struct oa_service_row rows[3];
    memset(rows, 0, sizeof rows);
    rows[0].handle = 400;
    rows[1].handle = 401;
    rows[2].handle = 402;
    struct oa_service_list services = { .rows = rows, .count = 3 };
    const size_t order[3] = { 2, 0, 1 };
    struct oa_tuner_view stored = {
        .services = &services, .order = order, .count = 3
    };
    assert(oa_tuner_handle_at(&stored, 0, &handle) == 0 && handle == 402);
    assert(oa_tuner_handle_at(&stored, 1, &handle) == 0 && handle == 400);
    assert(oa_tuner_handle_at(&stored, 2, &handle) == 0 && handle == 401);
    assert(oa_tuner_handle_at(&stored, 3, &handle) == -ERANGE);

    struct oa_tuner_view identity = {
        .services = &services, .count = 3
    };
    assert(oa_tuner_handle_at(&identity, 1, &handle) == 0 && handle == 401);

    const size_t bad_order[3] = { 0, 3, 1 };
    struct oa_tuner_view bad = {
        .services = &services, .order = bad_order, .count = 3
    };
    assert(oa_tuner_handle_at(&bad, 1, &handle) == -EINVAL);
    rows[2].handle = 0;
    assert(oa_tuner_handle_at(&stored, 0, &handle) == -EINVAL);

    /* Inverse mapping used by the CEC 0x9D route: selected handle -> view
     * position. Duplicate handles keep the first row. */
    int position = -1;
    assert(oa_tuner_position_of_handle(&channels, 100, &position) == 0 &&
           position == 0);
    assert(oa_tuner_position_of_handle(&channels, 300, &position) == 0 &&
           position == 2);
    assert(oa_tuner_position_of_handle(&channels, 999, &position) == -ENOENT);
    assert(oa_tuner_position_of_handle(&channels, 0, &position) == -EINVAL);
    assert(oa_tuner_position_of_handle(NULL, 100, &position) == -EINVAL);
    assert(oa_tuner_position_of_handle(&channels, 100, NULL) == -EINVAL);
    entries[1].handle = 100;
    assert(oa_tuner_position_of_handle(&channels, 100, &position) == 0 &&
           position == 0);
    entries[1].handle = 200;
    struct oa_channel_snapshot empty = {
        .entries = entries, .count = 0, .source = 1
    };
    assert(oa_tuner_position_of_handle(&empty, 100, &position) == -ENOENT);

    /* Stored service-list inverse used when no runtime active list exists. */
    rows[2].handle = 402;
    assert(oa_tuner_position_of_service_handle(&services, NULL, 3, 400,
                                               &position) == 0 &&
           position == 0);
    assert(oa_tuner_position_of_service_handle(&services, NULL, 3, 402,
                                               &position) == 0 &&
           position == 2);
    assert(oa_tuner_position_of_service_handle(&services, NULL, 3, 999,
                                               &position) == -ENOENT);
    assert(oa_tuner_position_of_service_handle(&services, order, 3, 402,
                                               &position) == 0 &&
           position == 0);
    assert(oa_tuner_position_of_service_handle(&services, order, 3, 400,
                                               &position) == 0 &&
           position == 1);
    assert(oa_tuner_position_of_service_handle(&services, order, 3, 401,
                                               &position) == 0 &&
           position == 2);
    assert(oa_tuner_position_of_service_handle(&services, bad_order, 3, 401,
                                               &position) == -EINVAL);
    assert(oa_tuner_position_of_service_handle(NULL, NULL, 0, 400,
                                               &position) == -EINVAL);
    assert(oa_tuner_position_of_service_handle(&services, NULL, 3, 0,
                                               &position) == -EINVAL);
    assert(oa_tuner_position_of_service_handle(&services, NULL, 3, 400,
                                               NULL) == -EINVAL);

    puts("ok tuner-view: runtime list, stored order, identity order, "
         "position/size/handle rejection, selected-handle inverse, "
         "stored service-list inverse");
    return 0;
}
