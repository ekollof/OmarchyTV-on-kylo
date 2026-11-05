/* Offline PVR stub tests: vendor-shaped start/stop/index-query plus persistent
 * file semantics. No hardware or vendor code. */
#include "openaurora/pvr.h"
#include "openaurora/broadcast.h"
#include "openaurora/dvr.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static struct oa_pvr_request request(uint32_t service, int64_t start,
    int64_t end, uint32_t repeat, int32_t padding, const char *name)
{
    struct oa_pvr_request r;
    memset(&r, 0, sizeof r);
    r.service_handle = service;
    r.start_ms = start;
    r.end_ms = end;
    r.repeat_days = repeat;
    r.start_padding = padding;
    r.end_padding = -padding;
    snprintf(r.name, sizeof r.name, "%s", name);
    return r;
}

static void test_persist(const char *dir)
{
    const struct oa_pvr_backend *backend = oa_pvr_stub_backend();
    assert(backend && !strcmp(backend->name, "pvr-stub"));
    char path[OA_PVR_PATH_MAX], tmp[OA_PVR_PATH_MAX + 8];
    snprintf(path, sizeof path, "%s/pvr.db", dir);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    assert(strlen(path) < OA_PVR_PATH_MAX - 8);

    assert(backend->open(path) == 0);
    assert(backend->count() == 0);

    uint32_t id1 = 0, id2 = 0;
    struct oa_pvr_request r = request(1, 1000, 2000, 0, 0, "One");
    assert(backend->start(&r, &id1) == 0 && id1 == 1);
    r = request(2, 3000, 4000, 0x7f, 30, "Two");
    assert(backend->start(&r, &id2) == 0 && id2 == 2);
    assert(backend->count() == 2);

    struct oa_pvr_job job;
    assert(backend->query(0, &job) == 0);
    assert(job.id == 1 && job.service_handle == 1 && job.state ==
        OA_PVR_SCHEDULED && !strcmp(job.name, "One"));
    assert(backend->query(1, &job) == 0);
    assert(job.repeat_days == 0x7f && job.start_padding == 30 &&
        job.end_padding == -30);
    assert(backend->query(2, &job) == -ENOENT);
    assert(backend->query(0, NULL) == -EINVAL);

    assert(backend->stop(1) == 0);
    assert(backend->stop(1) == -EALREADY);
    assert(backend->stop(99) == -ENOENT);
    assert(backend->query(0, &job) == 0 && job.state == OA_PVR_CANCELED);
    assert(access(tmp, F_OK) != 0);

    backend->close();
    assert(backend->count() == 0);
    assert(backend->open(path) == 0);
    assert(backend->count() == 2);
    assert(backend->query(0, &job) == 0 && job.id == 1 &&
        job.state == OA_PVR_CANCELED);
    assert(backend->query(1, &job) == 0 && job.id == 2 &&
        job.state == OA_PVR_SCHEDULED);
    r = request(1, 5000, 6000, 0, 0, "Three");
    uint32_t id3 = 0;
    assert(backend->start(&r, &id3) == 0 && id3 == 3);
    backend->close();

    FILE *file = fopen(path, "rb");
    assert(file);
    char line[64];
    assert(fgets(line, sizeof line, file));
    assert(!strcmp(line, "OAPVR1\n"));
    fclose(file);

    /* Invalid persistent data fails closed and leaves the file untouched. */
    snprintf(path, sizeof path, "%s/bad.db", dir);
    file = fopen(path, "wb");
    assert(file);
    fputs("OAPVR1\nnot-a-record\n", file);
    fclose(file);
    assert(backend->open(path) == -EINVAL);
    assert(backend->count() == 0);
    file = fopen(path, "rb");
    assert(file);
    assert(fgets(line, sizeof line, file) && !strcmp(line, "OAPVR1\n"));
    fclose(file);

    /* A directory path is rejected. */
    assert(backend->open(dir) == -EISDIR);
}

static void test_validation(const char *dir)
{
    const struct oa_pvr_backend *backend = oa_pvr_stub_backend();
    char path[OA_PVR_PATH_MAX];
    snprintf(path, sizeof path, "%s/validation.db", dir);
    assert(backend->open(path) == 0);
    uint32_t id = 0;
    struct oa_pvr_request r = request(0, 1, 2, 0, 0, "bad");
    assert(backend->start(&r, &id) == -EINVAL);
    r = request(1, 2, 1, 0, 0, "bad");
    assert(backend->start(&r, &id) == -EINVAL);
    r = request(1, 1, 2, 0, 0, "");
    assert(backend->start(&r, &id) == -EINVAL);
    r = request(1, 1, 2, 0, 0, "bad\tname");
    assert(backend->start(&r, &id) == -EINVAL);
    r = request(1, 1, 2, 0, 0, "ok");
    assert(backend->start(&r, &id) == 0 && id == 1);
    backend->close();
}

static void test_capacity(const char *dir)
{
    const struct oa_pvr_backend *backend = oa_pvr_stub_backend();
    char path[OA_PVR_PATH_MAX];
    snprintf(path, sizeof path, "%s/full.db", dir);
    assert(backend->open(path) == 0);
    for (unsigned i = 0; i < OA_PVR_MAX_JOBS; i++) {
        char name[32];
        snprintf(name, sizeof name, "Job %u", i);
        struct oa_pvr_request r = request(1, 1000 + i * 10, 2000 + i * 10,
            0, 0, name);
        uint32_t id = 0;
        assert(backend->start(&r, &id) == 0 && id == i + 1);
    }
    struct oa_pvr_request extra = request(1, 999999, 1000000, 0, 0, "Full");
    assert(backend->start(&extra, NULL) == -ENOSPC);
    backend->close();
    assert(backend->open(path) == 0);
    assert(backend->count() == OA_PVR_MAX_JOBS);
    backend->close();
}

struct dvr_mock {
    int opens, writes, flushes, closes;
    int fd;
    int fail_open, fail_write, zero_write, partial;
    size_t written;
};

static int dvr_open(void *ctx, int adapter, int index)
{
    struct dvr_mock *m = ctx;
    assert(adapter == 0 && index == 0);
    m->opens++;
    return m->fail_open ? -EACCES : m->fd;
}

static long dvr_write(void *ctx, int fd, const void *data, size_t len)
{
    struct dvr_mock *m = ctx;
    assert(fd == m->fd && data && len);
    m->writes++;
    if (m->fail_write) return -EIO;
    if (m->zero_write) return 0;
    if (m->partial && len > 60) len = 60;
    m->written += len;
    return (long)len;
}

static int dvr_flush(void *ctx, int fd)
{
    struct dvr_mock *m = ctx;
    assert(fd == m->fd);
    m->flushes++;
    return 0;
}

static int dvr_close(void *ctx, int fd)
{
    struct dvr_mock *m = ctx;
    assert(fd == m->fd);
    m->closes++;
    return 0;
}

static void test_dvr_sink(void)
{
    assert(oa_dvr_linux_ops());
    uint8_t data[200];
    memset(data, 0xa5, sizeof data);
    struct dvr_mock mock;
    memset(&mock, 0, sizeof mock);
    mock.fd = 88;
    mock.partial = 1;
    const struct oa_dvr_ops ops = { dvr_open, dvr_write, dvr_flush,
        dvr_close };

    assert(oa_dvr_sink_write(data, sizeof data) == -ENODEV);
    assert(oa_dvr_sink_flush() == -ENODEV);
    assert(oa_dvr_sink_open(0, 0, &ops, &mock) == 0);
    assert(oa_dvr_sink_is_open());
    assert(oa_dvr_sink_write(data, sizeof data) == (long)sizeof data);
    assert(mock.writes == 4 && mock.written == sizeof data);
    assert(oa_dvr_sink_flush() == 0 && mock.flushes == 1);
    oa_dvr_sink_close();
    assert(mock.closes == 1 && !oa_dvr_sink_is_open());
    oa_dvr_sink_close();

    mock.fail_open = 1;
    assert(oa_dvr_sink_open(0, 0, &ops, &mock) == -EACCES);
    mock.fail_open = 0;
    mock.fail_write = 1;
    assert(oa_dvr_sink_open(0, 0, &ops, &mock) == 0);
    assert(oa_dvr_sink_write(data, 10) == -EIO);
    oa_dvr_sink_close();
    mock.fail_write = 0;
    mock.zero_write = 1;
    assert(oa_dvr_sink_open(0, 0, &ops, &mock) == 0);
    assert(oa_dvr_sink_write(data, 10) == -EIO);
    oa_dvr_sink_close();
    mock.zero_write = 0;
    assert(oa_dvr_sink_open(0, 0, NULL, NULL) == -EINVAL);
}

struct dvr_hal_seen {
    int opens, writes, closes;
    int mode;
    int fail;
};

static struct dvr_hal_seen dvr_hal;

static int dvr_hal_open(int adapter, int index, int mode)
{
    assert(adapter == 0 && index == 2);
    dvr_hal.opens++;
    dvr_hal.mode = mode;
    if (dvr_hal.fail == 1) {
        errno = 0;
        return -1;
    }
    if (dvr_hal.fail == 2) {
        errno = EACCES;
        return -1;
    }
    return 91;
}

static long dvr_hal_write(int fd, const void *data, size_t len)
{
    assert(fd == 91 && data && len == 1);
    dvr_hal.writes++;
    errno = 0;
    return -1;
}

static int dvr_hal_flush(int fd)
{
    (void)fd;
    return 0;
}

static int dvr_hal_close(int fd)
{
    assert(fd == 91);
    dvr_hal.closes++;
    return 0;
}

static void test_dvr_linux_hal(void)
{
    memset(&dvr_hal, 0, sizeof dvr_hal);
    const struct oa_dvr_hal hal = {
        dvr_hal_open, dvr_hal_write, dvr_hal_flush, dvr_hal_close
    };
    oa_dvr_set_hal(&hal);
    const struct oa_dvr_ops *ops = oa_dvr_linux_ops();
    dvr_hal.fail = 1;
    assert(oa_dvr_sink_open(0, 2, ops, NULL) == -EIO);
    assert(dvr_hal.opens == 1 && dvr_hal.mode == 0 && dvr_hal.closes == 0);
    dvr_hal.fail = 2;
    assert(oa_dvr_sink_open(0, 2, ops, NULL) == -EACCES);
    dvr_hal.fail = 0;
    assert(oa_dvr_sink_open(0, 2, ops, NULL) == 0);
    uint8_t packet = 0x47;
    assert(oa_dvr_sink_write(&packet, 1) == -EIO);
    assert(dvr_hal.writes == 1);
    oa_dvr_sink_close();
    assert(dvr_hal.closes == 1 && !oa_dvr_sink_is_open());
    oa_dvr_set_hal(NULL);
}

int main(void)
{
    char dir[] = "/tmp/oa-pvr-test-XXXXXX";
    assert(mkdtemp(dir));
    test_persist(dir);
    test_validation(dir);
    test_capacity(dir);
    test_dvr_sink();
    test_dvr_linux_hal();
    unlink("/tmp/oa-pvr-test-unused");
    char path[OA_PVR_PATH_MAX];
    const char *files[] = { "pvr.db", "bad.db", "validation.db", "full.db" };
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, files[i]);
        unlink(path);
    }
    assert(rmdir(dir) == 0);
    puts("ok pvr stub: id/index semantics, persistence and DVR sink");
    return 0;
}
