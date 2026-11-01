#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/audio_netlink.h"

int main(void)
{
    uint8_t storage[OA_AUDIO_NETLINK_FRAME_SIZE + 2], before[sizeof storage];
    uint8_t *packet = storage + 1;
    memset(storage, 0xa5, sizeof storage);
    assert(!oa_audio_netlink_get_gain(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 0x11223344, 0));
    const uint8_t header[16] = {0x10,0x20,0,0,0,0,1,0,0,0,0,0,0x44,0x33,0x22,0x11};
    assert(!memcmp(packet, header, sizeof header) && !memcmp(packet + 16, "GetOPUGain", 10));
    for (size_t i = 26; i < OA_AUDIO_NETLINK_FRAME_SIZE; i++) assert(!packet[i]);
    assert(storage[0] == 0xa5 && storage[sizeof storage - 1] == 0xa5);
    assert(!oa_audio_netlink_set_table(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 0x11223344, 9, 0x12345678, 101));
    const uint8_t table[] = {'V','O','L','C','f','g',9,0,0,0,0x78,0x56,0x34,0x12,101,0,0,0};
    assert(!memcmp(packet, header, 16) && !memcmp(packet+16, table, sizeof table));
    for (size_t i = 16 + sizeof table; i < OA_AUDIO_NETLINK_FRAME_SIZE; i++) assert(!packet[i]);
    assert(!oa_audio_netlink_select_table(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 0x11223344, 5, 9));
    const uint8_t select[] = {'S','e','t','O','P','U','V','o','l','C','f','g',5,0,0,0,9,0,0,0};
    assert(!memcmp(packet+16, select, sizeof select));
    for (size_t i = 16 + sizeof select; i < OA_AUDIO_NETLINK_FRAME_SIZE; i++) assert(!packet[i]);
    const uint8_t direct[] = {'S','e','t','O','P','U','G','a','i','n',5,0,0,0,0,0x80,0,0,0x64,0,0,0};
    assert(!oa_audio_netlink_set_opu_gain(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 0x11223344, 5, 0x8000, 100));
    assert(!memcmp(packet, header, 16) && !memcmp(packet + 16, direct, sizeof direct));
    for (size_t i = 16 + sizeof direct; i < OA_AUDIO_NETLINK_FRAME_SIZE; i++) assert(!packet[i]);
    assert(storage[0] == 0xa5 && storage[sizeof storage - 1] == 0xa5);
    puts("  ok   exact vendor netlink golden frames, packed unaligned binary parameters, all zero tails and canaries");
    memcpy(before, storage, sizeof storage);
    assert(oa_audio_netlink_get_gain(NULL, OA_AUDIO_NETLINK_FRAME_SIZE, 1, 0) == -1);
    assert(oa_audio_netlink_get_gain(packet, OA_AUDIO_NETLINK_FRAME_SIZE-1, 1, 0) == -1);
    assert(oa_audio_netlink_get_gain(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 0, 0) == -1);
    assert(oa_audio_netlink_get_gain(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 1, 6) == -1);
    assert(oa_audio_netlink_set_table(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 1, 10, 1, 101) == -1);
    assert(oa_audio_netlink_set_table(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 1, 0, 0, 101) == -1);
    assert(oa_audio_netlink_set_table(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 1, 0, UINT32_MAX - 200u, 101) == -1);
    assert(oa_audio_netlink_set_table(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 1, 0, 1, 100) == -1);
    assert(oa_audio_netlink_select_table(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 1, 6, 0) == -1);
    assert(oa_audio_netlink_select_table(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 1, 0, 10) == -1);
    assert(oa_audio_netlink_set_opu_gain(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 1, 6, 0, 0) == -1);
    assert(oa_audio_netlink_set_opu_gain(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 1, 0x100, 0, 0) == -1);
    assert(oa_audio_netlink_set_opu_gain(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 1, 0, 0x8001, 0) == -1);
    assert(oa_audio_netlink_set_opu_gain(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 1, 0, 0, UINT32_MAX) == -1);
    assert(oa_audio_netlink_set_opu_gain(packet, OA_AUDIO_NETLINK_FRAME_SIZE, 0, 0, 0, 0) == -1);
    assert(oa_audio_netlink_set_opu_gain(packet, OA_AUDIO_NETLINK_FRAME_SIZE - 1, 1, 0, 0, 0) == -1);
    assert(!memcmp(before, storage, sizeof storage));
    puts("  ok   invalid port/device/table/pointer/count/capacity rejects without changing output");

    uint8_t reply[85] = {83,0,0,0};
    memcpy(reply+16, "GetOPUGain", 10);
    reply[26] = 5; reply[30] = 0x34; reply[31] = 0x12; reply[35] = 0x80;
    reply[83] = 0xa5; /* Alignment byte is not payload and need not be zero. */
    oa_audio_opu_gain gain;
    assert(!oa_audio_netlink_gain_reply(&gain, reply, 83) && gain.device == 5 && gain.left == 0x1234 && gain.right == 0x8000);
    assert(!oa_audio_netlink_gain_reply(&gain, reply, 84));
    oa_audio_opu_gain saved = gain;
    for (size_t size = 0; size < sizeof reply; size++) {
        if (size == 83 || size == 84) continue;
        assert(oa_audio_netlink_gain_reply(&gain, reply, size) == -1 && !memcmp(&gain, &saved, sizeof gain));
    }
    const size_t bad[] = {0,4,8,12,16,25,26,82};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        uint8_t old = reply[bad[i]]; reply[bad[i]] = 0xff;
        assert(oa_audio_netlink_gain_reply(&gain, reply, 84) == -1 && !memcmp(&gain, &saved, sizeof gain));
        reply[bad[i]] = old;
    }
    assert(oa_audio_netlink_gain_reply(NULL, reply, 84) == -1);
    assert(oa_audio_netlink_gain_reply(&gain, NULL, 84) == -1);
    /* The firmware remaps cached100 to0x8000 before constructing this reply.
     * Decode must preserve either wire value, never pretend it is invertible. */
    memset(reply + 30, 0, 4); reply[30] = 100;
    assert(!oa_audio_netlink_gain_reply(&gain, reply, 84) && gain.left == 100);
    reply[30] = 0; reply[31] = 0x80;
    assert(!oa_audio_netlink_gain_reply(&gain, reply, 84) && gain.left == 0x8000);
    puts("  ok   complete kernel reply / optional alignment padding; malformed lengths, identity, command and NUL reject atomically");
    return 0;
}
