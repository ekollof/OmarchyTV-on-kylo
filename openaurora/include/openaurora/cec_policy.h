/* TV System Audio Control (CEC 1.3a 13.15) and ARC (CEC 1.4 13.17).
 * Messages only: no CEC device, amplifier register, or local HAL call.
 * ARC link adjacency and transmit capability are injected by the caller. */
#ifndef OPENAURORA_CEC_POLICY_H
#define OPENAURORA_CEC_POLICY_H
#include "openaurora/cec.h"
enum {
    OA_CEC_LA_TV = 0,
    OA_CEC_LA_AUDIO_SYSTEM = 5,
    OA_CEC_LA_BROADCAST = 15
};
enum {
    OA_CEC_OP_FEATURE_ABORT = 0x00,
    OA_CEC_OP_USER_PRESSED = 0x44,
    OA_CEC_OP_USER_RELEASED = 0x45,
    OA_CEC_OP_SYSTEM_AUDIO_REQUEST = 0x70,
    OA_CEC_OP_GIVE_AUDIO_STATUS = 0x71,
    OA_CEC_OP_SET_SYSTEM_AUDIO_MODE = 0x72,
    OA_CEC_OP_REPORT_AUDIO_STATUS = 0x7a,
    OA_CEC_OP_GIVE_SYSTEM_AUDIO_STATUS = 0x7d,
    OA_CEC_OP_SYSTEM_AUDIO_STATUS = 0x7e,
    OA_CEC_OP_INITIATE_ARC = 0xc0,
    OA_CEC_OP_REPORT_ARC_INITIATED = 0xc1,
    OA_CEC_OP_REPORT_ARC_TERMINATED = 0xc2,
    OA_CEC_OP_REQUEST_ARC_INITIATION = 0xc3,
    OA_CEC_OP_REQUEST_ARC_TERMINATION = 0xc4,
    OA_CEC_OP_TERMINATE_ARC = 0xc5
};
enum {
    OA_CEC_ABORT_UNRECOGNIZED = 0,
    OA_CEC_ABORT_WRONG_MODE = 1,
    OA_CEC_ABORT_NO_SOURCE = 2,
    OA_CEC_ABORT_BAD_OPERAND = 3,
    OA_CEC_ABORT_REFUSED = 4
};
enum {
    OA_CEC_UI_VOLUME_UP = 0x41,
    OA_CEC_UI_VOLUME_DOWN = 0x42,
    OA_CEC_UI_MUTE = 0x43
};
enum {
    OA_CEC_USER_VOLUME_UP = 1,
    OA_CEC_USER_VOLUME_DOWN = 2,
    OA_CEC_USER_MUTE = 3,
    OA_CEC_USER_RELEASE = 4
};
typedef struct {
    int system_audio;
    int arc;
    int local_volume_permitted;
    int speakers_muted; /* policy mute while System Audio is on; not an amp write */
    int external_known;
    int external_volume; /* -1 unknown; never a local speaker setter */
    int external_muted;
} oa_cec_audio_route;
typedef struct oa_cec_policy oa_cec_policy;
oa_cec_policy *oa_cec_policy_create(void);
void oa_cec_policy_destroy(oa_cec_policy *policy);
/* Reply count, or -1 if a required reply cannot be stored. 0 means ignored
 * or accepted with no reply. State changes only when the return is >= 0. */
int oa_cec_policy_handle(oa_cec_policy *policy, const oa_cec_message *incoming,
                         oa_cec_message *replies, int capacity);
void oa_cec_policy_route(const oa_cec_policy *policy, oa_cec_audio_route *out);
int oa_cec_policy_local_volume_permitted(const oa_cec_policy *policy);
/* Builds the TV request. Does not assume the amplifier has accepted it. */
int oa_cec_policy_request_system_audio(oa_cec_policy *policy, int on, uint16_t source_pa,
                                       oa_cec_message *out);
/* 1 builds Request ARC Initiation; 0 builds Request ARC Termination.
 * The TV never emits Initiate ARC or Terminate ARC. */
int oa_cec_policy_request_arc(oa_cec_policy *policy, int initiate, oa_cec_message *out);
int oa_cec_policy_query(oa_cec_policy *policy, int opcode, oa_cec_message *out);
/* 1 and fills *out when System Audio is on. 0 when the TV may use local volume.
 * -1 on a bad action or a missing buffer. */
int oa_cec_policy_user_volume(oa_cec_policy *policy, int action, oa_cec_message *out);
int oa_cec_policy_set_arc_link(oa_cec_policy *policy, int adjacent, int can_transmit);
size_t oa_cec_policy_save(const oa_cec_policy *policy, void *buf, size_t cap);
int oa_cec_policy_restore(oa_cec_policy *policy, const void *buf, size_t len);
#endif
