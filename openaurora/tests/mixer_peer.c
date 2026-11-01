/* Fault-injecting exec peer. No netlink or vendor dependency. */
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "openaurora/audio_broker.h"
int main(void)
{
    const char *mode = getenv("OA_MIXER_TEST_MODE");
    if (!mode) mode = "echo";
    uint8_t bytes[OA_AUDIO_BROKER_PACKET_SIZE + 1];
    for (;;) {
        ssize_t n = recv(0, bytes, OA_AUDIO_BROKER_PACKET_SIZE, MSG_TRUNC);
        if (n == 0) return 0;
        oa_audio_broker_request r;
        if (n < 0 || oa_audio_broker_decode_request(&r, bytes, (size_t)n)) return 1;
        if (!strcmp(mode, "exit")) return 0;
        if (!strcmp(mode, "hang")) { while (recv(0, bytes, sizeof bytes, 0) > 0) {} return 0; }
        oa_audio_broker_reply reply = { .ticket = r.ticket, .gain = { r.device, 123, 456 } };
        if (!strcmp(mode, "mismatch")) reply.ticket++;
        if (!strcmp(mode, "error")) { reply.error = 5; reply.gain.left = reply.gain.right = 0; }
        if (oa_audio_broker_encode_reply(bytes, sizeof bytes, &reply)) return 1;
        size_t length = !strcmp(mode, "oversize") ? sizeof bytes : OA_AUDIO_BROKER_PACKET_SIZE;
        if (send(0, bytes, length, MSG_NOSIGNAL) != (ssize_t)length) return 1;
        if (!strcmp(mode, "replyexit")) return 0;
    }
}
