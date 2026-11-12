#ifndef OPENAURORA_TUNER_VIEW_H
#define OPENAURORA_TUNER_VIEW_H

#include <stddef.h>
#include <stdint.h>

#include "openaurora/channels.h"
#include "openaurora/service_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pure service-view position -> service handle mapping.
 *
 * TunerManager::requestServiceSelectForDisplay @ 0x00f776b8 wraps
 * requestServiceSelectForDisplayInternal @ 0x00f771e0. The internal path:
 *   - rejects an empty EASIServiceList (TunerManager+0x7c);
 *   - rejects a pending standby request;
 *   - calls (TunerManager+0x1c)->vtable+0x54(position, view) to get the
 *     service handle;
 *   - calls (TunerManager+0x1c)->vtable+0xd4(handle, +0x58, bool) to select.
 *
 * The caller's view is the runtime active channel list
 * (ACTIVECHANNELLIST, row order, entry.handle) or the stored service list
 * ordered by the same view. This helper only resolves the handle; it performs
 * no tuner, source, HAL or UI I/O.
 */
struct oa_tuner_view {
    const struct oa_channel_snapshot *channels; /* runtime list wins */
    const struct oa_service_list *services;     /* stored fallback */
    const size_t *order; /* service-list row order; NULL = identity */
    size_t count;        /* number of view positions */
};

/* Returns 0 and stores a nonzero handle, or -EINVAL/-ERANGE. */
int oa_tuner_handle_at(const struct oa_tuner_view *view, int position,
                       uint32_t *handle);

/* Inverse of the active-list mapping: finds the first entry whose handle
 * matches. Returns 0 and a nonnegative position, -ENOENT when absent, or
 * -EINVAL for a null/invalid list, zero handle or null position. */
int oa_tuner_position_of_handle(const struct oa_channel_snapshot *channels,
                                uint32_t handle, int *position);

#ifdef __cplusplus
}
#endif

#endif
