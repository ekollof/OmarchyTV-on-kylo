#include <errno.h>
#include <limits.h>
#include <stdint.h>

#include "openaurora/tuner_view.h"

int oa_tuner_handle_at(const struct oa_tuner_view *view, int position,
                       uint32_t *handle)
{
    if (!view || !handle || position < 0) return -EINVAL;

    if (view->channels && view->channels->entries) {
        if ((size_t)position >= view->channels->count) return -ERANGE;
        uint32_t value = view->channels->entries[position].handle;
        if (value == 0) return -EINVAL;
        *handle = value;
        return 0;
    }

    if (!view->services || !view->services->rows) return -EINVAL;
    size_t count = view->count;
    if (count > view->services->count) count = view->services->count;
    if ((size_t)position >= count) return -ERANGE;
    size_t row = view->order ? view->order[position] : (size_t)position;
    if (row >= view->services->count) return -EINVAL;
    int64_t value = view->services->rows[row].handle;
    if (value <= 0 || (uint64_t)value > UINT32_MAX) return -EINVAL;
    *handle = (uint32_t)value;
    return 0;
}

int oa_tuner_position_of_handle(const struct oa_channel_snapshot *channels,
                                uint32_t handle, int *position)
{
    if (!channels || !channels->entries || !position || handle == 0)
        return -EINVAL;
    for (size_t i = 0; i < channels->count; i++) {
        if (channels->entries[i].handle != handle) continue;
        if (i > (size_t)INT_MAX) return -ERANGE;
        *position = (int)i;
        return 0;
    }
    return -ENOENT;
}
