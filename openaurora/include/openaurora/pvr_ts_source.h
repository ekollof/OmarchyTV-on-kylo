#ifndef OPENAURORA_PVR_TS_SOURCE_H
#define OPENAURORA_PVR_TS_SOURCE_H

#include <stddef.h>

#include "openaurora/dvr.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bounded MPEG-TS recording source feeding the existing mockable DVR sink.
 *
 * Firmware grounding (EIT-DVB-DVR-2026-11-05, libNDAL3.so read-only):
 * Hal_Demux_Dvb_Dvr_Open @0x4e7b9f44 (mode 0 -> O_WRONLY),
 * Hal_Demux_Dvb_Dvr_Write @0x4e7ba000 (write, the sink loops over partial
 * writes), Hal_Demux_Dvb_Dvr_Flush @0x4e7b9e48 (ioctl 0xc0086f68) and
 * Hal_Demux_Dvb_Dvr_Close @0x4e7b9fc4 (close). The 188-byte packet is the
 * MPEG-TS transport packet size (ISO/IEC 13818-1), a stream framing rule, not
 * a vendor ioctl requirement. This module does no device I/O itself: it reads
 * through an injected source and writes through oa_dvr_sink_*.
 *
 * Contract: begin opens the sink; pump reads up to max_packets whole TS
 * packets and writes them (a short read is EOF); end flushes then closes.
 * Every stage error is returned; end still flushes and closes after a write
 * error. Default-off in mod_pvr (pvr.dvr_ts); no real DVB device is opened
 * unless the host config opts in.
 */
#define OA_PVR_TS_PACKET_SIZE 188u
#define OA_PVR_TS_MAX_PACKETS 64u

struct oa_pvr_ts_io {
    void *context;
    /* >=0 bytes copied (0 = EOF), negative errno on failure. */
    long (*read)(void *context, void *buf, size_t len);
};

int oa_pvr_ts_begin(int adapter, int index, const struct oa_dvr_ops *ops,
                    void *ctx);
/* Returns packets written (0 at EOF), or a negative errno. */
long oa_pvr_ts_pump(const struct oa_pvr_ts_io *io, unsigned max_packets);
/* Flush then close. Returns the first error, or 0. */
int oa_pvr_ts_end(void);
void oa_pvr_ts_info(int *active, size_t *packets, size_t *bytes, int *error);

#ifdef __cplusplus
}
#endif

#endif
