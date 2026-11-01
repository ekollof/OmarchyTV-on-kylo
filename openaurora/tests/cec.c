#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/cec.h"

int main(void)
{
    uint8_t wire[17], back[16];
    for (unsigned header = 0; header < 256; header++) {
        for (size_t n = 1; n <= 16; n++) {
            wire[0] = (uint8_t)header;
            for (size_t i = 1; i < n; i++) wire[i] = (uint8_t)(header + i);
            oa_cec_message msg;
            size_t len = 99;
            assert(!oa_cec_decode(&msg, wire, n));
            assert(msg.initiator == header >> 4 && msg.destination == (header & 15));
            assert(msg.opcode == (n == 1 ? -1 : wire[1]));
            assert(!oa_cec_encode(&msg, back, sizeof back, &len) && len == n && !memcmp(wire, back, n));
        }
    }
    oa_cec_message msg = {0}, before;
    memset(&msg, 0xa5, sizeof msg); before = msg;
    assert(oa_cec_decode(&msg, wire, 0) == -1 && !memcmp(&msg, &before, sizeof msg));
    assert(oa_cec_decode(&msg, wire, 17) == -1 && !memcmp(&msg, &before, sizeof msg));
    assert(oa_cec_decode(NULL, wire, 1) == -1 && oa_cec_decode(&msg, NULL, 1) == -1);
    msg = (oa_cec_message){ .initiator = 0, .destination = 5, .opcode = 0x70,
                            .count = 2, .operands = { 0x20, 0 } };
    memset(back, 0x5a, sizeof back); size_t len = 99;
    assert(oa_cec_encode(&msg, back, 3, &len) == -1 && len == 99 && back[0] == 0x5a);
    msg.count = 15; assert(oa_cec_encode(&msg, back, 16, &len) == -1);
    msg.count = 0; msg.opcode = -2; assert(oa_cec_encode(&msg, back, 16, &len) == -1);
    msg.opcode = 256; assert(oa_cec_encode(&msg, back, 16, &len) == -1);
    msg.opcode = -1; msg.initiator = 16; assert(oa_cec_encode(&msg, back, 16, &len) == -1);
    msg.initiator = 0; msg.count = 1; assert(oa_cec_encode(&msg, back, 16, &len) == -1);
    puts("  ok   all 256 headers / 1..16-byte frame round trips, poll distinction and atomic bounds rejection");

    for (unsigned operand = 0; operand < 256; operand++) {
        int v = -9, muted = -9; uint8_t encoded = 0;
        int valid = (operand & 127) <= 100 || (operand & 127) == 127;
        assert(oa_cec_audio_status_decode((uint8_t)operand, &v, &muted) == (valid ? 0 : -1));
        if (valid) {
            assert(muted == (int)(operand >> 7));
            assert(v == ((operand & 127) == 127 ? -1 : (int)(operand & 127)));
            assert(!oa_cec_audio_status_encode(v, muted, &encoded) && encoded == operand);
        } else assert(v == -9 && muted == -9);
    }
    uint8_t operand = 9;
    assert(oa_cec_audio_status_encode(101, 0, &operand) == -1 && operand == 9);
    assert(oa_cec_audio_status_encode(-2, 0, &operand) == -1);
    assert(oa_cec_audio_status_encode(0, 2, &operand) == -1);
    puts("  ok   all audio-status bytes: mute bit, 0..100 percent, unknown 127 and reserved rejection");

    /* Golden wire examples, independent of the encoder. */
    const uint8_t valid[][4] = {
        { 0x05, 0x70, 0x20, 0 }, { 0x05, 0x70 }, { 0x05, 0x71 },
        { 0x50, 0x72, 1 }, { 0x5f, 0x72, 0 }, { 0x50, 0x7a, 0xb2 },
        { 0x05, 0x7d }, { 0x50, 0x7e, 1 }, { 0x05, 0x44, 0x41 },
        { 0x05, 0x44, 0x42 }, { 0x05, 0x44, 0x43 }, { 0x05, 0x45 }
    };
    const size_t sizes[] = { 4, 2, 2, 3, 3, 3, 2, 3, 3, 3, 3, 2 };
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        assert(!oa_cec_decode(&msg, valid[i], sizes[i]) && oa_cec_validate_audio(&msg) == OA_CEC_VALID);
        msg.count = 14; assert(oa_cec_validate_audio(&msg) == OA_CEC_INVALID);
    }
    msg = (oa_cec_message){ .initiator = 0, .destination = 15, .opcode = 0x71 };
    assert(oa_cec_validate_audio(&msg) == OA_CEC_INVALID);
    msg.destination = 5; msg.opcode = 0x70; msg.count = 1;
    assert(oa_cec_validate_audio(&msg) == OA_CEC_INVALID);
    msg.opcode = 0x7e; msg.operands[0] = 2;
    assert(oa_cec_validate_audio(&msg) == OA_CEC_INVALID);
    msg.opcode = 0x44; msg.operands[0] = 0x00;
    assert(oa_cec_validate_audio(&msg) == OA_CEC_UNSUPPORTED);
    msg.opcode = 0x82; assert(oa_cec_validate_audio(&msg) == OA_CEC_UNSUPPORTED);
    puts("  ok   System Audio / key golden frames, operand counts, addressing and explicit unsupported scope");

    for (int opcode = 0xc0; opcode <= 0xc5; opcode++) {
        int rx = opcode == 0xc0 || opcode == 0xc5;
        msg = (oa_cec_message){ .initiator = rx ? 5 : 0, .destination = rx ? 0 : 5, .opcode = opcode };
        assert(oa_cec_validate_tv_arc(&msg) == OA_CEC_VALID);
        msg.initiator = rx ? 0 : 5; msg.destination = rx ? 5 : 0;
        assert(oa_cec_validate_tv_arc(&msg) == OA_CEC_INVALID);
        msg.destination = 15; assert(oa_cec_validate_tv_arc(&msg) == OA_CEC_INVALID);
    }
    assert(oa_cec_physical_address_valid(0) && oa_cec_physical_address_valid(0x2000));
    assert(oa_cec_physical_address_valid(0x21f0) && oa_cec_physical_address_valid(0xf000));
    assert(!oa_cec_physical_address_valid(0x2010) && !oa_cec_physical_address_valid(0xffff));
    puts("  ok   all six ARC directions, broadcast rejection and physical-address tree rules");

    msg = (oa_cec_message){ .initiator = 0, .destination = 8, .opcode = 0x46 };
    assert(oa_cec_validate_identity(&msg) == OA_CEC_VALID);
    msg.destination = 15; assert(oa_cec_validate_identity(&msg) == OA_CEC_INVALID);
    msg = (oa_cec_message){ .initiator = 8, .destination = 0, .opcode = 0x47,
                            .count = 10, .operands = {'S','t','e','a','m',' ','D','e','c','k'} };
    assert(oa_cec_validate_identity(&msg) == OA_CEC_VALID);
    msg.destination = 15; assert(oa_cec_validate_identity(&msg) == OA_CEC_INVALID);
    msg.destination = 0; msg.count = 0; assert(oa_cec_validate_identity(&msg) == OA_CEC_INVALID);
    msg.count = 15; assert(oa_cec_validate_identity(&msg) == OA_CEC_INVALID);
    msg.count = 14; memset(msg.operands, 'x', sizeof msg.operands);
    assert(oa_cec_validate_identity(&msg) == OA_CEC_VALID);
    for (unsigned ch = 0; ch < 256; ch++) {
        msg.operands[13] = (uint8_t)ch;
        assert(oa_cec_validate_identity(&msg) == (ch >= 0x20 && ch <= 0x7e ? OA_CEC_VALID : OA_CEC_INVALID));
    }
    msg = (oa_cec_message){ .initiator = 5, .destination = 15, .opcode = 0x72,
                            .count = 1, .operands = {1} };
    assert(oa_cec_validate_audio(&msg) == OA_CEC_VALID);
    msg.destination = 0; assert(oa_cec_validate_audio(&msg) == OA_CEC_VALID);
    puts("  ok   spec overrides vendor filter: directed OSD name, all printable-ASCII boundaries, direct/broadcast System Audio Mode");

    msg = (oa_cec_message){ .initiator = 8, .destination = 15, .opcode = 0x84,
                            .count = 3, .operands = {0x10, 0, 4} };
    assert(oa_cec_validate_identity(&msg) == OA_CEC_VALID);
    msg.destination = 0; assert(oa_cec_validate_identity(&msg) == OA_CEC_INVALID);
    msg.destination = 15;
    for (unsigned type = 0; type < 256; type++) {
        msg.operands[2] = (uint8_t)type;
        assert(oa_cec_validate_identity(&msg) == (type <= 7 && type != 2 ? OA_CEC_VALID : OA_CEC_INVALID));
    }
    msg = (oa_cec_message){ .initiator = 8, .destination = 0, .opcode = 0x90,
                            .count = 1 };
    for (unsigned status = 0; status < 256; status++) {
        msg.operands[0] = (uint8_t)status;
        assert(oa_cec_validate_identity(&msg) == (status <= 3 ? OA_CEC_VALID : OA_CEC_INVALID));
    }
    msg.operands[0] = 0; msg.destination = 15;
    assert(oa_cec_validate_identity(&msg) == OA_CEC_INVALID);
    msg.destination = 8; msg.initiator = 0; msg.opcode = 0x83; msg.count = 0;
    assert(oa_cec_validate_identity(&msg) == OA_CEC_VALID);
    msg.opcode = 0x8f; assert(oa_cec_validate_identity(&msg) == OA_CEC_VALID);
    msg.opcode = 0x82; assert(oa_cec_validate_identity(&msg) == OA_CEC_UNSUPPORTED);
    puts("  ok   physical-address broadcast, CEC 1.4 device types, directed power-status values and explicit unsupported scope");
    return 0;
}
