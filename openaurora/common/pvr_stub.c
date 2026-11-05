/* Offline PVR stub: persistent recording-id store with vendor-shaped
 * start/stop/index-query operations. See include/openaurora/pvr.h. */
#include "openaurora/pvr.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct stub {
    struct oa_pvr_job jobs[OA_PVR_MAX_JOBS];
    size_t count;
    uint32_t next_id;
    char path[OA_PVR_PATH_MAX];
    int open;
};

static struct stub g_stub;

static int valid_name(const char *name)
{
    if (!name) return 0;
    size_t len = strlen(name);
    if (!len || len >= sizeof(((struct oa_pvr_job *)0)->name)) return 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
        if (*p < 0x20 || *p > 0x7e || *p == '\t') return 0;
    return 1;
}

static int parse_u32(const char *text, uint32_t *out)
{
    if (!text || !*text) return -1;
    char *end;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno || *end || value > UINT32_MAX) return -1;
    *out = (uint32_t)value;
    return 0;
}

static int parse_i32(const char *text, int32_t *out)
{
    if (!text || !*text) return -1;
    char *end;
    errno = 0;
    long long value = strtoll(text, &end, 10);
    if (errno || *end || value < INT32_MIN || value > INT32_MAX) return -1;
    *out = (int32_t)value;
    return 0;
}

static int parse_i64(const char *text, int64_t *out)
{
    if (!text || !*text) return -1;
    char *end;
    errno = 0;
    long long value = strtoll(text, &end, 10);
    if (errno || *end) return -1;
    *out = (int64_t)value;
    return 0;
}

static int parse_line(char *line)
{
    char *fields[9];
    int count = 0;
    char *cursor = line;
    for (int i = 0; i < 8; i++) {
        char *tab = strchr(cursor, '\t');
        if (!tab) return -1;
        *tab = 0;
        fields[count++] = cursor;
        cursor = tab + 1;
    }
    fields[count++] = cursor;
    if (strchr(cursor, '\t')) return -1;

    uint32_t id, service, repeat, state;
    int64_t start, end;
    int32_t start_padding, end_padding;
    if (parse_u32(fields[0], &id) || !id ||
        parse_u32(fields[1], &service) || !service ||
        parse_i64(fields[2], &start) || parse_i64(fields[3], &end) ||
        start >= end ||
        parse_u32(fields[4], &repeat) ||
        parse_i32(fields[5], &start_padding) ||
        parse_i32(fields[6], &end_padding) ||
        parse_u32(fields[7], &state) || state > OA_PVR_FAILED ||
        !valid_name(fields[8]))
        return -1;
    for (size_t i = 0; i < g_stub.count; i++)
        if (g_stub.jobs[i].id == id) return -1;
    if (g_stub.count >= OA_PVR_MAX_JOBS) return -1;

    struct oa_pvr_job *job = &g_stub.jobs[g_stub.count++];
    memset(job, 0, sizeof *job);
    job->id = id;
    job->service_handle = service;
    job->start_ms = start;
    job->end_ms = end;
    job->repeat_days = repeat;
    job->start_padding = start_padding;
    job->end_padding = end_padding;
    job->state = (uint8_t)state;
    snprintf(job->name, sizeof job->name, "%s", fields[8]);
    return 0;
}

static int save(void)
{
    if (!g_stub.open) return -EINVAL;
    char tmp[OA_PVR_PATH_MAX + 8];
    if ((size_t)snprintf(tmp, sizeof tmp, "%s.tmp", g_stub.path) >=
        sizeof tmp)
        return -ENAMETOOLONG;
    FILE *file = fopen(tmp, "wb");
    if (!file) return -errno;
    int rc = 0;
    if (fprintf(file, "OAPVR1\n") < 0) rc = -EIO;
    for (size_t i = 0; i < g_stub.count && !rc; i++) {
        const struct oa_pvr_job *job = &g_stub.jobs[i];
        if (fprintf(file,
            "%u\t%u\t%lld\t%lld\t%u\t%d\t%d\t%u\t%s\n",
            job->id, job->service_handle, (long long)job->start_ms,
            (long long)job->end_ms, job->repeat_days, job->start_padding,
            job->end_padding, job->state, job->name) < 0)
            rc = -EIO;
    }
    if (!rc && fflush(file) != 0) rc = -EIO;
    if (!rc && fsync(fileno(file)) != 0) rc = -errno;
    if (fclose(file) != 0 && !rc) rc = -errno;
    if (rc) {
        unlink(tmp);
        return rc;
    }
    if (rename(tmp, g_stub.path) != 0) {
        rc = -errno;
        unlink(tmp);
        return rc;
    }
    return 0;
}

static int stub_open(const char *path)
{
    if (!path || !*path || strlen(path) >= OA_PVR_PATH_MAX - 4)
        return -EINVAL;
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return -EISDIR;
    memset(&g_stub, 0, sizeof g_stub);
    snprintf(g_stub.path, sizeof g_stub.path, "%s", path);
    g_stub.next_id = 1;
    FILE *file = fopen(path, "rb");
    if (!file) {
        if (errno == ENOENT) {
            g_stub.open = 1;
            return 0;
        }
        return -errno;
    }
    char line[1024];
    if (!fgets(line, sizeof line, file)) {
        fclose(file);
        return -EINVAL;
    }
    size_t header_len = strlen(line);
    while (header_len && (line[header_len - 1] == '\n' ||
        line[header_len - 1] == '\r'))
        line[--header_len] = 0;
    if (strcmp(line, "OAPVR1") != 0) {
        fclose(file);
        return -EINVAL;
    }
    uint32_t max_id = 0;
    while (fgets(line, sizeof line, file)) {
        size_t len = strlen(line);
        if (len && line[len - 1] != '\n' && !feof(file)) {
            fclose(file);
            return -EINVAL;
        }
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = 0;
        if (!len) continue;
        if (parse_line(line) != 0) {
            fclose(file);
            return -EINVAL;
        }
        if (g_stub.jobs[g_stub.count - 1].id > max_id)
            max_id = g_stub.jobs[g_stub.count - 1].id;
    }
    if (ferror(file)) {
        fclose(file);
        return -EIO;
    }
    fclose(file);
    if (max_id == UINT32_MAX) return -EINVAL;
    g_stub.next_id = max_id + 1;
    g_stub.open = 1;
    return 0;
}

static void stub_close(void)
{
    g_stub.open = 0;
}

static int stub_start(const struct oa_pvr_request *request,
    uint32_t *recording_id)
{
    if (!g_stub.open || !request || !request->service_handle ||
        request->start_ms >= request->end_ms ||
        !valid_name(request->name))
        return -EINVAL;
    if (g_stub.count >= OA_PVR_MAX_JOBS) return -ENOSPC;
    uint32_t old_next = g_stub.next_id;
    struct oa_pvr_job *job = &g_stub.jobs[g_stub.count++];
    memset(job, 0, sizeof *job);
    job->id = g_stub.next_id++;
    job->service_handle = request->service_handle;
    job->start_ms = request->start_ms;
    job->end_ms = request->end_ms;
    job->repeat_days = request->repeat_days;
    job->start_padding = request->start_padding;
    job->end_padding = request->end_padding;
    job->state = OA_PVR_SCHEDULED;
    snprintf(job->name, sizeof job->name, "%s", request->name);
    int rc = save();
    if (rc) {
        g_stub.count--;
        g_stub.next_id = old_next;
        return rc;
    }
    if (recording_id) *recording_id = job->id;
    return 0;
}

static int stub_stop(uint32_t recording_id)
{
    if (!g_stub.open || !recording_id) return -EINVAL;
    for (size_t i = 0; i < g_stub.count; i++) {
        struct oa_pvr_job *job = &g_stub.jobs[i];
        if (job->id != recording_id) continue;
        if (job->state != OA_PVR_SCHEDULED) return -EALREADY;
        uint8_t previous = job->state;
        job->state = OA_PVR_CANCELED;
        int rc = save();
        if (rc) job->state = previous;
        return rc;
    }
    return -ENOENT;
}

static size_t stub_count(void)
{
    return g_stub.open ? g_stub.count : 0;
}

static int stub_query(size_t index, struct oa_pvr_job *out)
{
    if (!g_stub.open || !out) return -EINVAL;
    if (index >= g_stub.count) return -ENOENT;
    *out = g_stub.jobs[index];
    return 0;
}

static const struct oa_pvr_backend stub_backend = {
    "pvr-stub", stub_open, stub_close, stub_start, stub_stop, stub_count,
    stub_query
};

const struct oa_pvr_backend *oa_pvr_stub_backend(void)
{
    return &stub_backend;
}
