/* Read-only EIT section-filter probe for a live/staged TV.
 *
 * Opens /dev/dvb adapter0 demux0, installs an EIT PID filter through the
 * standard Linux DVB API (or resolved vendor wrappers) and prints parsed
 * events until the timeout. It does not tune, write the DVR, or touch the
 * framebuffer. */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "openaurora/eit_dvb.h"
#include "openaurora/eit_section.h"

static int events;
static int sections;

static int on_event(const struct oa_eit_event_info *event, void *user)
{
    (void)user;
    if (events < 12)
        printf("event service=%u id=%u start=%u end=%u actual=%u running=%u name=%s\n",
            event->service_id, event->event_id, event->range.begin,
            event->range.end, event->actual, event->running_status,
            event->name);
    events++;
    return 0;
}

int main(int argc, char **argv)
{
    int seconds = argc > 1 ? atoi(argv[1]) : 5;
    int adapter = argc > 2 ? atoi(argv[2]) : 0;
    int demux = argc > 3 ? atoi(argv[3]) : 0;
    unsigned pid = argc > 4 ? (unsigned)strtoul(argv[4], NULL, 0) : 0x12;
    if (seconds < 1 || seconds > 60 || adapter < 0 || demux < 0 ||
        pid == 0 || pid > 0x1fff) {
        fprintf(stderr, "usage: %s [seconds] [adapter] [demux] [pid]\n", argv[0]);
        return 2;
    }
    int rc = oa_eit_dvb_open(adapter, demux, (uint16_t)pid,
        oa_eit_dvb_linux_ops(), NULL);
    if (rc) {
        fprintf(stderr, "oa_eit_dvb_open: %s (%d)\n", strerror(-rc), rc);
        return 1;
    }
    time_t end = time(NULL) + seconds;
    while (time(NULL) < end) {
        uint8_t section[4096];
        size_t len = 0;
        rc = oa_eit_dvb_next(section, sizeof section, &len);
        if (rc < 0) {
            fprintf(stderr, "oa_eit_dvb_next: %s (%d)\n", strerror(-rc), rc);
            break;
        }
        if (rc == 0) {
            usleep(100000);
            continue;
        }
        sections++;
        int parsed = oa_eit_parse_section(section, len, on_event, NULL);
        if (parsed < 0)
            fprintf(stderr, "eit parse: %s (%d)\n", strerror(-parsed), parsed);
    }
    oa_eit_dvb_close();
    printf("sections=%d events=%d\n", sections, events);
    return 0;
}
