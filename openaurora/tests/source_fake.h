#ifndef OPENAURORA_TEST_SOURCE_FAKE_H
#define OPENAURORA_TEST_SOURCE_FAKE_H

#include <stdint.h>

enum source_fake_kind {
    SF_OPEN = 1, SF_CLOSE, SF_SELECT, SF_START, SF_AUDIO, SF_VPLT,
    SF_INFO_GET, SF_INFO_SET, SF_DRM_GET, SF_FD_GET, SF_FD_SET,
    SF_DRM_DEV, SF_PLANE, SF_PLANE_INFO, SF_DP, SF_EXT, SF_SD, SF_IOCTL
};

struct source_fake_event {
    int kind;
    int64_t a, b, c, d, e, f, g, h, i;
};

void source_fake_reset(void);
int source_fake_count(void);
const struct source_fake_event *source_fake_at(int index);
const char *source_fake_log_text(void);

#endif
