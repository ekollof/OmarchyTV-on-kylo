#include <string.h>
#include "openaurora/vendor_cec.h"

int oa_cec_raw_prepare(oa_cec_raw_packet *p, uint8_t *buffer, size_t cap)
{
    if (!p || !buffer || cap < OA_CEC_MAX_FRAME) return -1;
    oa_cec_raw_packet packet = {0};
    packet.bytes = buffer;
    memset(buffer, 0, OA_CEC_MAX_FRAME); *p = packet;
    return 0;
}
int oa_cec_raw_encode(oa_cec_raw_packet *p, uint8_t *buffer, size_t cap,
                      const oa_cec_message *message)
{
    if (!p || !buffer || cap < OA_CEC_MAX_FRAME) return -1;
    uint8_t frame[OA_CEC_MAX_FRAME] = {0}; size_t len;
    if (oa_cec_encode(message, frame, sizeof frame, &len)) return -1;
    oa_cec_raw_packet packet = {0};
    packet.bytes = buffer; packet.length = (uint8_t)len; packet.status = 1;
    memcpy(buffer, frame, sizeof frame); *p = packet;
    return 0;
}
int oa_cec_raw_decode(oa_cec_message *out, const oa_cec_raw_packet *p,
                      const uint8_t *buffer, size_t cap)
{
    if (!out || !p || !buffer || cap < OA_CEC_MAX_FRAME || p->hardware_id) return -1;
    if (!p->length) return 0;
    if (p->status != 1 || p->bytes != buffer || p->length > OA_CEC_MAX_FRAME) return -1;
    return oa_cec_decode(out, buffer, p->length) ? -1 : 1;
}
int oa_cec_vpl_prepare(oa_cec_vpl_packet *p, uint8_t *buffer, size_t cap)
{
    if (!p || !buffer || cap < OA_CEC_MAX_FRAME) return -1;
    oa_cec_vpl_packet packet = {0}; packet.operands = buffer;
    memset(buffer, 0, OA_CEC_MAX_FRAME); *p = packet;
    return 0;
}
int oa_cec_vpl_encode(oa_cec_vpl_packet *p, uint8_t *buffer, size_t cap,
                      const oa_cec_message *message)
{
    if (!p || !buffer || cap < OA_CEC_MAX_FRAME) return -1;
    uint8_t frame[OA_CEC_MAX_FRAME]; size_t len;
    if (oa_cec_encode(message, frame, sizeof frame, &len) || len == 1 || message->initiator) return -1;
    oa_cec_vpl_packet packet = {0}; packet.operands = buffer;
    packet.count = (uint8_t)message->count; packet.destination = message->destination;
    packet.opcode = (uint8_t)message->opcode;
    uint8_t operands[OA_CEC_MAX_FRAME] = {0};
    memcpy(operands, frame + 2, message->count);
    memcpy(buffer, operands, sizeof operands); *p = packet;
    return 0;
}
int oa_cec_vpl_decode(oa_cec_message *out, const oa_cec_vpl_packet *p,
                      const uint8_t *buffer, size_t cap, int ioctl_ok, int accepted)
{
    if (!out || !p || !buffer || cap < OA_CEC_MAX_FRAME) return -1;
    if (ioctl_ok != 1 || accepted != 1) return -2;
    if (p->operands != buffer || p->status != 1 || p->hardware_id || p->poll > 1 ||
        p->count > OA_CEC_MAX_OPERANDS || p->initiator > 15 || p->destination > 15) return -1;
    oa_cec_message message = {0};
    message.initiator = p->initiator; message.destination = p->destination;
    message.opcode = p->poll ? -1 : p->opcode;
    /* Header-only receive leaves the opcode/count irrelevant. Our prepare()
     * zeros the whole request; a poll never turns into opcode-zero Feature Abort. */
    message.count = p->poll ? 0 : p->count;
    if (message.count) memcpy(message.operands, buffer, message.count);
    *out = message; return 1;
}
