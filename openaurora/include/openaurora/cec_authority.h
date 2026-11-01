/* VPL_CEC_Open's authority step, before DRV_CEC_Open.
 * Source: ntkdriver VPL_CEC_Open at 0x616ce4 and DRV_STBC_WriteRegister at
 * 0x530574. The mirror is the 32-bit word at physical 0xfc040200. When bit 1
 * is clear, the driver writes STBC register 0xf011 with the mirror's low byte
 * plus bit 1. WriteRegister returns 1 on IPC success and 0 on IPC failure.
 * This helper does not map memory or send IPC. The caller supplies both. */
#ifndef OPENAURORA_CEC_AUTHORITY_H
#define OPENAURORA_CEC_AUTHORITY_H
#include <stdint.h>
#define OA_CEC_AUTHORITY_REGISTER UINT16_C(0xf011)
#define OA_CEC_AUTHORITY_BIT 2u
typedef struct {
    void *context;
    /* 0 and *value written, or -1 with errno. */
    int (*read_mirror)(void *context, uint32_t *value);
    /* 1 = IPC success, 0 = IPC failure. Same convention as the driver. */
    int (*write_register)(void *context, uint16_t reg, uint8_t value);
} oa_cec_authority_io;
/* 0 when bit 1 is already set or the write returns 1. -1 otherwise.
 * A clear bit always attempts the write before the caller may open CEC. */
int oa_cec_authority_ensure(const oa_cec_authority_io *io);
#endif
