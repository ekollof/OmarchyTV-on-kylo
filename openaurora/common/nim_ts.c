/*
 * Offline EIT transport-stream replay backend.
 *
 * This is a real MPEG-2 TS demux source (PID filter, continuity, pointer
 * fields, split sections) fed from a file. The NIM tune/signal side reuses the
 * stub until a live tuner backend is accepted; no device or vendor library is
 * touched.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/eit_ts.h"
#include "openaurora/nim.h"

#define TS_QUEUE_SECTIONS 8

static struct {
    FILE *file;
    struct oa_eit_ts demux;
    uint8_t queue[TS_QUEUE_SECTIONS][OA_EIT_TS_SECTION_MAX];
    size_t length[TS_QUEUE_SECTIONS];
    unsigned head;
    unsigned count;
} g_ts;

static int queue_section(const uint8_t *section, size_t len, void *user)
{
    (void)user;
    if (!section || len > OA_EIT_TS_SECTION_MAX) return -E2BIG;
    if (g_ts.count >= TS_QUEUE_SECTIONS) return -ENOSPC;
    unsigned slot = (g_ts.head + g_ts.count) % TS_QUEUE_SECTIONS;
    memcpy(g_ts.queue[slot], section, len);
    g_ts.length[slot] = len;
    g_ts.count++;
    return 0;
}

int oa_nim_ts_open(const char *path)
{
    oa_nim_ts_close();
    if (!path || !*path) return -EINVAL;
    FILE *file = fopen(path, "rb");
    if (!file) return -ENOENT;
    g_ts.file = file;
    oa_eit_ts_init(&g_ts.demux, 0x12); /* DVB EIT PID */
    return 0;
}

void oa_nim_ts_close(void)
{
    if (g_ts.file) fclose(g_ts.file);
    memset(&g_ts, 0, sizeof g_ts);
}

static int ts_next_eit(uint8_t *buf, size_t cap, size_t *len)
{
    if (!buf || !len) return -EINVAL;
    *len = 0;
    if (!g_ts.file) return 0;
    while (g_ts.count == 0) {
        uint8_t packet[OA_EIT_TS_PACKET_SIZE];
        size_t got = fread(packet, 1, sizeof packet, g_ts.file);
        if (got == 0) return 0;
        if (got != sizeof packet) return -EINVAL;
        int rc = oa_eit_ts_feed(&g_ts.demux, packet, sizeof packet,
            queue_section, NULL);
        if (rc < 0) return rc;
    }
    unsigned slot = g_ts.head;
    size_t length = g_ts.length[slot];
    if (cap < length) return -ENOSPC;
    memcpy(buf, g_ts.queue[slot], length);
    *len = length;
    g_ts.head = (g_ts.head + 1) % TS_QUEUE_SECTIONS;
    g_ts.count--;
    return 1;
}

static const struct oa_tuner_backend *ts_backend(void)
{
    static struct oa_tuner_backend backend;
    if (!backend.name) {
        backend.name = "nim-ts-replay";
        backend.nim = oa_nim_stub_ops();
        backend.next_eit = ts_next_eit;
    }
    return &backend;
}

const struct oa_tuner_backend *oa_nim_ts_backend(void)
{
    return ts_backend();
}
