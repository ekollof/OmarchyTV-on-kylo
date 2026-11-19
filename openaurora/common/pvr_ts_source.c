/* Bounded MPEG-TS source feeding the mockable DVR sink. Pure/host-testable;
 * no device or dlopen here (the sink owns the DVR boundary). */
#include "openaurora/pvr_ts_source.h"

#include "openaurora/dvr.h"

#include <errno.h>
#include <string.h>

static struct {
    int active;
    size_t packets, bytes;
    int error;
    unsigned char buf[OA_PVR_TS_PACKET_SIZE * OA_PVR_TS_MAX_PACKETS];
} g_ts;

void oa_pvr_ts_info(int *active, size_t *packets, size_t *bytes, int *error)
{
    if (active) *active = g_ts.active;
    if (packets) *packets = g_ts.packets;
    if (bytes) *bytes = g_ts.bytes;
    if (error) *error = g_ts.error;
}

static void reset(void)
{
    memset(&g_ts, 0, sizeof g_ts);
}

int oa_pvr_ts_begin(int adapter, int index, const struct oa_dvr_ops *ops,
                    void *ctx)
{
    oa_pvr_ts_end();
    reset();
    int rc = oa_dvr_sink_open(adapter, index, ops, ctx);
    if (rc < 0)
        return rc;
    g_ts.active = 1;
    return 0;
}

long oa_pvr_ts_pump(const struct oa_pvr_ts_io *io, unsigned max_packets)
{
    if (!io || !io->read)
        return -EINVAL;
    if (!g_ts.active)
        return -ENODEV;
    if (max_packets == 0 || max_packets > OA_PVR_TS_MAX_PACKETS)
        return -EINVAL;
    size_t want = (size_t)max_packets * OA_PVR_TS_PACKET_SIZE;
    long got = io->read(io->context, g_ts.buf, want);
    if (got < 0) {
        g_ts.error = (int)got;
        return got;
    }
    /* Only whole TS packets are written; a partial trailing read is EOF. */
    size_t whole = ((size_t)got / OA_PVR_TS_PACKET_SIZE) * OA_PVR_TS_PACKET_SIZE;
    if (whole == 0)
        return 0;
    long written = oa_dvr_sink_write(g_ts.buf, whole);
    if (written < 0) {
        g_ts.error = (int)written;
        return written;
    }
    if (written != (long)whole) {
        g_ts.error = -EIO;
        return -EIO;
    }
    size_t packets = whole / OA_PVR_TS_PACKET_SIZE;
    g_ts.packets += packets;
    g_ts.bytes += whole;
    return (long)packets;
}

int oa_pvr_ts_end(void)
{
    if (!g_ts.active)
        return 0;
    int flush_rc = oa_dvr_sink_flush();
    oa_dvr_sink_close();
    g_ts.active = 0;
    if (flush_rc < 0) {
        g_ts.error = flush_rc;
        return flush_rc;
    }
    return 0;
}
