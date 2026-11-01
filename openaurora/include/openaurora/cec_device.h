/* MB230 raw ntkcec device backend. This is BLOCKING code for an owning worker,
 * not a host event-loop callback. Source/limits: CEC-OWNERSHIP-2026-11-01.
 * The caller must already own CEC authority/wake policy, serialize operations,
 * and keep this object and any injected I/O table alive through close. Merely
 * opening the device does not establish STBC authority. No device is auto-opened.
 */
#ifndef OPENAURORA_CEC_DEVICE_H
#define OPENAURORA_CEC_DEVICE_H
#include "openaurora/cec.h"
typedef struct {
    void *context;
    int (*open)(void *context, const char *path, int flags);
    int (*ioctl)(void *context, int fd, unsigned long request, void *argument);
    /* Linux close consumes the descriptor even when reporting an error. */
    int (*close)(void *context, int fd);
} oa_cec_device_io;
typedef struct {
    int fd;
    int initialized;
    const oa_cec_device_io *io;
} oa_cec_device;
#define OA_CEC_DEVICE_INIT { .fd = -1, .initialized = 0, .io = NULL }

/* io == NULL selects real syscalls, supported only with the target 32-bit ARM
 * pointer ABI. Injected I/O is for offline fixtures; it is not hardware proof.
 * Failure after fd open unwinds that fd, without tearing down a context that
 * failed to initialize (which may belong to an earlier owner).
 */
int oa_cec_device_open(oa_cec_device *device, const char *path, const oa_cec_device_io *io);
/* Zero means completed successfully, -1 reports errno. The device is initialized
 * as TV logical address 0: other initiators reject before any I/O. No automatic
 * userspace retransmission: kernel TX already retries and an interrupted command
 * is not safe to replay blindly. */
int oa_cec_device_send(oa_cec_device *device, const oa_cec_message *message);
/* 1 message / 0 empty / -1 errno. Consumes raw RX without the defective VPL
 * filter. Output unchanged on empty/error; opcode/spec policy is a separate layer.
 */
int oa_cec_device_receive(oa_cec_device *device, oa_cec_message *message);
/* A failed hardware-close ioctl RETAINS fd and ownership for explicit retry;
 * callers may not discard the object or unload its owner in that state. A final
 * OS-close error consumes the fd; never retry that integer after possible reuse.
 */
int oa_cec_device_close(oa_cec_device *device);
#endif
