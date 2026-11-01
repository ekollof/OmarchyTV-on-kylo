/* Real mod_cec policy service plus the real audio module. No CEC device,
 * netlink socket, or amplifier register is opened. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host.h"
#include "lifecycle_retry.h"
#include "openaurora/services.h"

static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[160];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}
static oa_cec_message frame(unsigned initiator, unsigned destination, int opcode,
                            const uint8_t *operands, size_t count)
{
    oa_cec_message message;
    memset(&message, 0, sizeof message);
    message.initiator = (uint8_t)initiator;
    message.destination = (uint8_t)destination;
    message.opcode = opcode;
    message.count = count;
    if (count) memcpy(message.operands, operands, count);
    return message;
}
static void expect_bytes(const oa_cec_message *message, unsigned initiator, unsigned destination,
                         int opcode, const uint8_t *operands, size_t count)
{
    assert(message->initiator == initiator && message->destination == destination);
    assert(message->opcode == opcode && message->count == count);
    if (count) assert(!memcmp(message->operands, operands, count));
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    oa_host host;
    char out[512];
    assert(!oa_host_init(&host, argv[1], argv[2]));
    host.log_level = OA_LOG_ERROR;
    assert(!command(&host, "load plat", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start plat", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load audio", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start audio", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load cec", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start cec", out, sizeof out) && !strcmp(out, "ok\n"));
    const oa_cec_service *owner = host.api.lookup(&host, OA_SVC_CEC_OWNER, 1);
    const oa_cec_audio_iface *audio_cec = host.api.lookup(&host, OA_SVC_CEC_AUDIO, 1);
    const oa_audio_iface *audio = host.api.lookup(&host, OA_SVC_AUDIO, 1);
    assert(owner && !owner->hardware_open_supported());
    assert(audio_cec && audio && audio->is_available());
    oa_cec_audio_route route;
    audio_cec->route(&route);
    assert(!route.system_audio && !route.arc && route.local_volume_permitted && !route.speakers_muted);
    assert(!route.external_known && route.external_volume == -1 && !route.external_muted);
    assert(audio->set_volume(20) == 0 && audio->get_volume() == 20);

    uint8_t on = 1;
    oa_cec_message incoming = frame(5, 15, OA_CEC_OP_SET_SYSTEM_AUDIO_MODE, &on, 1);
    oa_cec_message reply;
    assert(audio_cec->handle(&incoming, &reply, 1) == 0);
    audio_cec->route(&route);
    assert(route.system_audio && route.speakers_muted && !route.local_volume_permitted && !route.arc);
    assert(audio->set_volume(30) == -1 && audio->set_mute(1) == -1 && audio->get_volume() == 20);

    uint8_t status = 0x28;
    incoming = frame(5, 0, OA_CEC_OP_REPORT_AUDIO_STATUS, &status, 1);
    assert(audio_cec->handle(&incoming, NULL, 0) == 0);
    audio_cec->route(&route);
    assert(route.external_known && route.external_volume == 40 && !route.external_muted);
    assert(audio->get_volume() == 20);

    assert(audio_cec->user_volume(OA_CEC_USER_VOLUME_UP, &reply) == 1);
    uint8_t up = OA_CEC_UI_VOLUME_UP;
    expect_bytes(&reply, 0, 5, OA_CEC_OP_USER_PRESSED, &up, 1);
    assert(audio_cec->user_volume(OA_CEC_USER_RELEASE, &reply) == 1);
    expect_bytes(&reply, 0, 5, OA_CEC_OP_USER_RELEASED, NULL, 0);
    assert(audio->get_volume() == 20);
    incoming = frame(5, 0, OA_CEC_OP_USER_PRESSED, &up, 1);
    assert(audio_cec->handle(&incoming, &reply, 1) == 0 && audio->get_volume() == 20);

    assert(audio_cec->request_system_audio(1, 0x1000, &reply) == 0);
    uint8_t pa[] = { 0x10, 0x00 };
    expect_bytes(&reply, 0, 5, OA_CEC_OP_SYSTEM_AUDIO_REQUEST, pa, 2);
    assert(audio_cec->request_system_audio(1, 0x0100, &reply) == -1);
    assert(audio_cec->request_system_audio(0, 0, &reply) == 0);
    expect_bytes(&reply, 0, 5, OA_CEC_OP_SYSTEM_AUDIO_REQUEST, NULL, 0);
    audio_cec->route(&route);
    assert(route.system_audio);
    uint8_t off = 0;
    incoming = frame(5, 0, OA_CEC_OP_SYSTEM_AUDIO_STATUS, &off, 1);
    assert(audio_cec->handle(&incoming, NULL, 0) == 0);
    assert(audio_cec->local_volume_permitted());
    assert(audio_cec->user_volume(OA_CEC_USER_VOLUME_UP, &reply) == 0);
    assert(audio->set_volume(30) == 0 && audio->get_volume() == 30);

    incoming = frame(5, 0, OA_CEC_OP_GIVE_AUDIO_STATUS, NULL, 0);
    assert(audio_cec->handle(&incoming, &reply, 1) == 1);
    uint8_t refused[] = { OA_CEC_OP_GIVE_AUDIO_STATUS, OA_CEC_ABORT_REFUSED };
    expect_bytes(&reply, 0, 5, OA_CEC_OP_FEATURE_ABORT, refused, 2);
    incoming = frame(5, 15, OA_CEC_OP_GIVE_SYSTEM_AUDIO_STATUS, NULL, 0);
    assert(audio_cec->handle(&incoming, &reply, 1) == 0);
    assert(audio_cec->query(OA_CEC_OP_GIVE_SYSTEM_AUDIO_STATUS, &reply) == 0);
    expect_bytes(&reply, 0, 5, OA_CEC_OP_GIVE_SYSTEM_AUDIO_STATUS, NULL, 0);
    assert(audio_cec->query(OA_CEC_OP_GIVE_AUDIO_STATUS, &reply) == 0);
    expect_bytes(&reply, 0, 5, OA_CEC_OP_GIVE_AUDIO_STATUS, NULL, 0);

    incoming = frame(5, 0, OA_CEC_OP_INITIATE_ARC, NULL, 0);
    assert(audio_cec->handle(&incoming, &reply, 1) == 1);
    uint8_t not_adjacent[] = { OA_CEC_OP_INITIATE_ARC, OA_CEC_ABORT_REFUSED };
    expect_bytes(&reply, 0, 5, OA_CEC_OP_FEATURE_ABORT, not_adjacent, 2);
    audio_cec->route(&route);
    assert(!route.arc);
    assert(!audio_cec->set_arc_link(1, 0));
    assert(audio_cec->handle(&incoming, &reply, 1) == 1);
    uint8_t no_source[] = { OA_CEC_OP_INITIATE_ARC, OA_CEC_ABORT_NO_SOURCE };
    expect_bytes(&reply, 0, 5, OA_CEC_OP_FEATURE_ABORT, no_source, 2);
    audio_cec->route(&route);
    assert(!route.arc);
    assert(audio_cec->handle(&incoming, &reply, 0) == -1);
    assert(!audio_cec->set_arc_link(1, 1));
    assert(audio_cec->handle(&incoming, &reply, 1) == 1);
    expect_bytes(&reply, 0, 5, OA_CEC_OP_REPORT_ARC_INITIATED, NULL, 0);
    audio_cec->route(&route);
    assert(route.arc && route.local_volume_permitted);
    incoming = frame(5, 0, OA_CEC_OP_TERMINATE_ARC, NULL, 0);
    assert(audio_cec->handle(&incoming, &reply, 1) == 1);
    expect_bytes(&reply, 0, 5, OA_CEC_OP_REPORT_ARC_TERMINATED, NULL, 0);
    audio_cec->route(&route);
    assert(!route.arc);
    incoming = frame(4, 0, OA_CEC_OP_INITIATE_ARC, NULL, 0);
    assert(audio_cec->handle(&incoming, &reply, 1) == 1);
    uint8_t wrong_role[] = { OA_CEC_OP_INITIATE_ARC, OA_CEC_ABORT_REFUSED };
    expect_bytes(&reply, 0, 4, OA_CEC_OP_FEATURE_ABORT, wrong_role, 2);
    incoming = frame(5, 15, OA_CEC_OP_INITIATE_ARC, NULL, 0);
    assert(audio_cec->handle(&incoming, &reply, 1) == 0);
    assert(!audio_cec->request_arc(1, &reply));
    expect_bytes(&reply, 0, 5, OA_CEC_OP_REQUEST_ARC_INITIATION, NULL, 0);
    assert(!audio_cec->request_arc(0, &reply));
    expect_bytes(&reply, 0, 5, OA_CEC_OP_REQUEST_ARC_TERMINATION, NULL, 0);
    incoming = frame(5, 0, OA_CEC_OP_REQUEST_ARC_INITIATION, NULL, 0);
    assert(audio_cec->handle(&incoming, &reply, 1) == 1);
    uint8_t not_rx[] = { OA_CEC_OP_REQUEST_ARC_INITIATION, OA_CEC_ABORT_REFUSED };
    expect_bytes(&reply, 0, 5, OA_CEC_OP_FEATURE_ABORT, not_rx, 2);

    on = 1;
    incoming = frame(5, 15, OA_CEC_OP_SET_SYSTEM_AUDIO_MODE, &on, 1);
    assert(audio_cec->handle(&incoming, NULL, 0) == 0);
    assert(!audio_cec->set_arc_link(1, 1));
    incoming = frame(5, 0, OA_CEC_OP_INITIATE_ARC, NULL, 0);
    assert(audio_cec->handle(&incoming, &reply, 1) == 1);
    status = 0xa8;
    incoming = frame(5, 0, OA_CEC_OP_REPORT_AUDIO_STATUS, &status, 1);
    assert(audio_cec->handle(&incoming, NULL, 0) == 0);
    assert(!lifecycle_retry(&host, "reload cec", out, sizeof out) && !strcmp(out, "ok\n"));
    host.api.release(&host, OA_SVC_CEC_OWNER);
    host.api.release(&host, OA_SVC_CEC_AUDIO);
    host.api.release(&host, OA_SVC_AUDIO);
    owner = host.api.lookup(&host, OA_SVC_CEC_OWNER, 1);
    audio_cec = host.api.lookup(&host, OA_SVC_CEC_AUDIO, 1);
    audio = host.api.lookup(&host, OA_SVC_AUDIO, 1);
    assert(owner && !owner->hardware_open_supported() && audio_cec && audio);
    audio_cec->route(&route);
    assert(route.system_audio && route.arc && route.speakers_muted && !route.local_volume_permitted);
    assert(route.external_known && route.external_volume == 40 && route.external_muted);
    assert(audio->set_volume(10) == -1 && audio->get_volume() == 30);
    assert(!command(&host, "status cec", out, sizeof out));
    assert(strstr(out, "cec.owner:v1") && strstr(out, "cec.audio:v1") && strstr(out, "state   started"));

    host.stop_requested = 1;
    assert(!oa_host_run(&host));
    puts("  ok   System Audio mutes local volume; ARC follows 1.4 direction; route survives reload");
    return 0;
}
