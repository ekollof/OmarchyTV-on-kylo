#include <string.h>
#include "openaurora/audio_broker.h"

static void put32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(value >> (i * 8));
}
static uint32_t get32(const uint8_t *p)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; i++) value |= (uint32_t)p[i] << (i * 8);
    return value;
}
static void header(uint8_t *p, uint32_t op, uint64_t ticket)
{
    memcpy(p, "OAB1", 4);
    put32(p + 4, op);
    put32(p + 8, (uint32_t)ticket);
    put32(p + 12, (uint32_t)(ticket >> 32));
}
static uint64_t ticket(const uint8_t *p)
{ return (uint64_t)get32(p + 8) | (uint64_t)get32(p + 12) << 32; }
static int valid(const void *bytes, size_t length, uint32_t op)
{
    const uint8_t *p = bytes;
    return p && length == OA_AUDIO_BROKER_PACKET_SIZE && !memcmp(p, "OAB1", 4) &&
        get32(p + 4) == op && ticket(p) != 0;
}
int oa_audio_broker_encode_request(void *bytes, size_t cap, const oa_audio_broker_request *r)
{
    if (!bytes || cap < OA_AUDIO_BROKER_PACKET_SIZE || !r || !r->ticket || r->device > 5 ||
        r->timeout_ms > OA_AUDIO_BROKER_MAX_TIMEOUT_MS) return -1;
    uint8_t p[OA_AUDIO_BROKER_PACKET_SIZE] = {0};
    header(p, 1, r->ticket);
    put32(p + 16, r->device);
    put32(p + 20, r->timeout_ms);
    memcpy(bytes, p, sizeof p);
    return 0;
}
int oa_audio_broker_decode_request(oa_audio_broker_request *out, const void *bytes, size_t length)
{
    if (!out || !valid(bytes, length, 1)) return -1;
    const uint8_t *p = bytes;
    oa_audio_broker_request r = { ticket(p), get32(p + 16), get32(p + 20) };
    if (r.device > 5 || r.timeout_ms > OA_AUDIO_BROKER_MAX_TIMEOUT_MS ||
        get32(p + 24) || get32(p + 28)) return -1;
    *out = r;
    return 0;
}
int oa_audio_broker_encode_reply(void *bytes, size_t cap, const oa_audio_broker_reply *r)
{
    if (!bytes || cap < OA_AUDIO_BROKER_PACKET_SIZE || !r || !r->ticket ||
        r->error < 0 || r->error > 4095 || r->gain.device > 5 ||
        (r->error && (r->gain.left || r->gain.right))) return -1;
    uint8_t p[OA_AUDIO_BROKER_PACKET_SIZE] = {0};
    header(p, UINT32_C(0x80000001), r->ticket);
    put32(p + 16, (uint32_t)r->error);
    put32(p + 20, r->gain.device);
    put32(p + 24, r->gain.left);
    put32(p + 28, r->gain.right);
    memcpy(bytes, p, sizeof p);
    return 0;
}
int oa_audio_broker_decode_reply(oa_audio_broker_reply *out, const void *bytes, size_t length)
{
    if (!out || !valid(bytes, length, UINT32_C(0x80000001))) return -1;
    const uint8_t *p = bytes;
    uint32_t error = get32(p + 16);
    oa_audio_broker_reply r = { .ticket = ticket(p),
        .gain = { get32(p + 20), get32(p + 24), get32(p + 28) } };
    if (error > 4095 || r.gain.device > 5 || (error && (r.gain.left || r.gain.right))) return -1;
    r.error = (int)error;
    *out = r;
    return 0;
}
