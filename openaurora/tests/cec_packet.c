#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/vendor_cec.h"

static void same(const oa_cec_message *a, const oa_cec_message *b)
{
    assert(a->initiator == b->initiator && a->destination == b->destination &&
           a->opcode == b->opcode && a->count == b->count);
    assert(!memcmp(a->operands, b->operands, a->count));
}
int main(void)
{
    struct { uint8_t left, bytes[16], right; } backing = { .left = 0x5a, .right = 0xa5 };
    uint8_t *buffer = backing.bytes, frame[16];
    oa_cec_raw_packet raw;
    oa_cec_vpl_packet vpl;
    oa_cec_message message, out;
    for (unsigned header = 0; header < 256; header++) for (size_t len = 1; len <= 16; len++) {
        frame[0] = (uint8_t)header;
        for (size_t i = 1; i < len; i++) frame[i] = (uint8_t)(header + i);
        assert(!oa_cec_decode(&message, frame, len));
        assert(!oa_cec_raw_encode(&raw, buffer, 16, &message));
        assert(raw.status == 1 && !raw.hardware_id && raw.length == len && !memcmp(buffer, frame, len));
        assert(oa_cec_raw_decode(&out, &raw, buffer, 16) == 1); same(&message, &out);
        assert(!oa_cec_vpl_prepare(&vpl, buffer, 16));
        vpl.status = 1; vpl.initiator = message.initiator; vpl.destination = message.destination;
        vpl.poll = len == 1; vpl.opcode = len == 1 ? 99 : (uint8_t)message.opcode;
        vpl.count = (uint8_t)message.count;
        memcpy(buffer, message.operands, message.count);
        assert(oa_cec_vpl_decode(&out, &vpl, buffer, 16, 1, 1) == 1); same(&message, &out);
        assert(backing.left == 0x5a && backing.right == 0xa5);
    }
    puts("  ok   all 256 headers / 1..16-byte frames through raw and VPL receive layouts; canaries intact");

    message = (oa_cec_message){ .initiator = 0, .destination = 5, .opcode = 0x70,
                                .count = 2, .operands = { 0x20, 0 } };
    assert(!oa_cec_vpl_encode(&vpl, buffer, 16, &message));
    assert(vpl.count == 2 && vpl.opcode == 0x70 && vpl.destination == 5 && !vpl.hardware_id && !vpl.status);
    assert(buffer[0] == 0x20 && buffer[1] == 0);
    for (size_t i = 2; i < 16; i++) assert(buffer[i] == 0);
    message.count = 14;
    for (unsigned i = 0; i < 14; i++) message.operands[i] = (uint8_t)(i + 1);
    assert(!oa_cec_vpl_encode(&vpl, buffer, 16, &message) && !memcmp(buffer, message.operands, 14));
    assert(!buffer[14] && !buffer[15]);
    uint8_t snapshot[16]; memcpy(snapshot, buffer, 16);
    oa_cec_vpl_packet saved = vpl;
    message.initiator = 4;
    assert(oa_cec_vpl_encode(&vpl, buffer, 16, &message) == -1 && !memcmp(buffer, snapshot, 16));
    assert(!memcmp(&saved, &vpl, sizeof vpl));
    message.initiator = 0; message.count = 0; message.opcode = -1;
    assert(oa_cec_vpl_encode(&vpl, buffer, 16, &message) == -1);
    puts("  ok   VPL outgoing TV commands preserve operands; zero tail; foreign initiator and poll reject atomically");

    assert(oa_cec_vpl_prepare(&vpl, buffer, 14) == -1 && !memcmp(buffer, snapshot, 16));
    assert(oa_cec_raw_prepare(&raw, buffer, 14) == -1);
    assert(oa_cec_raw_encode(&raw, buffer, 14, &message) == -1);
    assert(oa_cec_vpl_encode(&vpl, buffer, 14, &message) == -1);
    assert(!oa_cec_raw_prepare(&raw, buffer, 16));
    memset(&out, 0xa5, sizeof out); oa_cec_message original = out;
    raw.bytes = NULL;
    assert(oa_cec_raw_decode(&out, &raw, buffer, 16) == 0 && !memcmp(&out, &original, sizeof out));
    raw.length = 3; raw.status = 1;
    assert(oa_cec_raw_decode(&out, &raw, buffer, 16) == -1);
    raw.bytes = buffer; raw.length = 17;
    assert(oa_cec_raw_decode(&out, &raw, buffer, 16) == -1);
    raw.length = 3; raw.status = 0;
    assert(oa_cec_raw_decode(&out, &raw, buffer, 16) == -1);
    assert(!memcmp(&out, &original, sizeof out));
    puts("  ok   mandatory 16-byte storage, empty NULL-buffer read, bad pointer/length/status never change output");

    assert(!oa_cec_vpl_prepare(&vpl, buffer, 16));
    vpl.status = 1; vpl.count = 1; vpl.initiator = 5; vpl.opcode = 0x7a; buffer[0] = 0xb2;
    assert(oa_cec_vpl_decode(&out, &vpl, buffer, 16, 1, 0) == -2);
    assert(oa_cec_vpl_decode(&out, &vpl, buffer, 16, 0, 1) == -2);
    assert(!memcmp(&out, &original, sizeof out));
    assert(oa_cec_vpl_decode(&out, &vpl, buffer, 16, 1, 1) == 1 && out.operands[0] == 0xb2);
    original = out;
    for (unsigned field = 0; field < 6; field++) {
        oa_cec_vpl_packet bad = vpl;
        switch (field) {
        case 0: bad.operands = NULL; break;
        case 1: bad.count = 15; break;
        case 2: bad.initiator = 16; break;
        case 3: bad.destination = 16; break;
        case 4: bad.hardware_id = 1; break;
        default: bad.poll = 2; break;
        }
        assert(oa_cec_vpl_decode(&out, &bad, buffer, 16, 1, 1) == -1 && !memcmp(&out, &original, sizeof out));
    }
    puts("  ok   ioctl success cannot hide rejected driver status; malformed VPL metadata rejects atomically");
    return 0;
}
