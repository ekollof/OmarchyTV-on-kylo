/* MB230 ntkdriver_nondovi CEC packet layouts. Recovered from this target's
 * DRV_CEC_GetCMD, EX_PLAT_CEC_ext_ctrls and KER_CEC_IOCtl instructions.
 * These are pointer-bearing process-local ABI objects, never reload snapshots.
 * No device access or initialization is performed by the marshalling helpers.
 * See CEC-TRANSPORT-CONTRACT-2026-10-31 for ownership/reopen constraints. */
#ifndef OPENAURORA_VENDOR_CEC_H
#define OPENAURORA_VENDOR_CEC_H
#include <stddef.h>
#include <stdint.h>
#include "openaurora/cec.h"

/* Kernel char-device packet: buffer contains the WHOLE CEC frame. */
typedef struct {
    uint8_t status;
    uint8_t reserved0[3];
    uint8_t *bytes;
    uint8_t length;
    uint8_t reserved1[3];
    uint32_t hardware_id;
} oa_cec_raw_packet;

/* VPL/NDAL Process_Command packet: buffer contains only OPERANDS. The kernel
 * nevertheless copies 16 bytes to it on receive, including on rejected frames. */
typedef struct {
    uint8_t status;
    uint8_t poll;
    uint8_t reserved[2];
    uint8_t *operands;
    uint8_t count, initiator, destination, opcode;
    uint32_t hardware_id;
} oa_cec_vpl_packet;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(oa_cec_raw_packet) == 16, "ARM raw CEC packet size");
_Static_assert(offsetof(oa_cec_raw_packet, bytes) == 4, "ARM raw CEC pointer");
_Static_assert(offsetof(oa_cec_raw_packet, length) == 8, "ARM raw CEC length");
_Static_assert(offsetof(oa_cec_raw_packet, hardware_id) == 12, "ARM raw CEC hardware id");
_Static_assert(sizeof(oa_cec_vpl_packet) == 16, "ARM VPL CEC packet size");
_Static_assert(offsetof(oa_cec_vpl_packet, operands) == 4, "ARM VPL CEC pointer");
_Static_assert(offsetof(oa_cec_vpl_packet, count) == 8, "ARM VPL CEC count");
_Static_assert(offsetof(oa_cec_vpl_packet, opcode) == 11, "ARM VPL CEC opcode");
_Static_assert(offsetof(oa_cec_vpl_packet, hardware_id) == 12, "ARM VPL CEC hardware id");
#endif

/* All helpers require a 16-byte backing buffer, including the VPL operand case.
 * The caller retains both packet and buffer throughout an eventual I/O call.
 * Host models have native pointer widths; only the ARM asserts describe TV ABI.
 * All encode/prepare outputs are unchanged on invalid arguments. */
int oa_cec_raw_prepare(oa_cec_raw_packet *packet, uint8_t *buffer, size_t cap);
int oa_cec_raw_encode(oa_cec_raw_packet *packet, uint8_t *buffer, size_t cap,
                      const oa_cec_message *message);
/* 1 = decoded, 0 = empty, -1 = malformed/error. Decoding is after successful
 * I/O only; it cannot prove that ioctl itself succeeded. Empty raw reads may
 * return a NULL packet buffer. No output message is changed on 0/-1. */
int oa_cec_raw_decode(oa_cec_message *out, const oa_cec_raw_packet *packet,
                      const uint8_t *buffer, size_t cap);
int oa_cec_vpl_prepare(oa_cec_vpl_packet *packet, uint8_t *buffer, size_t cap);
/* VPL send selects the actual initiator from driver's logical-address state.
 * This target helper accepts only TV initiator 0, with hardware_id 0.
 * Header-only polling uses a different VPL control and is not encoded here. */
int oa_cec_vpl_encode(oa_cec_vpl_packet *packet, uint8_t *buffer, size_t cap,
                      const oa_cec_message *message);
/* Returns 1 decoded, -1 malformed, -2 not accepted by the driver. The caller
 * must supply BOTH ioctl success and the separate first-control status byte.
 * NDAL Process_Command's boolean result alone loses that byte. A caller must
 * check VPL queue count before requesting receive (the getter decrements it).
 * Driver acceptance is not spec validation: its table rejects some spec-valid
 * messages. This strict decoder is not an end-to-end compliant transport. */
int oa_cec_vpl_decode(oa_cec_message *out, const oa_cec_vpl_packet *packet,
                      const uint8_t *buffer, size_t cap, int ioctl_ok, int accepted);
#endif
