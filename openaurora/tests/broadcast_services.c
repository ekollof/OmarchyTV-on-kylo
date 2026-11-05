/* Host fixture contracts for tuner scan, EPG queries and PVR reservations. */
#include "host.h"
#include "openaurora/services.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int command(oa_host *h, const char *text, char *out, size_t cap)
{
    char line[128];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(h, line, out, cap);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    oa_host host;
    assert(!oa_host_init(&host, argv[1], NULL));
    host.log_level = -1;
    char out[256];
    assert(!command(&host, "load broadcast_fixture", out, sizeof out));
    assert(!command(&host, "start broadcast_fixture", out, sizeof out));

    const oa_tuner_iface *tuner = host.api.lookup(&host, OA_SVC_TUNER, 1);
    const oa_epg_iface *epg = host.api.lookup(&host, OA_SVC_EPG, 1);
    const oa_pvr_iface *pvr = host.api.lookup(&host, OA_SVC_PVR, 1);
    assert(tuner && epg && pvr);

    struct oa_tuner_status status;
    assert(!tuner->status(&status));
    assert(status.scan_state == OA_TUNER_IDLE && status.services_found == 0);
    assert(tuner->scan_start(0, 0) == -EINVAL);
    assert(!tuner->scan_start(OA_TUNER_NET_DVB_T, 1));
    assert(!tuner->status(&status));
    assert(status.scan_state == OA_TUNER_SCANNING && status.progress == 25);
    for (int i = 0; i < 4; i++) assert(!tuner->status(&status));
    assert(status.scan_state == OA_TUNER_DONE && status.progress == 100);
    assert(status.services_found == 12);
    assert(tuner->select(13) == -EINVAL);
    assert(!tuner->select(5));
    assert(tuner->selected() == 5);
    assert(tuner->scan_cancel() == -EINVAL);
    assert(!tuner->scan_start(OA_TUNER_NET_DVB_C, 1));
    assert(!tuner->scan_cancel());
    assert(!tuner->status(&status));
    assert(status.scan_state == OA_TUNER_CANCELED && tuner->selected() == 0);

    struct oa_epg_event events[8];
    size_t count = 0;
    assert(epg->query(1, 0, 4000, events, 8, &count) == 0 && count == 2);
    assert(!strcmp(events[0].name, "Alpha News") &&
        !strcmp(events[1].name, "Alpha Movie"));
    assert(epg->query(1, 0, 10000, events, 8, &count) == 0 && count == 2);
    assert(epg->query(0, 0, 10000, events, 8, &count) == 0 && count == 4);
    assert(epg->query(1, 0, 10000, events, 1, &count) == 0 && count == 2);
    assert(epg->query(1, 4000, 4000, events, 8, &count) == -EINVAL);
    assert(epg->query(1, 0, 10000, NULL, 1, &count) == -EINVAL);
    assert(epg->query(99, 0, 10000, events, 8, &count) == 0 && count == 0);

    uint32_t id1 = 0, id2 = 0;
    assert(pvr->add(1, 1000, 2000, "One", &id1) == 0 && id1 == 1);
    assert(pvr->add(2, 3000, 4000, "Two", &id2) == 0 && id2 == 2);
    assert(pvr->add(0, 1000, 2000, "bad", NULL) == -EINVAL);
    assert(pvr->add(1, 2000, 1000, "bad", NULL) == -EINVAL);
    struct oa_pvr_job jobs[4];
    assert(pvr->list(jobs, 4, &count) == 0 && count == 2);
    assert(jobs[0].id == 1 && jobs[0].state == OA_PVR_SCHEDULED);
    assert(pvr->cancel(1) == 0);
    assert(pvr->cancel(1) == -EALREADY);
    assert(pvr->cancel(99) == -ENOENT);
    assert(pvr->list(jobs, 1, &count) == 0 && count == 2 &&
        jobs[0].state == OA_PVR_CANCELED);

    assert(!command(&host, "reload broadcast_fixture", out, sizeof out));
    tuner = host.api.lookup(&host, OA_SVC_TUNER, 1);
    pvr = host.api.lookup(&host, OA_SVC_PVR, 1);
    assert(tuner && pvr);
    assert(!tuner->status(&status));
    assert(status.scan_state == OA_TUNER_IDLE);
    assert(pvr->list(jobs, 4, &count) == 0 && count == 0);

    host.api.release(&host, OA_SVC_PVR);
    host.api.release(&host, OA_SVC_EPG);
    host.api.release(&host, OA_SVC_TUNER);
    assert(!command(&host, "stop broadcast_fixture", out, sizeof out));
    assert(!host.api.lookup(&host, OA_SVC_TUNER, 1));
    assert(!command(&host, "unload broadcast_fixture", out, sizeof out));
    puts("ok broadcast fixture: tuner scan state machine, EPG query bounds and PVR reservations");
    return 0;
}
