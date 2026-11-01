/* Transport-independent CEC framing and System Audio / ARC wire contracts.
 * No device access, routing policy, automatic replies or audio HAL calls.
 * See CEC-AUDIO-CONTRACT-2026-10-31 for specification revision and scope. */
#ifndef OPENAURORA_CEC_H
#define OPENAURORA_CEC_H
#include <stddef.h>
#include <stdint.h>
#define OA_CEC_MAX_FRAME 16
#define OA_CEC_MAX_OPERANDS 14
typedef struct {
    uint8_t initiator, destination;
    int opcode; /* -1 = header-only poll; zero is Feature Abort, not a poll */
    size_t count;
    uint8_t operands[OA_CEC_MAX_OPERANDS];
} oa_cec_message;
enum { OA_CEC_VALID = 0, OA_CEC_UNSUPPORTED = 1, OA_CEC_INVALID = -1 };
int oa_cec_decode(oa_cec_message *out, const uint8_t *wire, size_t len);
/* On error, output and *len are unchanged. */
int oa_cec_encode(const oa_cec_message *msg, uint8_t *wire, size_t cap, size_t *len);
/* Known System Audio messages, volume/mute key subset, and ARC opcodes only.
 * UNSUPPORTED is not permission to feature-abort: a higher policy layer decides.
 * This checks wire shape/addressing; capability, adjacency and role policy are
 * separate. ARC direction is checked separately below for TV(0)/Audio System(5). */
int oa_cec_validate_audio(const oa_cec_message *msg);
/* CEC 1.4 discovery/name/power address-mode and operand grammar: 46/47, 83/84,
 * 8f/90. Global initiator/device roles, allocation and local capabilities remain
 * policy-layer checks. This deliberately does not reproduce the vendor filter. */
int oa_cec_validate_identity(const oa_cec_message *msg);
int oa_cec_validate_tv_arc(const oa_cec_message *msg);
int oa_cec_audio_status_decode(uint8_t operand, int *volume, int *muted);
int oa_cec_audio_status_encode(int volume, int muted, uint8_t *operand);
int oa_cec_physical_address_valid(uint16_t address);
#endif
