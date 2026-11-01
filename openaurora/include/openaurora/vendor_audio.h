/* Fixed MB230 libNDAL3 ABI, checked against ARM instructions, not saved Ghidra
 * void prototypes. See CEC-AUDIO-CONTRACT-2026-10-31. Zero from message-backed
 * Ctrl operations does not prove send/receiver/DSP success: Hal_Audio_Send loses
 * the sendmsg result. See AUDIO-GAIN-TRANSPORT-2026-11-01. */
#ifndef OPENAURORA_VENDOR_AUDIO_H
#define OPENAURORA_VENDOR_AUDIO_H
#include <stdint.h>
#define OA_AUDIO_PARAM_WORDS 14
#ifndef OA_NO_VENDOR
extern int Hal_Audio_Protect_Init(void);
extern int Hal_Audio_Mixer_Ctrl_Init(void);
extern int Hal_Audio_Proxy_Init(void);
extern int Hal_Audio_Ctrl(int opcode, uint32_t *params);
#endif
#endif
