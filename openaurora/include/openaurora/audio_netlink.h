/* MB230/PreInit mixer messages, not generic Linux audio protocol.
 * Firmware source and synchronous dispatch/status limits: AUDIO-NETLINK-2026-11-01.
 * Encoding does not execute anything. Native execution belongs to an owning
 * worker/process; a full send is not a receiver/DSP success acknowledgement.
 */
#ifndef OPENAURORA_AUDIO_NETLINK_H
#define OPENAURORA_AUDIO_NETLINK_H
#include <stddef.h>
#include <stdint.h>
#define OA_AUDIO_NETLINK_FRAME_SIZE 8208u
typedef struct { uint32_t device, left, right; } oa_audio_opu_gain;
/* Exact vendor 16-byte netlink header + zero-padded 8192-byte payload. Output
 * unchanged on rejection. Port id must identify the owning process for replies.
 */
int oa_audio_netlink_get_gain(void *out, size_t cap, uint32_t port, unsigned device);
/* Direct SetOPUGain: each channel is0..0x8000, not a UI percentage. This
 * bypasses the UI volume-table/balance/ratio path; no conversion is implied.
 * The capture selectors>=0x100 are deliberately outside this device0..5 API. */
int oa_audio_netlink_set_opu_gain(void *out, size_t cap, uint32_t port,
                                unsigned device, unsigned left, unsigned right);
/* Pointer is explicitly the target user's 32-bit address, not a host pointer.
 * The receiver copies its data during synchronous kernel dispatch. Own/retain
 * the table through the entire syscall, including failure, before releasing it.
 * This fixed TV AQ-table operation requires all 101 points. */
int oa_audio_netlink_set_table(void *out, size_t cap, uint32_t port, unsigned index, uint32_t address, unsigned count);
int oa_audio_netlink_select_table(void *out, size_t cap, uint32_t port, unsigned device, unsigned index);
/* Decode only a complete kernel GetOPUGain reply. The caller must also verify
 * recvmsg's actual peer is kernel port0, no truncation, and expected device.
 * No lower-operation status is present; values alone do not certify DSP/audio.
 * Firmware returns cached fields and remaps exactly100 to0x8000: this reply
 * is NOT an invertible snapshot suitable for restoring direct SetOPUGain.
 * Output unchanged on rejection. Alignment padding is not meaningful payload. */
int oa_audio_netlink_gain_reply(oa_audio_opu_gain *out, const void *data, size_t size);
#endif
