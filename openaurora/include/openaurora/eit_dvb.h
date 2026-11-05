#ifndef OPENAURORA_EIT_DVB_H
#define OPENAURORA_EIT_DVB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DVB demux section-filter EIT source.
 *
 * The default ops open /dev/dvb/adapterN/demuxN through the shipped
 * Hal_Demux_Dvb_* wrappers, install a PID-only section filter and read one
 * complete section per call. Close goes through Hal_Demux_Dvb_Close, which
 * issues DMX_STOP before close. Tests inject the same ops table, or resolved
 * HAL entry points; no device is opened unless a backend enables this source.
 */

struct oa_eit_dvb_ops {
    int (*open_demux)(void *ctx, int adapter, int index);
    int (*set_section_filter)(void *ctx, int fd, uint16_t pid);
    long (*read_section)(void *ctx, int fd, uint8_t *buf, size_t cap);
    int (*close_demux)(void *ctx, int fd);
};

/* Already-resolved Hal_Demux_Dvb_Open / Set_Sectionfilter / Close. */
struct oa_eit_dvb_hal {
    int (*open_demux)(int adapter, int index);
    int (*set_section_filter)(int fd, uint16_t pid, size_t filter_len,
        void *filter, void *mask, void *mode, char flags);
    int (*close_demux)(int fd);
};

const struct oa_eit_dvb_ops *oa_eit_dvb_linux_ops(void);
/* Install resolved wrappers. NULL clears them. Does not dlopen or open a device. */
void oa_eit_dvb_set_hal(const struct oa_eit_dvb_hal *hal);

int oa_eit_dvb_open(int adapter, int index, uint16_t pid,
    const struct oa_eit_dvb_ops *ops, void *ctx);
void oa_eit_dvb_close(void);
/* Returns 1 with one complete section, 0 for no data/closed, or -errno. */
int oa_eit_dvb_next(uint8_t *buf, size_t cap, size_t *len);

#ifdef __cplusplus
}
#endif

#endif
