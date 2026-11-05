/* Private OGS1 local seqpacket IPC. Not a vendor ABI. The helper owns all
 * blocking EGL calls; the future parent provider must use nonblocking I/O.
 * One request/completion at a time is the parent contract. No raw pointers.
 */
#ifndef OPENAURORA_SCENE_BROKER_H
#define OPENAURORA_SCENE_BROKER_H
#include <stddef.h>
#include <stdint.h>
#define OA_SCENE_PACKET_SIZE 64u
#define OA_SCENE_WIDTH 1920u
#define OA_SCENE_HEIGHT 1080u
#define OA_SCENE_IMAGE_BYTES (OA_SCENE_WIDTH * OA_SCENE_HEIGHT * 4u)
enum { OA_SCENE_OPEN = 1, OA_SCENE_FRAME = 2, OA_SCENE_CLOSE = 3 };
typedef struct {
    uint64_t ticket, revision;
    unsigned operation, width, height, type, upload;
    /* FRAME upload rectangle in the fixed 1920x1080 scene. A full upload uses
     * 0,0,1920,1080; cached frames carry all four as zero. */
    unsigned x, y, w, h;
    float progress;
} oa_scene_request;
typedef struct {
    uint64_t ticket, revision;
    unsigned operation;
    int error;
} oa_scene_reply;
/* Exact64-byte little-endian records; float32 bit encoding, nonzero ticket,
 * fixed profile, bounded type/progress, operation-specific zero fields.
 * Output unchanged on rejection. Replies echo requested revision, including
 * failure; they do not claim a failed upload entered the GPU cache. */
int oa_scene_encode_request(void *out, size_t size, const oa_scene_request *request);
int oa_scene_decode_request(oa_scene_request *out, const void *bytes, size_t size);
int oa_scene_encode_reply(void *out, size_t size, const oa_scene_reply *reply);
int oa_scene_decode_reply(oa_scene_reply *out, const void *bytes, size_t size);
/* Sender keeps its descriptor. No automatic send/EINTR replay. nonblocking
 * selects MSG_DONTWAIT. Receiver returns1 packet /0 disconnect /-1 errno,
 * closes all descriptors on rejection and gives ownership of at most one
 * CLOEXEC image descriptor on success (*image=-1 when absent). */
int oa_scene_packet_send(int socket, const uint8_t bytes[OA_SCENE_PACKET_SIZE], int image, int nonblocking);
int oa_scene_packet_receive(int socket, uint8_t bytes[OA_SCENE_PACKET_SIZE], int *image, int nonblocking);
/* Immutable anonymous file: 1..OA_SCENE_IMAGE_BYTES of top-down RGBA, seals
 * preventing writes, resize and seal changes. A partial frame is w*h*4 bytes;
 * the FRAME rectangle in the protocol carries its placement. Linux syscall
 * avoids newer glibc symbol requirements. Caller owns returned fd; -1/errno if
 * unsupported. No unsealed fallback. */
int oa_scene_image_create(const void *rgba, size_t size);
typedef struct {
    int (*open)(void **owner, unsigned width, unsigned height);
    int (*render)(void *owner, const uint8_t *rgba, size_t size,
                  unsigned x, unsigned y, unsigned w, unsigned h,
                  unsigned type, float progress);
    int (*finish)(void *owner); /* completed GPU reads before releasing image memory/reply */
    int (*close)(void **owner); /* failure retains unfinished owner; success sets NULL */
} oa_scene_backend;
/* Blocking child loop. Strictly increasing tickets; OPEN once; uploaded image
 * revisions strictly increase; cached frames must match current revision.
 * An upload needs exactly one sealed image fd; other operations need none.
 * Failed render/finish is terminal and retains the image until owner closes.
 * CLOSE may report failure and accept a later CLOSE retry. EOF/protocol failure
 * retries unfinished close off-loop until success; never drops a live owner.
 * Backend table/code stays borrowed until this function returns. No success
 * reply claims physical scanout: FRAME means render+swap accepted and GPU done.
 */
int oa_scene_broker_serve(int socket, const oa_scene_backend *backend);
#endif
