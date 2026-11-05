/* Runtime active-list ingestion. See include/openaurora/channels.h for the
 * recovered ACTIVECHANNELLIST shape and limits. */
#include "openaurora/channels.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static struct oa_channel_entry g_entries[OA_CHANNELS_MAX];
static struct oa_channel_snapshot g_snapshot;

static const char *element_end(const char *start)
{
    char quote = 0;
    for (const char *p = start; *p; p++) {
        if (quote) {
            if (*p == quote) quote = 0;
        } else if (*p == '\'' || *p == '"') {
            quote = *p;
        } else if (*p == '>') {
            return p;
        }
    }
    return NULL;
}

static const char *attribute(const char *start, const char *end,
    const char *name)
{
    size_t len = strlen(name);
    for (const char *p = start; p + len + 2 <= end; p++) {
        if (p != start && p[-1] != ' ' && p[-1] != '\t' && p[-1] != '\n' &&
            p[-1] != '\r')
            continue;
        if (strncmp(p, name, len) || p[len] != '=' ||
            (p[len + 1] != '\'' && p[len + 1] != '"'))
            continue;
        return p + len + 2;
    }
    return NULL;
}

static int append_entity(char *out, size_t cap, size_t *used,
    const char *entity)
{
    char value = 0;
    if (!strcmp(entity, "amp")) value = '&';
    else if (!strcmp(entity, "lt")) value = '<';
    else if (!strcmp(entity, "gt")) value = '>';
    else if (!strcmp(entity, "quot")) value = '"';
    else if (!strcmp(entity, "apos")) value = '\'';
    if (!value) return -1;
    if (*used + 1 >= cap) return -1;
    out[(*used)++] = value;
    return 0;
}

static int copy_attribute(const char *start, const char *end, const char *name,
    char *out, size_t cap)
{
    const char *value = attribute(start, end, name);
    if (!value) return -1;
    char quote = value[-1];
    const char *stop = strchr(value, quote);
    if (!stop || stop > end) return -1;
    size_t used = 0;
    for (const char *p = value; p < stop; p++) {
        if (*p == '&') {
            const char *semi = strchr(p, ';');
            if (!semi || semi >= stop) return -1;
            size_t len = (size_t)(semi - p - 1);
            char entity[16];
            if (!len || len >= sizeof entity) return -1;
            memcpy(entity, p + 1, len);
            entity[len] = 0;
            if (append_entity(out, cap, &used, entity)) return -1;
            p = semi;
        } else {
            if (used + 1 >= cap) return -1;
            out[used++] = *p;
        }
    }
    out[used] = 0;
    return 0;
}

static int number_attribute(const char *start, const char *end,
    const char *name, unsigned long long max, uint32_t *out)
{
    char text[32];
    if (copy_attribute(start, end, name, text, sizeof text)) return -1;
    if (!text[0]) return -1;
    char *stop;
    errno = 0;
    unsigned long long value = strtoull(text, &stop, 10);
    if (errno || *stop || value > max) return -1;
    *out = (uint32_t)value;
    return 0;
}

static int parse_service(const char *start, const char *end,
    struct oa_channel_entry *entry)
{
    uint32_t rsn, handle, onid, tsid, sid, type;
    if (number_attribute(start, end, "rsn", UINT32_MAX, &rsn) ||
        number_attribute(start, end, "handle", UINT32_MAX, &handle) ||
        number_attribute(start, end, "onid", UINT16_MAX, &onid) ||
        number_attribute(start, end, "tsid", UINT16_MAX, &tsid) ||
        number_attribute(start, end, "sid", UINT16_MAX, &sid) ||
        number_attribute(start, end, "type", UINT8_MAX, &type))
        return -1;
    memset(entry, 0, sizeof *entry);
    if (copy_attribute(start, end, "name", entry->name, sizeof entry->name))
        return -1;
    entry->rsn = rsn;
    entry->handle = handle;
    entry->onid = (uint16_t)onid;
    entry->tsid = (uint16_t)tsid;
    entry->sid = (uint16_t)sid;
    entry->type = (uint8_t)type;
    return 0;
}

static int parse_xml(const char *xml, size_t len, struct oa_channel_entry *out,
    size_t *count)
{
    if (!xml || !out || !count || !len) return -EINVAL;
    char *copy = malloc(len + 1);
    if (!copy) return -ENOMEM;
    memcpy(copy, xml, len);
    copy[len] = 0;
    const char *list = strstr(copy, "<active_list");
    const char *close = strstr(copy, "</active_list>");
    if (!list || !close || close < list) {
        free(copy);
        return -EINVAL;
    }
    const char *cursor = list;
    size_t rows = 0;
    while ((cursor = strstr(cursor, "<service ")) != NULL && cursor < close) {
        const char *end = element_end(cursor);
        if (!end || end > close) {
            free(copy);
            return -EINVAL;
        }
        if (rows >= OA_CHANNELS_MAX) {
            free(copy);
            return -ENOSPC;
        }
        if (parse_service(cursor, end, &out[rows])) {
            free(copy);
            return -EINVAL;
        }
        rows++;
        cursor = end + 1;
    }
    free(copy);
    *count = rows;
    return 0;
}

static const struct oa_channel_snapshot *channel_snapshot(void)
{
    if (!g_snapshot.count) return NULL;
    return &g_snapshot;
}

static int channel_ingest_xml(const char *xml, size_t len)
{
    struct oa_channel_entry *parsed =
        malloc(sizeof g_entries);
    if (!parsed) return -ENOMEM;
    size_t count = 0;
    int rc = parse_xml(xml, len, parsed, &count);
    if (!rc) {
        if (count) memcpy(g_entries, parsed, count * sizeof *parsed);
        g_snapshot.entries = count ? g_entries : NULL;
        g_snapshot.count = count;
        g_snapshot.source = 1;
        g_snapshot.lcn_known = 0;
        g_snapshot.visibility_known = 0;
    }
    free(parsed);
    return rc ? rc : (int)count;
}

static void channel_clear(void)
{
    memset(&g_snapshot, 0, sizeof g_snapshot);
}

const oa_channel_iface oa_channel_runtime_iface = {
    channel_snapshot, channel_ingest_xml, channel_clear
};
