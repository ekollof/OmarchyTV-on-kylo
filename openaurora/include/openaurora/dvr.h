#ifndef OPENAURORA_DVR_H
#define OPENAURORA_DVR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Live DVR sink boundary.
 *
 * The default ops open /dev/dvb/adapterN/dvrN (write mode) through the shipped
 * Hal_Demux_Dvb_Dvr_* wrappers and append transport-stream bytes with
 * write/flush/close. Tests inject the same ops table; the scheduling engine
 * that supplies the TS stream remains separate from this sink.
 */

struct oa_dvr_ops {
    int (*open_dvr)(void *ctx, int adapter, int index);
    long (*write_dvr)(void *ctx, int fd, const void *data, size_t len);
    int (*flush_dvr)(void *ctx, int fd);
    int (*close_dvr)(void *ctx, int fd);
};

/* Already-resolved Hal_Demux_Dvb_Dvr_Open / Write / Flush / Close. */
struct oa_dvr_hal {
    int (*open_dvr)(int adapter, int index, int mode);
    long (*write_dvr)(int fd, const void *data, size_t len);
    int (*flush_dvr)(int fd);
    int (*close_dvr)(int fd);
};

const struct oa_dvr_ops *oa_dvr_linux_ops(void);
/* Install resolved wrappers. NULL clears them. Does not dlopen or open a device. */
void oa_dvr_set_hal(const struct oa_dvr_hal *hal);

int oa_dvr_sink_open(int adapter, int index, const struct oa_dvr_ops *ops,
    void *ctx);
/* Writes the whole buffer (looping over partial writes). Returns bytes written
 * or a negative errno. */
long oa_dvr_sink_write(const void *data, size_t len);
int oa_dvr_sink_flush(void);
void oa_dvr_sink_close(void);
int oa_dvr_sink_is_open(void);

#ifdef __cplusplus
}
#endif

#endif
