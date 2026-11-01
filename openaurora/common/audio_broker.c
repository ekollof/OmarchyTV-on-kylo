#include <errno.h>
#include <sys/socket.h>
#include "openaurora/audio_broker.h"

int oa_audio_broker_serve(int fd, const oa_audio_client_io *io)
{
    int type = 0;
    socklen_t length = sizeof type;
    struct sockaddr_storage peer;
    socklen_t peer_length = sizeof peer;
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &length) ||
        getpeername(fd, (struct sockaddr *)&peer, &peer_length)) return -1;
    if (type != SOCK_SEQPACKET || peer.ss_family != AF_UNIX) { errno = EPROTOTYPE; return -1; }
    oa_audio_client client = OA_AUDIO_CLIENT_INIT;
    int primary = 0;
    for (;;) {
        uint8_t bytes[OA_AUDIO_BROKER_PACKET_SIZE];
        ssize_t n = recv(fd, bytes, sizeof bytes, MSG_TRUNC);
        if (n == 0) break;
        if (n < 0) {
            if (errno == EINTR) continue; /* no request consumed/executed */
            primary = errno;
            break;
        }
        oa_audio_broker_request request;
        if (oa_audio_broker_decode_request(&request, bytes, (size_t)n)) {
            primary = EPROTO;
            break;
        }
        oa_audio_broker_reply reply = { .ticket = request.ticket, .gain = { .device = request.device } };
        int rc = client.fd >= 0 ? 0 : oa_audio_client_open(&client, io);
        if (rc == 0) rc = oa_audio_client_gain(&client, request.device, (int)request.timeout_ms, &reply.gain);
        if (rc != 0) {
            reply.error = errno ? errno : EIO;
            reply.gain = (oa_audio_opu_gain){ .device = request.device };
        }
        if (oa_audio_broker_encode_reply(bytes, sizeof bytes, &reply)) { primary = EPROTO; break; }
        /* A reply failure is terminal. Never replay a kernel request. */
        if (send(fd, bytes, sizeof bytes, MSG_NOSIGNAL) != (ssize_t)sizeof bytes) {
            primary = errno ? errno : EIO;
            break;
        }
    }
    if (oa_audio_client_close(&client) != 0 && !primary) primary = errno ? errno : EIO;
    if (primary) { errno = primary; return -1; }
    return 0;
}
