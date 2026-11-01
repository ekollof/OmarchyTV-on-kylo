#include <string.h>
#include "openaurora/audio_netlink.h"

static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8*i));
}
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int build(void *out, size_t cap, uint32_t port, const char *name, const uint32_t words[14])
{
    if (!out || cap < OA_AUDIO_NETLINK_FRAME_SIZE || !port) return -1;
    uint8_t *bytes = out;
    memset(bytes, 0, OA_AUDIO_NETLINK_FRAME_SIZE);
    put32(bytes, OA_AUDIO_NETLINK_FRAME_SIZE);
    bytes[6] = 1; /* type0, flags REQUEST, sequence0; vendor doesn't request ACK. */
    put32(bytes + 12, port);
    size_t length = strlen(name);
    memcpy(bytes + 16, name, length); /* No separator/NUL before binary params. */
    for (unsigned i = 0; i < 14; i++) put32(bytes + 16 + length + 4*i, words[i]);
    return 0;
}
int oa_audio_netlink_get_gain(void *out, size_t cap, uint32_t port, unsigned device)
{
    if (device > 5) return -1;
    const uint32_t words[14] = {device};
    return build(out, cap, port, "GetOPUGain", words);
}
int oa_audio_netlink_set_opu_gain(void *out, size_t cap, uint32_t port,
                                unsigned device, unsigned left, unsigned right)
{
    if (device > 5 || left > 0x8000 || right > 0x8000) return -1;
    const uint32_t words[14] = {device, left, right};
    return build(out, cap, port, "SetOPUGain", words);
}
int oa_audio_netlink_set_table(void *out, size_t cap, uint32_t port, unsigned index, uint32_t address, unsigned count)
{
    if (index > 9 || !address || address > UINT32_MAX - 201u || count != 101) return -1;
    const uint32_t words[14] = {index, address, count};
    return build(out, cap, port, "VOLCfg", words);
}
int oa_audio_netlink_select_table(void *out, size_t cap, uint32_t port, unsigned device, unsigned index)
{
    if (device > 5 || index > 9) return -1;
    const uint32_t words[14] = {device, index};
    return build(out, cap, port, "SetOPUVolCfg", words);
}
int oa_audio_netlink_gain_reply(oa_audio_opu_gain *out, const void *data, size_t size)
{
    enum { LENGTH = 16 + 10 + 56 + 1, ALIGNED = (LENGTH + 3) & ~3 };
    if (!out || !data || (size != LENGTH && size != ALIGNED)) return -1;
    const uint8_t *bytes = data;
    if (get32(bytes) != LENGTH || get32(bytes + 4) || get32(bytes + 8) || get32(bytes + 12) ||
        memcmp(bytes + 16, "GetOPUGain", 10) || bytes[LENGTH-1]) return -1;
    oa_audio_opu_gain decoded = {get32(bytes + 26), get32(bytes + 30), get32(bytes + 34)};
    if (decoded.device > 5) return -1;
    *out = decoded;
    return 0;
}
