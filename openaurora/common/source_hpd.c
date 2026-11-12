#include <errno.h>
#include <string.h>

#include "openaurora/source_hpd.h"

unsigned oa_source_hpd_pin(unsigned port)
{
    switch (port) {
    case 1: return 203; /* 0xcb fallback; PLAT_VIU argument 0 */
    case 2: return 200; /* 0xc8, table 0x0139f448[0] */
    case 3: return 201; /* 0xc9, table[1] */
    case 4: return 202; /* 0xca, table[2] */
    default: return 0;
    }
}

unsigned oa_source_hpd_bulk_index(unsigned port)
{
    /* Table 0x4e7d614c in libNDAL3: pin 200..204 -> 2,3,1,0,4.
     * HDMI1..4 are pins 203,200,201,202, so their indices are 0,2,3,1. */
    switch (port) {
    case 1: return 0;
    case 2: return 2;
    case 3: return 3;
    case 4: return 1;
    default: return 0xff;
    }
}

void oa_source_hpd_init(struct oa_source_hpd_state *state)
{
    if (!state) return;
    memset(state, 0, sizeof *state);
}

int oa_source_hpd_update(struct oa_source_hpd_state *state, unsigned port,
                         uint8_t level, struct oa_source_hpd_event *events,
                         size_t max)
{
    if (!state || port < 1 || port > OA_SOURCE_HPD_PORTS || level > 1)
        return -EINVAL;

    size_t index = port - 1;
    uint8_t previous = state->level[index];
    if (previous == level) return 0;

    size_t required = level ? 1u : 2u;
    if (!events || max < required) return -ENOSPC;

    if (level) {
        events[0].port = port;
        events[0].status = OA_SOURCE_HPD_CONNECTED;
    } else {
        /* _ice_CheckHdmiPlugStatus calls the app callback with 1
         * (disconnected), then the video event, then 2 (attributes changed). */
        events[0].port = port;
        events[0].status = OA_SOURCE_HPD_DISCONNECTED;
        events[1].port = port;
        events[1].status = OA_SOURCE_HPD_ATTRIBUTES_CHANGED;
    }
    state->level[index] = level;
    return (int)required;
}

int oa_source_hpd_sample(const struct oa_source_hpd_io *io, unsigned port,
                         uint8_t *level)
{
    if (!io || !io->read_bulk || !level) return -EINVAL;
    unsigned index = oa_source_hpd_bulk_index(port);
    if (index == 0xff) return -EINVAL;

    uint8_t bytes[OA_SOURCE_HPD_BULK_BYTES];
    memset(bytes, 0, sizeof bytes);
    int rc = io->read_bulk(io->context, bytes);
    if (rc < 0) return rc;
    if (rc != 0) return -EPROTO;
    *level = bytes[index] ? 1 : 0;
    return 0;
}
