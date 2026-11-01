/* Private local IPC for the separate-PID mixer owner. This is our protocol,
 * not vendor netlink. Version1 supports GetOPUGain only; no hardware setters.
 * Integers are little-endian and every packet is exactly32 bytes. */
#ifndef OPENAURORA_AUDIO_BROKER_H
#define OPENAURORA_AUDIO_BROKER_H
#include "openaurora/audio_client.h"
#define OA_AUDIO_BROKER_PACKET_SIZE 32u
#define OA_AUDIO_BROKER_MAX_TIMEOUT_MS 2000u
typedef struct {
    uint64_t ticket;
    unsigned device;
    unsigned timeout_ms;
} oa_audio_broker_request;
typedef struct {
    uint64_t ticket;
    int error; /* positive errno or0; no DSP/audio operation ACK exists */
    oa_audio_opu_gain gain; /* error replies retain only requested device */
} oa_audio_broker_reply;
/* Encoding/decoding is atomic on rejection. Nonzero ticket, device0..5,
 * bounded timeout, exact magic/op/size and reserved fields are checked. */
int oa_audio_broker_encode_request(void *bytes, size_t cap, const oa_audio_broker_request *request);
int oa_audio_broker_decode_request(oa_audio_broker_request *out, const void *bytes, size_t length);
int oa_audio_broker_encode_reply(void *bytes, size_t cap, const oa_audio_broker_reply *reply);
int oa_audio_broker_decode_reply(oa_audio_broker_reply *out, const void *bytes, size_t length);
/* Blocking child loop, for a dedicated executable, never the host event loop.
 * fd must be an AF_UNIX/SOCK_SEQPACKET connection. Borrowed IPC fd is not closed
 * here. NULL io selects native client syscalls on ARM32; fixtures inject I/O.
 * Malformed/truncated packets terminate before opening/sending mixer traffic.
 * EOF closes the session. Bind is lazy on the first validated request. */
int oa_audio_broker_serve(int fd, const oa_audio_client_io *io);
#endif
