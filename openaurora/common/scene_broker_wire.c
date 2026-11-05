#include <float.h>
#include <math.h>
#include <string.h>
#include "openaurora/scene_broker.h"
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24, "IEEE float32 required");
static void put32(uint8_t *p, uint32_t v)
{ for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (i*8)); }
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static void put64(uint8_t *p, uint64_t v) { put32(p, (uint32_t)v); put32(p+4, (uint32_t)(v>>32)); }
static uint64_t get64(const uint8_t *p) { return get32(p) | (uint64_t)get32(p+4)<<32; }
static int zero(const uint8_t *p, size_t size)
{ for (size_t i = 0; i < size; i++) if (p[i]) return 0; return 1; }
static int request_valid(const oa_scene_request *r)
{
    if (!r || !r->ticket) return 0;
    if (r->operation == OA_SCENE_OPEN)
        return r->width == OA_SCENE_WIDTH && r->height == OA_SCENE_HEIGHT &&
            !r->revision && !r->type && !r->upload && !r->x && !r->y && !r->w && !r->h &&
            r->progress == 0;
    if (r->operation == OA_SCENE_FRAME) {
        if (r->width || r->height || !r->revision || r->type > 8 || r->upload > 1 ||
            !isfinite(r->progress) || r->progress < 0 || r->progress > 1)
            return 0;
        if (!r->upload)
            return !r->x && !r->y && !r->w && !r->h;
        return r->w && r->h && r->x < OA_SCENE_WIDTH && r->y < OA_SCENE_HEIGHT &&
            r->w <= OA_SCENE_WIDTH - r->x && r->h <= OA_SCENE_HEIGHT - r->y;
    }
    return r->operation == OA_SCENE_CLOSE && !r->width && !r->height && !r->revision &&
        !r->type && !r->upload && !r->x && !r->y && !r->w && !r->h && r->progress == 0;
}
static void header(uint8_t *p, unsigned operation, uint64_t ticket)
{ memcpy(p, "OGS1", 4); put32(p+4, operation); put64(p+8, ticket); }
int oa_scene_encode_request(void *out, size_t size, const oa_scene_request *r)
{
    if (!out || size < OA_SCENE_PACKET_SIZE || !request_valid(r)) return -1;
    uint8_t p[OA_SCENE_PACKET_SIZE] = {0};
    header(p, r->operation, r->ticket);
    if (r->operation == OA_SCENE_OPEN) { put32(p+16, r->width); put32(p+20, r->height); }
    else if (r->operation == OA_SCENE_FRAME) {
        uint32_t bits; memcpy(&bits, &r->progress, 4);
        put64(p+16, r->revision); put32(p+24, r->type); put32(p+28, bits); put32(p+32, r->upload);
        put32(p+36, r->x); put32(p+40, r->y); put32(p+44, r->w); put32(p+48, r->h);
    }
    memcpy(out, p, sizeof p);
    return 0;
}
int oa_scene_decode_request(oa_scene_request *out, const void *bytes, size_t size)
{
    if (!out || !bytes || size != OA_SCENE_PACKET_SIZE) return -1;
    const uint8_t *p = bytes;
    if (memcmp(p, "OGS1", 4)) return -1;
    oa_scene_request r = {.operation = get32(p+4), .ticket = get64(p+8)};
    if (r.operation == OA_SCENE_OPEN) {
        r.width = get32(p+16); r.height = get32(p+20);
        if (!zero(p+24, 40)) return -1;
    } else if (r.operation == OA_SCENE_FRAME) {
        uint32_t bits = get32(p+28);
        r.revision = get64(p+16); r.type = get32(p+24); r.upload = get32(p+32);
        r.x = get32(p+36); r.y = get32(p+40); r.w = get32(p+44); r.h = get32(p+48);
        memcpy(&r.progress, &bits, 4);
        if (!zero(p+52, 12)) return -1;
    } else if (!zero(p+16, 48)) return -1;
    if (!request_valid(&r)) return -1;
    *out = r;
    return 0;
}
static int reply_valid(const oa_scene_reply *r)
{
    return r && r->ticket && r->error >= 0 && r->error <= 4095 &&
        ((r->operation == OA_SCENE_FRAME && r->revision) ||
         ((r->operation == OA_SCENE_OPEN || r->operation == OA_SCENE_CLOSE) && !r->revision));
}
int oa_scene_encode_reply(void *out, size_t size, const oa_scene_reply *r)
{
    if (!out || size < OA_SCENE_PACKET_SIZE || !reply_valid(r)) return -1;
    uint8_t p[OA_SCENE_PACKET_SIZE] = {0};
    header(p, r->operation | UINT32_C(0x80000000), r->ticket);
    put64(p+16, r->revision); put32(p+24, (uint32_t)r->error);
    memcpy(out, p, sizeof p);
    return 0;
}
int oa_scene_decode_reply(oa_scene_reply *out, const void *bytes, size_t size)
{
    if (!out || !bytes || size != OA_SCENE_PACKET_SIZE) return -1;
    const uint8_t *p = bytes;
    unsigned op = get32(p+4);
    if (memcmp(p, "OGS1", 4) || !(op & UINT32_C(0x80000000)) ||
        get32(p+24) > 4095 || !zero(p+28, 36)) return -1;
    oa_scene_reply r = {.operation = op ^ UINT32_C(0x80000000), .ticket = get64(p+8),
        .revision = get64(p+16), .error = (int)get32(p+24)};
    if (!reply_valid(&r)) return -1;
    *out = r;
    return 0;
}
