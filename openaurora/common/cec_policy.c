#include <stdlib.h>
#include <string.h>
#include "openaurora/cec_policy.h"

struct oa_cec_policy {
    int system_audio;
    int arc;
    int external_known;
    int external_volume;
    int external_muted;
    int adjacent;
    int can_transmit;
    int pressed;
};

oa_cec_policy *oa_cec_policy_create(void)
{
    oa_cec_policy *policy = calloc(1, sizeof *policy);
    if (!policy) return NULL;
    policy->external_volume = -1;
    return policy;
}
void oa_cec_policy_destroy(oa_cec_policy *policy) { free(policy); }

static void apply_mode(oa_cec_policy *policy, int on)
{
    policy->system_audio = on ? 1 : 0;
    if (!policy->system_audio) policy->pressed = 0;
}

static int directed(const oa_cec_message *message)
{
    return message->destination != OA_CEC_LA_BROADCAST;
}
static int for_tv(const oa_cec_message *message)
{
    return message->destination == OA_CEC_LA_TV || message->destination == OA_CEC_LA_BROADCAST;
}
static int feature(int opcode)
{
    switch (opcode) {
    case OA_CEC_OP_SYSTEM_AUDIO_REQUEST:
    case OA_CEC_OP_GIVE_AUDIO_STATUS:
    case OA_CEC_OP_SET_SYSTEM_AUDIO_MODE:
    case OA_CEC_OP_REPORT_AUDIO_STATUS:
    case OA_CEC_OP_GIVE_SYSTEM_AUDIO_STATUS:
    case OA_CEC_OP_SYSTEM_AUDIO_STATUS:
    case OA_CEC_OP_INITIATE_ARC:
    case OA_CEC_OP_REPORT_ARC_INITIATED:
    case OA_CEC_OP_REPORT_ARC_TERMINATED:
    case OA_CEC_OP_REQUEST_ARC_INITIATION:
    case OA_CEC_OP_REQUEST_ARC_TERMINATION:
    case OA_CEC_OP_TERMINATE_ARC:
        return 1;
    default:
        return 0;
    }
}
static int abort_reply(oa_cec_message *out, int capacity, uint8_t follower, uint8_t opcode, int reason)
{
    if (!out || capacity < 1) return -1;
    memset(out, 0, sizeof *out);
    out->initiator = OA_CEC_LA_TV;
    out->destination = follower;
    out->opcode = OA_CEC_OP_FEATURE_ABORT;
    out->count = 2;
    out->operands[0] = opcode;
    out->operands[1] = (uint8_t)reason;
    return 1;
}
static int one_reply(oa_cec_message *out, int capacity, int opcode)
{
    if (!out || capacity < 1) return -1;
    memset(out, 0, sizeof *out);
    out->initiator = OA_CEC_LA_TV;
    out->destination = OA_CEC_LA_AUDIO_SYSTEM;
    out->opcode = opcode;
    return 1;
}

static int handle_arc(oa_cec_policy *policy, const oa_cec_message *incoming,
                      oa_cec_message *replies, int capacity)
{
    if (oa_cec_validate_tv_arc(incoming) != OA_CEC_VALID) {
        if (!directed(incoming)) return 0;
        int reason = oa_cec_validate_audio(incoming) == OA_CEC_INVALID ?
            OA_CEC_ABORT_BAD_OPERAND : OA_CEC_ABORT_REFUSED;
        return abort_reply(replies, capacity, incoming->initiator, (uint8_t)incoming->opcode, reason);
    }
    if (incoming->opcode == OA_CEC_OP_INITIATE_ARC) {
        if (!policy->adjacent)
            return abort_reply(replies, capacity, incoming->initiator,
                               OA_CEC_OP_INITIATE_ARC, OA_CEC_ABORT_REFUSED);
        if (!policy->can_transmit)
            return abort_reply(replies, capacity, incoming->initiator,
                               OA_CEC_OP_INITIATE_ARC, OA_CEC_ABORT_NO_SOURCE);
        int count = one_reply(replies, capacity, OA_CEC_OP_REPORT_ARC_INITIATED);
        if (count == 1) policy->arc = 1;
        return count;
    }
    if (incoming->opcode == OA_CEC_OP_TERMINATE_ARC) {
        if (!policy->adjacent && !policy->arc)
            return abort_reply(replies, capacity, incoming->initiator,
                               OA_CEC_OP_TERMINATE_ARC, OA_CEC_ABORT_REFUSED);
        int count = one_reply(replies, capacity, OA_CEC_OP_REPORT_ARC_TERMINATED);
        if (count == 1) policy->arc = 0;
        return count;
    }
    return abort_reply(replies, capacity, incoming->initiator,
                       (uint8_t)incoming->opcode, OA_CEC_ABORT_REFUSED);
}

int oa_cec_policy_handle(oa_cec_policy *policy, const oa_cec_message *incoming,
                         oa_cec_message *replies, int capacity)
{
    if (!policy || !incoming) return -1;
    /* Inbound user-control traffic is not a local volume write. The TV sends
     * volume keys outward only while System Audio Mode is on. */
    if (incoming->opcode == OA_CEC_OP_USER_PRESSED || incoming->opcode == OA_CEC_OP_USER_RELEASED)
        return 0;
    if (!feature(incoming->opcode) || !for_tv(incoming)) return 0;
    if (incoming->opcode >= OA_CEC_OP_INITIATE_ARC && incoming->opcode <= OA_CEC_OP_TERMINATE_ARC)
        return handle_arc(policy, incoming, replies, capacity);
    if (oa_cec_validate_audio(incoming) != OA_CEC_VALID) {
        if (!directed(incoming)) return 0;
        return abort_reply(replies, capacity, incoming->initiator,
                           (uint8_t)incoming->opcode, OA_CEC_ABORT_BAD_OPERAND);
    }
    switch (incoming->opcode) {
    case OA_CEC_OP_SET_SYSTEM_AUDIO_MODE:
    case OA_CEC_OP_SYSTEM_AUDIO_STATUS:
        apply_mode(policy, incoming->operands[0]);
        return 0;
    case OA_CEC_OP_REPORT_AUDIO_STATUS: {
        int volume = 0, muted = 0;
        if (oa_cec_audio_status_decode(incoming->operands[0], &volume, &muted) != 0) return -1;
        policy->external_known = 1;
        policy->external_volume = volume;
        policy->external_muted = muted;
        return 0;
    }
    case OA_CEC_OP_SYSTEM_AUDIO_REQUEST:
    case OA_CEC_OP_GIVE_AUDIO_STATUS:
    case OA_CEC_OP_GIVE_SYSTEM_AUDIO_STATUS:
        if (!directed(incoming)) return 0;
        return abort_reply(replies, capacity, incoming->initiator,
                           (uint8_t)incoming->opcode, OA_CEC_ABORT_REFUSED);
    default:
        return 0;
    }
}

void oa_cec_policy_route(const oa_cec_policy *policy, oa_cec_audio_route *out)
{
    if (!out) return;
    memset(out, 0, sizeof *out);
    if (!policy) return;
    out->system_audio = policy->system_audio;
    out->arc = policy->arc;
    out->local_volume_permitted = !policy->system_audio;
    out->speakers_muted = policy->system_audio;
    out->external_known = policy->external_known;
    out->external_volume = policy->external_known ? policy->external_volume : -1;
    out->external_muted = policy->external_known ? policy->external_muted : 0;
}
int oa_cec_policy_local_volume_permitted(const oa_cec_policy *policy)
{
    return policy && !policy->system_audio;
}

static int to_amplifier(oa_cec_message *out, int opcode, const uint8_t *operands, size_t count)
{
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->initiator = OA_CEC_LA_TV;
    out->destination = OA_CEC_LA_AUDIO_SYSTEM;
    out->opcode = opcode;
    out->count = count;
    if (count) memcpy(out->operands, operands, count);
    return oa_cec_validate_audio(out) == OA_CEC_VALID ? 0 : -1;
}

int oa_cec_policy_request_system_audio(oa_cec_policy *policy, int on, uint16_t source_pa,
                                       oa_cec_message *out)
{
    if (!policy || (on != 0 && on != 1)) return -1;
    uint8_t operands[2];
    size_t count = 0;
    if (on) {
        if (!oa_cec_physical_address_valid(source_pa)) return -1;
        operands[0] = (uint8_t)(source_pa >> 8);
        operands[1] = (uint8_t)source_pa;
        count = 2;
    }
    return to_amplifier(out, OA_CEC_OP_SYSTEM_AUDIO_REQUEST, operands, count);
}
int oa_cec_policy_request_arc(oa_cec_policy *policy, int initiate, oa_cec_message *out)
{
    if (!policy || !out || (initiate != 0 && initiate != 1)) return -1;
    int opcode = initiate ? OA_CEC_OP_REQUEST_ARC_INITIATION : OA_CEC_OP_REQUEST_ARC_TERMINATION;
    oa_cec_message message;
    if (to_amplifier(&message, opcode, NULL, 0) != 0) return -1;
    if (oa_cec_validate_tv_arc(&message) != OA_CEC_VALID) return -1;
    *out = message;
    return 0;
}
int oa_cec_policy_query(oa_cec_policy *policy, int opcode, oa_cec_message *out)
{
    if (!policy) return -1;
    if (opcode != OA_CEC_OP_GIVE_AUDIO_STATUS && opcode != OA_CEC_OP_GIVE_SYSTEM_AUDIO_STATUS)
        return -1;
    return to_amplifier(out, opcode, NULL, 0);
}
int oa_cec_policy_user_volume(oa_cec_policy *policy, int action, oa_cec_message *out)
{
    if (!policy) return -1;
    if (action < OA_CEC_USER_VOLUME_UP || action > OA_CEC_USER_RELEASE) return -1;
    if (!policy->system_audio) return 0;
    if (action == OA_CEC_USER_RELEASE) {
        if (!policy->pressed) return 0;
        if (to_amplifier(out, OA_CEC_OP_USER_RELEASED, NULL, 0) != 0) return -1;
        policy->pressed = 0;
        return 1;
    }
    uint8_t key = action == OA_CEC_USER_VOLUME_UP ? OA_CEC_UI_VOLUME_UP :
        action == OA_CEC_USER_VOLUME_DOWN ? OA_CEC_UI_VOLUME_DOWN : OA_CEC_UI_MUTE;
    if (!out) return -1;
    if (to_amplifier(out, OA_CEC_OP_USER_PRESSED, &key, 1) != 0) return -1;
    policy->pressed = key;
    return 1;
}
int oa_cec_policy_set_arc_link(oa_cec_policy *policy, int adjacent, int can_transmit)
{
    if (!policy || (adjacent != 0 && adjacent != 1) || (can_transmit != 0 && can_transmit != 1))
        return -1;
    policy->adjacent = adjacent;
    policy->can_transmit = can_transmit;
    return 0;
}

size_t oa_cec_policy_save(const oa_cec_policy *policy, void *buf, size_t cap)
{
    if (!policy) return 0;
    if (!buf || cap < 8) return 8;
    uint8_t *bytes = buf;
    bytes[0] = (uint8_t)policy->system_audio;
    bytes[1] = (uint8_t)policy->arc;
    bytes[2] = (uint8_t)policy->external_known;
    bytes[3] = policy->external_volume < 0 ? 255 : (uint8_t)policy->external_volume;
    bytes[4] = (uint8_t)policy->external_muted;
    bytes[5] = (uint8_t)policy->adjacent;
    bytes[6] = (uint8_t)policy->can_transmit;
    bytes[7] = (uint8_t)policy->pressed;
    return 8;
}
int oa_cec_policy_restore(oa_cec_policy *policy, const void *buf, size_t len)
{
    if (!policy || !buf || len != 8) return -1;
    const uint8_t *bytes = buf;
    int pressed = bytes[7];
    int volume = bytes[3] == 255 ? -1 : bytes[3];
    if (bytes[0] > 1 || bytes[1] > 1 || bytes[2] > 1 || bytes[4] > 1 || bytes[5] > 1 || bytes[6] > 1)
        return -1;
    if ((bytes[3] > 100 && bytes[3] != 255) || (volume < 0 && bytes[2] && bytes[3] != 255)) return -1;
    if (!bytes[2] && (bytes[3] != 255 || bytes[4] != 0)) return -1;
    if (!bytes[0] && pressed) return -1;
    if (pressed && pressed != OA_CEC_UI_VOLUME_UP && pressed != OA_CEC_UI_VOLUME_DOWN &&
        pressed != OA_CEC_UI_MUTE)
        return -1;
    policy->system_audio = bytes[0];
    policy->arc = bytes[1];
    policy->external_known = bytes[2];
    policy->external_volume = volume;
    policy->external_muted = bytes[4];
    policy->adjacent = bytes[5];
    policy->can_transmit = bytes[6];
    policy->pressed = pressed;
    return 0;
}
