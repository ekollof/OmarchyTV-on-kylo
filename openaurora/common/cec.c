#include <string.h>
#include "openaurora/cec.h"

static int shape(const oa_cec_message *m)
{
    return m && m->initiator <= 15 && m->destination <= 15 &&
        m->count <= OA_CEC_MAX_OPERANDS && m->opcode >= -1 && m->opcode <= 255 &&
        (m->opcode != -1 || m->count == 0);
}
int oa_cec_decode(oa_cec_message *out, const uint8_t *wire, size_t len)
{
    if (!out || !wire || len < 1 || len > OA_CEC_MAX_FRAME) return -1;
    oa_cec_message m = {0};
    m.initiator = wire[0] >> 4; m.destination = wire[0] & 15;
    m.opcode = len == 1 ? -1 : wire[1];
    m.count = len > 1 ? len - 2 : 0;
    if (m.count) memcpy(m.operands, wire + 2, m.count);
    *out = m;
    return 0;
}
int oa_cec_encode(const oa_cec_message *m, uint8_t *wire, size_t cap, size_t *len)
{
    if (!shape(m) || !wire || !len) return -1;
    size_t n = m->opcode == -1 ? 1 : 2 + m->count;
    if (cap < n) return -1;
    uint8_t bytes[OA_CEC_MAX_FRAME] = {0};
    bytes[0] = (uint8_t)((m->initiator << 4) | m->destination);
    if (m->opcode != -1) {
        bytes[1] = (uint8_t)m->opcode;
        memcpy(bytes + 2, m->operands, m->count);
    }
    memcpy(wire, bytes, n); *len = n;
    return 0;
}
int oa_cec_physical_address_valid(uint16_t address)
{
    if (address == UINT16_C(0xffff)) return 0;
    int zero = 0;
    for (int shift = 12; shift >= 0; shift -= 4) {
        unsigned nibble = (address >> shift) & 15u;
        if (!nibble) zero = 1;
        else if (zero) return 0;
    }
    return 1;
}
int oa_cec_audio_status_decode(uint8_t operand, int *volume, int *muted)
{
    unsigned v = operand & 127u;
    if (!volume || !muted || (v > 100 && v != 127)) return -1;
    *volume = v == 127 ? -1 : (int)v;
    *muted = operand >> 7;
    return 0;
}
int oa_cec_audio_status_encode(int volume, int muted, uint8_t *operand)
{
    if (!operand || volume < -1 || volume > 100 || (muted != 0 && muted != 1)) return -1;
    *operand = (uint8_t)((volume == -1 ? 127 : volume) | (muted << 7));
    return 0;
}
int oa_cec_validate_identity(const oa_cec_message *m)
{
    if (!shape(m)) return OA_CEC_INVALID;
    int broadcast = 0, valid = 0;
    switch (m->opcode) {
    case 0x46: case 0x83: case 0x8f:
        valid = m->count == 0; break;
    case 0x47:
        valid = m->count >= 1 && m->count <= 14;
        for (size_t i = 0; valid && i < m->count; i++)
            if (m->operands[i] < 0x20 || m->operands[i] > 0x7e) valid = 0;
        break;
    case 0x84:
        broadcast = 1;
        valid = m->count == 3 && m->operands[2] <= 7 && m->operands[2] != 2 &&
            oa_cec_physical_address_valid((uint16_t)((m->operands[0] << 8) | m->operands[1]));
        break;
    case 0x90:
        valid = m->count == 1 && m->operands[0] <= 3; break;
    default: return OA_CEC_UNSUPPORTED;
    }
    return valid && ((m->destination == 15) == broadcast) ? OA_CEC_VALID : OA_CEC_INVALID;
}
int oa_cec_validate_audio(const oa_cec_message *m)
{
    if (!shape(m)) return OA_CEC_INVALID;
    int both = 0, valid = 0;
    switch (m->opcode) {
    case 0x70: /* System Audio Mode Request: omitted PA means Off. */
        valid = m->count == 0 || (m->count == 2 &&
            oa_cec_physical_address_valid((uint16_t)((m->operands[0] << 8) | m->operands[1])));
        break;
    case 0x71: case 0x7d: /* Give Audio / System Audio Mode Status */
    case 0x45: /* User Control Released */
    case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc4: case 0xc5:
        valid = m->count == 0; break;
    case 0x72: /* Set System Audio Mode: direct or broadcast. */
        both = 1;
        /* fall through */
    case 0x7e: /* System Audio Mode Status: direct only. */
        valid = m->count == 1 && m->operands[0] <= 1; break;
    case 0x7a: {
        int volume, muted;
        valid = m->count == 1 && !oa_cec_audio_status_decode(m->operands[0], &volume, &muted);
        break;
    }
    case 0x44: /* This helper implements only the three volume-control keys. */
        if (!m->count) return OA_CEC_INVALID;
        if (m->operands[0] < 0x41 || m->operands[0] > 0x43) return OA_CEC_UNSUPPORTED;
        valid = m->count == 1; break;
    default: return OA_CEC_UNSUPPORTED;
    }
    if (!valid || m->initiator == 15 || (!both && m->destination == 15)) return OA_CEC_INVALID;
    return OA_CEC_VALID;
}
int oa_cec_validate_tv_arc(const oa_cec_message *m)
{
    int rc = oa_cec_validate_audio(m);
    if (rc != OA_CEC_VALID) return rc;
    if (m->opcode < 0xc0 || m->opcode > 0xc5) return OA_CEC_UNSUPPORTED;
    /* TV transmits ARC audio; Audio System receives it. HDMI direction is the
     * opposite of ARC direction. CEC 1.4 13.17.2.1/2 and Table 25. */
    int rx_initiates = m->opcode == 0xc0 || m->opcode == 0xc5;
    return m->initiator == (rx_initiates ? 5 : 0) &&
        m->destination == (rx_initiates ? 0 : 5) ? OA_CEC_VALID : OA_CEC_INVALID;
}
