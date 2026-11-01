#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include "openaurora/scene_broker.h"
static int failure(void) { return errno > 0 && errno <= 4095 ? errno : EIO; }
static int close_owner(const oa_scene_backend *backend, void **owner)
{
    if (!*owner) return 0;
    errno = 0;
    int rc = backend->close(owner);
    if (!rc && !*owner) return 0;
    if (!errno) errno = EIO;
    return -1;
}
int oa_scene_broker_serve(int fd, const oa_scene_backend *backend)
{
    if (!backend || !backend->open || !backend->render || !backend->finish || !backend->close) {
        errno = EINVAL; return -1;
    }
    int type; socklen_t length = sizeof type;
    struct sockaddr_storage peer; socklen_t peer_length = sizeof peer;
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &length) ||
        getpeername(fd, (struct sockaddr *)&peer, &peer_length)) return -1;
    if (type != SOCK_SEQPACKET || peer.ss_family != AF_UNIX) { errno = EPROTOTYPE; return -1; }
    void *owner = NULL, *mapping = MAP_FAILED;
    uint64_t last_ticket = 0, revision = 0;
    int opened = 0, closing = 0, primary = 0;
    for (;;) {
        uint8_t bytes[OA_SCENE_PACKET_SIZE];
        int image = -1;
        int received = oa_scene_packet_receive(fd, bytes, &image, 0);
        if (received < 0 && errno == EINTR) continue;
        if (received <= 0) { if (received < 0) primary = failure(); break; }
        oa_scene_request request;
        if (oa_scene_decode_request(&request, bytes, sizeof bytes) || request.ticket <= last_ticket ||
            ((request.operation != OA_SCENE_FRAME || !request.upload) && image >= 0)) {
            if (image >= 0) (void)close(image);
            primary = EPROTO; break;
        }
        last_ticket = request.ticket;
        oa_scene_reply reply = {.operation = request.operation, .ticket = request.ticket,
                                .revision = request.revision};
        int terminal = 0, done = 0;
        if (request.operation == OA_SCENE_OPEN) {
            if (opened || closing) { primary = EPROTO; break; }
            opened = 1;
            errno = 0;
            if (backend->open(&owner, request.width, request.height) || !owner) {
                reply.error = failure(); terminal = 1;
            }
        } else if (request.operation == OA_SCENE_CLOSE) {
            closing = 1;
            if (close_owner(backend, &owner)) reply.error = failure();
            else done = 1;
        } else if (!opened || closing || !owner) {
            reply.error = EPROTO; terminal = 1;
        } else {
            if (request.upload) {
                struct stat st;
                int seals = image < 0 ? -1 : (int)syscall(SYS_fcntl, image, F_GET_SEALS);
                const int required = F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
                if (image < 0 || request.revision <= revision || fstat(image, &st) ||
                    !S_ISREG(st.st_mode) || st.st_size != (off_t)OA_SCENE_IMAGE_BYTES ||
                    seals < 0 || (seals & required) != required) {
                    reply.error = EPROTO; terminal = 1;
                } else {
                    /* Target mmap_region 8017464c rejects VM_SHARED when the
                     * seal made i_mmap_writable negative, even for PROT_READ.
                     * Private read-only pages still reference the sealed file;
                     * no write or resize is possible and no eager copy occurs. */
                    mapping = mmap(NULL, OA_SCENE_IMAGE_BYTES, PROT_READ, MAP_PRIVATE, image, 0);
                    if (mapping == MAP_FAILED) { reply.error = failure(); terminal = 1; }
                }
            } else if (request.revision != revision) { reply.error = ENODATA; terminal = 1; }
            if (image >= 0) { (void)close(image); image = -1; }
            if (!terminal) {
                const uint8_t *rgba = request.upload ? mapping : NULL;
                errno = 0;
                if (backend->render(owner, rgba, rgba ? OA_SCENE_IMAGE_BYTES : 0,
                                    request.type, request.progress)) {
                    reply.error = failure(); terminal = 1;
                } else {
                    errno = 0;
                    if (backend->finish(owner)) { reply.error = failure(); terminal = 1; }
                    else {
                        revision = request.revision;
                        if (mapping != MAP_FAILED) {
                            (void)munmap(mapping, OA_SCENE_IMAGE_BYTES); mapping = MAP_FAILED;
                        }
                    }
                }
            }
        }
        if (image >= 0) (void)close(image);
        if (oa_scene_encode_reply(bytes, sizeof bytes, &reply)) { primary = EPROTO; break; }
        if (oa_scene_packet_send(fd, bytes, -1, 0)) { primary = failure(); break; }
        if (terminal) { primary = reply.error; break; }
        if (done) break;
    }
    /* Close can block or fail: this runs only in the child. Keep the owner and
     * any failed-frame mapping alive until its outstanding resources drain. */
    while (close_owner(backend, &owner)) {
        if (!primary) primary = failure();
        struct timespec delay = {.tv_nsec = 100000000};
        (void)nanosleep(&delay, NULL);
    }
    if (mapping != MAP_FAILED) (void)munmap(mapping, OA_SCENE_IMAGE_BYTES);
    if (primary) { errno = primary; return -1; }
    return 0;
}
