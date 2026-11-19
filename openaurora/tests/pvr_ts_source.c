/* Bounded DVR TS source: fake ops/reads only, no device or dlopen.
 * Asserts the recovered libNDAL3 sink sequence open -> write(s) -> flush ->
 * close, 188-byte packet framing, per-call bounds and error handling. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/dvr.h"
#include "openaurora/pvr_ts_source.h"

static struct {
    char events[512];
    size_t at;
    size_t written;
    int write_calls;
    long write_fail;
    int adapter, index;
} g_ops;

static void ev(const char *fmt, int a, int b)
{
    g_ops.at += (size_t)snprintf(g_ops.events + g_ops.at,
                                 sizeof g_ops.events - g_ops.at, fmt, a, b);
}
static const char *events(void) { return g_ops.events; }
static void clear_events(void) { memset(&g_ops, 0, sizeof g_ops); }

static int fake_open(void *ctx, int adapter, int index)
{
    (void)ctx; g_ops.adapter = adapter; g_ops.index = index;
    ev("open(%d,%d),", adapter, index);
    return 42;
}
static long fake_write(void *ctx, int fd, const void *data, size_t len)
{
    (void)ctx;
    (void)data;
    ev("write(%d),", fd, (int)len);
    if (g_ops.write_fail) return g_ops.write_fail;
    g_ops.written += len;
    g_ops.write_calls++;
    return (long)len;               /* whole writes; sink also handles partial */
}
static int fake_flush(void *ctx, int fd) { (void)ctx; ev("flush(%d),", fd, 0); return 0; }
static int fake_close(void *ctx, int fd) { (void)ctx; ev("close(%d),", fd, 0); return 0; }
static const struct oa_dvr_ops fake_ops = {
    fake_open, fake_write, fake_flush, fake_close
};

static struct {
    const unsigned char *data;
    size_t len, at, max;
    long fail;
} g_read;

static long fake_read(void *ctx, void *buf, size_t len)
{
    (void)ctx;
    if (g_read.fail) return g_read.fail;
    size_t left = g_read.len > g_read.at ? g_read.len - g_read.at : 0;
    size_t n = left < len ? left : len;
    if (n > g_read.max) n = g_read.max;
    memcpy(buf, g_read.data + g_read.at, n);
    g_read.at += n;
    return (long)n;
}

int main(void)
{
    unsigned char ts[OA_PVR_TS_PACKET_SIZE * 70];
    memset(ts, 0x47, sizeof ts);    /* TS sync byte pattern */
    const struct oa_pvr_ts_io io = { NULL, fake_read };

    clear_events();
    assert(oa_pvr_ts_begin(3, 1, &fake_ops, NULL) == 0);
    assert(strcmp(events(), "open(3,1),") == 0);
    assert(oa_dvr_sink_is_open() == 1);

    /* 64 whole packets is the per-call bound: one write of 12032 bytes. */
    g_read.data = ts; g_read.len = sizeof ts; g_read.at = 0;
    g_read.max = sizeof ts; g_read.fail = 0;
    assert(oa_pvr_ts_pump(&io, 64) == 64);
    assert(strcmp(events(), "open(3,1),write(42),") == 0);
    size_t packets = 0, bytes = 0;
    int active = 0, error = 0;
    oa_pvr_ts_info(&active, &packets, &bytes, &error);
    assert(active == 1 && packets == 64 && error == 0);
    assert(bytes == 64 * OA_PVR_TS_PACKET_SIZE);
    int writes_after_first = g_ops.write_calls;

    /* A second bounded call drains the remaining six packets, then EOF. */
    assert(oa_pvr_ts_pump(&io, 64) == 6);
    assert(g_ops.write_calls == writes_after_first + 1);
    assert(oa_pvr_ts_pump(&io, 64) == 0);
    int writes_at_eof = g_ops.write_calls;
    assert(oa_pvr_ts_pump(&io, 64) == 0);
    assert(g_ops.write_calls == writes_at_eof);
    assert(oa_pvr_ts_end() == 0);
    assert(strstr(events(), "flush(42),close(42),") != NULL);
    oa_pvr_ts_info(&active, NULL, NULL, NULL);
    assert(active == 0 && oa_dvr_sink_is_open() == 0);

    /* A short trailing read is EOF, not a partial packet write. */
    clear_events();
    assert(oa_pvr_ts_begin(0, 0, &fake_ops, NULL) == 0);
    g_read.data = ts; g_read.len = 100; g_read.at = 0; g_read.max = 100;
    assert(oa_pvr_ts_pump(&io, 64) == 0);
    assert(g_ops.write_calls == 0);
    assert(oa_pvr_ts_end() == 0);
    assert(strcmp(events(), "open(0,0),flush(42),close(42),") == 0);

    /* Read failure: no write, session stays active, error is recorded. */
    clear_events();
    assert(oa_pvr_ts_begin(1, 2, &fake_ops, NULL) == 0);
    g_read.fail = -EIO;
    assert(oa_pvr_ts_pump(&io, 4) == -EIO);
    assert(g_ops.write_calls == 0);
    oa_pvr_ts_info(&active, NULL, NULL, &error);
    assert(active == 1 && error == -EIO);
    assert(oa_pvr_ts_end() == 0);
    assert(strstr(events(), "flush(42),close(42),") != NULL);

    /* Write failure still flushes and closes at end. */
    clear_events();
    assert(oa_pvr_ts_begin(1, 2, &fake_ops, NULL) == 0);
    g_read.fail = 0; g_read.data = ts; g_read.len = sizeof ts;
    g_read.at = 0; g_read.max = sizeof ts;
    g_ops.write_fail = -EIO;
    assert(oa_pvr_ts_pump(&io, 1) == -EIO);
    assert(oa_pvr_ts_end() == 0);
    assert(strstr(events(), "flush(42),close(42),") != NULL);

    /* Argument/state guards. */
    assert(oa_pvr_ts_pump(NULL, 1) == -EINVAL);
    assert(oa_pvr_ts_begin(0, 0, &fake_ops, NULL) == 0);
    assert(oa_pvr_ts_pump(&io, 0) == -EINVAL);
    assert(oa_pvr_ts_pump(&io, OA_PVR_TS_MAX_PACKETS + 1) == -EINVAL);
    assert(oa_pvr_ts_end() == 0);
    assert(oa_pvr_ts_end() == 0);       /* idempotent */
    assert(oa_pvr_ts_pump(&io, 1) == -ENODEV);
    assert(oa_pvr_ts_begin(0, 0, NULL, NULL) == -EINVAL);

    puts("ok pvr ts source: open/write/flush/close, 188-byte framing, "
         "bounded pump, EOF, error paths");
    return 0;
}
